// The hostel's building prefab and its furniture, from the base game's House_Town_E_2I02 (Saint-Philippe b14).
//
//   node tools/buildings/hostel/prune.mjs          write Hostel.et and Furniture/Hostel_furniture_01.et
//   node tools/buildings/hostel/prune.mjs --check  exit 1 when the files are not what this script writes
//
// Which vanilla props of House_Town_E_2I02's furniture set (262 children) the hostel keeps. Everything here is a
// base-game prop. This is the shell card (#2289): the set is flattened and cut to the house's rooms; the hostel's own
// props (bunks, the reception desk, lockers) come in pass 1.
//
// Building space: the front is +z. Floors stand at y 0.3 (ground), 4.1 (first) and 7.5 (attic). The ground floor's
// west half (x < 0) is the old workshop and store: its tools, crates, paint and sacks go, and the room becomes the
// hostel's luggage hall. The attic holds junk only and is cut whole. Light switches (22 tiny meshes) and the roof
// aerial go; repeats are capped per floor. The suitcases and the typewriter go too: in the vanilla set they hang a
// few cm to 0.4 m above what is under them (building check).
//
// Hostel.et is the vanilla house (its doors, windows and glass) with the furniture child pointed at our own copy and
// the vanilla's world coords and angles dropped. The copy is the vanilla furniture flattened to one child per prop,
// cut by the rules below; a vanilla prop that stood on a cut support goes with it (judged with the same offline tops as
// `tools\cli building check hostel`). Writing twice gives the same bytes; a missing `.meta` gets a fresh GUID once
// and keeps it (rule 2).
import { randomBytes } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { gameVfs, readText } from "../../lib/game-prefab.mjs";
import { emitEntity, entities } from "../town-hall/furniture-lib.mjs";
import { floaters, gameReader, prefabChildren, specSupports } from "../pipeline/surface.mjs";
import { floorsOf } from "../pipeline/floaters.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const DIR = join(ROOT, "addons", "LF_Everon1989", "Prefabs", "Buildings", "Hostel");
const VANILLA = "Prefabs/Structures/Houses/Town/House_Town_E_2I02/House_Town_E_2I02.et";
const VANILLA_FURNITURE = "Prefabs/Structures/Houses/Town/House_Town_E_2I02/Furniture/House_Town_E_2I02_furniture_01.et";
export const BUILDING = "Hostel.et";
export const FURNITURE = "Furniture/Hostel_furniture_01.et";
const CHECK = process.argv.includes("--check");

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

export function guidOf(rel) {
  const meta = join(DIR, `${rel}.meta`);
  if (existsSync(meta)) return /Name "\{([0-9A-F]{16})\}/.exec(readFileSync(meta, "utf8"))?.[1];
  if (CHECK) throw new Error(`${rel}.meta is missing`);
  const guid = randomBytes(8).toString("hex").toUpperCase();
  mkdirSync(dirname(meta), { recursive: true });
  writeFileSync(meta, metaText(guid, `Prefabs/Buildings/Hostel/${rel}`));
  return guid;
}

// A floor from the height: the first floor's ceiling lamps hang at 7.0..7.1, the attic's junk lies at 7.5.
export function floorOf(y) {
  if (y < 3.9) return 0;
  if (y < 7.3) return 1;
  return 2;
}

const DROP = /LightSwitch_02|TvAntenna|UtilityBoxExt|Workbench|Cart_01|SackTruck|Rack_01|PaintCan|Crate_01|CrateWooden|BoxWooden|CardboardBoxStack|Cardboard_Pile|Bucket_01|Broom_01|SackUniversal|WoodKindling|Litter_01|Pallet_01|BarrelMetal|Notebook_01_Stack|AshtrayStreet|Suitcase_01|Typewriter_01/;

// At most N of a prop per floor, in file order. Unlisted props stay.
const LIMIT = [
  [/RadiatorCastIron_01/, 3],
  [/LightWall_01/, 2],
  [/PaintingBig_0|PaintingSmall_01/, 2],
  [/ClockWall_01/, 1],
  [/PlantRoom_0/, 2],
  [/ShelfDecorative_01/, 2],
];

export function makeKeep() {
  const count = new Map();
  return (e) => {
    const floor = floorOf(e.y);
    if (floor === 2) return false;
    if (DROP.test(e.path)) return false;
    if (floor === 1 && /TVSet_01/.test(e.path)) return false; // its stand is not in the set: it hangs 0.47 m over the carpet
    for (const [rx, max] of LIMIT) {
      if (!rx.test(e.path)) continue;
      const key = `${rx.source}@${floor}`;
      const n = (count.get(key) ?? 0) + 1;
      count.set(key, n);
      if (n > max) return false;
    }
    return true;
  };
}

const strip = (t) => t.split("\n").filter((l) => !l.startsWith("//")).join("\n").replace(/^\n+/, "");

async function main() {
  const vfs = await gameVfs();
  const vanilla = strip(readText(ROOT, vfs, VANILLA));
  const vFurniture = strip(readText(ROOT, vfs, VANILLA_FURNITURE));
  const fGuid = guidOf(FURNITURE);
  guidOf(BUILDING);

  const oldRef = /GenericEntity : "(\{[0-9A-F]+\})Prefabs\/Structures\/Houses\/Town\/House_Town_E_2I02\/Furniture\/House_Town_E_2I02_furniture_01\.et"/;
  if (!oldRef.test(vanilla)) throw new Error("the vanilla house has no furniture child");
  const building = vanilla
    .replace(oldRef, `GenericEntity : "{${fGuid}}Prefabs/Buildings/Hostel/${FURNITURE}"`)
    .replace(/^ coords [-\d. ]+\n/m, "")
    .replace(/^ angles [-\d. ]+\n/m, "");

  const root = /^(GenericEntity : "[^"]+" \{\n ID "[0-9A-F]+")/.exec(vFurniture)?.[1];
  if (!root) throw new Error("cannot read the vanilla furniture root");
  const all = entities(vFurniture);
  let kept = all.filter(makeKeep());
  const spec = JSON.parse(readFileSync(join(ROOT, "tools", "buildings", "hostel", "spec.json"), "utf8"));
  const reader = gameReader(ROOT);
  const shell = (list) => `${root}\n {\n${list.map(emitEntity).join("\n")}\n }\n}\n`;
  const idOf = (e) => /ID "([0-9A-F]+)"/.exec(e.node.lines.find((l) => l.startsWith("ID ")) ?? "")?.[1];
  let orphaned = 0;
  for (let round = 0; round < 3; round++) {
    const ents = prefabChildren(shell(kept));
    const floors = await floorsOf(spec, ents, reader);
    const gone = new Set((await floaters(ents, { reader, floors, extra: specSupports(spec) })).filter((p) => p.kind === "unsupported").map((p) => p.id));
    if (!gone.size) break;
    orphaned += gone.size;
    kept = kept.filter((e) => !gone.has(idOf(e)));
  }

  const outs = [
    [join(DIR, BUILDING), `${building.trimEnd()}\n`],
    [join(DIR, FURNITURE), shell(kept)],
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
  console.log(`hostel shell: ${kept.length} vanilla prop(s) kept of ${all.length}, ${orphaned} dropped with a cut support (floors ${floors.join("/")})${CHECK ? (drift ? " DRIFT" : " current") : " written"}`);
  if (CHECK && drift) process.exit(1);
}
main().catch((e) => {
  console.error(e.message);
  process.exit(1);
});
