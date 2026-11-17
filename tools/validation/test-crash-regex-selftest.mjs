// Proves the test harness's crash-line pattern ($CrashLineRe in tools/test/test-e2e.ps1) does not call a
// healthy boot a crash. PowerShell -match ignores case, so the old bare `VME` matched "Na-vme-sh" in
// `PATHFINDING : Navmesh Init 'BTRlike'`, which every world with an SCR_AIWorld (CoreOnEveron) prints, and
// every test boot there failed as "server crashed" (2026-11-16). Real VMEs and exceptions must still match.
// Does not boot the game.
// inputs: tools/test/test-e2e.ps1, tools/validation/test-crash-regex-selftest.mjs
//
// red-proof 2026-11-16: with the pattern's `(?-i:\bVME\b)` put back to a bare `VME`, the two Navmesh lines
// FAIL ("a healthy Navmesh line counted as a crash"); with it as written the run is green.
import { spawnSync } from "node:child_process";
import { readFileSync } from "node:fs";
import { join } from "node:path";
import { fileURLToPath } from "node:url";

const here = fileURLToPath(new URL(".", import.meta.url));
const ps1 = readFileSync(join(here, "..", "test", "test-e2e.ps1"), "utf8");
const m = ps1.match(/^\$CrashLineRe = '(.*)'\s*$/m);
if (!m) {
  console.error("test-crash-regex-selftest: FAIL no `$CrashLineRe = '...'` line in tools/test/test-e2e.ps1");
  process.exit(1);
}

const cases = [
  ["22:12:56.471     PATHFINDING   : Navmesh Init 'BTRlike'", false, "a healthy Navmesh line counted as a crash"],
  ["22:12:56.471      PATHFINDING(W): No navmesh file specified! Will initialize empty navmesh world.", false, "a healthy navmesh warning counted as a crash"],
  ["22:12:56.627      SCRIPT    (W): SCR_MapEntity: Cannot get the size from terrain. Using default.", false, "a healthy warning counted as a crash"],
  ["SCRIPT       : VME: NULL pointer to instance in LF_Foo.Bar", true, "a VME line was missed"],
  ["SCRIPT (E): Virtual Machine Exception", true, "a Virtual Machine Exception line was missed"],
  ["Unhandled Exception in thread", true, "an exception line was missed"],
  ["something VME happened", true, "a stand-alone VME word was missed"],
];

const script = [
  `$re = '${m[1]}'`,
  `$cases = [Console]::In.ReadToEnd() | ConvertFrom-Json`,
  `$out = foreach ($c in $cases) { [bool]($c -match $re) }`,
  `$out | ConvertTo-Json -Compress`,
].join("\n");
const r = spawnSync("powershell", ["-NoProfile", "-NonInteractive", "-Command", script], {
  input: JSON.stringify(cases.map((c) => c[0])),
  encoding: "utf8",
  windowsHide: true,
});
if (r.status !== 0) {
  console.error(`test-crash-regex-selftest: FAIL powershell exited ${r.status}: ${r.stderr}`);
  process.exit(1);
}
let got = JSON.parse(r.stdout.trim());
if (!Array.isArray(got)) got = [got];
const failures = [];
cases.forEach(([line, want, why], i) => {
  if (got[i] !== want) failures.push(`${why}: ${line}`);
});
if (failures.length) {
  console.error(`test-crash-regex-selftest: FAIL ${failures.length} case(s)\n  ${failures.join("\n  ")}`);
  process.exit(1);
}
console.log(`test-crash-regex-selftest: ok (${cases.length} cases)`);
