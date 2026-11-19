import { spawnSync } from "node:child_process";
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, arrive, clientBox, newestLog, sessionDir, sessionPaths } from "../session.mjs";

// M5 #3185: the clocks of a dedicated server's clients follow the faster night (#3149), a late joiner's too.
// The server switches the vanilla manager's day duration at dusk and dawn (LF_DayCycleApply.SwitchPhase); a client
// extrapolates time from the duration it was told, so a client might drift or jump at the flip, and one that joins
// at night might run at the day rate. Run in four steps (extends clock-late-proof):
//   1. the session is seeded with a short day and `nightSpeed` 2 (`--write-config`), starting an hour before dusk;
//   2. client 1 plays across dusk: its day length token halves (day seconds / nightSpeed) when the flip reaches it;
//   3. client 2 joins during the night;
//   4. over `--seconds` (default 90) both clients are read in turn; each client's hour must stay within `--limit`
//      (default 2) in-game minutes of the SERVER's clock, and each client's slope must be nightSpeed times the day
//      rate, within 10%.
// The server's clock has no harness box, so it is reconstructed from its own console.log: the line
// `day cycle: night at <h> h (...)` gives the hour at the flip and the log time stamp the instant, and the night
// runs at the configured rate from there (the server clock is linear in real time within a phase).
//
//   clock-night-proof [--minutes 120] [--night 2] [--start 17] [--seconds 90] [--limit 2] [--dusk-wait 15]
//   clock-night-proof --write-config <dir> [--minutes 120] [--night 2] [--start 17]   for `session up --seed`
//   clock-night-proof --selftest                                                     the judges, no game
//
// Run:  session up --clients 1 --seed <dir>/config.json ; session clock-night-proof
const DEFAULT_MINUTES = 120;
const DEFAULT_NIGHT = 2;
const DEFAULT_START = 12.5; // the test world's manager reports sunset 13.36 h and sunrise 1.55 h on a dedicated server (2026-11-19)
const DEFAULT_SECONDS = 90;
const DEFAULT_LIMIT_MINUTES = 2;
const DEFAULT_DUSK_WAIT_MIN = 15;
const RATE_TOLERANCE = 0.1;
const SAMPLE_GAP_MS = 2500;

export function configFor(minutes = DEFAULT_MINUTES, nightSpeed = DEFAULT_NIGHT, startHour = DEFAULT_START) {
  return { schemaVersion: 1, dayCycle: { mode: "accelerated", dayLengthMinutes: minutes, nightSpeed, startHour } };
}

// "8.25/7200" -> { hour, daySeconds }; null when the line has no usable clock.
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

// The LAST night flip a server console.log reports: { hour, msOfDay } or null. Engine lines start `HH:MM:SS.mmm`.
export function parseNightFlip(logText) {
  let found = null;
  for (const line of String(logText).split(/\r?\n/)) {
    const m = /^\s*(\d{1,2}):(\d{2}):(\d{2})(?:\.(\d+))?.*day cycle: night at ([\d.]+) h/.exec(line);
    if (!m) continue;
    const ms = Number((m[4] ?? "0").padEnd(3, "0").slice(0, 3));
    found = { hour: Number(m[5]), msOfDay: ((Number(m[1]) * 60 + Number(m[2])) * 60 + Number(m[3])) * 1000 + ms };
  }
  return found;
}

// Epoch ms of a log time-of-day nearest to `nearEpoch` (the log has no date; the flip is within the last day).
export function flipEpoch(msOfDay, nearEpoch) {
  const d = new Date(nearEpoch);
  let t = new Date(d.getFullYear(), d.getMonth(), d.getDate()).getTime() + msOfDay;
  if (t > nearEpoch + 60000) t -= 86400000;
  return t;
}

// Pure. The server's hour at real time `at`, in the night phase that started at `flipAt` at hour `flipHour`;
// the night runs at `nightHoursPerSecond`.
export function serverHour(flipHour, flipAt, nightHoursPerSecond, at) {
  let h = flipHour + ((at - flipAt) / 1000) * nightHoursPerSecond;
  h %= 24;
  if (h < 0) h += 24;
  return h;
}

// Pure. A client's hour against the server's.
export function judgeWithServer(label, clientHour, serverHr, limitMinutes) {
  const minutes = Math.abs(wrapDiff(clientHour, serverHr)) * 60;
  const detail = `${label} reads ${clientHour.toFixed(4)} h, the server's clock is ${serverHr.toFixed(4)} h: ${minutes.toFixed(2)} in-game minutes apart (limit ${limitMinutes})`;
  return { ok: minutes <= limitMinutes, detail, minutes };
}

// Pure. samples are [{ hour, at }] in time order; the slope of the unwrapped hour against real time (least squares:
// a client's clock steps with replication) against `expectedPerSecond` game hours per real second.
export function judgeSlope(label, samples, expectedPerSecond, tolerance = RATE_TOLERANCE) {
  if (!Array.isArray(samples) || samples.length < 3) return { ok: false, detail: `${label}: fewer than three clock reads` };
  const t0 = samples[0].at;
  const xs = [];
  const ys = [];
  let wrap = 0;
  for (let i = 0; i < samples.length; i++) {
    if (i > 0 && samples[i].hour + wrap < ys[i - 1] - 12) wrap += 24;
    xs.push((samples[i].at - t0) / 1000);
    ys.push(samples[i].hour + wrap);
  }
  if (!(xs[xs.length - 1] > 0)) return { ok: false, detail: `${label}: the reads are not in order` };
  const mx = xs.reduce((a, b) => a + b, 0) / xs.length;
  const my = ys.reduce((a, b) => a + b, 0) / ys.length;
  let num = 0;
  let den = 0;
  for (let i = 0; i < xs.length; i++) {
    num += (xs[i] - mx) * (ys[i] - my);
    den += (xs[i] - mx) ** 2;
  }
  const slope = den > 0 ? num / den : 0;
  const ratio = slope / expectedPerSecond;
  const detail = `${label}: ${samples.length} reads over ${xs[xs.length - 1].toFixed(1)} s, ${(slope / (24 / 86400)).toFixed(2)}x a 24 h day (expected ${(expectedPerSecond / (24 / 86400)).toFixed(2)}x, measured/expected ${ratio.toFixed(3)})`;
  return { ok: Math.abs(ratio - 1) <= tolerance, detail, ratio, slope };
}

// Pure. The day length a client was told must be the night's after the flip: the day's divided by nightSpeed.
export function judgeNightDuration(label, daySeconds, minutes, nightSpeed) {
  const want = (minutes * 60) / nightSpeed;
  return { ok: Math.abs(daySeconds - want) <= 1, detail: `${label} was told a ${daySeconds} s day, the night promises ${want} s` };
}

function selftest() {
  const fails = [];
  const t0 = 1000000;
  // 120-minute day: 0.2 h/s... = 12x -> 0.2 h per 60 s; night 24x -> 0.4 h per 60 s
  const perSecond = (x) => (x * 24) / 86400;
  const series = (n, step, x, start = 21, lag = 0) => {
    const out = [];
    for (let i = 0; i < n; i++) out.push({ hour: (start + i * step * perSecond(x) - (i % 2 ? lag : 0) + 24) % 24, at: t0 + i * step * 1000 });
    return out;
  };
  const slope = (s, want, label) => {
    if (judgeSlope("c", s, perSecond(24)).ok !== want) fails.push(`slope ${label}`);
  };
  slope(series(30, 3, 24), true, "24x");
  slope(series(30, 3, 24, 21, 0.01), true, "24x with replication lag");
  slope(series(30, 3, 12), false, "12x (the day rate at night)");
  slope(series(30, 3, 27), false, "27x");
  slope(series(30, 3, 24, 23.5), true, "across midnight");
  if (judgeSlope("c", series(2, 3, 24), perSecond(24)).ok) fails.push("slope needs three reads");
  if (judgeWithServer("c", 21, 21 + 1.5 / 60, 2).ok !== true) fails.push("server within limit");
  if (judgeWithServer("c", 21, 21 + 2.5 / 60, 2).ok !== false) fails.push("server over limit");
  if (judgeWithServer("c", 23.99, 0.0, 2).ok !== true) fails.push("server across midnight");
  if (!judgeNightDuration("c", 3600, 120, 2).ok) fails.push("night duration ok");
  if (judgeNightDuration("c", 7200, 120, 2).ok) fails.push("day duration after dusk must fail");
  const log = "12:00:01.000  DEFAULT : x\n20:15:30.250  SCRIPT : [LFDebug:Atmosphere] day cycle: night at 19.02 h (sunrise 5, sunset 19), a day now lasts 3600 s\n";
  const flip = parseNightFlip(log);
  if (!flip || flip.hour !== 19.02 || flip.msOfDay !== ((20 * 60 + 15) * 60 + 30) * 1000 + 250) fails.push(`parseNightFlip ${JSON.stringify(flip)}`);
  if (parseNightFlip("20:15:30.250 SCRIPT : day cycle: day at 5 h") !== null) fails.push("parseNightFlip must ignore a day flip");
  const noon = new Date(2026, 10, 19, 20, 20, 0).getTime();
  if (flipEpoch(flip.msOfDay, noon) !== new Date(2026, 10, 19, 20, 15, 30, 250).getTime()) fails.push("flipEpoch same day");
  const justAfterMidnight = new Date(2026, 10, 20, 0, 5, 0).getTime();
  if (flipEpoch(flip.msOfDay, justAfterMidnight) !== new Date(2026, 10, 19, 20, 15, 30, 250).getTime()) fails.push("flipEpoch previous day");
  const sh = serverHour(19, 0, perSecond(24), 60000);
  if (Math.abs(sh - 19.4) > 1e-9) fails.push(`serverHour ${sh}`);
  if (parseClock("a clock=21.5/3600")?.daySeconds !== 3600) fails.push("parseClock");
  if (configFor().dayCycle.nightSpeed !== 2) fails.push("configFor");
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "clock-night-proof selftest: FAIL" : "clock-night-proof selftest: PASS");
  return fails.length ? 1 : 0;
}

async function runClockNight(args) {
  if (args.includes("--selftest")) return selftest();
  const val = (f, d) => (args.includes(f) ? args[args.indexOf(f) + 1] : d);
  const minutes = Number(val("--minutes", DEFAULT_MINUTES));
  const night = Number(val("--night", DEFAULT_NIGHT));
  const start = Number(val("--start", DEFAULT_START));
  if (args.includes("--write-config")) {
    const dir = val("--write-config", "");
    if (!dir) { console.log("usage: clock-night-proof --write-config <dir> [--minutes N] [--night N] [--start H]"); return 2; }
    mkdirSync(dir, { recursive: true });
    const file = join(dir, "config.json");
    writeFileSync(file, JSON.stringify(configFor(minutes, night, start), null, 2) + "\n");
    console.log(`wrote ${file}`);
    return 0;
  }
  const seconds = Number(val("--seconds", DEFAULT_SECONDS));
  const limit = Number(val("--limit", DEFAULT_LIMIT_MINUTES));
  const duskWait = Number(val("--dusk-wait", DEFAULT_DUSK_WAIT_MIN));
  if (!(minutes > 0) || !(night > 1) || !(seconds >= 20) || !(limit > 0)) { console.log("FAIL  --minutes must be positive, --night above 1, --seconds at least 20, --limit positive"); return 2; }

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
      ? "clock-night-proof: FAIL"
      : `clock-night-proof: PASS (a client that played through dusk and one that joined at night both stay within ${limit} in-game minutes of the server and run the night at ${night}x the day rate)`);
    return fails.length ? 1 : 0;
  };
  const read = (n, label, quiet = false) => {
    const before = Date.now();
    const r = on(n, { verb: "seen" }, 15000);
    const after = Date.now();
    const clock = r.status === "ok" ? parseClock(r.detail) : null;
    if (!quiet) console.log(`client ${n} ${label}: ${r.status} ${clock ? `clock=${clock.hour.toFixed(4)}h day_s=${clock.daySeconds}` : r.detail}`);
    return clock ? { hour: clock.hour, daySeconds: clock.daySeconds, at: (before + after) / 2 } : null;
  };

  // Sunrise and sunset as the server's manager reports them in its flip line, for a failure message.
  const logSunTimes = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    const m = f ? /\(sunrise ([\d.]+), sunset ([\d.]+)\)[^\n]*$/m.exec(readFileSync(f, "utf8")) : null;
    return m ? `sunrise ${m[1]} h, sunset ${m[2]} h` : "";
  };

  const dayHoursPerSecond =24 / (minutes * 60);
  const nightHoursPerSecond = dayHoursPerSecond * night;

  arrive(on, 1, wait);
  wait(2000);
  const first = read(1, "first read");
  if (!first) { fails.push("clock read: client 1 has no readable clock (no character, no time manager, or a build without the seen clock token, #1303)"); return done(); }
  if (Math.abs(first.daySeconds - minutes * 60) > 1 && Math.abs(first.daySeconds - (minutes * 60) / night) > 1) fails.push(`day length: client 1 was told a ${first.daySeconds} s day, the config promises ${minutes * 60} s (seed the session with --write-config's file)`);

  // Step 2: wait for dusk to reach client 1: its told day length drops to the night's.
  const nightSeconds = (minutes * 60) / night;
  const daySamples = [];
  const deadline = Date.now() + duskWait * 60000;
  if (first.daySeconds <= nightSeconds + 1) {
    const sun = logSunTimes();
    fails.push(`dusk: client 1 was already on the night's ${nightSeconds} s day at hour ${first.hour.toFixed(2)} on its first read, so no dusk was crossed${sun ? ` (the server reports ${sun})` : ""}: seed an earlier --start (an hour or so before sunset) with --write-config`);
    return done();
  }
  let crossed = null;
  while (!crossed && Date.now() < deadline) {
    const r = read(1, "waiting for dusk", true);
    if (r) {
      daySamples.push(r);
      if (r.daySeconds <= nightSeconds + 1) crossed = r;
    }
    wait(3000);
  }
  if (!crossed) { fails.push(`dusk: client 1's day length never changed to the night's ${nightSeconds} s within ${duskWait} min (it is ${daySamples.length ? daySamples[daySamples.length - 1].daySeconds : first.daySeconds} s, hour ${(daySamples.length ? daySamples[daySamples.length - 1] : first).hour.toFixed(2)}): the flip did not reach the client, or the start hour is too early (--start)`); return done(); }
  console.log(`dusk reached client 1 at hour ${crossed.hour.toFixed(4)} (day length ${crossed.daySeconds} s)`);

  // The day rate client 1 measured on the way, only when the day lasted long enough to say anything.
  const dayPart = daySamples.filter((s) => s.daySeconds > nightSeconds + 1);
  if (dayPart.length >= 6 && dayPart[dayPart.length - 1].at - dayPart[0].at >= 20000) {
    const dayRate = judgeSlope("client 1 by day", dayPart, dayHoursPerSecond);
    console.log(dayRate.detail);
    if (!dayRate.ok) fails.push(`day rate: ${dayRate.detail}`);
  }

  // Step 3: client 2 joins during the night.
  const joined = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "join", "2"], { windowsHide: true, stdio: "inherit" });
  if (joined.status !== 0) { fails.push("join: client 2 did not join at night"); return done(); }
  arrive(on, 2, wait);
  wait(2000);

  // The server's clock, rebuilt from its console.log flip line.
  const logFile = newestLog(join(sessionPaths(dir).server, "logs"));
  const flip = logFile ? parseNightFlip(readFileSync(logFile, "utf8")) : null;
  const flipAt = flip ? flipEpoch(flip.msOfDay, Date.now()) : 0;
  if (!flip) fails.push(`server clock: no "day cycle: night at <h> h" line in ${logFile ?? "the server's console.log"} (the server never switched to the night duration, or the log is not where the session says)`);
  else console.log(`server flipped to night at ${flip.hour} h (log time stamp ${new Date(flipAt).toISOString()})`);

  // Step 4: read both clients in turn for `seconds`.
  const one = [];
  const two = [];
  const stop = Date.now() + seconds * 1000;
  while (Date.now() < stop) {
    const a = read(1, "night", true);
    const b = read(2, "night", true);
    if (a) one.push(a);
    if (b) two.push(b);
    wait(SAMPLE_GAP_MS);
  }
  if (!two.length) { fails.push("clock read: client 2 never had a readable clock"); return done(); }
  for (const [label, list] of [["client 1", one], ["client 2", two]]) {
    const last = list[list.length - 1];
    if (!last) { fails.push(`clock read: ${label} lost its clock`); continue; }
    const dur = judgeNightDuration(label, list[0].daySeconds, minutes, night);
    console.log(dur.detail);
    if (!dur.ok) fails.push(`night duration: ${dur.detail}`);
    if (flip) {
      let worst = null;
      for (const s of list) {
        const j = judgeWithServer(label, s.hour, serverHour(flip.hour, flipAt, nightHoursPerSecond, s.at), limit);
        if (!worst || j.minutes > worst.minutes) worst = j;
      }
      console.log(`worst of ${list.length}: ${worst.detail}`);
      if (!worst.ok) fails.push(`server clock: ${worst.detail}`);
    }
    const rate = judgeSlope(`${label} at night`, list, nightHoursPerSecond);
    console.log(rate.detail);
    if (!rate.ok) fails.push(`night rate: ${rate.detail}`);
  }
  return done();
}

export default {
  name: "clock-night-proof",
  noReset: true,
  usage: "clock-night-proof [--minutes 120] [--night 2] [--start 17] [--seconds 90] [--limit 2] [--dusk-wait 15] | --write-config <dir> | --selftest   (session up --clients 1 --seed <dir>/config.json)",
  run: (ctx) => runClockNight(ctx.args ?? []),
};
