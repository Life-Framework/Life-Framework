// tools/validation/e2e-payload-guard-selftest.mjs - validate-e2e-payload-guard (#2812).
// inputs: tools/validation/validate-e2e-payload-guard.mjs, tools/lib/e2e-payload-guard.mjs, tools/validation/e2e-payload-guard-selftest.mjs, addons/LifeFramework/Scripts/Game/Tests/LF_E2E.c
//
// Runs the validator over a throwaway tree with a core Tests folder and the serializer:
//   - guarded pass-2 checks (LF_E2E.RequiresPayload, SaveHas, a callee that calls RequiresSeed first) pass;
//   - an unguarded Owner.VerifyE2E( call (the #2798 shape), a hand-rolled BootSaveVersion() vs PayloadSinceVersion()
//     compare, PayloadSinceVersion in an e2e pass-2 file and a guard naming an unregistered payload each fail,
//     naming file and line;
//   - the real helper exists with the three entry points the rules point at.
//
// red-proof: 2026-11-18 with the `VerifyE2E` rule removed from guardFindings, "an unguarded VerifyE2E call
// fails" printed FAIL.
import { spawnSync } from "node:child_process";
import { copyFileSync, existsSync, mkdirSync, readFileSync, rmSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { registeredPayloads, stripComments } from "../lib/e2e-payload-guard.mjs";
import { scratchDir } from "../lib/scratch.mjs";

let failed = 0;
const check = (ok, msg) => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${msg}`);
  if (!ok) failed++;
};

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, "..", "..");
const fx = (n) => join(here, "fixtures", "e2e-payload-guard", n);
const validator = join(here, "validate-e2e-payload-guard.mjs");
const dir = scratchDir("lf-e2e-payload-guard-");
const tests = join(dir, "addons", "LifeFramework", "Scripts", "Game", "Tests");
const ser = join(dir, "addons", "LifeFramework", "Scripts", "Game", "Persistence", "Serializers");
for (const d of [tests, ser]) mkdirSync(d, { recursive: true });
copyFileSync(fx("LF_PersistenceComponentSerializer.c"), join(ser, "LF_PersistenceComponentSerializer.c"));
const run = () => spawnSync(process.execPath, [validator], { env: { ...process.env, LF_E2E_GUARD_ROOT: dir }, encoding: "utf8" });
const put = (n) => copyFileSync(fx(n), join(tests, n));
const out = (r) => r.stdout.trim().split("\n")[0];

try {
  check(registeredPayloads(readFileSync(fx("LF_PersistenceComponentSerializer.c"), "utf8")).join() === "reputation,itemkeys", "registeredPayloads reads the order and skips comments");
  check(!stripComments('a // b\n"x//y"').includes("b") && stripComments('"x//y"').includes("x//y"), "stripComments keeps strings");

  put("LF_Test_E2E_Good.c");
  put("LF_Test_SeededPersist.c");
  let r = run();
  check(r.status === 0, `guarded checks pass (${out(r)})`);

  put("LF_Test_NewPersist.c");
  put("LF_Test_E2E_Bad.c");
  r = run();
  check(r.status === 1 && /LF_Test_E2E_Bad\.c:6 .*LF_Test_NewPersist\.VerifyE2E is not guarded/.test(r.stdout), `an unguarded VerifyE2E call fails (${out(r)})`);
  rmSync(join(tests, "LF_Test_E2E_Bad.c"));

  put("LF_Test_E2E_HandRolled.c");
  r = run();
  check(r.status === 1 && /LF_Test_E2E_HandRolled\.c:5 hand-rolled/.test(r.stdout), `a hand-rolled version compare fails (${out(r)})`);
  rmSync(join(tests, "LF_Test_E2E_HandRolled.c"));

  put("LF_Test_E2E_PayloadSince.c");
  r = run();
  check(r.status === 1 && /LF_Test_E2E_PayloadSince\.c:5 PayloadSinceVersion in an e2e/.test(r.stdout), `PayloadSinceVersion in an e2e file fails (${out(r)})`);
  rmSync(join(tests, "LF_Test_E2E_PayloadSince.c"));

  put("LF_Test_E2E_HandRolledSection.c");
  r = run();
  check(r.status === 1 && /LF_Test_E2E_HandRolledSection\.c:5 hand-rolled BootHadSection/.test(r.stdout), `a hand-rolled BootHadSection fails (${out(r)})`);
  rmSync(join(tests, "LF_Test_E2E_HandRolledSection.c"));

  put("LF_Test_E2E_UnknownPayload.c");
  r = run();
  check(r.status === 1 && /LF_Test_E2E_UnknownPayload\.c:5 .*"reputashun"/.test(r.stdout), `a guard naming an unregistered payload fails (${out(r)})`);
  rmSync(join(tests, "LF_Test_E2E_UnknownPayload.c"));

  rmSync(join(tests, "LF_Test_NewPersist.c"));
  r = run();
  check(r.status === 0, `passes again once the offending files are gone (${out(r)})`);

  const helper = join(root, "addons", "LifeFramework", "Scripts", "Game", "Tests", "LF_E2E.c");
  const text = existsSync(helper) ? readFileSync(helper, "utf8") : "";
  for (const fn of ["SaveHas", "RequiresPayload", "RequiresSeed", "SaveHasSection", "RequiresSection"]) check(new RegExp(String.raw`static\s+bool\s+${fn}\s*\(`).test(text), `LF_E2E.${fn} exists`);
} finally {
  rmSync(dir, { recursive: true, force: true });
}

if (failed) {
  console.log(`e2e-payload-guard-selftest: ${failed} FAIL`);
  process.exit(1);
}
console.log("e2e-payload-guard-selftest: OK");
