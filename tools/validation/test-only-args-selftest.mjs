// Proves a long test selection reaches the harness without passing the Windows command-line limit (#4842):
// the core leg of an unsharded `--tier all` named 1367+ classes on one powershell argument and spawn failed
// with ENAMETOOLONG before any test started. A long list now travels as -OnlyList <path>; a short one stays -Only.
// inputs: tools/lib/test-only-args.mjs, tools/test/test-e2e.ps1, tools/cli.mjs, tools/validation/test-only-args-selftest.mjs
// red-proof 2026-11-24 (#4842): with onlyArgs returning ["-Only", only.join(",")] unchanged, "a 60 KB list becomes -OnlyList"
// and "the command line stays short" fail, and on Windows spawning powershell with that argument fails with ENAMETOOLONG.
import { readFileSync, writeFileSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { scratchDir } from "../lib/scratch.mjs";
import { onlyArgs, MAX_INLINE_ONLY } from "../lib/test-only-args.mjs";

const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};

const here = fileURLToPath(new URL(".", import.meta.url));
const scratch = scratchDir("lf-onlyargs-");

// Short list: unchanged (--only keeps working).
const short = ["LF_Test_A", "LF_Test_B"];
check(JSON.stringify(onlyArgs(scratch, short)) === JSON.stringify(["-Only", "LF_Test_A,LF_Test_B"]), "a short list stays -Only a,b");
check(onlyArgs(scratch, []).length === 0, "an empty selection adds no argument");

// Long list: a synthetic 1500-class selection, about 60 KB.
const long = [];
for (let i = 0; i < 1500; i++) long.push(`LF_Test_Synthetic_Selection_Number_${String(i).padStart(5, "0")}`);
const joined = long.join(",");
check(joined.length > 32767, `the synthetic list is past the Windows limit (${joined.length} chars)`);
const a = onlyArgs(scratch, long);
check(a[0] === "-OnlyList", "a 60 KB list becomes -OnlyList");
check(a.join(" ").length < 1000 && a.join(" ").length < MAX_INLINE_ONLY, `the command line stays short (${a.join(" ").length} chars)`);
let body = "";
if (a[0] === "-OnlyList") {
  try {
    body = readFileSync(a[1], "utf8");
  } catch (e) {
    failures.push(`the list file is unreadable: ${e.message.slice(0, 120)}`);
  }
}
check(body === joined, "the file holds the whole list, unchanged");

// The harness reads the file into the same list the game reads.
const ps1 = readFileSync(join(here, "..", "test", "test-e2e.ps1"), "utf8");
check(/\[string\]\$OnlyList/.test(ps1), "test-e2e.ps1 declares -OnlyList");
check(/ReadAllText\(\$OnlyList\)/.test(ps1), "test-e2e.ps1 reads the -OnlyList list");
const cli = readFileSync(join(here, "..", "cli.mjs"), "utf8");
check(/\.\.\.onlyArgs\(root, only\)/.test(cli) && !/\["-Only", only\.join\(","\)\]/.test(cli), "runLocalTest builds the selection through onlyArgs, never a raw -Only join");

// The real constraint, where it exists: the old path (the whole list on argv) fails to spawn on Windows; the new one runs.
if (process.platform === "win32") {
  const stub = join(scratch, "stub.ps1");
  writeFileSync(stub, ["param([string]$Only, [string]$OnlyList)", "if ($Only -or $OnlyList) { exit 0 }", "exit 3", ""].join("\n"), "utf8");
  const run = (args) => spawnSync("powershell.exe", ["-NoProfile", "-ExecutionPolicy", "Bypass", "-File", stub, ...args], { encoding: "utf8", windowsHide: true });
  const old = run(["-Only", joined]);
  check(old.error && /ENAMETOOLONG/.test(String(old.error.code ?? old.error.message)), `the old path (-Only <60 KB>) fails to spawn (${old.error ? old.error.code : "no error"})`);
  const now = run(a);
  check(!now.error && now.status === 0, `the new path spawns (${now.error ? now.error.code : "status " + now.status})`);
}

if (failures.length) {
  console.log(`test-only-args-selftest: FAIL\n  ${failures.join("\n  ")}`);
  process.exit(1);
}
console.log("test-only-args-selftest: OK (a 1500-class list travels as a file, a short one stays on the command line)");
