// node tools/assets/sign-carriers/build.mjs [--blender-only]
// The sign carrier kit (#3307): runs build.py in headless Blender to write six one-mesh quads, copies them to
// Assets/Signs/Carriers/, and writes each model's .xob.meta and carrier prefab with their .meta. A re-run keeps every
// GUID it finds, so nothing churns. Workbench then builds the .xob beside each .fbx (the rdb refresh of any test or
// session run does it). It prints the {GUID}path carrier references the LF_SignStyles.conf size rows use.
import { existsSync, mkdirSync, readFileSync, writeFileSync, copyFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { freshGuid, readGuid, xobMeta, xobMetaIds } from "../pipeline/meta.mjs";
import { guidsInUse } from "../pipeline/addon.mjs";
import { runHeadless } from "../../lib/blender.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = resolve(HERE, "..", "..", "..");
const ADDON = join(ROOT, "addons", "LifeFramework");
const ASSET_DIR = "Assets/Signs/Carriers";
const PREFAB_DIR = "Prefabs/Props/Signs/Carriers";
const SOURCE = join(ADDON, "Assets", "Signs", "LF_BoardFace.fbx");
const EMAT = "{355E4F25429C2EF1}Assets/Signs/Data/LF_SignFace.emat";
const MATERIAL = "LF_SignFace";

export const KEYS = ["1x1", "4x3", "3x2", "2x1", "4x1", "1x2"];

// The carrier prefab: one MeshObject, the xob, the face material assigned, no shadow, like LF_BoardFace.et.
export function prefabText(xob, ids) {
  return `GenericEntity {
 ID "${ids.entity}"
 components {
  MeshObject "{${ids.mesh}}" {
   Object "${xob}"
   Materials {
    MaterialAssignClass "{${ids.assign}}" {
     SourceMaterial "${MATERIAL}"
     AssignedMaterial "${EMAT}"
    }
   }
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
  return {
    entity: pick(/^ID "([0-9A-F]{16})"/m),
    mesh: pick(/MeshObject "\{([0-9A-F]{16})\}"/),
    assign: pick(/MaterialAssignClass "\{([0-9A-F]{16})\}"/),
  };
}

function main() {
  const out = join(ROOT, "tmp", "signcar");
  mkdirSync(out, { recursive: true });
  runHeadless(join(HERE, "build.py"), [SOURCE, out, join(out, "report.json")], { log: join(out, "blender.log") });
  const report = JSON.parse(readFileSync(join(out, "report.json"), "utf8"));
  const used = guidsInUse(ROOT);
  const refs = {};
  mkdirSync(join(ADDON, ASSET_DIR), { recursive: true });
  mkdirSync(join(ADDON, PREFAB_DIR), { recursive: true });
  for (const key of KEYS) {
    const name = `LF_SignQuad_${key}`;
    const info = report[key];
    if (!info || info.materials.length !== 1 || info.materials[0] !== MATERIAL) throw new Error(`${name}: the build did not give one ${MATERIAL} material`);
    const fbx = `${ASSET_DIR}/${name}.fbx`;
    const xobRes = `${ASSET_DIR}/${name}.xob`;
    copyFileSync(info.fbx, join(ADDON, fbx));
    const xobMetaFile = join(ADDON, xobRes + ".meta");
    const xobGuid = readGuid(xobMetaFile) || freshGuid(used);
    const ids = xobMetaIds(existsSync(xobMetaFile) ? readFileSync(xobMetaFile, "utf8") : "", used);
    writeFileSync(xobMetaFile, xobMeta(xobRes, xobGuid, { materials: [{ source: MATERIAL, emat: EMAT }], colliders: [], ids }));

    const etRes = `${PREFAB_DIR}/${name}.et`;
    const etFile = join(ADDON, etRes);
    const etGuid = readGuid(etFile + ".meta") || freshGuid(used);
    writeFileSync(etFile, prefabText(`{${xobGuid}}${xobRes}`, prefabIds(existsSync(etFile) ? readFileSync(etFile, "utf8") : "", used)));
    if (!existsSync(etFile + ".meta")) writeFileSync(etFile + ".meta", prefabMeta(etRes, etGuid));
    refs[key] = `{${etGuid}}${etRes}`;
    console.log(`${key.padEnd(4)} ${info.width.toFixed(4)} x ${info.height.toFixed(4)} m  ${refs[key]}`);
  }
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) main();
