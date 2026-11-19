// tools/validation/sign-materials-selftest.mjs - validate-sign-materials.
// inputs: tools/validation/validate-sign-materials.mjs, tools/lib/sign-materials.mjs, tools/validation/sign-materials-selftest.mjs
//
// Runs the validator over a throwaway tree:
//   - a pole plate that assigns the face and the LOD slot passes;
//   - the same prefab with only the face slot fails with file:line (the black far-LOD sign);
//   - a prefab that assigns an .emat no addon has fails;
//   - a sign .emat with no map and a black Color fails; a $rendertarget one passes.
//
// red-proof: 2026-11-19 with SIGN_BODY_LOD_SLOTS emptied, "a face-only pole plate fails" printed FAIL.
import { spawnSync } from "node:child_process";
import { mkdirSync, writeFileSync } from "node:fs";
import { scratchDir } from "../lib/scratch.mjs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { SIGN_BODY_LOD_SLOTS, signMaterialProblems } from "../lib/sign-materials.mjs";

let failed = 0;
const check = (ok, msg) => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${msg}`);
  if (!ok) failed++;
};

const validator = join(dirname(fileURLToPath(import.meta.url)), "validate-sign-materials.mjs");
const dir = scratchDir("lf-sign-materials-");
const prefabs = join(dir, "addons", "LF_X", "Prefabs");
const data = join(dir, "addons", "LF_X", "Assets", "Signs", "Data");
for (const d of [prefabs, data]) mkdirSync(d, { recursive: true });
const run = () => spawnSync(process.execPath, [validator], { env: { ...process.env, LF_SIGNMAT_ROOT: dir }, encoding: "utf8" });

const slot = (id, src, path) => `    MaterialAssignClass "{${id}}" {\n     SourceMaterial "${src}"\n     AssignedMaterial "{355E4F25429C2EF1}${path}"\n    }\n`;
const prefab = (...slots) => `GenericEntity : "{2F5C4F11D17E1725}Prefabs/x.et" {\n components {\n  MeshObject "{54969A6AAAADA251}" {\n   Materials {\n${slots.join("")}   }\n  }\n }\n}\n`;
const FACE = "Assets/Signs/Data/LF_SignFace.emat";
const noop = () => "MatPBRBasic {\n Color 1 1 1 1\n BCRMap \"$rendertarget\"\n}\n";

try {
  check(Object.keys(SIGN_BODY_LOD_SLOTS).length === 5, "five vanilla sign bodies are known");
  const both = prefab(slot("01", "SignGeneric_01", FACE), slot("02", "SignGeneric_01_MLOD", FACE));
  const faceOnly = prefab(slot("01", "SignGeneric_01", FACE));
  check(signMaterialProblems(both, noop).length === 0, "a pole plate with the face and LOD slots passes");
  check(signMaterialProblems(faceOnly, noop).length === 1, "a face-only pole plate fails");
  check(signMaterialProblems(faceOnly, noop)[0].line === 5, "the failing line is the face slot's");
  check(signMaterialProblems(prefab(slot("01", "SignCommercial_01", FACE), slot("02", "MLOD", FACE)), noop).length === 0, "the fascia's bare MLOD slot counts");
  check(signMaterialProblems(prefab(slot("01", "Whatever", FACE)), noop).length === 0, "an unknown body is only checked for its material");
  check(signMaterialProblems(both, () => null).length === 1, "an emat no addon has fails once, not once per slot");
  check(signMaterialProblems(both, () => "MatPBRBasic {\n Color 0 0 0 1\n}\n").length === 1, "a sign emat with no map and a black Color fails");
  check(signMaterialProblems(both, () => "MatPBRBasic {\n Color 0 0 0 1\n BCRMap \"$rendertarget\"\n}\n").length === 0, "a $rendertarget emat passes whatever its Color");

  writeFileSync(join(data, "LF_SignFace.emat"), noop());
  writeFileSync(join(prefabs, "Good.et"), both);
  let r = run();
  check(r.status === 0, `a clean tree passes (${r.stdout.trim().split("\n")[0]})`);

  writeFileSync(join(prefabs, "Bad.et"), faceOnly);
  r = run();
  check(r.status === 1 && /Bad\.et:5 assigns SignGeneric_01 but not SignGeneric_01_MLOD/.test(r.stdout), `the validator names the face-only prefab (${r.stdout.trim().split("\n")[0]})`);
} finally {
  // scratchDir removes the folder when the process ends.
}

if (failed) {
  console.log(`SUMMARY sign-materials-selftest fail ${failed}`);
  process.exit(1);
}
console.log("PASS  sign-materials-selftest");
