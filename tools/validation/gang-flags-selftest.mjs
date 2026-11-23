// tools/validation/gang-flags-selftest.mjs
// inputs: tools/assets/gang-flags/build.mjs, tools/assets/gang-flags/spec.json, addons/LF_Everon1989/Assets/Props/Flags/Gang/**
//
// Proves the twenty gang flags (#4219, #4389) render: every look prefab overrides the vanilla flag cloth's own
// MeshObject (a different component GUID shadows the mesh and the cloth vanishes) and every flag material inherits
// the vanilla flag material (`Cull none`; a plain MatPBRBasic draws one side, so a flag seen from behind vanished).
// red-proof: with the prefab MeshObject GUID {6A38C81C58F9A8A7} (the first build) this printed "FAIL Ashen: cloth MeshObject is {6A38C81C58F9A8A7}" (2026-11-23).
// red-proof: with the plain `MatPBRBasic {` emats this printed "FAIL Ashen: emat does not inherit the vanilla flag material" (2026-11-23).

import { existsSync, readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const PACK = join(ROOT, "addons", "LF_Everon1989");
const b = await import(pathToFileURL(join(ROOT, "tools", "assets", "gang-flags", "build.mjs")).href);
const VANILLA_CLOTH_MESH = "52AE82368499B365";
let failed = 0;
const fail = (m) => { console.log(`FAIL  ${m}`); failed++; };

// The pure helpers first: a plain emat is rewritten to inherit, an inheriting one is left alone.
const plain = 'MatPBRBasic {\n BCRMap "x"\n}\n';
if (b.inheritsCloth(plain)) fail("a plain MatPBRBasic counted as inheriting the flag material");
const fixed = b.withClothBase(plain);
if (!b.inheritsCloth(fixed)) fail("withClothBase did not make the emat inherit");
if (b.withClothBase(fixed) !== fixed) fail("withClothBase is not idempotent");

const names = b.flagNames();
if (names.length !== 20) fail(`expected 20 flags, spec has ${names.length}`);
for (const n of names) {
  const et = join(PACK, b.prefabRes(n));
  const emat = join(PACK, b.ematRes(n));
  if (!existsSync(et)) { fail(`${n}: ${b.prefabRes(n)} is missing`); continue; }
  const mesh = /MeshObject "\{([0-9A-F]{16})\}"/.exec(readFileSync(et, "utf8"));
  if (!mesh || mesh[1] !== VANILLA_CLOTH_MESH) fail(`${n}: cloth MeshObject is {${mesh ? mesh[1] : "missing"}}, the vanilla flag cloth's is {${VANILLA_CLOTH_MESH}}`);
  if (!existsSync(emat)) { fail(`${n}: ${b.ematRes(n)} is missing`); continue; }
  if (!b.inheritsCloth(readFileSync(emat, "utf8"))) fail(`${n}: emat does not inherit the vanilla flag material`);
}

if (failed) { console.log(`gang-flags-selftest: ${failed} failure(s)`); process.exit(1); }
console.log(`gang-flags-selftest: ok (${names.length} flags)`);
