// tools/validation/host-capacity-selftest.mjs
// inputs: tools/host/**, tools/validation/fixtures/host-capacity/**
//
// Proves `tools\cli host capacity` (#1499): a fixture perf CSV (#1406's header) and a fixture server
// log with `[LFDebug:Perf] players=N fps=F frameMs=M entities=E` lines (#1452's format) at 10 players
// give the expected CPU and upload per player and the estimated player count; a heavier run says the
// 40-player budget is not met; no perf lines, or no CSV rows, is a clear "no data" with exit code 2.
// red-proof: before tools/host/capacity.mjs existed this file died on the import:
// "Cannot find module '.../tools/host/capacity.mjs'" (2026-11-15).

import { cpSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve, dirname } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FX = join(ROOT, "tools", "validation", "fixtures", "host-capacity");
const cap = await import(pathToFileURL(join(ROOT, "tools", "host", "capacity.mjs")).href);
const cmd = await import(pathToFileURL(join(ROOT, "tools", "host", "commands", "capacity.mjs")).href);
const want = JSON.parse(readFileSync(join(FX, "expected.json"), "utf8"));
const csvText = readFileSync(join(FX, "perf-10p.csv"), "utf8");
const logText = readFileSync(join(FX, "console-10p.log"), "utf8");

const failures = [];
const fail = (m) => failures.push(m);
const near = (a, b) => Math.abs(a - b) < 1e-6;
const budget = { cpuPct: 80, uploadMbit: 20, targetPlayers: 40 };

const rows = cap.parseCsv(csvText);
if (rows.length !== 4) fail(`the CSV has 4 data rows, got ${rows.length}`);
const samples = cap.parsePerfLines(logText);
if (samples.length !== 4) fail(`the log has 4 perf lines (the count/ledger and persistence lines are not), got ${samples.length}`);

const e = cap.estimate(rows, samples, budget);
if (e.error) fail(`estimate failed: ${e.error}`);
else {
  for (const k of ["players", "cpuAvg", "upAvg", "cpuPerPlayer", "upPerPlayer", "fpsAvg", "frameMsMax", "entitiesMax", "estimate", "upEstimate"]) {
    if (!near(e[k], want[k])) fail(`${k}: got ${e[k]}, want ${want[k]}`);
  }
  if (e.limit !== want.limit) fail(`limit: got ${e.limit}, want ${want.limit}`);
  if (cap.summaryLine(e, budget) !== want.line) fail(`line differs:\n  got  ${cap.summaryLine(e, budget)}\n  want ${want.line}`);
}

// Double the per-player cost (a --players override of 5): 25 players, under the 40 budget.
const heavy = cap.estimate(rows, samples, budget, { players: 5 });
if (heavy.error || heavy.estimate !== 25) fail(`--players 5 should estimate 25, got ${JSON.stringify(heavy)}`);
else if (cap.summaryLine(heavy, budget) !== want.overLine) fail(`over line differs:\n  got  ${cap.summaryLine(heavy, budget)}\n  want ${want.overLine}`);

// No data: no perf lines, no CSV rows, and only idle (0-player) lines each say so instead of dividing by 0.
for (const [name, r, s] of [
  ["no perf lines", rows, cap.parsePerfLines("20:00:01.000 SCRIPT : [LFDebug:Host] hello")],
  ["no CSV rows", cap.parseCsv("time,cpu_pct,ws_mb,up_mbit,players\n"), samples],
  ["only idle lines", rows, cap.parsePerfLines("20:00:05.100 SCRIPT : [LFDebug:Perf] players=0 fps=60.0 frameMs=16.6 entities=1200")],
]) {
  const x = cap.estimate(r, s, budget);
  if (!x.error || !/no data/i.test(x.error)) fail(`${name}: expected a "no data" error, got ${JSON.stringify(x)}`);
}
// A populated `players` column in the CSV beats the log.
const withCol = cap.estimate(cap.parseCsv(csvText.replace(/,\s*$/gm, ",20")), [], budget);
if (withCol.error || withCol.players !== 20 || withCol.estimate !== 100) fail(`a players column of 20 should estimate 100, got ${JSON.stringify(withCol)}`);

// The command, end to end in a temp host dir: exit 0 and the line; with no perf lines exit 2.
const dir = mkdtempSync(join(tmpdir(), "lf-host-capacity-"));
try {
  const perfDir = join(dir, "perf");
  cpSync(join(FX, "perf-10p.csv"), join(perfDir, "perf-2026-11-15T20-00-30-000Z.csv"));
  const logFile = join(dir, "console.log");
  writeFileSync(logFile, logText);
  const out = [];
  const real = console.log;
  console.log = (...a) => out.push(a.join(" "));
  let code;
  let codeNoData;
  try {
    code = await cmd.main(["--log", logFile], { root: ROOT, hostDir: dir });
    out.push("---");
    writeFileSync(logFile, "20:00:01.000 SCRIPT : [LFDebug:Host] nothing\n");
    codeNoData = await cmd.main(["--log", logFile], { root: ROOT, hostDir: dir });
  } finally {
    console.log = real;
  }
  if (code !== 0) fail(`host capacity at 10 players should exit 0, got ${code}`);
  if (!out.some((l) => l === want.line)) fail(`the command did not print the estimate line:\n${out.join("\n")}`);
  if (codeNoData !== 2) fail(`host capacity with no perf lines should exit 2, got ${codeNoData}`);
  if (!out.slice(out.indexOf("---")).some((l) => /no data/i.test(l))) fail("the no-data run did not say no data");
} finally {
  rmSync(dir, { recursive: true, force: true });
}

if (failures.length) {
  console.log(`host-capacity-selftest: ${failures.length} failure(s)`);
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}
console.log("host-capacity-selftest: ok");
