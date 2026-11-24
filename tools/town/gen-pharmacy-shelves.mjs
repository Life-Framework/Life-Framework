// Writes the pharmacy's host shelf stock (#4180 round 3, polish pass): the `pharmacy-host-*` dressing rows of
// tools/town/kits/pharmacy.json and the mesh-only pack prefabs they stand. Each board of the drugstore's nine
// wooden shelf units carries one row of single packages directly on the wood (no piles, no stacks), packed from
// end to end: a first-aid case, aspirin bottles, gauze and dressing packs. The first pass put three or four
// 7 cm bottles on a 1.3 m board, which read as empty.
//
//   node tools/town/gen-pharmacy-shelves.mjs [--check]
//   node tools/cli.mjs town stamp pharmacy --place saint-philippe     (then writes the layer)
//
// The unit poses are kit-frame numbers read once from the first pass's rows (they came from the vanilla furniture prefab
// `ShopHouse_E_2I01t_furniture_drugstore_01.et` with `tools\cli measure --surfaces`); the board heights are the
// measured tops of `ShelfUniversal_01_*` plus the kit's floor. Every width below is a measured bound of the mesh
// times its scale. Deterministic: the same bytes twice (`--check` exits 1 when the committed files differ).
// A file keeps the resource GUID of its .meta; a new prefab gets a new one. The work runs only when this file is the
// command: CI imports every tools module.
import { createHash, randomBytes } from "node:crypto";
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const KIT = join(ROOT, "tools/town/kits/pharmacy.json");
const PACK = join(ROOT, "addons/LF_Everon1989");
const DIR = "Prefabs/Props/Commercial";

// The mesh-only decor prefabs: nothing a player can pick up, nothing sellable.
// `w` and `d` are the measured width and depth (m) of the mesh at scale 1, `lift` the metres the pivot sits above the
// mesh's lowest point's plane (a negative bound), `scale` the dress-up factor.
export const PACKS = {
  Aspirin: { file: "LF_PharmDisplay_Aspirin.et", w: 0.072, d: 0.076, lift: 0, scale: 1.5 },
  Gauze: { file: "LF_PharmDisplay_Gauze.et", w: 0.106, d: 0.063, lift: 0.006, scale: 1.4 },
  Tourniquet: { file: "LF_PharmDisplay_Tourniquet.et", w: 0.091, d: 0.094, lift: 0, scale: 1.5 },
  MedKit: {
    file: "LF_PharmDisplay_MedKit.et",
    xob: "{79CA5DD14C0940E9}Assets/Items/Equipment/Kits/MedicalKit_01/MedicalKit_01.xob",
    w: 0.27, d: 0.118, lift: 0.025, scale: 1,
  },
  GauzePack: {
    file: "LF_PharmDisplay_GauzePack.et",
    xob: "{AC449262EE17E21B}Assets/Items/Medicine/Gauze_02/Gauze_02_package.xob",
    w: 0.218, d: 0.068, lift: 0, scale: 1.3,
  },
};

// Kit-frame shelf units: the centre of a board (x, z), the yaw the unit faces, the usable board length.
const UNITS = [
  { id: "w1", at: [-1.291, 3.927], yaw: 90, len: 1.3, boards: [1, 2, 3] },
  { id: "w2", at: [5.536, -1.208], yaw: -90, len: 1.3, boards: [1, 2, 3] },
  { id: "w3", at: [-1.292, 5.381], yaw: 90, len: 1.3, boards: [1, 2, 3] },
  { id: "w4", at: [1.542, -2.742], yaw: 0, len: 1.3, boards: [1, 2, 3] },
  { id: "w5", at: [5.532, 5.302], yaw: -90, len: 1.3, boards: [1, 2, 3] },
  { id: "n1", at: [2.796, -2.79], yaw: 0, len: 0.88, boards: [1, 2, 3], narrow: true },
  { id: "n2", at: [5.547, -2.43], yaw: -90, len: 0.88, boards: [1, 2, 3], narrow: true },
];

// The two corner units are L-shaped, and only the spots the first pass proved to be wood are used (a corner board is not a
// rectangle in the kit frame: a pack off these spots floated in the stamp's audit). Three per board, 0.45 m apart, so a
// first-aid case or a gauze pack fits at each.
const CORNERS = [
  { id: "c1", yaw: -90, boards: { 2: [[4.9, 6.943], [4.9, 6.493], [5.35, 6.943]], 3: [[4.9, 6.723], [5.2, 6.493], [5.57, 6.943]] } },
  { id: "c2", yaw: 180, boards: { 2: [[-1.193, 6.37], [-0.743, 6.37], [-1.193, 6.82]], 3: [[-0.973, 6.37], [-0.743, 6.67], [-1.193, 7.04]] } },
];
const CORNER_KINDS = ["MedKit", "Aspirin", "GauzePack", "Aspirin", "Aspirin", "MedKit"];

// Board tops above the kit origin: the shelf's measured tops 0.588, 1.081, 1.574 plus the interior floor 0.196 and a 5 mm rest, plus 3 cm
// that `look contact` measured (every pack sat 3.5 cm into the wood at the first heights).
const BOARD_Y = { wide: { 1: 0.819, 2: 1.312, 3: 1.805 }, narrow: { 1: 0.822, 2: 1.319, 3: 1.808 }, corner: { 2: 1.316, 3: 1.805 } };
// How far in front of the board's middle a package stands (the board is 0.37 m deep; a case is 0.12).
const FORWARD = 0.03;
const GAP = 0.025;
// The order along a board. A board starts at a different place in the cycle so no two neighbours repeat.
const CYCLE = ["MedKit", "Aspirin", "Aspirin", "GauzePack", "Aspirin", "MedKit", "Aspirin", "GauzePack", "Aspirin", "Aspirin"];

// One corner board: the grid, a pack per cell in the corner cycle.
export function cornerRows(c, board, start) {
  return c.boards[board].map(([x, z], i) => {
    const k = CORNER_KINDS[(start + i) % CORNER_KINDS.length];
    const row = {
      id: `pharmacy-host-${c.id}-b${board}-${i + 1}`, name: `Pharmacy_host_${c.id}_b${board}_${i + 1}`, prefab: `{${GUIDS[k]}}${DIR}/${PACKS[k].file}`,
      role: "dressing", verb: "on floor", offset: [r3(x), r3(BOARD_Y.corner[board] + PACKS[k].lift * PACKS[k].scale), r3(z)], yaw: c.yaw, free: true, wall: "free", floor: "f0",
    };
    if (PACKS[k].scale !== 1) row.scale = PACKS[k].scale;
    return row;
  });
}

const r3 = (n) => Math.round(n * 1000) / 1000;
const widthOf = (k) => PACKS[k].w * PACKS[k].scale;

// One board's rows: items packed left to right inside `len`, the leftover split at both ends.
export function boardRows(unit, board, start) {
  const kinds = [];
  let used = 0;
  for (let i = 0; i < CYCLE.length * 2; i++) {
    const k = CYCLE[(start + i) % CYCLE.length];
    const w = widthOf(k);
    if (used + (kinds.length ? GAP : 0) + w > unit.len) {
      // Does a smaller pack still fit the gap? Try the cycle once more for a narrower one before closing the row.
      const fit = CYCLE.map((c, j) => ({ c, j })).filter(({ c }) => used + GAP + widthOf(c) <= unit.len).sort((a, b) => widthOf(b.c) - widthOf(a.c))[0];
      if (fit) { kinds.push(fit.c); used += GAP + widthOf(fit.c); }
      break;
    }
    used += (kinds.length ? GAP : 0) + w;
    kinds.push(k);
  }
  const yaw = unit.yaw;
  const rad = (yaw * Math.PI) / 180;
  const along = [Math.cos(rad), -Math.sin(rad)]; // unit-local +x in the kit's (x, z)
  const front = [Math.sin(rad), Math.cos(rad)]; // the way the unit faces
  const y0 = BOARD_Y[unit.corner ? "corner" : unit.narrow ? "narrow" : "wide"][board];
  let t = -used / 2;
  return kinds.map((k, i) => {
    const w = widthOf(k);
    const mid = t + w / 2;
    t += w + GAP;
    const x = unit.at[0] + along[0] * mid + front[0] * FORWARD;
    const z = unit.at[1] + along[1] * mid + front[1] * FORWARD;
    const id = `pharmacy-host-${unit.id}-b${board}-${i + 1}`;
    const row = {
      id,
      name: `Pharmacy_host_${unit.id}_b${board}_${i + 1}`,
      prefab: `{${GUIDS[k]}}${DIR}/${PACKS[k].file}`,
      role: "dressing",
      verb: "on floor",
      offset: [r3(x), r3(y0 + PACKS[k].lift * PACKS[k].scale), r3(z)],
      yaw,
      free: true,
      wall: "free",
      floor: "f0",
    };
    if (PACKS[k].scale !== 1) row.scale = PACKS[k].scale;
    return row;
  });
}

let GUIDS = {};

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

const hex = (seed) => createHash("sha1").update(seed).digest("hex").slice(0, 16).toUpperCase();

// A prefab's resource GUID: the one in its .meta, else a fresh one.
function guidOf(file) {
  const meta = join(PACK, DIR, file + ".meta");
  const g = existsSync(meta) ? /Name "\{([0-9A-F]{16})\}/.exec(readFileSync(meta, "utf8"))?.[1] : null;
  return g ?? randomBytes(8).toString("hex").toUpperCase();
}

// The planned files: { path, text }. The three first-pass prefabs are not rewritten; the new ones are.
export function plan() {
  const files = [];
  for (const [k, p] of Object.entries(PACKS)) {
    GUIDS[k] = guidOf(p.file);
    if (!p.xob) continue; // a first-pass prefab: already committed
    const body = `GenericEntity {
 ID "${hex(`${p.file}#e`)}"
 components {
  MeshObject "{${hex(`${p.file}#m`)}}" {
   Object "${p.xob}"
  }
 }
 coords 0 0 0
}
`;
    files.push({ path: join(PACK, DIR, p.file), text: body });
    files.push({ path: join(PACK, DIR, p.file + ".meta"), text: metaText(GUIDS[k], `${DIR}/${p.file}`) });
  }
  const kit = JSON.parse(readFileSync(KIT, "utf8"));
  const keep = kit.rows.filter((r) => !r.id.startsWith("pharmacy-host-"));
  const rows = [];
  let n = 0;
  for (const unit of UNITS) for (const b of unit.boards) rows.push(...boardRows(unit, b, (n++ * 5) % CYCLE.length));
  for (const c of CORNERS) for (const b of [2, 3]) rows.push(...cornerRows(c, b, (n++ * 3) % CORNER_KINDS.length));
  kit.rows = [...keep, ...rows];
  files.push({ path: KIT, text: JSON.stringify(kit, null, 2) + "\n" });
  return { files, rows: rows.length };
}

function main() {
  const check = process.argv.includes("--check");
  const { files, rows } = plan();
  let differ = 0;
  for (const f of files) {
    const same = existsSync(f.path) && readFileSync(f.path, "utf8") === f.text;
    if (!same) differ++;
    if (!check) writeFileSync(f.path, f.text);
  }
  console.log(`${check ? "check" : "wrote"} ${rows} host shelf rows, ${files.length} files, ${differ} differ`);
  if (check && differ) process.exit(1);
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) main();
