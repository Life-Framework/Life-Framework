// The fence yard's building prefab and its furniture, from the base game's Workshop_01_Main and Workshop_01_Side.
//
//   node tools/buildings/fence-yard/prune.mjs          write FenceYard.et, FenceYard_Side.et and their furniture
//   node tools/buildings/fence-yard/prune.mjs --check  exit 1 when the files are not what this script writes
//
// FenceYard.et is the vanilla Workshop_01_Main (doors, windows, entries) with its furniture child pointed at our
// copy, and Workshop_01_Side as a child annex behind it, as the pair stands at Saint-Philippe b33/b36. Each copy
// of the furniture is the vanilla set flattened to one child per prop, with the repeats and the props the yard
// does not need left out (switches, the stray cart, spare crates and barrels). Writing the files twice gives the
// same bytes; a missing `.meta` is made with a fresh GUID and kept after that (rule 2).
import { randomBytes } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { gameVfs, readText } from "../../lib/game-prefab.mjs";
import { emitEntity, entities } from "../town-hall/furniture-lib.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = join(HERE, "..", "..", "..");
const REL = "Prefabs/Buildings/FenceYard";
const DIR = join(ROOT, "addons", "LF_Everon1989", ...REL.split("/"));
const V = "Prefabs/Structures/Industrial/Houses/Workshop_01";
const CHECK = process.argv.includes("--check");

// The annex in the main building's space (front +z): Saint-Philippe b36 relative to b33 (world 4544.2 10663.2
// yaw -101 against 4546.2 10670 yaw -11), pushed 0.16 m back so its wall clears the main's back wall collider.
export const SIDE = { x: -3.26, z: -6.45, yaw: -90 };

// Props that never stay, and at most N of a prop per building in file order (unlisted props stay).
const DROP = /LightSwitch_02|Cart_01|PlantRoom_04|Cardboard_02|Bucket_01|WoodKindling_Crate_01|CoatRack_01/;
const LIMIT = [
  [/Crate_01_green/, 2],
  [/BarrelFuel_01_closed_old/, 2],
  [/CabinetCardFile_01/, 1],
  [/BoxWooden_01/, 1],
];

export function makeKeep() {
  const count = new Map();
  return (e) => {
    if (DROP.test(e.path)) return false;
    for (const [rx, max] of LIMIT) {
      if (!rx.test(e.path)) continue;
      const n = (count.get(rx.source) ?? 0) + 1;
      count.set(rx.source, n);
      if (n > max) return false;
    }
    return true;
  };
}

const metaText = (guid, path) => `MetaFileClass {
 Name "{${guid}}${path}"
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

// The GUID a file already has, or a fresh one (kept from then on).
function guidOf(rel) {
  const meta = join(DIR, `${rel}.meta`);
  if (existsSync(meta)) return /Name "\{([0-9A-F]{16})\}/.exec(readFileSync(meta, "utf8"))?.[1];
  if (CHECK) throw new Error(`${rel}.meta is missing`);
  const guid = randomBytes(8).toString("hex").toUpperCase();
  mkdirSync(dirname(meta), { recursive: true });
  writeFileSync(meta, metaText(guid, `${REL}/${rel}`));
  return guid;
}

const strip = (t) => t.replace(/\r/g, "").split("\n").filter((l) => !l.startsWith("//")).join("\n").replace(/^\n+/, "");
const esc = (s) => s.replace(/[.*+?^${}()|[\]\/]/g, "\$&");

// One building: the vanilla prefab with its furniture child repointed, and the pruned furniture copy.
function building(vfs, stem, vFurn, out, keep, extra = "") {
  const vanilla = strip(readText(ROOT, vfs, `${V}/${stem}.et`));
  const vFurniture = strip(readText(ROOT, vfs, `${V}/Furniture/${vFurn}.et`));
  const furnRel = `Furniture/${out}_furniture_01.et`;
  const fGuid = guidOf(furnRel);
  guidOf(`${out}.et`);
  const oldRef = new RegExp(`GenericEntity : "\{[0-9A-F]+\}${esc(`${V}/Furniture/${vFurn}.et`)}"`);
  if (!oldRef.test(vanilla)) throw new Error(`${stem} has no furniture child`);
  let text = vanilla.replace(oldRef, `GenericEntity : "{${fGuid}}${REL}/${furnRel}"`).replace(/^ coords [-\d. ]+\n/m, "").trimEnd();
  if (extra) text = text.replace(/\n \}\n\}$/, `\n${extra}\n }\n}`);
  const root = /^(GenericEntity : "[^"]+" \{\n ID "[0-9A-F]+")/.exec(vFurniture)?.[1];
  if (!root) throw new Error(`cannot read the ${vFurn} root`);
  const all = entities(vFurniture);
  const kept = all.filter(keep);
  const furniture = `${root}\n {\n${kept.map(emitEntity).join("\n")}\n }\n}\n`;
  return { files: [[join(DIR, `${out}.et`), `${text}\n`], [join(DIR, ...furnRel.split("/")), furniture]], kept: kept.length, of: all.length };
}

async function main() {
  const vfs = await gameVfs();
  const side = building(vfs, "Workshop_01_Side", "Workshop_01_Side_Furniture_01", "FenceYard_Side", makeKeep());
  const sideGuid = guidOf("FenceYard_Side.et");
  // The annex child: a fixed ID (rule 2: made once), placed in the main building's space.
  const annex = `  SCR_DestructibleBuildingEntity Annex : "{${sideGuid}}${REL}/FenceYard_Side.et" {\n   ID "FE4CE0A2D5B1E001"\n   coords ${SIDE.x} 0 ${SIDE.z}\n   angleY ${SIDE.yaw}\n  }`;
  const main = building(vfs, "Workshop_01_Main", "Workshop_01_Main_furniture_01", "FenceYard", makeKeep(), annex);

  let drift = false;
  for (const [file, text] of [...main.files, ...side.files]) {
    const cur = existsSync(file) ? readFileSync(file, "utf8") : null;
    if (cur === text) continue;
    drift = true;
    if (!CHECK) {
      mkdirSync(dirname(file), { recursive: true });
      writeFileSync(file, text);
    }
  }
  console.log(`fence-yard shell: workshop ${main.kept}/${main.of} vanilla prop(s) kept, annex ${side.kept}/${side.of}${CHECK ? (drift ? " DRIFT" : " current") : " written"}`);
  if (CHECK && drift) process.exit(1);
}
main().catch((e) => {
  console.error(e.message);
  process.exit(1);
});
