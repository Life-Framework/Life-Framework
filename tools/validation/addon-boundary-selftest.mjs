// tools/validation/addon-boundary-selftest.mjs
//
// Proves validate-addon-boundary fails on each violation it exists for. Every
// case builds a fixture addons/ tree in a temp dir: a clean core and pack, then
// one planted violation. The clean tree must pass and each planted tree must
// report its violation.
// red-proof: with the GUID check disabled in validate-addon-boundary.mjs this
// printed "FAIL core prefab references a pack GUID: expected
// /references pack GUID \{AB12CD34EF567890\}/" and exited 1 (2026-11-08).

import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { checkBoundary, CORE, PACK } from "./validate-addon-boundary.mjs";

const PACK_PREFAB_GUID = "AB12CD34EF567890";
const PACK_PROJECT_GUID = "B1E84D5687E11724";
const CORE_PROJECT_GUID = "79636E668EA37AC9";

function put(root, path, text) {
  const full = join(root, path);
  mkdirSync(dirname(full), { recursive: true });
  writeFileSync(full, text);
}

function cleanTree() {
  const root = mkdtempSync(join(tmpdir(), "lf-boundary-"));
  put(root, `${CORE}/${CORE}.gproj`, `GameProject {\n ID "${CORE}"\n GUID "${CORE_PROJECT_GUID}"\n Dependencies {\n  "58D0FB3206B6F859"\n }\n}\n`);
  put(root, `${CORE}/Prefabs/Props/Signs/LF_Sign_Board.et`, `GenericEntity {\n ID "1"\n}\n`);
  put(root, `${CORE}/Prefabs/Props/Signs/LF_Sign_Board.et.meta`, `MetaFileClass {\n Name "{1111111111111111}Prefabs/Props/Signs/LF_Sign_Board.et"\n}\n`);
  put(root, `${CORE}/Scripts/Game/LF_Core.c`, `class LF_Core\n{\n}\n`);
  put(root, `${PACK}/${PACK}.gproj`, `GameProject {\n ID "${PACK}"\n GUID "${PACK_PROJECT_GUID}"\n Dependencies {\n  "58D0FB3206B6F859" "${CORE_PROJECT_GUID}"\n }\n}\n`);
  // The pack inherits a core prefab: pack -> core is the allowed direction.
  put(root, `${PACK}/Prefabs/Signs/Sign_Pub_Everon.et`, `GenericEntity : "{1111111111111111}Prefabs/Props/Signs/LF_Sign_Board.et" {\n}\n`);
  put(root, `${PACK}/Prefabs/Signs/Sign_Pub_Everon.et.meta`, `MetaFileClass {\n Name "{${PACK_PREFAB_GUID}}Prefabs/Signs/Sign_Pub_Everon.et"\n}\n`);
  put(root, `${PACK}/README.md`, `# pack\n`);
  return root;
}

const cases = [
  {
    name: "core prefab references a pack GUID",
    plant: (r) => put(r, `${CORE}/Prefabs/Uses.et`, `GenericEntity : "{${PACK_PREFAB_GUID}}Prefabs/Signs/Sign_Pub_Everon.et" {\n}\n`),
    expect: /references pack GUID \{AB12CD34EF567890\}/,
  },
  {
    name: "core layer references a pack path without its GUID",
    plant: (r) => put(r, `${CORE}/Worlds/W_Layers/a.layer`, `GenericEntity : "Prefabs/Signs/Sign_Pub_Everon.et" {\n}\n`),
    expect: /references pack path Prefabs\/Signs\/Sign_Pub_Everon\.et/,
  },
  {
    name: "core gproj depends on the pack project",
    plant: (r) => put(r, `${CORE}/${CORE}.gproj`, `GameProject {\n ID "${CORE}"\n GUID "${CORE_PROJECT_GUID}"\n Dependencies {\n  "58D0FB3206B6F859" "${PACK_PROJECT_GUID}"\n }\n}\n`),
    expect: /depends on the pack project B1E84D5687E11724/,
  },
  {
    name: "core config names the pack addon",
    plant: (r) => put(r, `${CORE}/Configs/x.conf`, `X {\n m_sPack "${PACK}"\n}\n`),
    expect: /names the pack addon/,
  },
  {
    name: "pack holds script that is not allowlisted",
    plant: (r) => put(r, `${PACK}/Scripts/Game/LFE_Glue.c`, `class LFE_Glue\n{\n}\n`),
    expect: /LF_Everon1989\/Scripts\/Game\/LFE_Glue\.c: script in the pack/,
  },
  {
    name: "core script uses a pack class",
    plant: (r) => {
      put(r, `${PACK}/Scripts/Game/LFE_Glue.c`, `class LFE_Glue\n{\n}\n`);
      put(r, `${CORE}/Scripts/Game/LF_Uses.c`, `class LF_Uses\n{\n\tref LFE_Glue m_Glue;\n}\n`);
    },
    expect: /uses pack class LFE_Glue/,
  },
];

let failed = 0;
const roots = [];
try {
  const clean = cleanTree();
  roots.push(clean);
  const cleanErrors = checkBoundary(clean);
  if (cleanErrors.length) {
    failed++;
    console.log(`FAIL clean tree reported violations:\n  ${cleanErrors.join("\n  ")}`);
  }
  const noPack = cleanTree();
  roots.push(noPack);
  rmSync(join(noPack, PACK), { recursive: true });
  if (checkBoundary(noPack).length) {
    failed++;
    console.log("FAIL a core with no pack folder reported violations");
  }
  for (const c of cases) {
    const root = cleanTree();
    roots.push(root);
    c.plant(root);
    const errors = checkBoundary(root);
    if (!errors.some((e) => c.expect.test(e))) {
      failed++;
      console.log(`FAIL ${c.name}: expected ${c.expect}, got:\n  ${errors.join("\n  ") || "(no violations)"}`);
    }
  }
} finally {
  // Fixture trees are plain files under the OS temp dir: no junctions.
  for (const r of roots) rmSync(r, { recursive: true, force: true });
}

if (failed) {
  console.log(`addon-boundary-selftest: ${failed} case(s) failed`);
  process.exit(1);
}
console.log(`addon-boundary-selftest: OK (clean tree, no pack, ${cases.length} planted violations caught)`);
