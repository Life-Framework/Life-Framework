import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, newestLog, sessionDir, sessionPaths } from "../session.mjs";

// M4 #1565 (#129's second harness line): a session booted with the day cycle in `realtime` mode shows the
// in-game time of day equal to the host's clock, within one minute, and advancing at 1x.
//
// The server applies `dayCycle` from the owner config.json at start (#1301); in `realtime` it starts the world
// clock at its own local time of day (plus `clockOffsetMinutes`) and runs a 24 h day, 1x. This reads the CLIENT's
// copy of the clock through the `seen` verb's `clock=<hour>/<day seconds>` token (#1303), every 3 s for
// `--seconds` (default 60), and judges two things against the clock of the machine the proof runs on (a proof
// on a pool machine runs where its server is, so that is the host's clock):
//   "matches the host clock"  the first read is within one in-game minute of the host's time of day (+ offset)
//   "advances at 1x"          the least-squares slope of all reads is 1x a 24 h day, within 10%
//
//   day-realtime-proof [--seconds 60] [--offset 0]     (--offset = the clockOffsetMinutes the session was seeded with)
//   day-realtime-proof --write-config <realtime|accelerated> <dir> [--offset 0]
//                                                       writes <dir>/config.json for `session up --seed <dir>/config.json`
//   day-realtime-proof --selftest                       the judges on made-up numbers, no game
//
// Pass:  --write-config realtime <dir> ; session up --clients 1 --seed <dir>/config.json ; session day-realtime-proof
// Red:   --write-config accelerated <dir> (a 120-minute day that starts 6 h away from the host's time of day)
//        ; the same up and proof: it must FAIL at "matches the host clock".
const MATCH_TOLERANCE_HOURS = 1 / 60; // one in-game minute
const RATE_TOLERANCE = 0.1; // a client's clock steps with replication; 10% of 1x over a minute is a 6 s lag
const DEFAULT_SECONDS = 60;
const SAMPLE_GAP_MS = 3000;
const RED_DAY_MINUTES = 120;
const RED_HOURS_AWAY = 6;

// The host's time of day in hours (0 to 24) at real time `ms`, local time, plus the configured offset.
export function hostHour(ms, offsetMinutes = 0) {
  const d = new Date(ms);
  const h = d.getHours() + d.getMinutes() / 60 + d.getSeconds() / 3600 + d.getMilliseconds() / 3600000 + offsetMinutes / 60;
  return ((h % 24) + 24) % 24;
}

// Config the session is seeded with: only the day cycle, so no restart time or autosave moves under the run.
// The accelerated (red) one starts RED_HOURS_AWAY from `nowMs`' time of day so it cannot match by luck.
export function configFor(mode, offsetMinutes = 0, nowMs = Date.now()) {
  if (mode === "accelerated") {
    const startHour = Math.round(hostHour(nowMs, offsetMinutes + RED_HOURS_AWAY * 60) * 100) / 100;
    return { schemaVersion: 1, dayCycle: { mode, dayLengthMinutes: RED_DAY_MINUTES, startHour } };
  }
  const dayCycle = { mode };
  if (offsetMinutes) dayCycle.clockOffsetMinutes = offsetMinutes;
  return { schemaVersion: 1, dayCycle };
}

// "8.25/86400" -> { hour: 8.25, daySeconds: 86400 }; null when the line has no usable clock.
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

// Pure. A client read { hour, at } against the host's time of day at the same instant, wrap-safe across midnight.
export function judgeMatch(read, offsetMinutes = 0) {
  const host = hostHour(read.at, offsetMinutes);
  const diff = wrapDiff(read.hour, host);
  const detail = `the client's clock reads ${read.hour.toFixed(4)} h, the host's ${host.toFixed(4)} h (${(diff * 60).toFixed(2)} in-game minutes apart, at most ${(MATCH_TOLERANCE_HOURS * 60).toFixed(0)})`;
  return { ok: Math.abs(diff) <= MATCH_TOLERANCE_HOURS, reason: detail, diffHours: diff };
}

// Pure. samples are [{ hour, at }] (at = real ms of the read), at least two, in time order. The rate is the
// least-squares slope of the unwrapped hour against real time (a client's copy lags in steps, so two reads
// are not enough); 1x is 24 h per 86400 s.
export function judgeRate(samples) {
  if (!Array.isArray(samples) || samples.length < 2) return { ok: false, reason: "fewer than two clock reads" };
  const t0 = samples[0].at;
  const xs = [];
  const ys = [];
  let wrap = 0;
  for (let i = 0; i < samples.length; i++) {
    if (i > 0 && samples[i].hour + wrap < ys[i - 1] - 12) wrap += 24;
    xs.push((samples[i].at - t0) / 1000);
    ys.push(samples[i].hour + wrap);
  }
  const realSeconds = xs[xs.length - 1];
  if (!(realSeconds > 0)) return { ok: false, reason: "the reads are not in order" };
  const mx = xs.reduce((a, b) => a + b, 0) / xs.length;
  const my = ys.reduce((a, b) => a + b, 0) / ys.length;
  let num = 0;
  let den = 0;
  for (let i = 0; i < xs.length; i++) {
    num += (xs[i] - mx) * (ys[i] - my);
    den += (xs[i] - mx) ** 2;
  }
  const speed = (den > 0 ? num / den : 0) / (24 / 86400);
  const moved = ys[ys.length - 1] - ys[0];
  const detail = `${samples.length} reads over ${realSeconds.toFixed(1)} s: the clock moved ${moved.toFixed(5)} h, slope ${speed.toFixed(3)}x real time (1x within ${RATE_TOLERANCE * 100}%)`;
  return { ok: Math.abs(speed - 1) <= RATE_TOLERANCE, reason: detail, speed };
}

// A made-up run: `n` reads `step` s apart on a clock moving `x` times real time, starting at `startMs`, with `lagH` hours of lag on odd reads.
function series(n, step, x, startMs, startOffsetH = 0, lagH = 0) {
  const out = [];
  for (let i = 0; i < n; i++) {
    const at = startMs + i * step * 1000;
    const hour = (hostHour(startMs) + startOffsetH + (i * step * x) / 3600 - (i % 2 ? lagH : 0) + 48) % 24;
    out.push({ hour, at });
  }
  return out;
}

function selftest() {
  const fails = [];
  const noon = new Date(2026, 10, 15, 12, 0, 0).getTime();
  const nearMidnight = new Date(2026, 10, 15, 23, 59, 30).getTime();
  const check = (label, got, want) => { if (got !== want) fails.push(`${label}: expected ${want}, got ${got}`); };

  // matches the host clock
  check("exact match", judgeMatch({ hour: 12, at: noon }).ok, true);
  check("30 s ahead", judgeMatch({ hour: 12 + 30 / 3600, at: noon }).ok, true);
  check("2 min ahead", judgeMatch({ hour: 12 + 2 / 60, at: noon }).ok, false);
  check("6 h away (the accelerated red seed)", judgeMatch({ hour: 18, at: noon }).ok, false);
  check("across midnight", judgeMatch({ hour: 0.002, at: nearMidnight }).ok, true);
  check("offset +90 min", judgeMatch({ hour: 13.5, at: noon }, 90).ok, true);
  check("offset ignored", judgeMatch({ hour: 12, at: noon }, 90).ok, false);

  // advances at 1x
  check("1x", judgeRate(series(21, 3, 1, noon)).ok, true);
  check("1x across midnight", judgeRate(series(21, 3, 1, nearMidnight)).ok, true);
  check("lag on odd reads averages out", judgeRate(series(21, 3, 1, noon, 0, 0.0005)).ok, true);
  check("12x (the flagship day)", judgeRate(series(21, 3, 12, noon)).ok, false);
  check("frozen", judgeRate(series(21, 3, 0, noon)).ok, false);
  check("15% fast", judgeRate(series(21, 3, 1.15, noon)).ok, false);
  check("one read", judgeRate([{ hour: 8, at: 1 }]).ok, false);

  // config and parsing
  check("realtime config mode", configFor("realtime").dayCycle.mode, "realtime");
  check("realtime config has no offset by default", "clockOffsetMinutes" in configFor("realtime").dayCycle, false);
  check("realtime config offset", configFor("realtime", 45).dayCycle.clockOffsetMinutes, 45);
  const red = configFor("accelerated", 0, noon).dayCycle;
  check("red config mode", red.mode, "accelerated");
  check("red config starts 6 h from the host", red.startHour, 18);
  check("red config does not match", judgeMatch({ hour: red.startHour, at: noon }).ok, false);
  check("parseClock hour", parseClock("x clock=8.5/86400")?.hour, 8.5);
  check("parseClock no manager", parseClock("x clock=-1/-1"), null);
  check("parseClock missing token", parseClock("no token"), null);

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "day-realtime-proof selftest: FAIL" : "day-realtime-proof selftest: PASS");
  return fails.length ? 1 : 0;
}

async function runDayRealtime(args) {
  const val = (f, d) => (args.includes(f) ? args[args.indexOf(f) + 1] : d);
  if (args.includes("--selftest")) return selftest();
  const offset = Number(val("--offset", 0));
  if (!Number.isFinite(offset)) { console.log("FAIL  --offset must be a number of minutes"); return 2; }
  if (args.includes("--write-config")) {
    const at = args.indexOf("--write-config");
    const mode = args[at + 1];
    const dir = args[at + 2];
    if (!["realtime", "accelerated"].includes(mode) || !dir) { console.log("usage: day-realtime-proof --write-config <realtime|accelerated> <dir> [--offset N]"); return 2; }
    mkdirSync(dir, { recursive: true });
    const file = join(dir, "config.json");
    writeFileSync(file, JSON.stringify(configFor(mode, offset), null, 2) + "\n");
    console.log(`wrote ${file}`);
    return 0;
  }
  const seconds = Number(val("--seconds", DEFAULT_SECONDS));
  if (!(seconds >= 5)) { console.log("FAIL  --seconds must be at least 5"); return 2; }

  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    return h.sendCommand(spec, timeout);
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);

  // What the server said it applied (informational: the client's own clock is the judge).
  const serverFile = newestLog(join(sessionPaths(dir).server, "logs"));
  if (serverFile) {
    const lines = readFileSync(serverFile, "utf8").split("\n").filter((l) => /\[LFDebug:Atmosphere\] day cycle/.test(l));
    for (const l of lines) console.log(`server: ${l.replace(/^.*\[LFDebug:Atmosphere\]\s*/, "").trim()}`);
    if (!lines.length) console.log("server: no `day cycle` line in the newest console.log (the config was not read, or the log is not flushed yet)");
  }

  arrive((n, spec, timeout) => on(n, spec, timeout), 1, wait);
  wait(2000);

  // Midpoint of the round trip stands for the instant the client read its clock.
  const read = (label) => {
    const before = Date.now();
    const r = on(1, { verb: "seen" }, 15000);
    const after = Date.now();
    const clock = r.status === "ok" ? parseClock(r.detail) : null;
    console.log(`client 1 ${label}: ${r.status} ${clock ? `clock=${clock.hour}h day_s=${clock.daySeconds}` : r.detail}`);
    return clock ? { hour: clock.hour, daySeconds: clock.daySeconds, at: (before + after) / 2 } : null;
  };

  const first = read("first read");
  if (!first) { console.log("FAIL  the client has no readable clock (no character, or no time manager)"); console.log("day-realtime-proof: FAIL"); return 1; }
  const match = judgeMatch(first, offset);
  console.log(match.reason);
  if (!match.ok) console.log("FAIL  matches the host clock");

  // One read every ~SAMPLE_GAP_MS until `seconds` have passed since the first, then the slope of them all.
  const samples = [first];
  let last = first;
  while (last.at - first.at < seconds * 1000) {
    wait(SAMPLE_GAP_MS);
    const s = read(`read at +${Math.round((Date.now() - first.at) / 1000)} s`);
    if (!s) { console.log("FAIL  the client lost its clock"); console.log("day-realtime-proof: FAIL"); return 1; }
    samples.push(s);
    last = s;
  }
  const rate = judgeRate(samples);
  console.log(rate.reason);
  if (!rate.ok) console.log("FAIL  advances at 1x");

  if (!match.ok || !rate.ok) { console.log("day-realtime-proof: FAIL"); return 1; }
  console.log("day-realtime-proof: PASS (the client's clock matches the host's within a minute and runs at 1x)");
  return 0;
}

export default {
  name: "day-realtime-proof",
  noReset: true,
  usage: "day-realtime-proof [--seconds 60] [--offset 0] | --write-config <realtime|accelerated> <dir> | --selftest   (session up --clients 1 --seed <dir>/config.json)",
  run: (ctx) => runDayRealtime(ctx.args ?? []),
};
