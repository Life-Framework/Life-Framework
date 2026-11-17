// The town hall's building prefab and its furniture, from the base game's MunicipalOffice_E_01.
//
//   node tools/buildings/town-hall/shell.mjs          write TownHall.et and Furniture/TownHall_furniture_01.et
//   node tools/buildings/town-hall/shell.mjs --check  exit 1 when the files are not what this script writes
//
// TownHall.et is the vanilla building prefab (its doors, windows, entry and signs) with the furniture child
// pointed at our own copy. The copy is the vanilla furniture flattened to one child per prop, with the props the
// town hall does not need left out (the infirmary, the extra WCs, the duplicated switches and radiators) and the
// decor block (decor.mjs) appended. Writing the files twice gives the same bytes; a missing `.meta` is made with a
// fresh GUID and kept after that (rule 2: a GUID is never reused or changed).
import { randomBytes } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { gameVfs, readText } from "../../lib/game-prefab.mjs";
import { gameDb } from "../pipeline/gamedb.mjs";
import { emitEntity, entities, floorOf } from "./furniture-lib.mjs";
import { budget, decorBlock } from "./decor.mjs";
import { makeKeep, transform } from "./prune.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const DIR = join(ROOT, "addons", "LF_Everon1989", "Prefabs", "Buildings", "TownHall");
const VANILLA = "Prefabs/Structures/Services/Government/MunicipalOffice_E_01/MunicipalOffice_E_01.et";
const VANILLA_FURNITURE = "Prefabs/Structures/Services/Government/MunicipalOffice_E_01/Furniture/MunicipalOffice_E_01_furniture_01.et";
export const BUILDING = "TownHall.et";
export const FURNITURE = "Furniture/TownHall_furniture_01.et";
const CHECK = process.argv.includes("--check");
const PASS = process.argv.includes("--pass") ? Number(process.argv[process.argv.indexOf("--pass") + 1]) : 2; // 0 BUILD, 1 POLISH 1, 2 POLISH 2

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
export function guidOf(rel) {
  const meta = join(DIR, `${rel}.meta`);
  if (existsSync(meta)) return /Name "\{([0-9A-F]{16})\}/.exec(readFileSync(meta, "utf8"))?.[1];
  const guid = randomBytes(8).toString("hex").toUpperCase();
  if (CHECK) throw new Error(`${rel}.meta is missing`);
  mkdirSync(dirname(meta), { recursive: true });
  writeFileSync(meta, metaText(guid, `Prefabs/Buildings/TownHall/${rel}`));
  return guid;
}

const strip = (t) => t.split("\n").filter((l) => !l.startsWith("//")).join("\n").replace(/^\n+/, "");

async function main() {
  const vfs = await gameVfs();
  const db = gameDb();
  const vanilla = strip(readText(ROOT, vfs, VANILLA));
  const vFurniture = strip(readText(ROOT, vfs, VANILLA_FURNITURE));
  const fGuid = guidOf(FURNITURE);
  guidOf(BUILDING);

  const oldRef = /GenericEntity : "(\{[0-9A-F]+\})Prefabs\/Structures\/Services\/Government\/MunicipalOffice_E_01\/Furniture\/MunicipalOffice_E_01_furniture_01\.et"/;
  if (!oldRef.test(vanilla)) throw new Error("the vanilla building has no furniture child");
  const building = vanilla
    .replace(oldRef, `GenericEntity : "{${fGuid}}Prefabs/Buildings/TownHall/${FURNITURE}"`)
    .replace(/^ coords [-\d. ]+\n/m, "");

  // The furniture: vanilla props that stay, then the decor block.
  const root = /^(GenericEntity : "[^"]+" \{\n ID "[0-9A-F]+")/.exec(vFurniture)?.[1];
  if (!root) throw new Error("cannot read the vanilla furniture root");
  const kept = entities(vFurniture).filter(makeKeep()).map(transform);
  const decor = PASS > 0 ? await decorBlock(PASS) : "";
  const furniture = `${root}\n {\n${kept.map(emitEntity).join("\n")}${decor ? `\n${decor}` : ""}\n }\n}\n`;

  const outs = [
    [join(DIR, BUILDING), `${building.trimEnd()}\n`],
    [join(DIR, FURNITURE), furniture],
  ];
  let drift = false;
  for (const [file, text] of outs) {
    const cur = existsSync(file) ? readFileSync(file, "utf8") : null;
    if (cur === text) continue;
    drift = true;
    if (!CHECK) {
      mkdirSync(dirname(file), { recursive: true });
      writeFileSync(file, text);
    }
  }
  const floors = [0, 0, 0];
  for (const e of kept) floors[floorOf(e.y)]++;
  const b = PASS > 0 ? budget(PASS) : { props: 0, lightRows: 0, lights: 0, shadows: 0 };
  console.log(`town-hall pass ${PASS}: ${kept.length + b.props + b.lightRows} furniture entities (${kept.length} vanilla, ${b.props} decor props, ${b.lightRows} light rows), ${b.lights} dynamic light(s), ${b.shadows} shadow caster(s); vanilla prop(s) kept of ${entities(vFurniture).length} (floors ${floors.join("/")})${CHECK ? (drift ? " DRIFT" : " current") : " written"}`);
  if (CHECK && drift) process.exit(1);
  void db;
}
main().catch((e) => {
  console.error(e.message);
  process.exit(1);
});
