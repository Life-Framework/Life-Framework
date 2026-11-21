// Pins the stale pass 2 fix of `tools\cli wt e2e` (#3569): pass 2 keeps the profile, so pass 1's log folder (with its
// SUMMARY) is still there when pass 2 starts. A pass 2 that makes no folder of its own must read as "no log" and fail,
// and the harness must skip the folders that existed before its launch.
// inputs: tools/**, agent-box/**, .githooks/**
// red-proof: with newestTestConsole ignoring `exclude` (and Get-LogDir not filtering $preLogDirs) the stale case
// returned pass 1's console.log and the run reported green without a boot (6 of 9 runs on 2026-11-19).
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { scratchDir } from "../lib/scratch.mjs";
import { checkReloadBoot, newestTestConsole, testLogDirs } from "../test/e2e-shutdown.mjs";

const failures = [];
const root = scratchDir("lf-e2e-stale-");
const put = (dir, text) => {
  const d = join(root, "server", "profile", "test", "logs", dir);
  mkdirSync(d, { recursive: true });
  writeFileSync(join(d, "console.log"), text);
};
const SUMMARY = "[LFTEST] SUMMARY tier=persistence passed=3 failed=0 total=3";

// Pass 1 left a log with a SUMMARY. Pass 2 has not booted yet.
put("logs_2026-11-19_10-00-00", `${SUMMARY}\n`);
const before = testLogDirs(root);
if (before.size !== 1) failures.push(`testLogDirs counted ${before.size} folders, expected 1`);
const stale = newestTestConsole(root, before);
if (stale !== null) failures.push(`a pass 2 with no folder of its own still reads ${stale}`);
if (checkReloadBoot(stale === null ? null : readFileSync(stale, "utf8")).ok) failures.push("the stale pass 2 passes the reload check");
if (!newestTestConsole(root)) failures.push("newestTestConsole without an exclude set lost the newest log");

// Pass 2 boots and makes its own folder.
put("logs_2026-11-19_10-05-00", `[LFDebug:Persistence] session continued from a save point\n${SUMMARY}\n`);
const fresh = newestTestConsole(root, before);
if (!fresh || !fresh.includes("10-05-00")) failures.push(`the reload boot's own log was not picked: ${fresh}`);
else if (!checkReloadBoot(readFileSync(fresh, "utf8")).ok) failures.push("a real reload boot fails the reload check");

// A boot that made its own folder but never loaded a save fails loudly.
put("logs_2026-11-19_10-09-00", `${SUMMARY}\n`);
const noLoad = newestTestConsole(root, before);
if (checkReloadBoot(noLoad ? readFileSync(noLoad, "utf8") : null).ok) failures.push("a boot that never loaded a save passes");

// The harness side: it must snapshot the old folders before the launch and skip them in Get-LogDir.
const ps1 = readFileSync(join(dirname(fileURLToPath(import.meta.url)), "..", "test", "test-e2e.ps1"), "utf8");
const snap = ps1.indexOf("$preLogDirs = @{}");
const launch = ps1.indexOf("Start-LfQuietProcess -FilePath $exe");
if (snap < 0 || launch < 0 || snap > launch) failures.push("test-e2e.ps1 does not snapshot $preLogDirs before it launches the server");
if (!/function Get-LogDir \{[^}]*\$preLogDirs\.ContainsKey/.test(ps1)) failures.push("test-e2e.ps1 Get-LogDir does not skip the folders from before the launch");

if (failures.length) {
  console.log("e2e-stale-pass2-selftest: FAIL");
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}
console.log("e2e-stale-pass2-selftest: OK (a pass 2 with no log of its own, or no save load, fails; the harness skips old log folders)");
