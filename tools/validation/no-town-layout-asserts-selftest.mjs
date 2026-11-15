// tools/validation/no-town-layout-asserts-selftest.mjs
// inputs: tools/validation/**
//
// Proves validate-no-town-layout-asserts fails a new test, proof, flow or validator that names a
// town entity or writes a position on the town, lets a baselined row pass, ignores grid names,
// comments, positions on the annex or far from the town and lines with the town-layout-ok pragma,
// and reports a retired baseline row.
// red-proof: written before the validator; the first run printed
// "Cannot find module './validate-no-town-layout-asserts.mjs'" and exited 1 (2026-11-16). With
// the baseline lookup in check() made always-true it printed
// "FAIL a new test quoting a town entity fails" and exited 1; restored, it passes.

import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { check, scanTree, TOWN_DIRS } from "./validate-no-town-layout-asserts.mjs";

const TESTS = "addons/LifeFramework/Scripts/Game/Tests";
function put(root, path, text) {
  const full = join(root, path);
  mkdirSync(dirname(full), { recursive: true });
  writeFileSync(full, text);
}
const test = (body) => `// tier: WORLD\nclass LF_Test_X : LF_Test\n{\n\toverride void Run(LF_TestContext ctx)\n\t{\n${body}\n\t}\n}\n`;

function tree() {
  const root = mkdtempSync(join(tmpdir(), "lf-townlayout-"));
  put(root, `${TOWN_DIRS[0]}/Annex.layer`, `GenericEntity Annex_Desk : "{1111111111111111}Prefabs/Desk.et" {\n coords 60 0 60\n}\n`);
  put(root, `${TOWN_DIRS[0]}/Street_Bank.layer`, `GenericEntity Roadmap_Bank_Vault : "{2222222222222222}Prefabs/Door.et" {\n coords 200 0 260\n}\n`);
  put(root, `${TOWN_DIRS[1]}/Village.layer`, `GenericEntity Quest_Clerk : "{3333333333333333}Prefabs/Npc.et" {\n coords 135 0 243\n}\n`);
  put(root, `${TESTS}/LF_Test_Old.c`, test(`\t\tIEntity v = GetGame().GetWorld().FindEntityByName("Roadmap_Bank_Vault");`));
  put(root, `${TESTS}/LF_Test_Grid.c`, test(`\t\tIEntity d = GetGame().GetWorld().FindEntityByName("Annex_Desk");\n\t\tvector a = "60 0 61";\n\t\tvector far = "1000 0 1000";\n\t\t// "Quest_Clerk" in a comment\n\t\tvector sweep = "200 0 260"; // town-layout-ok: a whole-world sweep`));
  return root;
}

const baseline = { [`${TESTS}/LF_Test_Old.c`]: ["name:Roadmap_Bank_Vault"] };
let failed = 0;
function expect(label, root, re, base = baseline) {
  const { errors } = check(root, base, scanTree(root));
  const ok = re ? errors.some((e) => re.test(e)) : errors.length === 0;
  if (!ok) {
    failed++;
    console.log(`FAIL ${label}\n  got: ${errors.join("\n       ") || "(no errors)"}`);
  }
}

const cases = [
  ["baselined, grid, far, comment and pragma lines pass", () => {}, null],
  ["a new test quoting a town entity fails", (r) => put(r, `${TESTS}/LF_Test_New.c`, test(`\t\tFindEntityByName("Quest_Clerk");`)), /LF_Test_New\.c: names town entity "Quest_Clerk"/],
  ["a new name in a baselined file fails", (r) => put(r, `${TESTS}/LF_Test_Old.c`, test(`\t\tFindEntityByName("Roadmap_Bank_Vault");\n\t\tFindEntityByName("Quest_Clerk");`)), /LF_Test_Old\.c: names town entity "Quest_Clerk"/],
  ["a test spawning on the town fails", (r) => put(r, `${TESTS}/LF_Test_Spot.c`, test(`\t\tLF_Utils.SpawnEntityPrefab(P, "201 0 258");`)), /LF_Test_Spot\.c: position 201,258 stands on the town/],
  ["a Vector() on the town fails", (r) => put(r, `${TESTS}/LF_Test_Vec.c`, test(`\t\tvector v = Vector(135, 0, 244);`)), /LF_Test_Vec\.c: position 135,244/],
  ["a proof naming a town entity fails", (r) => put(r, "agent-box/proofs/new-proof.mjs", `await h.stand("Quest_Clerk");\n`), /new-proof\.mjs: names town entity "Quest_Clerk"/],
  ["a flow naming a town entity fails", (r) => put(r, "agent-box/flows/new.txt", `# a walk\nlayer Roadmap_Bank_Vault LF_BankRobAction\n`), /new\.txt: names town entity "Roadmap_Bank_Vault"/],
  ["a validator with an x/z pair on the town fails", (r) => put(r, "tools/validation/validate-new.mjs", `const at = { x: 199, z: 261 };\n`), /validate-new\.mjs: position 199,261/],
];
for (const [label, edit, re] of cases) {
  const root = tree();
  try {
    edit(root);
    expect(label, root, re);
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
}

const root = tree();
try {
  const { retired } = check(root, { ...baseline, [`${TESTS}/LF_Test_Gone.c`]: ["name:Quest_Clerk"] }, scanTree(root));
  if (!retired.some((r) => r.includes("LF_Test_Gone.c"))) {
    failed++;
    console.log("FAIL a baseline row that no longer occurs is reported as retired");
  }
} finally {
  rmSync(root, { recursive: true, force: true });
}

// The town moved next to an old literal in a file this branch did not touch: a warning, not a failure.
const moved = tree();
try {
  put(moved, `${TESTS}/LF_Test_Spot.c`, test(`		LF_Utils.SpawnEntityPrefab(P, "201 0 258");`));
  const r = check(moved, baseline, scanTree(moved), [`${TESTS}/LF_Test_Other.c`]);
  if (r.errors.length || !r.warnings.some((w) => w.includes("LF_Test_Spot.c"))) {
    failed++;
    console.log(`FAIL a new row in an unchanged file only warns
  got: ${r.errors.join("; ") || "(no errors)"}`);
  }
  const r2 = check(moved, baseline, scanTree(moved), [`${TESTS}/LF_Test_Spot.c`]);
  if (!r2.errors.some((e) => e.includes("LF_Test_Spot.c"))) {
    failed++;
    console.log("FAIL a new row in a changed file fails");
  }
} finally {
  rmSync(moved, { recursive: true, force: true });
}

if (failed) {
  console.log(`no-town-layout-asserts-selftest: ${failed} case(s) failed`);
  process.exit(1);
}
console.log(`no-town-layout-asserts-selftest: OK (${cases.length + 3} cases)`);
