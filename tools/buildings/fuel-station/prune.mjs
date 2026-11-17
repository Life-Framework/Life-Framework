// The fuel station's pack prefabs, from the base game's FuelStation_E_01_V1 (forecourt slab, two canopies with a
// pump each, the shop building and the kiosk), with the vanilla furniture flattened and pruned.
//
//   node tools/buildings/fuel-station/prune.mjs          write the prefabs below
//   node tools/buildings/fuel-station/prune.mjs --check  exit 1 when the files are not what this script writes
//
// FuelStation.et          the V1 composition with its building and kiosk pointed at ours (canopies and pumps as
//                         vanilla; the canopy lamps without their shadow-casting light)
// FuelStation_Building.et the shop building (b107), its furniture child pointed at our copy
// FuelStation_Kiosk.et    the kiosk (b97), likewise
// Furniture/*.et          the vanilla furniture, one child per prop, repeats cut (see keep())
// Writing twice gives the same bytes; a missing `.meta` is made with a fresh GUID and kept after that (rule 2).
//
// Space: FuelStation.et's own frame is the vanilla composition's. Its street front (the pumps, the shop door) is -z,
// so spec.json says "front": "-z" and proof.json points are in that frame. Building rooms are in each building's
// own frame (front +z), turned 180 degrees inside the station.
import { randomBytes } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { gameVfs, readText } from "../../lib/game-prefab.mjs";
import { emitEntity, entities } from "../bank/furniture-lib.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const REL = "Prefabs/Buildings/FuelStation";
const DIR = join(ROOT, "addons", "LF_Everon1989", ...REL.split("/"));
const V = "Prefabs/Structures/Commercial/FuelStations/FuelStation_E_01";
const CHECK = process.argv.includes("--check");

// Rooms in the building's own frame (x, z), from `tools\cli measure ... --surfaces` (the ceiling slabs).
export const SHOP = { x: [-5.44, -2.44], z: [-2.6, 2.25] };
export const WORKSHOP = { x: [-2.44, 4.8], z: [-2.6, 0.4] };
const inside = (r, e) => e.x >= r.x[0] && e.x <= r.x[1] && e.z >= r.z[0] && e.z <= r.z[1];
const roomOf = (e) => (inside(SHOP, e) ? "shop" : inside(WORKSHOP, e) ? "workshop" : "other");

// Always gone: a second light switch by the same door, empty beer bottles, the newspaper stand in the workshop.
const GONE = [/BeerBottle_01/, (e) => /LightSwitch_02/.test(e.path) && e.x > -2.8, (e) => /NewspaperStand/.test(e.path) && roomOf(e) === "workshop"];
// Repeats: at most two of one clutter prefab per room (the first two in the vanilla file's order).
const CLUTTER = /PaintCan_01|Jerrycan_01|BoxWooden_01|ToolRack_01_tool_01|CardboardBoxStack_01|AshtrayStreet_01/;
const CAP = 2;

export function prune(list) {
  const seen = new Map();
  return list.filter((e) => {
    const name = e.path ?? "";
    if (GONE.some((g) => (typeof g === "function" ? g(e) : g.test(name)))) return false;
    if (!CLUTTER.test(name)) return true;
    const k = `${roomOf(e)}|${name}`;
    seen.set(k, (seen.get(k) ?? 0) + 1);
    return seen.get(k) <= CAP;
  });
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

function guidOf(rel) {
  const meta = join(DIR, `${rel}.meta`);
  if (existsSync(meta)) return /Name "\{([0-9A-F]{16})\}/.exec(readFileSync(meta, "utf8"))?.[1];
  if (CHECK) throw new Error(`${rel}.meta is missing`);
  const guid = randomBytes(8).toString("hex").toUpperCase();
  mkdirSync(dirname(meta), { recursive: true });
  writeFileSync(meta, metaText(guid, `${REL}/${rel}`));
  return guid;
}

const strip = (t) => t.split("\n").filter((l) => !l.startsWith("//")).join("\n").replace(/^\n+/, "");
const esc = (s) => s.replace(/[.*+?^${}()|[\]\/]/g, "\$&");
// Swap one `"{GUID}<vanilla path>"` reference for ours; throws when the vanilla text no longer has it.
function swap(text, vanillaPath, rel) {
  const re = new RegExp(`"\{[0-9A-F]{16}\}${esc(vanillaPath)}"`);
  if (!re.test(text)) throw new Error(`no reference to ${vanillaPath}`);
  return text.replace(re, `"{${guidOf(rel)}}${REL}/${rel}"`);
}

function furniture(text) {
  const root = /^(GenericEntity : "[^"]+" \{\n ID "[0-9A-F]+")/.exec(text)?.[1];
  if (!root) throw new Error("cannot read the vanilla furniture root");
  const all = entities(text);
  const kept = prune(all);
  return { text: `${root}\n {\n${kept.map(emitEntity).join("\n")}\n }\n}\n`, kept: kept.length, of: all.length };
}

async function main() {
  const vfs = await gameVfs();
  const read = (p) => strip(readText(ROOT, vfs, p));
  const bF = furniture(read(`${V}/Furniture/FuelStation_E_01_building_furniture_01.et`));
  const kF = furniture(read(`${V}/Furniture/FuelStation_E_01_building_small_furniture_01.et`));
  const building = swap(read(`${V}/HouseAddons/FuelStation_E_01_building.et`), `${V}/Furniture/FuelStation_E_01_building_furniture_01.et`, "Furniture/FuelStation_Building_furniture_01.et");
  const kiosk = swap(read(`${V}/HouseAddons/FuelStation_E_01_building_small.et`), `${V}/Furniture/FuelStation_E_01_building_small_furniture_01.et`, "Furniture/FuelStation_Kiosk_furniture_01.et");
  let station = read(`${V}/FuelStation_E_01_V1.et`);
  station = swap(station, `${V}/HouseAddons/FuelStation_E_01_building.et`, "FuelStation_Building.et");
  station = swap(station, `${V}/HouseAddons/FuelStation_E_01_building_small.et`, "FuelStation_Kiosk.et");
  // The canopy lamps: the vanilla exterior lamp carries a point light that leaves CastShadow unset (a shadow caster
  // in game, over the medium budget). The plain lamp is the same fixture with no light; pass 1 lights the canopy with
  // one LightEntity (CastShadow 0) and emissive covers.
  const LAMP = '"{B61A08FB644D2050}Prefabs/Props/Furniture/LampFluorescent/LampFluorescent_01_exterior.et"';
  if (!station.includes(LAMP)) throw new Error("no exterior canopy lamps in the vanilla station");
  station = station.replace(LAMP, '"{B3CF0487BB36323B}Prefabs/Props/Furniture/LampFluorescent/LampFluorescent_01.et"');
  guidOf("FuelStation.et");

  const outs = [
    ["FuelStation.et", station],
    ["FuelStation_Building.et", building],
    ["FuelStation_Kiosk.et", kiosk],
    ["Furniture/FuelStation_Building_furniture_01.et", bF.text],
    ["Furniture/FuelStation_Kiosk_furniture_01.et", kF.text],
  ];
  let drift = false;
  for (const [rel, body] of outs) {
    const file = join(DIR, rel);
    const text = `${body.trimEnd()}\n`;
    if (existsSync(file) && readFileSync(file, "utf8") === text) continue;
    drift = true;
    if (CHECK) console.log(`  differs: ${rel}`);
    else {
      mkdirSync(dirname(file), { recursive: true });
      writeFileSync(file, text);
    }
  }
  console.log(`fuel-station prune: building ${bF.kept}/${bF.of} props kept, kiosk ${kF.kept}/${kF.of}${CHECK ? (drift ? " DRIFT" : " current") : " written"}`);
  if (CHECK && drift) process.exit(1);
}
main().catch((e) => {
  console.error(`fuel-station prune: ${e.message}`);
  process.exit(1);
});
