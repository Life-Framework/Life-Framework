// tools/validation/no-test-resetall-selftest.mjs - validate-no-test-resetall (#2093).
// inputs: tools/validation/validate-no-test-resetall.mjs, tools/lib/test-resetall.mjs, tools/validation/no-test-resetall-selftest.mjs
//
// Runs the validator over a throwaway tree under the OS temp dir with a core test and a pack test:
//   - a test calling LF_StaticReset.ResetAll() fails it (core and pack), naming file and line;
//   - comments, strings and a hook call (`LF_Alpha.ResetStatics()`) pass;
//   - a non-test script (Core/) calling ResetAll is not its business.
//
// red-proof: 2026-11-16 with the pattern in resetAllLines never matching, "a core test calling ResetAll fails"
// printed FAIL.
import { spawnSync } from "node:child_process";
import { copyFileSync, mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { resetAllLines } from "../lib/test-resetall.mjs";

let failed = 0;
const check = (ok, msg) => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${msg}`);
  if (!ok) failed++;
};

const here = dirname(fileURLToPath(import.meta.url));
const fx = (n) => join(here, "fixtures", "no-test-resetall", n);
const validator = join(here, "validate-no-test-resetall.mjs");
const dir = mkdtempSync(join(tmpdir(), "lf-no-test-resetall-"));
const core = join(dir, "addons", "LifeFramework", "Scripts", "Game", "Tests");
const pack = join(dir, "addons", "LF_Everon1989", "Scripts", "Game", "Tests");
const coreDir = join(dir, "addons", "LifeFramework", "Scripts", "Game", "Core");
for (const d of [core, pack, coreDir]) mkdirSync(d, { recursive: true });
const run = () => spawnSync(process.execPath, [validator], { env: { ...process.env, LF_RESETALL_ROOT: dir }, encoding: "utf8" });

try {
  check(resetAllLines("a\n\tLF_StaticReset.ResetAll();\n").join() === "2", "resetAllLines reports the 1-based line");
  check(resetAllLines("LF_StaticReset . ResetAll ( );").length === 1, "resetAllLines ignores spacing");
  check(resetAllLines("// LF_StaticReset.ResetAll()\n/* LF_StaticReset.ResetAll()\n*/ string s = \"LF_StaticReset.ResetAll()\";").length === 0, "comments and strings are not calls");
  check(resetAllLines("LF_DownedPatient.ResetAll();").length === 0, "another class's ResetAll is not flagged");

  copyFileSync(fx("LF_Test_GoodHook.c"), join(core, "LF_Test_GoodHook.c"));
  writeFileSync(join(coreDir, "LF_Boot.c"), "class LF_Boot { void Go() { LF_StaticReset.ResetAll(); } }\n");
  let r = run();
  check(r.status === 0, `clean tree passes (${r.stdout.trim()})`);

  copyFileSync(fx("LF_Test_BadResetAll.c"), join(core, "LF_Test_BadResetAll.c"));
  r = run();
  check(r.status === 1 && /LF_Test_BadResetAll\.c:12 /.test(r.stdout), `a core test calling ResetAll fails (${r.stdout.trim().split("\n")[0]})`);
  rmSync(join(core, "LF_Test_BadResetAll.c"));

  copyFileSync(fx("LF_Test_BadResetAll.c"), join(pack, "LF_Test_EveronBad.c"));
  r = run();
  check(r.status === 1 && /LF_Everon1989\/Scripts\/Game\/Tests\/LF_Test_EveronBad\.c:12 /.test(r.stdout), "a pack test calling ResetAll fails");
  rmSync(join(pack, "LF_Test_EveronBad.c"));

  r = run();
  check(r.status === 0, "passes again once the call is gone");
} finally {
  rmSync(dir, { recursive: true, force: true });
}

if (failed) {
  console.log(`no-test-resetall-selftest: ${failed} FAIL`);
  process.exit(1);
}
console.log("no-test-resetall-selftest: OK");
