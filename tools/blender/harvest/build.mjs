// node tools/blender/harvest/build.mjs
// The wheat patch models (#3290): runs wheat_patch.py in headless Blender to write LF_WheatPatch_01.fbx and
// LF_WheatStubble_01.fbx (two LODs each), copies them to Assets/Harvest/WheatPatch/, and writes each model's .xob.meta,
// its .emat and .meta, and the prefab with its .meta. The textures are the base game's wheat polyplane maps, referenced
// by their game GUIDs; this adds none. A re-run keeps every GUID it finds, so nothing churns. Workbench then builds
// the .xob beside each .fbx (the rdb refresh of any test, look or session run does it).
import { copyFileSync, existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { ematMeta, freshGuid, readGuid, xobMeta, xobMetaIds } from "../../assets/pipeline/meta.mjs";
import { guidsInUse } from "../../assets/pipeline/addon.mjs";
import { runHeadless } from "../../lib/blender.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = resolve(HERE, "..", "..", "..");
const ADDON = join(ROOT, "addons", "LifeFramework");
const ASSET_DIR = "Assets/Harvest/WheatPatch";
const PREFAB_DIR = "Prefabs/Harvest";

// The base game's wheat polyplane maps (Assets/Vegetation/_Polyplanes), GUIDs from the game's resource database
// (tools/buildings/pipeline/gamedb.mjs), game 1.9. The prefab loads these by GUID, so they must not change.
const VANILLA = {
  bcr: "{6ADD1434EC0522EE}Assets/Vegetation/_Polyplanes/polyplane_wheat_01_BCR.edds",
  opacity: "{176459F4491F2679}Assets/Vegetation/_Polyplanes/polyplane_wheat_01_A.edds",
  ntc: "{F9B3F9B3277FA1EB}Assets/Vegetation/_Polyplanes/polyplane_wheat_01_NTC.edds",
};

// What each model is: its material name, the emat's wind (a cut field does not sway) and thickness.
export const MODELS = {
  LF_WheatPatch_01: { material: "LF_WheatPatch", wind: { speed: 0.45, length: 0.012 }, thickness: 0.35 },
  LF_WheatStubble_01: { material: "LF_WheatStubble", wind: { speed: 0, length: 0 }, thickness: 0.2 },
};

// The material, modelled on the base game's placed crops (Assets/Vegetation/Vegetables/Data/PotatoCrop_01.emat):
// a MatPBRTreeCrown, alpha-cut by the opacity map, lit through the leaf like the vanilla field plants.
export function ematText(model) {
  const m = MODELS[model];
  return `MatPBRTreeCrown {
 SpecularIBL 0.45 0.45 0.4 1
 PorosityScale 0.2
 MetalnessScale 0
 BCRMap "${VANILLA.bcr}"
 OpacityMap "${VANILLA.opacity}"
 NTCMap "${VANILLA.ntc}"
 WindLocalBranchSpeed ${m.wind.speed}
 WindLocalBranchLength ${m.wind.length}
 WindObjectScale 1
 GeometryAOIntensity 0
 ThicknessScale ${m.thickness}
 SSSIndirectionIntensity 0.5
 SSSColor 1 1 1 1
 SSSColorMultiply 1
}
`;
}

// The prefab: one MeshObject, the xob, no shadow (a field of alpha cards would cost a shadow draw per patch).
export function prefabText(xob, ids) {
  return `GenericEntity {
 ID "${ids.entity}"
 components {
  MeshObject "{${ids.mesh}}" {
   Object "${xob}"
   CastShadow 0
  }
 }
}
`;
}

export function prefabMeta(res, guid) {
  const cfg = ["PC", "XBOX_ONE : PC", "XBOX_SERIES : PC", "PS4 : PC", "HEADLESS : PC"];
  return `MetaFileClass {\n Name "{${guid}}${res}"\n Configurations {\n${cfg.map((c) => `  EntityTemplateResourceClass ${c} {\n  }\n`).join("")} }\n}\n`;
}

// The ids a prefab already carries, so a re-run rewrites the same text.
function prefabIds(text, used) {
  const pick = (re) => (re.exec(text) || [])[1] || freshGuid(used);
  return { entity: pick(/^ID "([0-9A-F]{16})"/m), mesh: pick(/MeshObject "\{([0-9A-F]{16})\}"/) };
}

function put(rel, text) {
  const file = join(ADDON, rel);
  mkdirSync(dirname(file), { recursive: true });
  writeFileSync(file, text);
}

function main() {
  const out = join(ROOT, "tmp", "wheat");
  mkdirSync(out, { recursive: true });
  runHeadless(join(HERE, "wheat_patch.py"), [out, join(out, "report.json")], { log: join(out, "blender.log") });
  const report = JSON.parse(readFileSync(join(out, "report.json"), "utf8"));
  const used = guidsInUse(ROOT);
  for (const [model, spec] of Object.entries(MODELS)) {
    const info = report[model];
    if (!info) throw new Error(`${model}: the build wrote no model`);
    for (const lod of info.lods) {
      if (lod.materials.length !== 1 || lod.materials[0] !== spec.material) throw new Error(`${lod.name}: not one ${spec.material} material`);
    }

    // The material and its .meta.
    const ematRes = `${ASSET_DIR}/Data/${spec.material}.emat`;
    const ematGuid = readGuid(join(ADDON, ematRes + ".meta")) || freshGuid(used);
    put(ematRes, ematText(model));
    if (!existsSync(join(ADDON, ematRes + ".meta"))) put(ematRes + ".meta", ematMeta(ematRes, ematGuid));

    // The model: the .fbx and the .xob.meta that binds the material.
    const fbxRes = `${ASSET_DIR}/${model}.fbx`;
    const xobRes = `${ASSET_DIR}/${model}.xob`;
    mkdirSync(join(ADDON, ASSET_DIR), { recursive: true });
    copyFileSync(info.fbx, join(ADDON, fbxRes));
    const xobMetaFile = join(ADDON, xobRes + ".meta");
    const xobGuid = readGuid(xobMetaFile) || freshGuid(used);
    const ids = xobMetaIds(existsSync(xobMetaFile) ? readFileSync(xobMetaFile, "utf8") : "", used);
    put(xobRes + ".meta", xobMeta(xobRes, xobGuid, { materials: [{ source: spec.material, emat: `{${ematGuid}}${ematRes}` }], colliders: [], ids }));

    // The prefab.
    const etRes = `${PREFAB_DIR}/${model}.et`;
    const etFile = join(ADDON, etRes);
    const etGuid = readGuid(etFile + ".meta") || freshGuid(used);
    put(etRes, prefabText(`{${xobGuid}}${xobRes}`, prefabIds(existsSync(etFile) ? readFileSync(etFile, "utf8") : "", used)));
    if (!existsSync(etFile + ".meta")) put(etRes + ".meta", prefabMeta(etRes, etGuid));
    console.log(`${model}: {${etGuid}}${etRes}  ${info.lods.map((l) => `${l.name} ${l.tris} tris`).join(", ")}`);
  }
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) main();
