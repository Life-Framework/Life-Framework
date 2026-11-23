// tools/vehicle-colours/build.mjs - the lot and respray colours of the flagship pack (#4618, epic #1826).
//
//   node tools/vehicle-colours/build.mjs          write the colour prefabs, their metas and the overlay conf
//   node tools/vehicle-colours/build.mjs --check  fail when a written file differs from what this would write
//
// A colour is a thin locked child of the model's locked car prefab (EL_S105_beige, ...): it overrides
// only the body materials, and a model's own door and logo parts, with the VANILLA colour variant's
// own emats and part prefabs. No texture or material of ours. Every GUID of a vanilla file (emat,
// element, component, door prefab) is read from the game, never typed; the GUIDs of our files are
// derived from the colour id, so a rebuild writes the same ones.
//
// A model's own paint is the base colour: its locked child, colour 0, sold today. A vanilla colour that
// is the mesh's own paint (S105 light blue, T601 blue) has no emat to assign and a child cannot
// un-assign what the locked parent assigns, so those two are left out.
//
// Core vs pack: the lot palette is what this server sells, so the prefabs and the overlay are the
// pack's (`LF_VehicleColours_Everon1989.conf`, listed in the pack header); the colour names are neutral
// and sit in the core string table.
import { createHash } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { gameVfs } from "../lib/game-prefab.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const PACK = "addons/LF_Everon1989";
const WHEELED = "Prefabs/Vehicles/Wheeled";
const OUT_DIR = "Prefabs/Vehicles/Colours";
const OVERLAY = "Configs/Vehicles/LF_VehicleColours_Everon1989.conf";

// The colour names the string table carries (`LF-Colour_<key>`) and their lot swatch, 0xRRGGBB.
export const COLOURS = {
  Beige: 0xd8c9a0, Blue: 0x2f5f9e, Brown: 0x6b4a2b, DarkBlue: 0x1f2f5a, DarkBrown: 0x3f2a1c, DarkRed: 0x6e1f1f,
  Forest: 0x3d5230, Green: 0x3f6b3a, Grey: 0x8a8f94, LightBlue: 0x7fa6c9, LightGreen: 0x9cc089, Orange: 0xd77a2b,
  Red: 0xb22a2a, Teal: 0x2c7a7b, White: 0xf2f2ee, Yellow: 0xe0c13a,
};

// One model: its catalog prefab, its locked child (the base colour) and the vanilla variants sold besides.
// `slots` names the vanilla slot prefabs (doors, a logo) a colour carries over with its paint.
const MODELS = [
  {
    id: "S105", catalog: "S105/S105_beige", locked: "S105/EL_S105_beige", baseColour: "Beige",
    variants: { DarkBlue: "S105_darkblue", DarkBrown: "S105_darkbrown", DarkRed: "S105_darkred", Green: "S105_green", LightGreen: "S105_lightgreen", Red: "S105_red", White: "S105_white", Yellow: "S105_yellow" },
  },
  {
    id: "V2101", catalog: "V2101/V2101_beige", locked: "V2101/EL_V2101_beige", baseColour: "Beige",
    variants: { DarkBlue: "V2101_darkBlue", DarkRed: "V2101_darkRed", Green: "V2101_green", LightBlue: "V2101_lightBlue", LightGreen: "V2101_lightGreen", Red: "V2101_red", White: "V2101_white", Yellow: "V2101_yellow" },
  },
  {
    id: "T601", catalog: "T601/T601_beige", locked: "T601/EL_T601_beige", baseColour: "Beige",
    variants: { Green: "T601_green", Grey: "T601_grey", Orange: "T601_orange", White: "T601_white" },
  },
  {
    id: "S1203", catalog: "S1203/S1203_transport_beige", locked: "S1203/EL_S1203_transport_beige", baseColour: "Beige",
    variants: { Blue: "S1203_transport_blue", Brown: "S1203_transport_brown", White: "S1203_transport_white", Yellow: "S1203_transport_yellow" },
  },
  {
    id: "UAZ469", catalog: "UAZ469/UAZ469_covered_CIV_blue", locked: "UAZ469/EL_UAZ469_covered_CIV_blue", baseColour: "Blue", slots: /^(Door_|.*Logo)/i,
    variants: { Brown: "UAZ469_covered_CIV_brown", Forest: "UAZ469_covered_CIV_forest", Teal: "UAZ469_covered_CIV_teal" },
  },
];

const sha = (s) => createHash("sha1").update(`lf-vehicle-colour:${s}`).digest("hex").slice(0, 16).toUpperCase();
const ref = (guid, rel) => `{${guid}}${rel}`;

// The GUID a meta file names for a repo resource.
function repoGuid(rel) {
  for (const addon of ["LifeFramework", "LF_Everon1989"]) {
    const f = join(ROOT, "addons", addon, `${rel}.meta`);
    if (existsSync(f)) return /Name "\{([0-9A-F]{16})\}/.exec(readFileSync(f, "utf8"))[1];
  }
  throw new Error(`no meta for ${rel}`);
}

const PARENT = /^[A-Za-z_]+ : "\{([0-9A-F]{16})\}([^"]+)"/;
const ASSIGN = /MaterialAssignClass "\{([0-9A-F]{16})\}" \{\s+SourceMaterial "([^"]+)"\s+AssignedMaterial "\{([0-9A-F]{16})\}([^"]+)"/g;

function parseAssigns(text) {
  const out = [];
  for (const m of text.matchAll(ASSIGN)) out.push({ element: m[1], source: m[2], emat: m[3], ematPath: m[4] });
  return out;
}

function parseSlots(text, pick) {
  const out = [];
  for (const m of text.matchAll(/^    RegisteringComponentSlotInfo (\w+) \{\n[\s\S]*?^    \}\n/gm)) if (pick.test(m[1])) out.push({ name: m[1], text: m[0] });
  return out;
}

const component = (text, cls) => new RegExp(`^  ${cls} "\\{([0-9A-F]{16})\\}"`, "m").exec(text)?.[1];

// The element GUID the locked parent already assigns each source material with: a child that reuses it
// overrides in place; a new GUID could add a second entry the parent's paint outranks.
function inheritedElements(vfs, rel) {
  const found = new Map();
  for (let path = rel, guard = 0; path && guard < 12; guard++) {
    if (!vfs.exists(path)) throw new Error(`vanilla prefab missing: ${path}`);
    const text = vfs.readTextFile(path);
    for (const a of parseAssigns(text)) if (!found.has(a.source)) found.set(a.source, a.element);
    path = PARENT.exec(text)?.[2];
  }
  return found;
}

const meta = (guid, rel) => `MetaFileClass {
 Name "{${guid}}${rel}"
 Configurations {
  EntityTemplateResourceClass PC {
  }
  EntityTemplateResourceClass XBOX_ONE : PC {
  }
  EntityTemplateResourceClass XBOX_SERIES : PC {
  }
  EntityTemplateResourceClass PS4 : PC {
  }
  EntityTemplateResourceClass PS5 : PC {
  }
  EntityTemplateResourceClass HEADLESS : PC {
  }
 }
}
`;

function plan(vfs) {
  const files = [];
  const sets = [];
  const names = new Set();
  for (const model of MODELS) {
    const catalogRel = `${WHEELED}/${model.catalog}.et`;
    const lockedRel = `${WHEELED}/${model.locked}.et`;
    const catalogGuid = parseGuidFromVfs(vfs, catalogRel);
    const lockedGuid = repoGuid(lockedRel);
    const inherited = inheritedElements(vfs, catalogRel);
    const colours = [{ name: model.baseColour, spawn: ref(lockedGuid, lockedRel) }];
    for (const [name, variant] of Object.entries(model.variants)) {
      const vanillaRel = `${WHEELED}/${model.id}/${variant}.et`;
      if (!vfs.exists(vanillaRel)) throw new Error(`vanilla colour variant missing: ${vanillaRel}`);
      const text = vfs.readTextFile(vanillaRel);
      const assigns = parseAssigns(text);
      if (assigns.length === 0) throw new Error(`${variant} assigns no material, so a child cannot paint it`);
      for (const a of assigns) if (!vfs.exists(a.ematPath)) throw new Error(`${variant}: emat missing from the game: ${a.ematPath}`);
      const mesh = component(text, "MeshObject");
      const rel = `${OUT_DIR}/LF_${model.id}_${name}.et`;
      const guid = sha(`${model.id}:${name}:prefab`);
      const elements = assigns.map((a) => `    MaterialAssignClass "{${inherited.get(a.source) ?? a.element}}" {\n     SourceMaterial "${a.source}"\n     AssignedMaterial "{${a.emat}}${a.ematPath}"\n    }\n`).join("");
      let body = `  MeshObject "{${mesh}}" {\n   Materials {\n${elements}   }\n  }\n`;
      if (model.slots) {
        const slots = parseSlots(text, model.slots);
        if (slots.length) body += `  SlotManagerComponent "{${component(text, "SlotManagerComponent")}}" {\n   Slots {\n${slots.map((s) => s.text).join("")}   }\n  }\n`;
      }
      files.push({ rel: `${PACK}/${rel}`, text: `Vehicle : "{${lockedGuid}}${lockedRel}" {\n ID "${sha(`${model.id}:${name}:id`)}"\n components {\n${body} }\n}\n` });
      files.push({ rel: `${PACK}/${rel}.meta`, text: meta(guid, rel) });
      colours.push({ name, spawn: ref(guid, rel) });
    }
    for (const c of colours) names.add(c.name);
    sets.push({ id: model.id, catalog: ref(catalogGuid, catalogRel), colours });
  }
  for (const n of names) if (!(n in COLOURS)) throw new Error(`no swatch for colour ${n}`);

  let conf = "LF_VehicleColoursConfig {\n m_aModels {\n";
  for (const s of sets) {
    conf += `  LF_VehicleColourSet "{${sha(`${s.id}:set`)}}" {\n   m_sOp "add"\n   m_Prefab "${s.catalog}"\n   m_aColours {\n`;
    for (const c of s.colours) conf += `    LF_VehicleColour "{${sha(`${s.id}:${c.name}:colour`)}}" {\n     m_sNameKey "#LF-Colour_${c.name}"\n     m_iSwatch ${COLOURS[c.name]}\n     m_SpawnPrefab "${c.spawn}"\n    }\n`;
    conf += "   }\n  }\n";
  }
  conf += " }\n}\n";
  files.push({ rel: `${PACK}/${OVERLAY}`, text: conf });
  files.push({ rel: `${PACK}/${OVERLAY}.meta`, text: `MetaFileClass {\n Name "{${sha("overlay")}}${OVERLAY}"\n Configurations {\n  CONFResourceClass PC {\n  }\n  CONFResourceClass XBOX_ONE : PC {\n  }\n  CONFResourceClass XBOX_SERIES : PC {\n  }\n  CONFResourceClass PS4 : PC {\n  }\n  CONFResourceClass HEADLESS : PC {\n  }\n }\n}\n` });
  return { files, names: [...names].sort() };
}

// The GUID the game gives a vanilla prefab: its own twin text names it nowhere, so ask the repo's
// references (the catalog constants name every catalog prefab as {GUID}path).
function parseGuidFromVfs(vfs, rel) {
  const hits = readFileSync(join(ROOT, "addons/LifeFramework/Scripts/Game/Feature/Vehicles/LF_VehicleCatalog.c"), "utf8").match(new RegExp(`\\{([0-9A-F]{16})\\}${rel.replace(/[.]/g, "\\.")}`));
  if (!hits) throw new Error(`no GUID for ${rel} in LF_VehicleCatalog.c`);
  return hits[1];
}

const check = process.argv.includes("--check");
const { files, names } = plan(await gameVfs());
let stale = 0;
for (const f of files) {
  const path = join(ROOT, f.rel);
  const have = existsSync(path) ? readFileSync(path, "utf8") : null;
  if (have === f.text) continue;
  stale++;
  if (check) {
    console.error(`vehicle-colours: ${f.rel} differs from the build`);
    continue;
  }
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, f.text);
}
console.log(`vehicle-colours: ${files.length} files, ${stale} ${check ? "stale" : "written"}; colour names: ${names.join(", ")}`);
if (check && stale) process.exit(1);
