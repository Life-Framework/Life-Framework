import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, newestLog, sessionDir, sessionPaths } from "../session.mjs";

// M4 #1303: does a client see the in-game clock run at the rate the day cycle promises (#1301)?
// The server applies `dayCycle` from the owner config.json at start; this reads the CLIENT's clock many times,
// every 3 s for `--seconds` (default 60), through the `seen` verb's `clock=<hour>/<day seconds>` token and passes when
// the slope of game hours per real second is within 5% of the configured day length. The clock is the client's
// own copy (the manager replicates it), so a day length the server set but never reached a client fails here.
//
//   day-rate [--minutes 120] [--seconds 60]       (expected day length in real minutes; 120 is the flagship's 12x)
//   day-rate --write-config <vanilla|accelerated> <dir> [--minutes 120]
//                                                  writes <dir>/config.json for `session up --seed <dir>/config.json`
//   day-rate --selftest                           the judge on made-up numbers, no game
//
// Pass:  session up --clients 1 --seed <dir-with-accelerated>/config.json ; harness.mjs proof day-rate
// Red:   session up --clients 1 --seed <dir-with-vanilla>/config.json     ; harness.mjs proof day-rate   (must FAIL)
const TOLERANCE = 0.05;
const DEFAULT_MINUTES = 120;
const DEFAULT_SECONDS = 60;
const SAMPLE_GAP_MS = 3000;

// Config the session is seeded with: only the day cycle, so no restart time or autosave moves under the run.
export function configFor(mode, minutes = DEFAULT_MINUTES) {
  const dayCycle = mode === "accelerated" ? { mode, dayLengthMinutes: minutes } : { mode };
  return { schemaVersion: 1, dayCycle };
}

// "8.25/7200" -> { hour: 8.25, daySeconds: 7200 }; null when the line has no usable clock.
export function parseClock(seenLine) {
  const m = /\bclock=(-?[\d.]+)\/(-?[\d.]+)/.exec(String(seenLine));
  if (!m) return null;
  const hour = Number(m[1]);
  const daySeconds = Number(m[2]);
  if (!Number.isFinite(hour) || hour < 0 || hour >= 24.0001) return null;
  return { hour, daySeconds };
}

// Pure. samples are [{ hour, at }] (at = real ms of the read), at least two, in time order; expectedMinutes is the
// configured day length. The rate is the least-squares slope of the unwrapped hour against real time, not the
// difference of two reads: a client's copy of the clock is replicated and lags in steps, so two reads a minute
// apart came out 7% slow against a server that measured 11.98x (2026-11-15).
export function judgeRate(samples, expectedMinutes) {
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
  const measuredPerSecond = den > 0 ? num / den : 0;
  const expectedPerSecond = 24 / (expectedMinutes * 60);
  const ratio = measuredPerSecond / expectedPerSecond;
  const speed = measuredPerSecond / (24 / 86400);
  const moved = ys[ys.length - 1] - ys[0];
  const detail = `${samples.length} reads over ${realSeconds.toFixed(1)} s: the clock moved ${moved.toFixed(4)} h, slope ${speed.toFixed(2)}x a 24 h day (the ${expectedMinutes}-minute day promises ${(1440 / expectedMinutes).toFixed(2)}x, measured/expected ${ratio.toFixed(3)})`;
  if (Math.abs(ratio - 1) > TOLERANCE) return { ok: false, reason: detail, ratio };
  return { ok: true, reason: detail, ratio };
}

// A made-up run: `n` reads `step` s apart on a clock moving `x` times a 24 h day, with `jitter` hours of lag on odd reads.
function series(n, step, x, jitter = 0, start = 8) {
  const out = [];
  for (let i = 0; i < n; i++) out.push({ hour: (start + (i * step * x * 24) / 86400 - (i % 2 ? jitter : 0) + 24) % 24, at: 1000000 + i * step * 1000 });
  return out;
}

function selftest() {
  const cases = [
    [series(21, 3, 12), 120, true], // 12x, the flagship's day
    [series(21, 3, 12 * 1.04), 120, true], // 4% fast is inside the tolerance
    [series(21, 3, 12 * 1.08), 120, false], // 8% fast is not
    [series(21, 3, 1), 120, false], // a vanilla 24 h day
    [series(21, 3, 0), 120, false], // a frozen clock
    [series(21, 3, 12, 0.004), 120, true], // lag of 0.004 h (14 game s) on every other read averages out
    [series(21, 3, 12, 0, 23.9), 120, true], // across midnight
    [series(21, 3, 24, 0), 60, true], // a 60-minute day is 24x
  ];
  const fails = [];
  cases.forEach(([samples, minutes, want], i) => {
    const got = judgeRate(samples, minutes).ok;
    if (got !== want) fails.push(`case ${i}: expected ${want}, got ${got}`);
  });
  if (judgeRate([{ hour: 8, at: 1 }], 120).ok) fails.push("one read passed");
  if (parseClock("x clock=8.5/7200")?.hour !== 8.5) fails.push("parseClock hour");
  if (parseClock("x clock=-1/-1") !== null) fails.push("parseClock no manager");
  if (parseClock("no token") !== null) fails.push("parseClock missing token");
  if (configFor("accelerated", 60).dayCycle.dayLengthMinutes !== 60) fails.push("configFor accelerated");
  if (configFor("vanilla").dayCycle.mode !== "vanilla") fails.push("configFor vanilla");
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "day-rate selftest: FAIL" : "day-rate selftest: PASS");
  return fails.length ? 1 : 0;
}

async function runDayRate(args) {
  const val = (f, d) => (args.includes(f) ? args[args.indexOf(f) + 1] : d);
  if (args.includes("--selftest")) return selftest();
  const minutes = Number(val("--minutes", DEFAULT_MINUTES));
  if (args.includes("--write-config")) {
    const at = args.indexOf("--write-config");
    const mode = args[at + 1];
    const dir = args[at + 2];
    if (!["vanilla", "accelerated"].includes(mode) || !dir) { console.log("usage: day-rate --write-config <vanilla|accelerated> <dir> [--minutes N]"); return 2; }
    mkdirSync(dir, { recursive: true });
    const file = join(dir, "config.json");
    writeFileSync(file, JSON.stringify(configFor(mode, minutes), null, 2) + "\n");
    console.log(`wrote ${file}`);
    return 0;
  }
  const seconds = Number(val("--seconds", DEFAULT_SECONDS));
  if (!(minutes > 0) || !(seconds >= 5)) { console.log("FAIL  --minutes must be positive and --seconds at least 5"); return 2; }

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
  if (!first) { console.log("FAIL  the client has no readable clock (no character, or no time manager)"); console.log("day-rate: FAIL"); return 1; }
  // One read every ~SAMPLE_GAP_MS until `seconds` have passed since the first, then the slope of them all.
  const samples = [first];
  let last = first;
  while (last.at - first.at < seconds * 1000) {
    wait(SAMPLE_GAP_MS);
    const s = read(`read at +${Math.round((Date.now() - first.at) / 1000)} s`);
    if (!s) { console.log("FAIL  the client lost its clock"); console.log("day-rate: FAIL"); return 1; }
    samples.push(s);
    last = s;
  }

  const verdict = judgeRate(samples, minutes);
  console.log(`client day length: ${last.daySeconds} s (config expects ${minutes * 60} s)`);
  console.log(verdict.reason);
  if (!verdict.ok) { console.log("FAIL  the client's clock does not run at the configured day rate"); console.log("day-rate: FAIL"); return 1; }
  console.log(`day-rate: PASS (a client sees the in-game clock at the ${minutes}-minute day's rate, within ${TOLERANCE * 100}%)`);
  return 0;
}

export default {
  name: "day-rate",
  noReset: true,
  usage: "day-rate [--minutes 120] [--seconds 60] | --write-config <vanilla|accelerated> <dir> | --selftest",
  run: (ctx) => runDayRate(ctx.args ?? []),
};
