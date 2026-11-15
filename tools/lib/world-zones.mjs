// tools/lib/world-zones.mjs: the two world zones (docs/architecture/world-zones.md).
//
// Grid: the net-fenced annex (x 20..112, z 20..112), which lives in GridWorld
// (Worlds/GridWorld/GridWorld_Layers, docs/architecture/three-worlds.md); until slice W6 deletes
// it, the legacy annex layers also sit in DebugWorld. One feature per cell, low look bar, written
// by `tools\cli grid`. Town: every other layer the playtest town loads
// (the DebugWorld village layers and the pack world Everon1989Town). Polished; written only by a
// town-placement pass (a `town-` worktree slug).
//
// Pure functions, no I/O except where a function says it reads the tree. Used by
// tools/commands/grid.mjs, tools/validation/validate-world-zones.mjs, agent-box/place-audit.mjs,
// tools/validation/validate-player-props.mjs and tools/visual/look.mjs.
import { existsSync, readFileSync, readdirSync } from "node:fs";
import { join } from "node:path";
import { CORE_ADDON, GRID_WORLD, PLAYTEST_WORLD, layersOf } from "./worlds.mjs";

export const DEBUG_LAYERS = `addons/${CORE_ADDON}/${layersOf(PLAYTEST_WORLD)}`;
// Where Grid_<slug>.layer files are written and claims are checked: GridWorld, not the playtest world.
export const GRID_LAYERS = `addons/${CORE_ADDON}/${layersOf(GRID_WORLD)}`;
export const PACK_TOWN_LAYERS = "addons/LF_Everon1989/Worlds/Everon1989Town/Everon1989Town_Layers";
export const CELLS_DIR = "tools/grid/cells";

// The annex fence (Annex.layer): the grid zone.
export const GRID = { x0: 20, z0: 20, x1: 112, z1: 112 };
// The rectangle the DebugWorld freeze watches: the fence plus 2 m (three-worlds.md "What counts as grid").
export const ANNEX_RECT = { x0: 18, z0: 18, x1: 114, z1: 114 };
// Cells: a 7 x 7 lattice of 12 m squares from 22,22 (2 m inside the annex fence). Each cell holds a
// 10 m fenced yard (1 m in from its edges) with a 5 m gap on the side facing the annex centre.
export const CELL = { size: 12, origin: 22, cols: 7, rows: 7, inset: 1, fence: 5 };
export const GRID_CENTRE = [66, 66];

// Ground no cell may take: the open ground world tests spawn their fixtures on
// (docs/roadmap/m1-25-village.md "Test annex"), and the track in through the gap in the north fence.
export const RESERVED = [
  { why: "world-test fixture ground", x0: 60, z0: 60, x1: 90, z1: 91 },
  { why: "world-test fixture spot 48,64", x0: 47, z0: 63, x1: 49, z1: 65 },
  { why: "the track in through the north gate", x0: 66, z0: 100, x1: 82, z1: 112 },
];

// Annex layers before the grid existed (validate-annex-fixtures.mjs keeps the list for tests).
// They are grid-zone layers: the legacy stations, with the low look bar.
export const LEGACY_ANNEX = new Set([
  "Annex", "Crimes", "HousesKeys", "Jobs", "NPCs", "PRFeatures", "Police", "Tests", "WhitelistChat",
  "Factions", "Gamemode", "default", "Shops",
]);
// Layers with no props: world settings and managers. Neither zone; nobody's look bar.
export const SYSTEM_LAYERS = new Set(["default", "Gamemode", "Factions"]);

export const SLUG_RX = /^[a-z0-9][a-z0-9-]{1,40}$/;
export const layerOfSlug = (slug) => `Grid_${slug.replace(/-/g, "_")}.layer`;
// The slug is in the claim file (its `layer` field); a layer name maps back only with dashes unknown.
// Entity names written into a grid layer: Grid_<slug with _>_<n>.
export const entityPrefix = (slug) => `Grid_${slug.replace(/-/g, "_")}`;

// "grid", "town" or "system" for a layer file name (with or without .layer) and the world it is in.
export function zoneOfLayer(file, world = "debug") {
  const name = String(file).replace(/\.layer$/, "");
  if (world === "pack") return "town";
  if (SYSTEM_LAYERS.has(name)) return "system";
  if (/^Grid_/.test(name) || LEGACY_ANNEX.has(name)) return "grid";
  return "town";
}

// The zone of a repo path, or null when it is not a world layer.
export function zoneOfPath(path) {
  const p = String(path).replace(/\\/g, "/");
  if (!p.endsWith(".layer")) return null;
  const base = p.slice(p.lastIndexOf("/") + 1);
  if (p.startsWith(`${DEBUG_LAYERS}/`) || p.startsWith(`${GRID_LAYERS}/`)) return zoneOfLayer(base, "debug");
  if (p.startsWith(`${PACK_TOWN_LAYERS}/`)) return zoneOfLayer(base, "pack");
  return null;
}

export const inGrid = (x, z) => x >= GRID.x0 && x <= GRID.x1 && z >= GRID.z0 && z <= GRID.z1;
export const inAnnexRect = (x, z) => x >= ANNEX_RECT.x0 && x <= ANNEX_RECT.x1 && z >= ANNEX_RECT.z0 && z <= ANNEX_RECT.z1;

// ------------------------------------------------------------------ cells

export function cellId(col, row) {
  return `c${col}r${row}`;
}

export function parseCellId(id) {
  const m = /^c(\d+)r(\d+)$/.exec(String(id));
  if (!m) return null;
  const col = Number(m[1]);
  const row = Number(m[2]);
  if (col >= CELL.cols || row >= CELL.rows) return null;
  return { col, row };
}

export function cellBounds(col, row) {
  const x0 = CELL.origin + col * CELL.size;
  const z0 = CELL.origin + row * CELL.size;
  return { x0, z0, x1: x0 + CELL.size, z1: z0 + CELL.size };
}

export function allCells() {
  const out = [];
  for (let row = 0; row < CELL.rows; row++) for (let col = 0; col < CELL.cols; col++) out.push({ id: cellId(col, row), col, row, ...cellBounds(col, row) });
  return out;
}

const overlaps = (a, b) => a.x0 < b.x1 && b.x0 < a.x1 && a.z0 < b.z1 && b.z0 < a.z1;

// The side of a cell its gap opens on: the one facing the annex centre along the larger offset.
// Returned as the compass word and the unit vector pointing out of the cell through the gap.
export function gapSide(cell) {
  const dx = GRID_CENTRE[0] - (cell.x0 + cell.x1) / 2;
  const dz = GRID_CENTRE[1] - (cell.z0 + cell.z1) / 2;
  if (Math.abs(dx) > Math.abs(dz)) return dx > 0 ? { side: "east", out: [1, 0] } : { side: "west", out: [-1, 0] };
  return dz >= 0 ? { side: "north", out: [0, 1] } : { side: "south", out: [0, -1] };
}

// Why a cell cannot be used, from the reserved ground and from entities that are not the grid's own
// (an entity is { x, z, layer }). Fence runs of the annex itself never block a cell.
export function blockers(cell, ents) {
  const why = RESERVED.filter((r) => overlaps(cell, r)).map((r) => r.why);
  const pad = { x0: cell.x0 + 0.5, z0: cell.z0 + 0.5, x1: cell.x1 - 0.5, z1: cell.z1 - 0.5 };
  const hits = ents.filter((e) => !/^Grid_/.test(e.layer) && !/NetFence/.test(e.prefab || "") && e.x >= pad.x0 && e.x <= pad.x1 && e.z >= pad.z0 && e.z <= pad.z1);
  if (hits.length) why.push(`${hits.length} existing entit${hits.length === 1 ? "y" : "ies"} (${[...new Set(hits.map((h) => h.layer))].join(", ")})`);
  return why;
}

// FNV-1a: the same slug starts its search at the same cell on every machine.
export function slugHash(slug) {
  let h = 0x811c9dc5;
  for (const ch of String(slug)) {
    h ^= ch.charCodeAt(0);
    h = Math.imul(h, 0x01000193) >>> 0;
  }
  return h >>> 0;
}

// The cell for a slug: the first usable cell from hash(slug) in lattice order. Two branches with
// different slugs rarely land on one cell; validate-world-zones fails if they do.
// `taken` is a Set of cell ids claimed in tools/grid/cells/.
export function allocate(slug, { ents = [], taken = new Set() } = {}) {
  const cells = allCells();
  const start = slugHash(slug) % cells.length;
  for (let k = 0; k < cells.length; k++) {
    const c = cells[(start + k) % cells.length];
    if (taken.has(c.id)) continue;
    if (blockers(c, ents).length) continue;
    return c;
  }
  return null;
}

// ------------------------------------------------------------------ the yard a cell gets

// Rotate local (+x along the run) into the world direction `d`: the yaw whose rot(yaw, 1, 0) is d.
// `rot` is place.mjs's convention (passed in so this file stays free of the tool's imports).
export function yawAlong(d, rot) {
  for (const y of [0, 90, 180, 270]) {
    const [x, z] = rot(y, 1, 0);
    if (Math.abs(x - d[0]) < 1e-6 && Math.abs(z - d[1]) < 1e-6) return y;
  }
  throw new Error(`no right-angle yaw maps +x to ${d}`);
}

// The fence runs of a cell's yard: 5 m segments (pivot at one end, extending along local +x),
// two per side, one left out on the gap side. Returns [{ at: [x, z], dir: [dx, dz] }], plus the
// sign spot (outside the open end of the gap) and the facing the sign and the props take.
export function yardPlan(cell) {
  const i = CELL.inset;
  const a = { x0: cell.x0 + i, z0: cell.z0 + i, x1: cell.x1 - i, z1: cell.z1 - i };
  const L = CELL.fence;
  const sides = {
    south: { from: [a.x0, a.z0], dir: [1, 0] },
    east: { from: [a.x1, a.z0], dir: [0, 1] },
    north: { from: [a.x1, a.z1], dir: [-1, 0] },
    west: { from: [a.x0, a.z1], dir: [0, -1] },
  };
  const gap = gapSide(cell);
  const runs = [];
  let signAt = null;
  for (const [name, s] of Object.entries(sides)) {
    for (let k = 0; k < 2; k++) {
      const at = [s.from[0] + s.dir[0] * L * k, s.from[1] + s.dir[1] * L * k];
      if (name === gap.side && k === 1) {
        // The open half: the sign stands just outside its far end, clear of the opening.
        const end = [s.from[0] + s.dir[0] * L * 2, s.from[1] + s.dir[1] * L * 2];
        signAt = [end[0] - s.dir[0] * 0.6 + gap.out[0] * 0.8, end[1] - s.dir[1] * 0.6 + gap.out[1] * 0.8];
        continue;
      }
      runs.push({ at, dir: s.dir });
    }
  }
  return { runs, signAt, gap, yard: a };
}

// Where the k-th call's row of props goes: a line parallel to the gap side, deeper rows first
// (row 0 at the back), facing the gap. Returns the two end points and the compass word.
export function rowLine(cell, k) {
  const { gap, yard } = yardPlan(cell);
  const depth = 7.5 - 2.5 * k; // from the gap side, inside a 10 m yard
  if (k < 0 || depth < 1.5) return null;
  const edge = 2; // keep row ends 2 m off the side fences
  if (gap.side === "north" || gap.side === "south") {
    const z = gap.side === "north" ? yard.z1 - depth : yard.z0 + depth;
    return { a: [yard.x0 + edge, z], b: [yard.x1 - edge, z], facing: gap.side };
  }
  const x = gap.side === "east" ? yard.x1 - depth : yard.x0 + depth;
  return { a: [x, yard.z0 + edge], b: [x, yard.z1 - edge], facing: gap.side };
}

// ------------------------------------------------------------------ the registry (reads the tree)

// Every claimed cell: one JSON file per feature in tools/grid/cells/ (golden rule 10).
export function readCells(root) {
  const dir = join(root, CELLS_DIR);
  if (!existsSync(dir)) return [];
  return readdirSync(dir)
    .filter((f) => f.endsWith(".json"))
    .sort()
    .map((f) => {
      const file = `${CELLS_DIR}/${f}`;
      try {
        return { file, ...JSON.parse(readFileSync(join(dir, f), "utf8")) };
      } catch (e) {
        return { file, error: String(e.message || e) };
      }
    });
}

// The look target of a claimed cell: the centre of its yard, seen from the gap side.
export function lookArea(entry) {
  const p = parseCellId(entry.cell);
  if (!p) return null;
  const c = { ...cellBounds(p.col, p.row) };
  const front = { north: 0, east: 90, south: 180, west: 270 }[gapSide(c).side];
  return { name: `grid-${entry.slug}`, x: (c.x0 + c.x1) / 2, y: 0, z: (c.z0 + c.z1) / 2, radius: 8, front };
}
