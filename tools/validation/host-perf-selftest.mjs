// tools/validation/host-perf-selftest.mjs
// inputs: tools/host/**, tools/validation/fixtures/host-perf/**
//
// Proves `tools\cli host perf` (#1406): a fixture series of raw samples (CPU seconds, working set,
// bytes sent) gives the expected CSV rows and one verdict line against the 16-player budget (ok,
// then over budget on upload, then on CPU); the sampling loop, driven with a fake sampler and a
// fake clock, takes samples every N seconds for at most the `--for` span and never reads a process.
// red-proof: before tools/host/perf.mjs existed this file died on the import:
// "Cannot find module '.../tools/host/perf.mjs'" (2026-11-15).

import { readFileSync } from "node:fs";
import { join, resolve, dirname } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const perf = await import(pathToFileURL(join(ROOT, "tools", "host", "perf.mjs")).href);
const fx = JSON.parse(readFileSync(join(ROOT, "tools", "validation", "fixtures", "host-perf", "series.json"), "utf8"));

const failures = [];
const fail = (m) => failures.push(m);

const rows = perf.deriveRows(fx.samples);
if (rows.length !== 3) fail(`4 samples should give 3 rows, got ${rows.length}`);

const csv = perf.toCsv(rows);
if (csv !== fx.expectedCsv) fail(`CSV differs:\n--- got\n${csv}--- want\n${fx.expectedCsv}`);

for (const [name, budget] of Object.entries(fx.budgets)) {
  const got = perf.verdictLine(rows, budget);
  if (got !== fx.expectedVerdict[name]) fail(`verdict "${name}" differs:\n  got  ${got}\n  want ${fx.expectedVerdict[name]}`);
}
if (!perf.verdict(rows, fx.budgets.ok).ok) fail("the ok series was judged over budget");
const over = perf.verdict(rows, fx.budgets.overUpload);
if (over.ok || !over.over.includes("upload") || over.over.includes("cpu")) fail(`overUpload judged wrong: ${JSON.stringify(over)}`);

// Edge cases: nothing to judge, a counter reset (process restarted) and a zero time step never go negative or NaN.
if (perf.deriveRows([]).length !== 0 || perf.deriveRows(fx.samples.slice(0, 1)).length !== 0) fail("fewer than 2 samples must give no rows");
if (!/no samples/i.test(perf.verdictLine([], fx.budgets.ok))) fail("an empty run did not say it had no samples");
const reset = perf.deriveRows([
  { t: 0, cpuSec: 50, wsMb: 1000, sentBytes: 9000000 },
  { t: 30000, cpuSec: 2, wsMb: 900, sentBytes: 100 },
  { t: 30000, cpuSec: 3, wsMb: 900, sentBytes: 200 },
]);
if (reset.some((r) => !(r.cpuPct >= 0) || !(r.upMbit >= 0))) fail(`a counter reset or zero step gave a negative or NaN row: ${JSON.stringify(reset)}`);

// The sampling loop: fake sampler and clock, no process and no real wait.
const cannedSamples = fx.samples.slice();
let now = fx.samples[0].t;
const slept = [];
const got = perf.collect({
  sample: () => ({ ...cannedSamples.shift() ?? { t: now, cpuSec: 0, wsMb: 0, sentBytes: 0 } }),
  sleep: (ms) => { slept.push(ms); now += ms; },
  now: () => now,
  everySec: 30,
  forSec: 90,
});
if (got.length !== 4) fail(`--for 90 --every 30 should take 4 samples (start plus 3), got ${got.length}`);
if (slept.length !== 3 || slept.some((ms) => ms !== 30000)) fail(`the loop should sleep 3 x 30000 ms, got ${JSON.stringify(slept)}`);
let calls = 0;
perf.collect({ sample: () => { calls++; return null; }, sleep: () => {}, now: () => 0, everySec: 30, forSec: 90 });
if (calls > 1) fail(`a sampler that returns null (the server is gone) should end the run at once, it was called ${calls} times`);

const args = perf.parseArgs(["--every", "10", "--for", "60", "--upload-mbit", "25"]);
if (args.everySec !== 10 || args.forSec !== 60 || args.uploadMbit !== 25) fail(`flags misparsed: ${JSON.stringify(args)}`);
const dflt = perf.parseArgs([]);
if (dflt.everySec !== 30 || dflt.forSec !== 120) fail(`defaults should be every 30, for 120: ${JSON.stringify(dflt)}`);
if (!perf.parseArgs(["--every", "0"]).error || !perf.parseArgs(["--for", "x"]).error) fail("a bad --every or --for was accepted");

if (failures.length) {
  for (const f of failures) console.log(`FAIL  ${f}`);
  process.exit(1);
}
console.log("host-perf-selftest ok");
