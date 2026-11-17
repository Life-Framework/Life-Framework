// The pub and cafe's building prefab and its pruned furniture, from the base game's PubVillage_E_2L01_V8_ProgresBar
// (Saint-Philippe b03, the pub with the wall sign).
//
//   node tools/buildings/pub/prune.mjs          write Pub.et and Furniture/Pub_furniture_01.et
//   node tools/buildings/pub/prune.mjs --check  exit 1 when the files are not what this script writes
//
// Pub.et is the vanilla V8 building prefab (its base, its two wall signs) with the furniture child pointed at our own
// copy. The copy is the vanilla furniture_02 set (287 props) flattened to one child per prop, with the repeats cut
// (switches, beds, bottles, curtains, radiators, the bath) so the shell spends about half the medium budget before the
// fit-out passes add anything. A vanilla prop that stood on something the cut removed goes with it (the same offline
// tops `tools\cli building check pub` uses). Writing the files twice gives the same bytes; a missing `.meta` is made
// with a fresh GUID and kept after that (rule 2).
//
// Building space is prefab space: x across, y up from the taproom floor, z along. The street face with the pub sign
// is the -z wall (the vanilla sign at z -9.1); `proof.json` stands the camera there.
import { randomBytes } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { gameVfs, readText } from "../../lib/game-prefab.mjs";
import { emitEntity, entities, floorOf, short } from "./furniture-lib.mjs";
import { blockOf, swapBlock } from "./decor-lib.mjs";
import { floaters, gameReader, prefabChildren, specSupports } from "../pipeline/surface.mjs";
import { floorsOf } from "../pipeline/floaters.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const DIR = join(ROOT, "addons", "LF_Everon1989", "Prefabs", "Buildings", "Pub");
const VANILLA = "Prefabs/Structures/Commercial/Pubs/PubVillage_E_2L01/PubVillage_E_2L01_V8_ProgresBar.et";
const VANILLA_FURNITURE = "Prefabs/Structures/Commercial/Pubs/PubVillage_E_2L01/Furniture/PubVillage_E_2L01_furniture_02.et";
export const BUILDING = "Pub.et";
export const FURNITURE = "Furniture/Pub_furniture_01.et";
const CHECK = process.argv.includes("--check");

// Props that never stay: light switches (15 tiny meshes), the bath, boiler and shower drain of the upstairs flat, the
// cleaning things, the first-aid box and life buoy (wall clutter the passes replace with the pub's own), and the small
// radio, which the base game stands 0.24 m inside the stacked wall shelf (`building check` flags it; pass 1 puts the
// cafe radio on a table).
const DROP = /LightSwitch_01|Bathtub_01|Boiler_01|ShowerDrain_01|Broom_01|Bucket_01|FirstAidBox_01|LifeBuoy_01|TowelRack_01|Carpet_03|RadioSmall_01/;

// Vanilla chair -> the seat prefab that replaces it (same mesh plus the sit action).
const SEATS = [
  [/Chair_01\/Chair_01_red\.et$/, "{3145FC3DB0E675E8}", "Prefabs/Buildings/Pub/Seats/LF_Seat_Chair_01_red.et"],
  [/Chair_01\/Chair_01_white\.et$/, "{1A8B1A9666D0C6CB}", "Prefabs/Buildings/Pub/Seats/LF_Seat_Chair_01_white.et"],
  [/Chair_02\/Chair_02_green\.et$/, "{69D6D9BFDFEC3225}", "Prefabs/Props/Seats/LF_Seat_Chair_02_green.et"],
];

// At most N of a prop per floor, in file order. Unlisted props stay (tables, bar, taps, kegs, kitchen, toilets).
const LIMIT = [
  [/BeerBottle_01/, 4],
  [/Ashtray_01/, 3],
  [/Chair_01_red/, 10],
  [/Chair_01_white/, 6],
  [/Chair_02/, 4],
  [/CurtainWindow_01_straight|CurtainsWindow_01_small/, 6],
  [/RadiatorCastIron/, 3],
  [/Bed_01/, 2],
  [/Nightstand_02/, 2],
  [/Cupboard_01_F/, 2],
  [/Shelf_01_long/, 2],
  [/Painting/, 3],
  [/PlantRoom/, 2],
  [/BinInterior/, 1],
  [/FireExtinguisher/, 1],
  [/Urinal_01/, 2],
  [/LightCeiling_03|LampFluorescent_01/, 3],
  [/LightWall_01/, 4],
  [/CoatRack|CoatHook/, 2],
  [/Armchair_01/, 1],
  [/Faucet_01|Sink_01/, 2],
  [/BoxWooden_01|Crate_01/, 4],
];

export function makeKeep() {
  const count = new Map();
  return (e) => {
    const floor = floorOf(e.y);
    if (DROP.test(e.path)) return false;
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
  writeFileSync(meta, metaText(guid, `Prefabs/Buildings/Pub/${rel}`));
  return guid;
}

const strip = (t) => t.split("\n").filter((l) => !l.startsWith("//")).join("\n").replace(/^\n+/, "");
const idOf = (e) => /ID "([0-9A-F]+)"/.exec(e.node.lines.find((l) => l.startsWith("ID ")) ?? "")?.[1];

async function main() {
  const vfs = await gameVfs();
  const vanilla = strip(readText(ROOT, vfs, VANILLA));
  const vFurniture = strip(readText(ROOT, vfs, VANILLA_FURNITURE));
  const fGuid = guidOf(FURNITURE);
  guidOf(BUILDING);

  const oldRef = /GenericEntity : "\{[0-9A-F]+\}Prefabs\/Structures\/Commercial\/Pubs\/PubVillage_E_2L01\/Furniture\/PubVillage_E_2L01_furniture_02\.et"/;
  if (!oldRef.test(vanilla)) throw new Error("the vanilla pub has no furniture child");
  const building = vanilla
    .replace(oldRef, `GenericEntity : "{${fGuid}}Prefabs/Buildings/Pub/${FURNITURE}"`)
    .replace(/^ coords [-\d. ]+\n/m, "");

  const root = /^(GenericEntity : "[^"]+" \{\n ID "[0-9A-F]+")/.exec(vFurniture)?.[1];
  if (!root) throw new Error("cannot read the vanilla furniture root");
  const all = entities(vFurniture);
  let kept = all.filter(makeKeep());

  const spec = JSON.parse(readFileSync(join(ROOT, "tools", "buildings", "pub", "spec.json"), "utf8"));
  const reader = gameReader(ROOT);
  const shell = (list) => `${root}\n {\n${list.map(emitEntity).join("\n")}\n }\n}\n`;
  let orphaned = 0;
  for (let round = 0; round < 3; round++) {
    const ents = prefabChildren(shell(kept));
    const floors = await floorsOf(spec, ents, reader);
    const gone = new Set((await floaters(ents, { reader, floors, extra: specSupports(spec) })).filter((p) => p.kind === "unsupported").map((p) => p.id));
    if (!gone.size) break;
    orphaned += gone.size;
    kept = kept.filter((e) => !gone.has(idOf(e)));
  }
  // Pass 1 (#2294): every kept chair is a seat players can sit on (Emote's LF_SitAction). Chair_02 has the core seat
  // prefab; the two Chair_01 colours get the pack's own (Seats/), the same action on the same base mesh.
  kept = kept.map((e) => {
    const to = SEATS.find(([re]) => re.test(e.path));
    return to ? { ...e, guid: to[1], path: to[2] } : e;
  });
  // decor.mjs's block (IDs CAFE...) stays where it is: prune rewrites only the vanilla set.
  const curFurniture = existsSync(join(DIR, FURNITURE)) ? readFileSync(join(DIR, FURNITURE), "utf8") : "";
  const decorBlock = blockOf(curFurniture, "CAFE");
  const furniture = decorBlock ? swapBlock(shell(kept), "CAFE", decorBlock) : shell(kept);

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
  const lamps = kept.filter((e) => /LightCeiling|LampFluorescent|LightWall|LightTable/.test(short(e.path))).length;
  console.log(`pub shell: ${kept.length} of ${all.length} vanilla props kept (floors ${floors.join("/")}), ${orphaned} dropped with the support the cut removed, ${lamps} lamp mesh(es), 0 dynamic lights${CHECK ? (drift ? " DRIFT" : " current") : " written"}`);
  if (CHECK && drift) process.exit(1);
}
main().catch((e) => {
  console.error(e.message);
  process.exit(1);
});
