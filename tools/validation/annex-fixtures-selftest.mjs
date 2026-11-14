// tools/validation/annex-fixtures-selftest.mjs
// inputs: tools/**, agent-box/**, .githooks/**, addons/**, docs/**, website/**, video/**
//
// Proves validate-annex-fixtures lets a baselined village reference pass, fails
// a new one (new file, or a new name in a baselined file), ignores annex names,
// spawned prefabs and comments, and reports a retired baseline row as stale.
// red-proof: with the nested-entity line in declaredNames (the `if (nested) names.push(...)` one) removed this printed
// "FAIL a new test names a village entity inside a $grp" and "... a village child entity" with "(no errors)" and
// exited 1 (2026-11-12); restored, it passes.
// red-proof: with the baseline lookup in checkAnnexFixtures made always-true this
// printed "FAIL a new test names a village entity: expected /names village
// entity "Roadmap_Bank_Vault"/" and exited 1 (2026-11-11).

import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { checkAnnexFixtures, LAYERS, TESTS } from "./validate-annex-fixtures.mjs";

function put(root, path, text) {
  const full = join(root, path);
  mkdirSync(dirname(full), { recursive: true });
  writeFileSync(full, text);
}

const test = (body) => `// tier: WORLD\nclass LF_Test_X extends LF_Test\n{\n\toverride void Run(LF_TestContext ctx)\n\t{\n${body}\n\t}\n}\n`;
const find = (n) => `\t\tctx.NotNull(GetGame().GetWorld().FindEntityByName("${n}"), "placed");`;

function tree() {
  const root = mkdtempSync(join(tmpdir(), "lf-annex-"));
  put(root, `${LAYERS}/Annex.layer`, `GenericEntity Annex_Desk : "{1111111111111111}Prefabs/Desk.et" {\n coords 0 0 0\n}\n`);
  put(root, `${LAYERS}/Street_Bank.layer`, `GenericEntity Roadmap_Bank_Vault : "{2222222222222222}Prefabs/Door.et" {\n components {\n  X "{3333333333333333}" {\n   m_sTargetId "Roadmap_Bank_Vault"\n  }\n }\n}\nGenericEntity Roadmap_Bank_Bag : "{2222222222222223}Prefabs/Bag.et" {\n}\n`);
  put(root, `${TESTS}/LF_Test_Old.c`, test(find("Roadmap_Bank_Bag")));
  put(root, `${TESTS}/LF_Test_Annex.c`, test(find("Annex_Desk")));
  return root;
}
const NESTED = `$grp GenericEntity : "{4444444444444444}Prefabs/Counter.et" {
 Roadmap_Nested_Depot {
  components {
   MeshObject {
    Object "{5555555555555555}a.xob"
   }
  }
  children {
   Roadmap_Nested_Child {
    coords 0 0 1
   }
  }
 }
}
`;
const BASELINE = { "LF_Test_Old.c": ["Roadmap_Bank_Bag"] };

const cases = [
  { name: "a new test names a village entity inside a $grp", plant: (r) => { put(r, `${LAYERS}/Street_Nested.layer`, NESTED); put(r, `${TESTS}/LF_Test_Nest.c`, test(find("Roadmap_Nested_Depot"))); }, expect: /LF_Test_Nest.c: names village entity "Roadmap_Nested_Depot"/ },
  { name: "a new test names a village child entity", plant: (r) => { put(r, `${LAYERS}/Street_Nested.layer`, NESTED); put(r, `${TESTS}/LF_Test_Kid.c`, test(find("Roadmap_Nested_Child"))); }, expect: /LF_Test_Kid.c: names village entity "Roadmap_Nested_Child"/ },
  { name: "a component override name is not an entity", plant: (r) => { put(r, `${LAYERS}/Street_Nested.layer`, NESTED); put(r, `${TESTS}/LF_Test_Comp.c`, test('		ctx.True(c.GetClassName() == "MeshObject", "x");')); }, expect: /^$/ , none: true },
  { name: "a new test names a village entity", plant: (r) => put(r, `${TESTS}/LF_Test_New.c`, test(find("Roadmap_Bank_Vault"))), expect: /LF_Test_New\.c: names village entity "Roadmap_Bank_Vault"/ },
  { name: "a baselined test names a second village entity", plant: (r) => put(r, `${TESTS}/LF_Test_Old.c`, test(find("Roadmap_Bank_Bag") + "\n" + find("Roadmap_Bank_Vault"))), expect: /LF_Test_Old\.c: names village entity "Roadmap_Bank_Vault"/ },
];

let failed = 0;
const roots = [];
try {
  const clean = tree();
  roots.push(clean);
  const res = checkAnnexFixtures(clean, BASELINE);
  if (res.errors.length || res.stale.length) {
    failed++;
    console.log(`FAIL clean tree (baselined village ref, annex ref) reported: ${[...res.errors, ...res.stale].join("; ")}`);
  }

  const ok = tree();
  roots.push(ok);
  put(ok, `${TESTS}/LF_Test_Spawn.c`, test(`\t\tIEntity e = LF_Utils.SpawnEntityPrefab("{2222222222222222}Prefabs/Door.et", vector.Zero);\n\t\t// FindEntityByName("Roadmap_Bank_Vault") is the old way`));
  if (checkAnnexFixtures(ok, BASELINE).errors.length) {
    failed++;
    console.log("FAIL a spawned prefab or a commented name was flagged");
  }

  for (const c of cases) {
    const root = tree();
    roots.push(root);
    c.plant(root);
    const { errors } = checkAnnexFixtures(root, BASELINE);
    if (c.none ? errors.length : !errors.some((e) => c.expect.test(e))) {
      failed++;
      console.log(`FAIL ${c.name}: expected ${c.expect}, got:\n  ${errors.join("\n  ") || "(no errors)"}`);
    }
  }

  const retired = tree();
  roots.push(retired);
  put(retired, `${TESTS}/LF_Test_Old.c`, test(find("Annex_Desk")));
  const r = checkAnnexFixtures(retired, BASELINE);
  if (r.errors.length || !r.stale.some((s) => /LF_Test_Old\.c: "Roadmap_Bank_Bag"/.test(s))) {
    failed++;
    console.log(`FAIL a retired baseline row should be stale, not an error: errors=${r.errors.join("; ")} stale=${r.stale.join("; ")}`);
  }
} finally {
  for (const r of roots) rmSync(r, { recursive: true, force: true });
}

if (failed) {
  console.log(`annex-fixtures-selftest: ${failed} case(s) failed`);
  process.exit(1);
}
console.log(`annex-fixtures-selftest: OK (baselined and annex refs pass, ${cases.length} new village refs caught, retired row reported stale)`);
