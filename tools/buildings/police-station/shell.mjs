// The police station's building prefab and its pruned furniture, from the base game's Villa_E_2I01_Police.
//
//   node tools/buildings/police-station/shell.mjs          write PoliceStation.et and Furniture/PoliceStation_furniture_01.et
//   node tools/buildings/police-station/shell.mjs --check  exit 1 when the files are not what this script writes
//   node tools/buildings/police-station/shell.mjs --list   print what the cut keeps and drops, per prop and floor
//
// PoliceStation.et is the vanilla police villa (its doors, windows and the barred cell) with the furniture child pointed at
// our own copy. The copy is the vanilla Villa_E_2I01_furniture_02 (231 props) flattened to one child per prop, with what a
// station does not need left out: the attic's junk (the kit never shows the attic), the 16 light switches, the repeats.
// The budget is why (MEDIUM, 300 entities for the building; the town hall cut its set the same way). The kit's own rows
// (systems, decor, lights) stay in the grid layer (fitout.mjs). Same bytes twice; a missing `.meta` gets a fresh GUID once.
import { randomBytes } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { gameVfs, readText } from "../../lib/game-prefab.mjs";
import { emitEntity, entities, short } from "../town-hall/furniture-lib.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
export const DIR_REL = "Prefabs/Buildings/PoliceStation";
const DIR = join(ROOT, "addons", "LF_Everon1989", ...DIR_REL.split("/"));
const VANILLA = "Prefabs/Structures/Houses/Villa/Villa_E_2I01/Villa_E_2I01_Police.et";
const VANILLA_FURNITURE = "Prefabs/Structures/Houses/Villa/Villa_E_2I01/Furniture/Villa_E_2I01_furniture_02.et";
export const BUILDING = "PoliceStation.et";
export const FURNITURE = "Furniture/PoliceStation_furniture_01.et";
const CHECK = process.argv.includes("--check");

// The villa's floors: ground 0 (its ceiling lamps hang at 3.3), first 3.8, the attic from 7.3.
export const floorOf = (y) => (y < 3.7 ? 0 : y < 7.0 ? 1 : 2);

// Props that never stay: switches (16 tiny meshes), the attic storeroom's paint, broom and bucket.
const DROP = /LightSwitch_02|Broom_01|Bucket_01|PaintCan_01/;
// At most N of a prop per floor, in file order; unlisted props stay.
const LIMIT = [
  [/RadiatorCastIron_01_/, 3],
  [/FireExtinguisher_01/, 1],
  [/Faucet_01|MirrorBathroom_01/, 1],
  [/Sink_01/, 2],
  [/CurtainsWindow_01/, 2],
  [/Crate_01_brown|BoxWooden_01|CardboardBoxStack_01/, 1],
  [/FileOffice_01_Stack/, 3],
];
// Ground props the kit replaces where they stand: the coat rack in the hall's walk line, the soda bottle and crate left on
// the canteen floor. The two hall benches either side of the duty-room door stay: they are the waiting area.
const OUT = [
  [/CoatRack_04/, -1.7, 0, -0.7],
  [/SodaBottle_01/, 1.2, 0, 4.1],
  [/Crate_01_base/, 1.3, 0, 4.3],
];
const near = (e, x, y, z, tol = 0.35) => Math.abs(e.x - x) < tol && Math.abs(e.y - y) < 0.5 && Math.abs(e.z - z) < tol;

export function makeKeep() {
  const count = new Map();
  return (e) => {
    const name = short(e.path);
    const floor = floorOf(e.y);
    if (floor === 2) return false; // the attic: crates, cardboard and junk nobody sees
    if (DROP.test(name)) return false;
    if (OUT.some(([rx, x, y, z]) => rx.test(name) && near(e, x, y, z))) return false;
    for (const [rx, max] of LIMIT) {
      if (!rx.test(name)) continue;
      const k = `${rx}|${floor}`;
      const n = (count.get(k) ?? 0) + 1;
      count.set(k, n);
      return n <= max;
    }
    return true;
  };
}

// The villa's ceiling lamps (LampFluorescent_01) ship dark: their cover gets the base game's emissive diffuser, so a room
// looks lit by its fitting at no dynamic-light cost (the town hall does the same). The kit's spots do the lighting.
const LIT = "{51611E400CB6FB8A}Assets/Props/Furniture/LampFluorescent_02/Data/LampFluorescent_02_cover_emissive.emat";
export function transform(e) {
  if (!/LampFluorescent_01/.test(e.path)) return e;
  const id = (/ID "([0-9A-F]+)"/.exec(e.node.lines.find((l) => l.startsWith("ID ")) ?? "")?.[1] ?? "0").padStart(16, "0");
  const assign = { head: `MaterialAssignClass "{AC1F${id.slice(4)}}"`, lines: ['SourceMaterial "LampFluorescent_01_cover"', `AssignedMaterial "${LIT}"`], kids: [] };
  const mesh = { head: 'MeshObject "{5872F0EB7FA40561}"', lines: [], kids: [{ head: "Materials", lines: [], kids: [assign] }] };
  return { ...e, node: { ...e.node, kids: [...e.node.kids, { head: "components", lines: [], kids: [mesh] }] } };
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
export function guidOf(rel) {
  const meta = join(DIR, `${rel}.meta`);
  if (existsSync(meta)) return /Name "\{([0-9A-F]{16})\}/.exec(readFileSync(meta, "utf8"))?.[1];
  if (CHECK) throw new Error(`${rel}.meta is missing`);
  const guid = randomBytes(8).toString("hex").toUpperCase();
  mkdirSync(dirname(meta), { recursive: true });
  writeFileSync(meta, metaText(guid, `${DIR_REL}/${rel}`));
  return guid;
}

export const buildingRef = () => `{${guidOf(BUILDING)}}${DIR_REL}/${BUILDING}`;

const strip = (t) => t.split("\n").filter((l) => !l.startsWith("//")).join("\n").replace(/^\n+/, "");

async function main() {
  const vfs = await gameVfs();
  const vanilla = strip(readText(ROOT, vfs, VANILLA));
  const vFurniture = strip(readText(ROOT, vfs, VANILLA_FURNITURE));
  const fGuid = guidOf(FURNITURE);
  guidOf(BUILDING);

  const oldRef = /GenericEntity : "(\{[0-9A-F]+\})Prefabs\/Structures\/Houses\/Villa\/Villa_E_2I01\/Furniture\/Villa_E_2I01_furniture_02\.et"/;
  if (!oldRef.test(vanilla)) throw new Error("the vanilla police villa has no furniture child");
  const building = vanilla.replace(oldRef, `GenericEntity : "{${fGuid}}${DIR_REL}/${FURNITURE}"`);

  const root = /^(GenericEntity : "[^"]+" \{\n ID "[0-9A-F]+")/.exec(vFurniture)?.[1];
  if (!root) throw new Error("cannot read the vanilla furniture root");
  const all = entities(vFurniture);
  const keep = makeKeep();
  const kept = all.filter(keep).map(transform);
  if (process.argv.includes("--list")) {
    const k2 = makeKeep();
    for (const e of all) console.log(`${k2(e) ? "keep" : "drop"} ${floorOf(e.y)} ${short(e.path)} ${e.x.toFixed(2)} ${e.y.toFixed(2)} ${e.z.toFixed(2)}`);
  }
  const furniture = `${root}\n {\n${kept.map(emitEntity).join("\n")}\n }\n}\n`;

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
  console.log(`police-station shell: ${kept.length} of ${all.length} vanilla props kept (floors ${floors.join("/")})${CHECK ? (drift ? " DRIFT" : " current") : " written"}`);
  if (CHECK && drift) process.exit(1);
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) {
  main().catch((e) => {
    console.error(e.message);
    process.exit(1);
  });
}
