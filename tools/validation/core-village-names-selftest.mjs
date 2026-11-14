// tools/validation/core-village-names-selftest.mjs
// inputs: tools/**, agent-box/**, .githooks/**, addons/**, docs/**, website/**, video/**
//
// Proves validate-core-village-names fails a core script that quotes a village
// entity (top-level in a layer, or inside a $grp), lets a baselined pair pass,
// and ignores comments, Tests/ files, annex names and component override names.
// red-proof: with the village-name test in scanCore made always-false this
// printed "FAIL a core script quotes a top-level village entity: expected
// /Feature\/Crime\/LF_Bad\.c: core script names village entity "Roadmap_Bank_Vault"/"
// and exited 1.

import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { LAYERS } from "./validate-annex-fixtures.mjs";
import { SCRIPTS, checkCoreVillageNames } from "./validate-core-village-names.mjs";

function put(root, path, text) {
  const full = join(root, path);
  mkdirSync(dirname(full), { recursive: true });
  writeFileSync(full, text);
}

const script = (body) => `class LF_X\n{\n\tstatic void Run()\n\t{\n${body}\n\t}\n}\n`;
const find = (n) => `\t\tIEntity e = GetGame().GetWorld().FindEntityByName("${n}");`;

function tree() {
  const root = mkdtempSync(join(tmpdir(), "lf-corevillage-"));
  put(root, `${LAYERS}/Annex.layer`, `GenericEntity Annex_Desk : "{1111111111111111}Prefabs/Desk.et" {\n coords 0 0 0\n}\n`);
  put(root, `${LAYERS}/Street_Bank.layer`, `GenericEntity Roadmap_Bank_Vault : "{2222222222222222}Prefabs/Door.et" {\n components {\n  MeshObject {\n   Object "{5555555555555555}a.xob"\n  }\n }\n}\n`);
  put(root, `${LAYERS}/Street_Nested.layer`, `$grp GenericEntity : "{4444444444444444}Prefabs/Counter.et" {\n Roadmap_Nested_Depot {\n  coords 0 0 1\n }\n}\n`);
  put(root, `${SCRIPTS}/Feature/Crime/LF_Good.c`, script(find("Annex_Desk") + "\n\t\t// FindEntityByName(\"Roadmap_Bank_Vault\") is the old way\n\t\tstring k = \"MeshObject\";"));
  put(root, `${SCRIPTS}/Tests/LF_Test_Uses.c`, script(find("Roadmap_Bank_Vault")));
  return root;
}

const cases = [
  { name: "a core script quotes a top-level village entity", plant: (r) => put(r, `${SCRIPTS}/Feature/Crime/LF_Bad.c`, script(find("Roadmap_Bank_Vault"))), expect: /Feature\/Crime\/LF_Bad\.c: core script names village entity "Roadmap_Bank_Vault"/ },
  { name: "a core script quotes a village entity inside a $grp", plant: (r) => put(r, `${SCRIPTS}/Core/LF_Nest.c`, script(find("Roadmap_Nested_Depot"))), expect: /Core\/LF_Nest\.c: core script names village entity "Roadmap_Nested_Depot"/ },
];

let failed = 0;
const roots = [];
try {
  const clean = tree();
  roots.push(clean);
  const res = checkCoreVillageNames(clean, {});
  if (res.errors.length || res.stale.length) {
    failed++;
    console.log(`FAIL clean tree (annex ref, comment, Tests/ file) reported: ${[...res.errors, ...res.stale].join("; ")}`);
  }

  for (const c of cases) {
    const root = tree();
    roots.push(root);
    c.plant(root);
    const { errors } = checkCoreVillageNames(root, {});
    if (!errors.some((e) => c.expect.test(e))) {
      failed++;
      console.log(`FAIL ${c.name}: expected ${c.expect}, got:\n  ${errors.join("\n  ") || "(no errors)"}`);
    }
  }

  const base = tree();
  roots.push(base);
  put(base, `${SCRIPTS}/Feature/Crime/LF_Bad.c`, script(find("Roadmap_Bank_Vault")));
  const allowed = checkCoreVillageNames(base, { "Feature/Crime/LF_Bad.c": ["Roadmap_Bank_Vault"] });
  if (allowed.errors.length) {
    failed++;
    console.log(`FAIL a baselined pair was flagged: ${allowed.errors.join("; ")}`);
  }
} finally {
  for (const r of roots) rmSync(r, { recursive: true, force: true });
}

if (failed) {
  console.log(`core-village-names-selftest: ${failed} case(s) failed`);
  process.exit(1);
}
console.log(`core-village-names-selftest: OK (${cases.length} village refs caught; comments, Tests/, annex names and overrides pass)`);
