// tools/validation/no-deposit-count-selftest.mjs - validate-no-deposit-count (#4824).
// inputs: tools/validation/validate-no-deposit-count.mjs, tools/lib/test-resetall.mjs, tools/validation/no-deposit-count-selftest.mjs
//
// Runs the validator over a throwaway tree: a core script and a pack script that call
// GetDepositItemCountByResource fail it, naming file and line; a comment or a string that names it passes.
//
// red-proof: 2026-11-24 with depositCountLines never matching, "a core script calling it fails" printed FAIL.
import { spawnSync } from "node:child_process";
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { depositCountLines } from "./validate-no-deposit-count.mjs";
import { scratchDir } from "../lib/scratch.mjs";

let failed = 0;
const check = (ok, msg) => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${msg}`);
  if (!ok) failed++;
};

const validator = join(dirname(fileURLToPath(import.meta.url)), "validate-no-deposit-count.mjs");
const dir = scratchDir("lf-no-deposit-count-");
const core = join(dir, "addons", "LifeFramework", "Scripts", "Game", "Feature");
const pack = join(dir, "addons", "LF_Everon1989", "Scripts", "Game");
for (const d of [core, pack]) mkdirSync(d, { recursive: true });
const run = () => spawnSync(process.execPath, [validator], { env: { ...process.env, LF_DEPOSIT_COUNT_ROOT: dir }, encoding: "utf8" });
const CALL = "class LF_Bad\n{\n\tint n = manager.GetDepositItemCountByResource(owner, prefab);\n}\n";

try {
  check(depositCountLines(CALL).join() === "3", "depositCountLines reports the 1-based line");
  check(depositCountLines("x = m.GetDepositItemCountByResource (a, b);").length === 1, "spacing before the bracket still counts");
  check(depositCountLines("// GetDepositItemCountByResource(a, b)\nstring s = \"GetDepositItemCountByResource(a)\";\n/* GetDepositItemCountByResource( */").length === 0, "comments and strings are not calls");
  check(depositCountLines("x = m.LF_GetDepositItemCountByResourceCached(a);").length === 0, "another name that contains it is not flagged");

  writeFileSync(join(core, "LF_Good.c"), "// no GetDepositItemCountByResource here (#4824)\nclass LF_Good {}\n");
  let r = run();
  check(r.status === 0, `a tree with only a comment passes (exit ${r.status}: ${r.stdout.trim()})`);

  writeFileSync(join(core, "LF_Bad.c"), CALL);
  r = run();
  check(r.status === 1 && /FAIL addons\/LifeFramework\/Scripts\/Game\/Feature\/LF_Bad\.c:3/.test(r.stdout), `a core script calling it fails (exit ${r.status}: ${r.stdout.trim()})`);
  rmSync(join(core, "LF_Bad.c"));

  writeFileSync(join(pack, "LF_PackBad.c"), CALL);
  r = run();
  check(r.status === 1 && /LF_Everon1989\/Scripts\/Game\/LF_PackBad\.c:3/.test(r.stdout), `a pack script calling it fails (exit ${r.status})`);
  rmSync(join(pack, "LF_PackBad.c"));

  r = run();
  check(r.status === 0, `passes again once the calls are gone (exit ${r.status})`);
} finally {
  rmSync(dir, { recursive: true, force: true });
}

console.log(failed ? `no-deposit-count-selftest: ${failed} failure(s)` : "no-deposit-count-selftest: OK");
process.exit(failed ? 1 : 0);
