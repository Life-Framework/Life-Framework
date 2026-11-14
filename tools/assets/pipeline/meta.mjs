// Writers for the text files Workbench would otherwise create through a dialog: resource .meta
// files with fresh GUIDs, and the .emat. Registering a new .fbx from a headless Workbench opens a
// modal window and hangs it (docs/research/asset-pipeline.md); a .meta that is already on disk
// makes the file a registered resource, so the plugin only has to rebuild it.
import { randomBytes } from "node:crypto";
import { existsSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";

const PLATFORMS = ["PC", "XBOX_ONE", "XBOX_SERIES", "PS4", "HEADLESS"];
// Config GUIDs under Configs/System/ResourceTypes/<platform>/, as Workbench writes them (read from
// this addon's Bottle_BCR, Bottle_NMO and HouseKey metas).
const CONF = {
  "TextureColorMap.conf": { PC: "EAB5DE3219F9CBA8", XBOX_ONE: "91D862F89991BFBE", XBOX_SERIES: "5FEAED1642ECE679", PS4: "12273E1A0928F0C4", HEADLESS: "BEAF5CD0C438676E" },
  "TextureNType.conf": { PC: "A968DA7F9A1E3A3E", XBOX_ONE: "7F6A4D372443A88D", XBOX_SERIES: "065D289C7FF8B20D", PS4: "29DF4A6CBBABE916", HEADLESS: "AFD658E4D0EB5FBC" },
  "MeshObjectCommon.conf": { PC: "0877E7C4BB2B2C9A", XBOX_ONE: "9D5B6207F7628CE2", XBOX_SERIES: "DD9F02115C764647", PS4: "53EC476BC921D99A", HEADLESS: "3A5B3356978039E8" },
};

const GUID_RE = /\{([0-9A-F]{16})\}/g;

// Every GUID in use under a tree of .meta files (resource GUIDs and the ids inside them).
export function usedGuids(dir, into = new Set()) {
  for (const e of readdirSync(dir, { withFileTypes: true })) {
    const p = join(dir, e.name);
    if (e.isDirectory()) usedGuids(p, into);
    else if (e.name.endsWith(".meta")) for (const m of readFileSync(p, "utf8").matchAll(GUID_RE)) into.add(m[1]);
  }
  return into;
}

// A GUID that `used` does not hold; it is added to the set so one batch never repeats one.
export function freshGuid(used, random = () => randomBytes(8)) {
  for (let i = 0; i < 100; i++) {
    const g = Buffer.from(random()).toString("hex").toUpperCase().padStart(16, "0");
    if (/^0+$/.test(g) || used.has(g)) continue;
    used.add(g);
    return g;
  }
  throw new Error("could not draw an unused GUID in 100 tries");
}

export function readGuid(metaFile) {
  if (!existsSync(metaFile)) return null;
  const m = /Name "\{([0-9A-F]{16})\}/.exec(readFileSync(metaFile, "utf8"));
  return m ? m[1] : null;
}

export function textureMeta(resPath, conf, guid) {
  const row = (p) => `  TIFFResourceClass ${p} : "{${CONF[conf][p]}}Configs/System/ResourceTypes/${p}/${conf}" {\n  }\n`;
  return `MetaFileClass {\n Name "{${guid}}${resPath}"\n Configurations {\n${PLATFORMS.map(row).join("")}  TIFFResourceClass PS5 : PC {\n  }\n }\n}\n`;
}

export function ematText({ bcr, nmo }) {
  return `MatPBRBasic {\n BCRMap "${bcr}"\n NMOMap "${nmo}"\n}\n`;
}

export function ematMeta(resPath, guid) {
  const row = (p, i) => `  EMATResourceClass ${p}${i ? " : PC" : ""} {\n  }\n`;
  return `MetaFileClass {\n Name "{${guid}}${resPath}"\n Configurations {\n${[...PLATFORMS, "PS5"].map(row).join("")} }\n}\n`;
}

// materials: [{ source, emat }] with emat as {GUID}path. colliders: [{ name, layerPreset, gamemat }].
// flags: FBXResourceClass switches such as ExportSceneHierarchy and ExportSkinning (a skinned weapon
// needs both, or the .xob comes out with no bones and no sockets).
export function xobMeta(resPath, guid, { materials, colliders, ids, flags = {} }) {
  const switches = Object.entries(flags).filter(([, v]) => v).map(([k]) => `   ${k} 1\n`).join("");
  const assigns = materials.map((m, i) => `    MaterialAssignClass "{${ids.assign[i]}}" {\n     SourceMaterial "${m.source}"\n     AssignedMaterial "${m.emat}"\n    }\n`).join("");
  const geoms = colliders.map((c) => `    GeometryParam ${c.name} {\n     LayerPreset "${c.layerPreset}"\n     SurfaceProperties {\n      "${c.gamemat}"\n     }\n     Mass 0\n     Margin 0\n    }\n`).join("");
  const common = (p) => `   Common TXOCommonClass "{${ids.common[p]}}" : "{${CONF["MeshObjectCommon.conf"][p]}}Configs/System/ResourceTypes/${p}/MeshObjectCommon.conf" {\n   }\n`;
  const others = PLATFORMS.slice(1).map((p) => `  FBXResourceClass ${p} : PC {\n${common(p)}  }\n`).join("");
  return `MetaFileClass {\n Name "{${guid}}${resPath}"\n Configurations {\n  FBXResourceClass PC {\n${switches}   MaterialAssigns {\n${assigns}   }\n   GeometryParams {\n${geoms}   }\n${common("PC")}  }\n${others}  FBXResourceClass PS5 : PC {\n  }\n }\n}\n`;
}

// The ids Workbench keeps inside an existing .xob.meta, so a re-import does not churn them.
export function xobMetaIds(text, used) {
  const assign = [...text.matchAll(/MaterialAssignClass "\{([0-9A-F]{16})\}"/g)].map((m) => m[1]);
  const common = {};
  for (const p of PLATFORMS) {
    const m = new RegExp(`TXOCommonClass "\\{([0-9A-F]{16})\\}" : "\\{[0-9A-F]{16}\\}Configs/System/ResourceTypes/${p}/`).exec(text);
    common[p] = m ? m[1] : freshGuid(used);
  }
  return { assign: assign.length ? assign : [freshGuid(used)], common };
}

// Write every text file one imported model needs. Texture and material files are kept when they
// exist. The model's .meta is rewritten each time with the GUID and ids it already had, because
// Workbench drops what it could not import (an invalid collider's GeometryParam, for one).
// Returns the {GUID}path strings. `model` names a second model (a deployable's folded state) that
// shares `name`'s textures and material; it defaults to `name`.
export function writeAssetMetas(addonDir, { assetDir, name, model = name, collider, flags, defaultEmat, used = usedGuids(addonDir) }) {
  const data = `${assetDir}/Data`;
  const out = {};
  const put = (rel, text) => writeFileSync(join(addonDir, rel), text);
  const guidFor = (rel) => readGuid(join(addonDir, rel + ".meta")) || freshGuid(used);
  if (defaultEmat) {
    // A spec with variants has no base set: the model's own material is the first variant's.
    out.emat = defaultEmat;
  } else {
    for (const [suffix, conf] of [["BCR", "TextureColorMap.conf"], ["NMO", "TextureNType.conf"]]) {
      const res = `${data}/${name}_${suffix}.edds`;
      const guid = guidFor(res);
      if (!existsSync(join(addonDir, res + ".meta"))) put(res + ".meta", textureMeta(res, conf, guid));
      out[suffix.toLowerCase()] = `{${guid}}${res}`;
    }
    const ematRes = `${data}/${name}.emat`;
    const ematGuid = guidFor(ematRes);
    if (!existsSync(join(addonDir, ematRes))) put(ematRes, ematText(out));
    if (!existsSync(join(addonDir, ematRes + ".meta"))) put(ematRes + ".meta", ematMeta(ematRes, ematGuid));
    out.emat = `{${ematGuid}}${ematRes}`;
  }
  const xobRes = `${assetDir}/${model}.xob`;
  const xobGuid = guidFor(xobRes);
  const metaFile = join(addonDir, xobRes + ".meta");
  const ids = xobMetaIds(existsSync(metaFile) ? readFileSync(metaFile, "utf8") : "", used);
  put(xobRes + ".meta", xobMeta(xobRes, xobGuid, { materials: [{ source: name, emat: out.emat }], colliders: [collider], ids, flags }));
  out.xob = `{${xobGuid}}${xobRes}`;
  return out;
}

// An emat for a variant: the plain two-map material, or the spec's template with $BCR and $NMO in it
// (a two-layer rock material that keeps the vanilla mask and macro maps, say).
export function variantEmatText({ bcr, nmo }, template) {
  if (!template) return ematText({ bcr, nmo });
  if (!template.includes("$BCR")) throw new Error("an emat template must reference $BCR");
  return template.replaceAll("$BCR", bcr).replaceAll("$NMO", nmo);
}

// Every text file the variants need (docs/asset-pipeline/README.md "Variants"): <Name>_<Variant>_BCR
// and _NMO texture metas under <assetDir>/Data, and one emat per variant. A variant that names an
// existing emat (v.emat) keeps that file's GUID, so prefabs that reference it need no edit; its text
// is the import's output and is rewritten every run. Returns [{ name, bcr, nmo, emat }] as {GUID}path.
export function writeVariantMetas(addonDir, { assetDir, name, variants, templates = {}, used = usedGuids(addonDir) }) {
  const data = `${assetDir}/Data`;
  const put = (rel, text) => writeFileSync(join(addonDir, rel), text);
  const guidFor = (rel) => readGuid(join(addonDir, rel + ".meta")) || freshGuid(used);
  return variants.map((v) => {
    const out = { name: v.name };
    for (const [suffix, conf] of [["BCR", "TextureColorMap.conf"], ["NMO", "TextureNType.conf"]]) {
      const res = `${data}/${name}_${v.name}_${suffix}.edds`;
      const guid = guidFor(res);
      if (!existsSync(join(addonDir, res + ".meta"))) put(res + ".meta", textureMeta(res, conf, guid));
      out[suffix.toLowerCase()] = `{${guid}}${res}`;
    }
    const ematRes = v.emat || `${data}/${name}_${v.name}.emat`;
    const ematGuid = guidFor(ematRes);
    put(ematRes, variantEmatText(out, templates[v.name]));
    if (!existsSync(join(addonDir, ematRes + ".meta"))) put(ematRes + ".meta", ematMeta(ematRes, ematGuid));
    out.emat = `{${ematGuid}}${ematRes}`;
    return out;
  });
}
