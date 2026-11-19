// node tools/blender/harvest-reaper/build.mjs
// The reaper model (#3292): runs reaper.py in headless Blender to write LF_Reaper_01.fbx (two LODs), copies it to
// Assets/Harvest/Reaper/, and writes the .xob.meta that binds four materials, each material's .emat and .meta. The
// textures are the base game's shared metal maps, referenced by their game GUIDs; this adds none. A re-run keeps
// every GUID it finds, so nothing churns. It does not touch the prefab: Prefabs/Harvest/LF_Reaper_01.et points at
// the .xob by the GUID in the .xob.meta. Workbench then builds the .xob beside the .fbx (`tools\cli rebuild-fbx`, or
// the rdb refresh of any test or look run).
import { copyFileSync, existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { ematMeta, freshGuid, readGuid, xobMeta, xobMetaIds } from "../../assets/pipeline/meta.mjs";
import { guidsInUse } from "../../assets/pipeline/addon.mjs";
import { runHeadless } from "../../lib/blender.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = resolve(HERE, "..", "..", "..");
const ADDON = join(ROOT, "addons", "LifeFramework");
const ASSET_DIR = "Assets/Harvest/Reaper";
const MODEL = "LF_Reaper_01";

// The base game's shared metal maps (Assets/_SharedData/Metal), GUIDs as the pack's own AB_*.emat files carry them
// (AB_Red_Paint, AB_Frame_Dark, AB_Metal_Steel). The materials load these by GUID, so they must not change.
const VANILLA = {
  paint: {
    bcr: "{6390F3AA091C74E1}Assets/_SharedData/Metal/ST_MetalPaint_Mate_1m_BCR.edds",
    nmo: "{E33A5A9E39D9E660}Assets/_SharedData/Metal/ST_MetalPaint_Mate_1m_NMO.edds",
  },
  coated: {
    bcr: "{BB440784A103CB06}Assets/_SharedData/Metal/ST_MetalPaint_Coated_1m_BCR.edds",
    nmo: "{839CBC1678749998}Assets/_SharedData/Metal/ST_MetalPaint_Coated_1m_NMO.edds",
  },
  steel: {
    bcr: "{DDFA1BE6FC945C12}Assets/_SharedData/Metal/ST_Metal_Stainles01_1m_BCR.edds",
    nmo: "{E522A07425E30E8C}Assets/_SharedData/Metal/ST_Metal_Stainles01_1m_NMO.edds",
  },
};

// The four materials, in the model's slot order. The tints are what makes it a 1989 kolkhoz kit: a faded field
// green on the frame, a sun-bleached brick red on the hopper and hubs, a worn bare steel on the bar and rims, and a
// near-black for tyres and gear cases. The maps carry the scuffs and the wear.
export const MATERIALS = {
  LF_Reaper_Paint: { maps: "paint", color: [0.022, 0.05, 0.02] },
  LF_Reaper_Red: { maps: "paint", color: [0.11, 0.014, 0.010] },
  LF_Reaper_Steel: { maps: "steel", color: [0.20, 0.19, 0.18] },
  LF_Reaper_Dark: { maps: "coated", color: [0.012, 0.012, 0.014] },
};

export function ematText(name, uvGuid) {
  const m = MATERIALS[name];
  const maps = VANILLA[m.maps];
  return `MatPBRBasic {
 Color ${m.color.join(" ")} 1
 BCRMap "${maps.bcr}"
 NMOMap "${maps.nmo}"
 GlobalMapsUVTransform MatUVTransform "{${uvGuid}}" {
  TilingU 1
  TilingV 1
 }
}
`;
}

// The id the emat's UV transform already carries, so a re-run rewrites the same text.
function uvGuidOf(file, used) {
  const m = existsSync(file) ? /MatUVTransform "\{([0-9A-F]{16})\}"/.exec(readFileSync(file, "utf8")) : null;
  return m ? m[1] : freshGuid(used);
}

function put(rel, text) {
  const file = join(ADDON, rel);
  mkdirSync(dirname(file), { recursive: true });
  writeFileSync(file, text);
}

function main() {
  const out = join(ROOT, "tmp", "reaper");
  mkdirSync(out, { recursive: true });
  runHeadless(join(HERE, "reaper.py"), [out, join(out, "report.json")], { log: join(out, "blender.log") });
  const info = JSON.parse(readFileSync(join(out, "report.json"), "utf8"))[MODEL];
  if (!info) throw new Error(`${MODEL}: the build wrote no model`);
  const slots = info.lods[0].materials;
  for (const lod of info.lods) {
    if (JSON.stringify(lod.materials) !== JSON.stringify(slots)) throw new Error(`${lod.name}: its materials differ from LOD0`);
  }
  for (const name of slots) if (!MATERIALS[name]) throw new Error(`the model uses ${name}, which build.mjs does not define`);

  const used = guidsInUse(ROOT);
  const assigns = [];
  for (const name of slots) {
    const ematRes = `${ASSET_DIR}/Data/${name}.emat`;
    const ematFile = join(ADDON, ematRes);
    const ematGuid = readGuid(ematFile + ".meta") || freshGuid(used);
    put(ematRes, ematText(name, uvGuidOf(ematFile, used)));
    if (!existsSync(ematFile + ".meta")) put(ematRes + ".meta", ematMeta(ematRes, ematGuid));
    assigns.push({ source: name, emat: `{${ematGuid}}${ematRes}` });
  }

  const fbxRes = `${ASSET_DIR}/${MODEL}.fbx`;
  const xobRes = `${ASSET_DIR}/${MODEL}.xob`;
  mkdirSync(join(ADDON, ASSET_DIR), { recursive: true });
  copyFileSync(info.fbx, join(ADDON, fbxRes));
  const xobMetaFile = join(ADDON, xobRes + ".meta");
  const xobGuid = readGuid(xobMetaFile) || freshGuid(used);
  const ids = xobMetaIds(existsSync(xobMetaFile) ? readFileSync(xobMetaFile, "utf8") : "", used);
  // One id per material assignment: xobMetaIds hands back one, so top it up.
  while (ids.assign.length < assigns.length) ids.assign.push(freshGuid(used));
  put(xobRes + ".meta", xobMeta(xobRes, xobGuid, { materials: assigns, colliders: [], ids }));
  const b = info.lods[0];
  console.log(`${MODEL}: {${xobGuid}}${xobRes}  ${info.lods.map((l) => `${l.name} ${l.tris} tris`).join(", ")}  bounds ${b.min.map((n) => n.toFixed(2))} .. ${b.max.map((n) => n.toFixed(2))}`);
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) main();
