import { spawnSync } from "node:child_process";
import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, arrive, clientBox, sessionDir } from "../session.mjs";

// M4 #1564: a late joiner sees the server's time of day. Client 1 plays alone on the flagship's 120-minute
// day (12x); client 2 joins `--late` real minutes after it (default 3, 36 in-game minutes). Both read their
// OWN copy of the world clock through the `seen` verb's `clock=<game hour>/<day seconds>` token (#1303) and
// must agree within one in-game minute; client 2's clock must then move at the 12x rate over the next
// `--seconds` (default 60), within 10%.
//
// The two reads are not simultaneous, so client 1 is read before and after client 2 and its clock is
// interpolated to the instant client 2 was read (the day is linear in real time).
//
//   clock-late-proof [--minutes 120] [--late 3] [--seconds 60] [--local-default]
//   clock-late-proof --write-config <dir> [--minutes 120]   writes <dir>/config.json for `session up --seed`
//   clock-late-proof --selftest                             the judges on made-up numbers, no game
//
// Pass:  session up --clients 1 --seed <dir>/config.json ; session clock-late-proof
// Red:   the same with --local-default: client 2's reading is the hour its clock would show from its local
//        default (the world's start hour, never advanced by the server) instead of the replicated time,
//        so it fails at "same time".
const DEFAULT_MINUTES = 120;
const DEFAULT_LATE = 3;
const DEFAULT_SECONDS = 60;
const START_HOUR = 8;
const SAME_TIME_HOURS = 1 / 60; // one in-game minute
const RATE_TOLERANCE = 0.1; // a client's clock steps with replication: a 60 s window measured 10.8x on a 12x day (0.90) in the red run

export function configFor(minutes = DEFAULT_MINUTES) {
  return { schemaVersion: 1, dayCycle: { mode: "accelerated", dayLengthMinutes: minutes, startHour: START_HOUR } };
}

// "8.25/7200" -> { hour, daySeconds }; null when the line has no usable clock (no manager reads -1/-1).
export function parseClock(seenLine) {
  const m = /\bclock=(-?[\d.]+)\/(-?[\d.]+)/.exec(String(seenLine));
  if (!m) return null;
  const hour = Number(m[1]);
  const daySeconds = Number(m[2]);
  if (!Number.isFinite(hour) || hour < 0 || hour >= 24.0001) return null;
  return { hour, daySeconds };
}

const wrapDiff = (a, b) => {
  let d = a - b;
  while (d > 12) d -= 24;
  while (d < -12) d += 24;
  return d;
};

// Client 1's hour at real time `at`, from its reads before (a) and after (b) that instant. Wrap-safe.
export function interpolate(a, b, at) {
  const span = b.at - a.at;
  if (!(span > 0)) return a.hour;
  const frac = (at - a.at) / span;
  let h = a.hour + wrapDiff(b.hour, a.hour) * frac;
  while (h < 0) h += 24;
  while (h >= 24) h -= 24;
  return h;
}

// Pure. one = client 1's hour at the instant of client 2's read, two = client 2's hour.
export function judgeSameTime(oneHour, twoHour) {
  const diff = Math.abs(wrapDiff(oneHour, twoHour));
  const minutes = diff * 60;
  const detail = `client 1 reads ${oneHour.toFixed(4)} h, client 2 reads ${twoHour.toFixed(4)} h: ${minutes.toFixed(2)} in-game minutes apart (limit 1)`;
  return { ok: diff <= SAME_TIME_HOURS, detail };
}

// Pure. a and b are { hour, at } reads of client 2 `at` ms apart; expectedMinutes is the configured day length.
export function judgeRate(a, b, expectedMinutes) {
  const realSeconds = (b.at - a.at) / 1000;
  if (!(realSeconds > 0)) return { ok: false, detail: "the two reads are not in order" };
  let moved = b.hour - a.hour;
  if (moved < 0) moved += 24;
  const expectedPerSecond = 24 / (expectedMinutes * 60);
  const ratio = moved / realSeconds / expectedPerSecond;
  const detail = `client 2's clock moved ${moved.toFixed(4)} h in ${realSeconds.toFixed(1)} s = ${(moved / realSeconds / (24 / 86400)).toFixed(2)}x (a ${expectedMinutes}-minute day promises ${(1440 / expectedMinutes).toFixed(2)}x, measured/expected ${ratio.toFixed(3)})`;
  return { ok: Math.abs(ratio - 1) <= RATE_TOLERANCE, detail };
}

function selftest() {
  const fails = [];
  const t0 = 1000000;
  const same = (a, b, want, label) => {
    if (judgeSameTime(a, b).ok !== want) fails.push(`same time ${label}`);
  };
  same(9.5, 9.5, true, "equal");
  same(9.5, 9.5 + 0.9 / 60, true, "under a minute");
  same(9.5, 9.5 + 1.2 / 60, false, "over a minute");
  same(23.99, 0.0, true, "across midnight, 0.6 minutes");
  same(8.0, 8.6, false, "36 minutes apart (the local-default failure)");
  // 12x: 60 s moves 0.2 h
  const rate = (a, b, want, label) => {
    if (judgeRate(a, b, 120).ok !== want) fails.push(`rate ${label}`);
  };
  rate({ hour: 9, at: t0 }, { hour: 9.2, at: t0 + 60000 }, true, "12x");
  rate({ hour: 9, at: t0 }, { hour: 9.208, at: t0 + 60000 }, true, "4% fast");
  rate({ hour: 9, at: t0 }, { hour: 9.216, at: t0 + 60000 }, true, "8% fast");
  rate({ hour: 9, at: t0 }, { hour: 9.24, at: t0 + 60000 }, false, "20% fast");
  rate({ hour: 9, at: t0 }, { hour: 9 + 1 / 60, at: t0 + 60000 }, false, "1x");
  rate({ hour: 9, at: t0 }, { hour: 9, at: t0 + 60000 }, false, "frozen");
  rate({ hour: 23.9, at: t0 }, { hour: 0.1, at: t0 + 60000 }, true, "across midnight");
  const mid = interpolate({ hour: 10, at: t0 }, { hour: 10.02, at: t0 + 10000 }, t0 + 5000);
  if (Math.abs(mid - 10.01) > 1e-9) fails.push(`interpolate midpoint ${mid}`);
  const wrapMid = interpolate({ hour: 23.99, at: t0 }, { hour: 0.01, at: t0 + 10000 }, t0 + 5000);
  if (Math.abs(wrapMid) > 1e-9 && Math.abs(wrapMid - 24) > 1e-9) fails.push(`interpolate across midnight ${wrapMid}`);
  if (parseClock("a clock=8.5/7200")?.hour !== 8.5) fails.push("parseClock hour");
  if (parseClock("a clock=-1/-1") !== null) fails.push("parseClock no manager");
  if (parseClock("no token") !== null) fails.push("parseClock missing token");
  if (configFor(60).dayCycle.dayLengthMinutes !== 60) fails.push("configFor");
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "clock-late-proof selftest: FAIL" : "clock-late-proof selftest: PASS");
  return fails.length ? 1 : 0;
}

async function runClockLate(args) {
  if (args.includes("--selftest")) return selftest();
  const val = (f, d) => (args.includes(f) ? args[args.indexOf(f) + 1] : d);
  const minutes = Number(val("--minutes", DEFAULT_MINUTES));
  if (args.includes("--write-config")) {
    const dir = val("--write-config", "");
    if (!dir) { console.log("usage: clock-late-proof --write-config <dir> [--minutes N]"); return 2; }
    mkdirSync(dir, { recursive: true });
    const file = join(dir, "config.json");
    writeFileSync(file, JSON.stringify(configFor(minutes), null, 2) + "\n");
    console.log(`wrote ${file}`);
    return 0;
  }
  const late = Number(val("--late", DEFAULT_LATE));
  const seconds = Number(val("--seconds", DEFAULT_SECONDS));
  const localDefault = args.includes("--local-default");
  if (!(minutes > 0) || !(late >= 0) || !(seconds >= 5)) { console.log("FAIL  --minutes must be positive, --late not negative, --seconds at least 5"); return 2; }

  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    return h.sendCommand(spec, timeout);
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const done = () => {
    for (const f of fails) console.log(`FAIL  ${f}`);
    console.log(fails.length
      ? "clock-late-proof: FAIL"
      : `clock-late-proof: PASS (a client that joined ${late} min late reads the server's time within 1 in-game minute and sees it run at the ${minutes}-minute day's rate)`);
    return fails.length ? 1 : 0;
  };

  // The round trip's midpoint stands for the instant the client read its clock.
  const read = (n, label) => {
    const before = Date.now();
    const r = on(n, { verb: "seen" }, 15000);
    const after = Date.now();
    const clock = r.status === "ok" ? parseClock(r.detail) : null;
    console.log(`client ${n} ${label}: ${r.status} ${clock ? `clock=${clock.hour.toFixed(4)}h day_s=${clock.daySeconds}` : r.detail}`);
    return clock ? { hour: clock.hour, daySeconds: clock.daySeconds, at: (before + after) / 2 } : null;
  };

  arrive(on, 1, wait);
  wait(2000);
  const startRead = read(1, "first read");
  if (!startRead) { fails.push("clock read: client 1 has no readable clock (no character, no time manager, or a build without the seen clock token, #1303)"); return done(); }
  console.log(`client 1 day length: ${startRead.daySeconds} s (config expects ${minutes * 60} s)`);
  if (Math.abs(startRead.daySeconds - minutes * 60) > 1) fails.push(`day length: client 1 was told a ${startRead.daySeconds} s day, the config promises ${minutes * 60} s (seed the session with --write-config's file)`);

  console.log(`waiting ${late} real minutes before client 2 joins`);
  wait(late * 60000);
  const before = read(1, "read before client 2 joins");
  const joined = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "join", "2"], { windowsHide: true, stdio: "inherit" });
  if (joined.status !== 0) { fails.push("join: client 2 did not join late"); return done(); }
  arrive(on, 2, wait);
  wait(2000);

  const twoReal = read(2, "first read");
  const after = read(1, "read after client 2's first read");
  if (!twoReal || !before || !after) { fails.push("clock read: a client lost its clock"); return done(); }
  let two = twoReal;
  if (localDefault) {
    // Red: take client 2's time from its local default (the world's start hour) instead of what it received.
    console.log(`--local-default: client 2's reading replaced by the local default ${startRead.hour.toFixed(4)} h`);
    two = { ...twoReal, hour: startRead.hour };
  }
  const oneAtTwo = interpolate(before, after, two.at);
  const same = judgeSameTime(oneAtTwo, two.hour);
  console.log(same.detail);
  if (!same.ok) fails.push(`same time: ${same.detail}`);

  wait(seconds * 1000);
  const twoLater = read(2, `read ${seconds} s later`);
  if (!twoLater) { fails.push("clock read: client 2 lost its clock"); return done(); }
  const rate = judgeRate(twoReal, twoLater, minutes);
  console.log(rate.detail);
  if (!rate.ok) fails.push(`rate: ${rate.detail}`);
  return done();
}

export default {
  name: "clock-late-proof",
  noReset: true,
  usage: "clock-late-proof [--minutes 120] [--late 3] [--seconds 60] [--local-default] | --write-config <dir> | --selftest   (session up --clients 1 --seed <dir>/config.json)",
  run: (ctx) => runClockLate(ctx.args ?? []),
};
