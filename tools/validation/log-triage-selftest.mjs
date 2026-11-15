// tools/validation/log-triage-selftest.mjs
// inputs: tools/lib/log-triage.mjs, tools/host/commands/triage.mjs, tools/validation/fixtures/log-triage/**, docs/debug-log-triage.md
//
// Proves the session triage: a fixture console.log with 3 known and 2 new kinds of error line
// groups into exactly 5 groups with the right known/new marks and counts, and
// `tools\cli host triage <log> --triage <table>` prints the same. No game, no network.
// It also runs the real docs/debug-log-triage.md over the lines a clean flagship start prints
// (fixtures/log-triage/clean-start.log) and wants every group known (#1638).
// red-proof: before tools/lib/log-triage.mjs existed this stopped at import time:
// "Cannot find module '.../tools/lib/log-triage.mjs'" (2026-11-15, #1269). The clean-start case failed with
// "6 new group(s)" before the collision-repair row was added to docs/debug-log-triage.md (#1638).

import { dirname, join, resolve } from "node:path";
import { readFileSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FIX = join(ROOT, "tools", "validation", "fixtures", "log-triage");
const { parseTriageTable, triageLog, matchKnown } = await import("../lib/log-triage.mjs");

let failed = 0;
function fail(msg) {
  console.log(`FAIL  ${msg}`);
  failed++;
}

const rows = parseTriageTable(readFileSync(join(FIX, "triage.md"), "utf8"));
const { groups } = triageLog(readFileSync(join(FIX, "console.log"), "utf8"), rows);

if (groups.length !== 5) fail(`expected exactly 5 groups, got ${groups.length}: ${groups.map((g) => g.key).join(" | ")}`);

const expect = [
  { has: "save point FAILED", known: true, verdict: "FIXED", count: 2 },
  { has: "job request refused", known: true, verdict: "BY DESIGN", count: 2 },
  { has: "VME: NULL pointer", known: true, verdict: "OPEN", count: 2 },
  { has: "mode switch exploded", known: false, count: 1 }, // tag-only INFO row must not vouch for an error
  { has: "Can't find variable", known: false, count: 2 }, // line numbers differ, one group
];
for (const e of expect) {
  const g = groups.find((x) => x.key.includes(e.has));
  if (!g) fail(`no group for "${e.has}"`);
  else {
    if (g.known !== e.known) fail(`"${e.has}" is marked ${g.known ? "known" : "new"}, want ${e.known ? "known" : "new"}`);
    if (e.verdict && g.verdict !== e.verdict) fail(`"${e.has}" verdict ${g.verdict}, want ${e.verdict}`);
    if (g.count !== e.count) fail(`"${e.has}" count ${g.count}, want ${e.count}`);
  }
}
// Noise that is not a triage line stays out: engine lines, plain info, a GUI warning.
for (const g of groups) {
  if (/Engine start|spawned 5\/5|Not a triage line/.test(g.key)) fail(`out-of-scope line grouped: ${g.key}`);
}
// New groups sort before known ones.
if (groups.length === 5 && groups[0].known) fail("new groups must be listed first");

// Pattern parsing: a later span in the same row inherits the row's [LFDebug:Tag].
if (!matchKnown("[LFDebug:Jobs] equipment refused: kit drawn by 5", rows)) fail("second span of a row did not inherit its [LFDebug:Jobs] tag");
if (matchKnown("[LFDebug:Quests] equipment refused: kit drawn by 5", rows)) fail("an inherited span matched under the wrong tag");

// A clean start is not a finding: the known boot lines of the real table leave no NEW group (#1638).
{
  const realRows = parseTriageTable(readFileSync(join(ROOT, "docs", "debug-log-triage.md"), "utf8"));
  const clean = triageLog(readFileSync(join(FIX, "clean-start.log"), "utf8"), realRows);
  const fresh = clean.groups.filter((g) => !g.known);
  if (clean.total !== 7) fail(`clean-start.log: expected 7 triage lines, got ${clean.total}`);
  if (matchKnown("[LFDebug:DebugWorld] collision repair skipped: world is unavailable", realRows)) fail("the collision-repair row vouches for its own error line");
  if (fresh.length) fail(`clean start: ${fresh.length} new group(s) in docs/debug-log-triage.md terms: ${fresh.map((g) => g.key).join(" | ")}`);
}

// The command, end to end through the CLI.
const run = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "host", "triage", join(FIX, "console.log"), "--triage", join(FIX, "triage.md")], {
  cwd: ROOT,
  encoding: "utf8",
  timeout: 60000,
  windowsHide: true,
});
const out = `${run.stdout ?? ""}${run.stderr ?? ""}`;
if (run.status !== 0) fail(`host triage exited ${run.status}: ${out.trim()}`);
else if (!/SUMMARY host-triage ok groups=5 known=3 new=2/.test(out)) fail(`host triage summary line missing or wrong: ${out.trim()}`);
else if (!/NEW\s+x2\s.*Can't find variable/.test(out) || !/KNOWN\s+x2\s.*FIXED/.test(out)) fail(`host triage did not print the marked groups: ${out.trim()}`);

if (failed) {
  console.log(`log-triage-selftest: ${failed} case(s) failed`);
  process.exit(1);
}
console.log("log-triage-selftest: OK (5 groups: 3 known, 2 new; counts, tag-only rows, tag inheritance, CLI)");
