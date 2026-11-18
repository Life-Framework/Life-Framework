// Proves the test rule of validate-registries (tools/lib/test-world.mjs testInfoProblems) on fixtures, and that
// the reader every tool uses (readTestInfo) finds every test in the tree. Does not boot the game.
// inputs: tools/lib/test-world.mjs, tools/validation/validate-registries.mjs, tools/validation/registries-selftest.mjs
// red-proof 2026-11-18 (#2618): with the attribute check in testInfoProblems commented out, "a class with no
// LF_TestInfo is rejected" failed; with the central-list line check removed, "a reintroduced central list is
// rejected" failed.
import { readFileSync } from "node:fs";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { parseTestInfo, readTestInfo, testInfoProblems } from "../lib/test-world.mjs";

const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};
const CORE = "addons/LifeFramework/Scripts/Game/Tests/LF_Test_Fixture.c";
const PACK = "addons/LF_Everon1989/Scripts/Game/Tests/LF_Test_EveronFixture.c";
const good = "// red-proof: x\n[LF_TestInfo(LF_TestTier.LOGIC)]\nclass LF_Test_A : LF_Test\n{\n}\n";

check(testInfoProblems(CORE, good).length === 0, "a test with its attribute passes");
check(testInfoProblems(CORE, "[LF_TestInfo(LF_TestTier.WORLD)]\n//! doc line\n// more\nclass LF_Test_A : LF_Test\n{\n}\n").length === 0, "comment lines between the attribute and the class are allowed");
check(testInfoProblems(CORE, "// tier: LOGIC\nclass LF_Test_A : LF_Test\n{\n}\n").length === 1, "a class with no LF_TestInfo is rejected");
check(testInfoProblems(CORE, "[LF_TestInfo(LF_TestTier.LOGIC)]\nclass LF_Test_A : LF_Test\n{\n}\n\nclass LF_Test_B : LF_Test\n{\n}\n").length === 1, "the second class of a file needs its own attribute");
check(testInfoProblems(CORE, "[LF_TestInfo(LF_TestTier.LOGIC)]\n[LF_TestInfo(LF_TestTier.WORLD)]\nclass LF_Test_A : LF_Test\n{\n}\n").some((m) => /two/.test(m)), "two attributes on one class are rejected");
check(testInfoProblems(CORE, "class LF_Test_Helper : LF_EventSubscriber\n{\n}\n").length === 0, "a helper class that is not an LF_Test needs no attribute");
check(testInfoProblems(CORE, "class LF_Test_Sub : LF_Test_A\n{\n}\n").length === 0, "a subclass of a test (not collected: attributes are not inherited) needs none");
check(testInfoProblems(PACK, good).some((m) => /pack: true/.test(m)), "a pack test without pack: true is rejected");
check(testInfoProblems(CORE, good.replace("LOGIC)", "LOGIC, pack: true)")).some((m) => /core test/.test(m)), "a core test with pack: true is rejected");
check(testInfoProblems(PACK, good.replace("LOGIC)", "WORLD, pack: true)")).length === 0, "a pack test with pack: true passes");
const central = "class LF_TestRegistrations\n{\n\tstatic void RegisterAll(LF_TestManager manager)\n\t{\n\t\tmanager.Register(new LF_Test_A(), LF_TestTier.LOGIC);\n\t}\n}\n";
check(testInfoProblems("addons/LifeFramework/Scripts/Game/Tests/LF_TestRegistrations.generated.c", central).length === 2, "a reintroduced central list is rejected");
check(testInfoProblems(CORE, "\t\tmanager.RegisterPack(new LF_Test_EveronA(), LF_TestTier.WORLD);\n").length === 1, "a pack registration row is rejected");

// The reader: tier and pack flag from the attribute, in class-name (byte) order.
const rows = parseTestInfo("[LF_TestInfo(LF_TestTier.PERF)]\nclass LF_Test_b : LF_Test {}\n[LF_TestInfo(LF_TestTier.WORLD, pack: true)]\n// note\nclass LF_Test_C : LF_Test {}\n");
check(rows.length === 2 && rows[0].tier === "PERF" && rows[1].pack && rows[1].cls === "LF_Test_C", `parseTestInfo reads tier and pack (${JSON.stringify(rows)})`);
const root = join(fileURLToPath(new URL(".", import.meta.url)), "..", "..");
const all = readTestInfo(root);
const names = all.map((t) => t.cls);
check(all.length > 800, `readTestInfo finds the suite (${all.length})`);
check(names.every((n, i) => i === 0 || names[i - 1] < n), "readTestInfo is sorted by class name, byte order (LF_TestManager's order)");
check(all.some((t) => t.pack) && all.filter((t) => t.pack).every((t) => /^LF_Test_Everon/.test(t.cls)), "pack tests are the LF_Test_Everon* classes");
check(/testInfoProblems\(f, text\)/.test(readFileSync(join(root, "tools", "validation", "validate-registries.mjs"), "utf8")), "validate-registries runs the test rule on every script file");

if (failures.length) {
  for (const f of failures) console.log(`FAIL ${f}`);
  process.exit(1);
}
console.log(`registries-selftest: OK (${all.length} tests read from [LF_TestInfo])`);
