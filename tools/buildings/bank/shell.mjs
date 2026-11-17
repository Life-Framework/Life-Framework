// The bank's building prefab and its furniture, from the base game's Office_E_01.
//
//   node tools/buildings/bank/shell.mjs          write Bank.et and Furniture/Bank_furniture_01.et
//   node tools/buildings/bank/shell.mjs --check  exit 1 when the files are not what this script writes
//
// Bank.et is the vanilla building prefab (its doors, windows, entry and signs) with the furniture child
// pointed at our own copy. The copy is the vanilla furniture flattened to one child per prop, with the props the
// bank does not need left out (the infirmary, the extra WCs, the duplicated switches and radiators) and the
// decor block (decor.mjs) appended. Writing the files twice gives the same bytes; a missing `.meta` is made with a
// fresh GUID and kept after that (rule 2: a GUID is never reused or changed).
import { randomBytes } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { gameVfs, readText } from "../../lib/game-prefab.mjs";
import { gameDb } from "../pipeline/gamedb.mjs";
import { hex16 } from "./decor-lib.mjs";
import { emitEntity, entities, floorOf, materialKid } from "./furniture-lib.mjs";
import { keep } from "./prune.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const DIR = join(ROOT, "addons", "LF_Everon1989", "Prefabs", "Buildings", "Bank");
const VANILLA = "Prefabs/Structures/Industrial/Houses/Office_E_01/Office_E_01.et";
const VANILLA_FURNITURE = "Prefabs/Structures/Industrial/Houses/Office_E_01/Furniture/Office_E_01_furniture_01.et";
export const BUILDING = "Bank.et";
export const FURNITURE = "Furniture/Bank_furniture_01.et";
const CHECK = process.argv.includes("--check");
// The vanilla door in the vault room's doorway (socket_door_int_left_02, x 3.92 on the corridor wall): the vault door
// (kit.mjs) closes that doorway instead.
const VAULT_DOOR_ID = "5D46D459E7DF6276";
// LightWall_01's MeshObject component and the base game's lit glass for it: every vanilla wall lamp glows, at no light.
const LAMP_MESH_ID = "5872F0EB7FA40561";
const LAMP_LIT = "Assets/Props/Furniture/LightWall_01/data/LightWall_01_Glass_lit.emat";

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
  writeFileSync(meta, metaText(guid, `Prefabs/Buildings/Bank/${rel}`));
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

  const oldRef = /GenericEntity : "(\{[0-9A-F]+\})Prefabs\/Structures\/Industrial\/Houses\/Office_E_01\/Furniture\/Office_E_01_furniture_01\.et"/;
  if (!oldRef.test(vanilla)) throw new Error("the vanilla building has no furniture child");
  const doorOut = new RegExp(`\\n   \\{\\n    ID "${VAULT_DOOR_ID}"\\n[\\s\\S]*?\\n   \\}(?=\\n)`);
  if (!doorOut.test(vanilla)) throw new Error(`the vanilla building has no door ${VAULT_DOOR_ID} (socket_door_int_left_02)`);
  const building = vanilla
    .replace(oldRef, `GenericEntity : "{${fGuid}}Prefabs/Buildings/Bank/${FURNITURE}"`)
    .replace(doorOut, "")
    .replace(/^ coords [-\d. ]+\n/m, "");

  // The furniture: vanilla props that stay, then the decor block.
  const root = /^(GenericEntity : "[^"]+" \{\n ID "[0-9A-F]+")/.exec(vFurniture)?.[1];
  if (!root) throw new Error("cannot read the vanilla furniture root");
  const kept = entities(vFurniture).filter(keep);
  // decor.mjs's block (IDs BA4C) is kept as it stands, so a shell re-run or --check leaves the dressing alone.
  const furniturePath = join(DIR, FURNITURE);
  const current = existsSync(furniturePath) ? readFileSync(furniturePath, "utf8") : "";
  const decor = (current.match(/^ {2}\w+(?: : "[^"]+")? \{\n {3}ID "BA4C[0-9A-F]+"\n[\s\S]*?\n {2}\}$/gm) ?? []).join("\n");
  const litGuid = db?.guidOf(LAMP_LIT);
  if (!litGuid) throw new Error(`no GUID for ${LAMP_LIT} in the game's resource database`);
  const idOf = (e) => e.node.lines.find((l) => l.startsWith("ID ")) ?? "";
  const lit = (e) => (/LightWall_01\.et$/.test(e.path) ? [materialKid(LAMP_MESH_ID, hex16("BA5E", `lit|${idOf(e)}`), "LightWall_01_Glass", `{${litGuid}}${LAMP_LIT}`)] : []);
  const furniture = `${root}\n {\n${kept.map((e) => emitEntity(e, lit(e))).join("\n")}${decor ? `\n${decor}` : ""}\n }\n}\n`;

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
  console.log(`bank shell: ${kept.length} vanilla prop(s) kept of ${entities(vFurniture).length} (floors ${floors.join("/")})${CHECK ? (drift ? " DRIFT" : " current") : " written"}`);
  if (CHECK && drift) process.exit(1);
  void db;
}
main().catch((e) => {
  console.error(e.message);
  process.exit(1);
});
