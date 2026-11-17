// tools/grid/lattice.mjs: the lattice of grid cells and the frame drawn on it (#1810, epic #1802).
//
// The lattice (constants and cell maths) is a leaf module: tools/lib/world-zones.mjs imports and
// re-exports it. The frame is GridWorld_Layers/Grid_frame.layer, written from here:
//   - GridMarker_cXrY  a pole plate at each cell's west edge reading its id (c3r1);
//   - GridPlate_cXrY   the plate above it: the slug of the cell's claim, FREE, or BLOCKED;
//   - GridPath_rR_N    gravel mounds in a line along the south edge of each row (not through a claim spanning two rows);
//   - GridMapBoard     at the gate spawn, listing cell to system.
// The frame is one claim, tools/grid/cells/frame.json, whose cell is "lattice": the checks accept its
// entities anywhere inside the lattice rectangle instead of inside one cell.
//
//   node tools/grid/lattice.mjs write   rewrite Grid_frame.layer from the claims and layers on disk
//   node tools/grid/lattice.mjs check   exit 1 when the file on disk is not what `write` would write
//   node tools/grid/lattice.mjs list    the map `grid list` prints, plus the marker names
//
// The plates are a snapshot of the claims: run `write` after claiming a cell to refresh them.

import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { frameSpawnLines } from "./cell-spawn.mjs";

// Cells: an 11 x 7 lattice of 12 m squares from 22,22 (2 m inside the annex fence). Each cell holds a
// 10 m fenced yard (1 m in from its edges) with a 5 m gap on the side facing the annex centre.
// The lattice grew east from 7 to 11 columns (capacity, 2026-11-16): existing cells keep their ids and
// coordinates, so every old claim stands. Growing again: raise `cols`, move the Annex.layer east wall
// (GRID.x1 below follows), `node tools/grid/lattice.mjs write`. The terrain (GridTerrain, 257 x 257 points
// at 2 m = 512 m) is far larger than the grid; keep GRID inside 0..512 (tools/validation/grid-world-selftest.mjs).
// GRID_CENTRE stays where it was so no old cell's gap flips to another side.
export const CELL = { size: 12, origin: 22, cols: 25, rows: 7, inset: 1, fence: 5 };
export const GRID_CENTRE = [66, 66];
// The outer fence (Annex.layer): 2 m outside the lattice on the west and south, 6 m on the east and north
// (the north gate and the track in through it live in that margin). x 20..160, z 20..112.
export const FENCE_MARGIN = { low: 2, high: 6 };
export const GRID_FENCE = {
  x0: CELL.origin - FENCE_MARGIN.low,
  z0: CELL.origin - FENCE_MARGIN.low,
  x1: CELL.origin + CELL.cols * CELL.size + FENCE_MARGIN.high,
  z1: CELL.origin + CELL.rows * CELL.size + FENCE_MARGIN.high,
};
export const TERRAIN_SIZE = 512;

export const FRAME_SLUG = "frame";
export const FRAME_CELL = "lattice";
export const FRAME_LAYER = "Grid_frame.layer";
export const isFrameClaim = (c) => !!c && c.slug === FRAME_SLUG;

export const cellId = (col, row) => `c${col}r${row}`;

// A claim may cover several cells for a big item (a 33 x 19 m hotel): "cell" is its low corner (west, south)
// and "span": [cols, rows] its size; no "span" is one cell. The claim's cells are a rectangle.
export function claimSpan(c) {
  const s = c && Array.isArray(c.span) ? c.span : [1, 1];
  const w = Number(s[0]);
  const h = Number(s[1]);
  return Number.isInteger(w) && Number.isInteger(h) && w >= 1 && h >= 1 ? { w, h } : null;
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

// The cell ids a claim covers (empty when the anchor or the span is bad or the block leaves the lattice).
export function claimCellIds(c) {
  const p = c && c.cell ? parseCellId(c.cell) : null;
  const s = claimSpan(c);
  if (!p || !s || p.col + s.w > CELL.cols || p.row + s.h > CELL.rows) return [];
  const out = [];
  for (let r = 0; r < s.h; r++) for (let k = 0; k < s.w; k++) out.push(cellId(p.col + k, p.row + r));
  return out;
}

// The claim's rectangle (the span's cells joined), or null.
export function claimRect(c) {
  const p = c && c.cell ? parseCellId(c.cell) : null;
  const s = claimSpan(c);
  if (!p || !s || !claimCellIds(c).length) return null;
  const a = cellBounds(p.col, p.row);
  const b = cellBounds(p.col + s.w - 1, p.row + s.h - 1);
  return { id: c.cell, col: p.col, row: p.row, w: s.w, h: s.h, x0: a.x0, z0: a.z0, x1: b.x1, z1: b.z1 };
}

export function allCells() {
  const out = [];
  for (let row = 0; row < CELL.rows; row++) for (let col = 0; col < CELL.cols; col++) out.push({ id: cellId(col, row), col, row, ...cellBounds(col, row) });
  return out;
}

// The whole lattice as one rectangle (x 22..154, z 22..106).
export const latticeRect = () => {
  const a = cellBounds(0, 0);
  const b = cellBounds(CELL.cols - 1, CELL.rows - 1);
  return { x0: a.x0, z0: a.z0, x1: b.x1, z1: b.z1 };
};

export const markerName = (col, row) => `GridMarker_${cellId(col, row)}`;
export const plateName = (col, row) => `GridPlate_${cellId(col, row)}`;
export const MAP_BOARD = "GridMapBoard";

// ------------------------------------------------------------------ the frame

const SIGN = { cls: "GenericEntity", ref: "{B7B489CA36A0E9F8}Prefabs/Props/Signs/LF_Sign_PolePlate.et", component: "{026619B31213409D}", baseY: 0.505 };
// GravelPile_01_base (a 8.3 x 5 m mound) scaled to a 2.5 x 1.5 m strip, laid end to end.
const GRAVEL = { cls: "StaticModelEntity", ref: "{D93A7286584E122C}Prefabs/Props/Construction/GravelPile_01/GravelPile_01_base.et", scale: 0.3, step: 2.4, y: 0.025, pivotX: 0.13 };
// Compass word -> the angleY a sign takes to face it (a sign's front is +z; north is +z, east +x).
export const YAW = { north: 0, east: 90, south: 180, west: 270 };
// Gate spawn (Gamemode.layer Spawn_Grid_Gate: 74,108 facing south): the board stands 3.5 m in front of it, facing it.
export const BOARD_AT = [74, 104.5];

const COLOURS = {
  marker: { rim: [0.1, 0.1, 0.1], paint: [0.25, 0.6, 0.95], text: [0.05, 0.05, 0.05] },
  claimed: { rim: [0.1, 0.1, 0.1], paint: [0.95, 0.78, 0.1], text: [0.1, 0.1, 0.1] },
  free: { rim: [0.1, 0.1, 0.1], paint: [0.4, 0.8, 0.4], text: [0.05, 0.05, 0.05] },
  blocked: { rim: [0.1, 0.1, 0.1], paint: [0.62, 0.62, 0.62], text: [0.1, 0.1, 0.1] },
  board: { rim: [0.1, 0.1, 0.1], paint: [0.95, 0.95, 0.9], text: [0.05, 0.05, 0.05] },
};

const num = (v) => {
  const r = Math.round(v * 1000) / 1000;
  return String(Object.is(r, -0) ? 0 : r);
};

// What a plate reads for each cell: the claim's slug, FREE, or BLOCKED (legacy stations or reserved ground).
// `claims` is readCells(); `blocked` a Set of cell ids that blockers() rejects.
export function cellStates(claims, blocked = new Set()) {
  const bySlug = new Map(claims.filter((c) => c.slug && !c.error && !isFrameClaim(c)).flatMap((c) => claimCellIds(c).map((id) => [id, c.slug])));
  return allCells().map((c) => {
    const slug = bySlug.get(c.id);
    const state = slug ? "claimed" : blocked.has(c.id) ? "blocked" : "free";
    return { ...c, state, label: slug ?? state };
  });
}

// The map board's title: cell to system, the claimed cells only (the plates carry the rest).
export function boardLabel(states) {
  const claimed = states.filter((s) => s.state === "claimed").map((s) => `${s.id} ${s.label}`);
  return `map ${claimed.length ? claimed.join(", ") : "no cells claimed"}`;
}

function signBlock(name, id, at, yaw, colour) {
  const lines = [
    `${SIGN.cls} ${name} : "${SIGN.ref}" {`,
    " components {",
    `  LF_SignComponent "${SIGN.component}" {`,
    `   m_sSignId "grid:${id}"`,
    `   m_vRimColor ${colour.rim.join(" ")}`,
    `   m_vPaintColor ${colour.paint.join(" ")}`,
    `   m_vTextColor ${colour.text.join(" ")}`,
    "  }",
    " }",
    ` coords ${num(at[0])} ${SIGN.baseY} ${num(at[1])}`,
  ];
  if (yaw) lines.push(` angleY ${yaw}`);
  lines.push("}");
  return lines;
}

// Every entity of the frame: [{ name, x, z }] with `lines` of layer text. Pure.
export function framePlan(states) {
  const out = [];
  for (const c of states) {
    // The west edge, in the 2 m corridor between columns: marker 1.5 m and plate 3.5 m north of the row's path.
    const mAt = [c.x0 + 0.5, c.z0 + 1.5];
    const pAt = [c.x0 + 0.5, c.z0 + 3.5];
    out.push({ name: markerName(c.col, c.row), x: mAt[0], z: mAt[1], lines: signBlock(markerName(c.col, c.row), c.id, mAt, YAW.south, COLOURS.marker) });
    out.push({ name: plateName(c.col, c.row), x: pAt[0], z: pAt[1], lines: signBlock(plateName(c.col, c.row), c.label, pAt, YAW.south, COLOURS[c.state]) });
  }
  const r = latticeRect();
  const slugAt = new Map(states.filter((s) => s.state === "claimed").map((s) => [`${s.col}_${s.row}`, s.label]));
  for (let row = 0; row < CELL.rows; row++) {
    const z = cellBounds(0, row).z0;
    const n = Math.floor((r.x1 - r.x0 - 2 * 1.2) / GRAVEL.step) + 1;
    for (let k = 0; k < n; k++) {
      const x = r.x0 + 1.2 + k * GRAVEL.step - GRAVEL.pivotX;
      // The path stops at a multi-row claim: its props may stand on the line between two of its rows.
      const col = Math.min(CELL.cols - 1, Math.max(0, Math.floor((x - CELL.origin) / CELL.size)));
      const above = slugAt.get(`${col}_${row - 1}`);
      if (above && above === slugAt.get(`${col}_${row}`)) continue;
      const name = `GridPath_r${row}_${k + 1}`;
      out.push({ name, x, z, lines: [`${GRAVEL.cls} ${name} : "${GRAVEL.ref}" {`, ` coords ${num(x)} ${GRAVEL.y} ${num(z)}`, ` scale ${GRAVEL.scale}`, "}"] });
    }
  }
  out.push(frameSpawnLines());
  out.push({ name: MAP_BOARD, x: BOARD_AT[0], z: BOARD_AT[1], lines: signBlock(MAP_BOARD, boardLabel(states), BOARD_AT, YAW.north, COLOURS.board) });
  return out;
}

export function frameLayerText(states) {
  const head = "// The grid frame (#1810): cell markers and plates, a gravel path along each row and the map board at the gate spawn.";
  const how = "// Written by `node tools/grid/lattice.mjs write` (tools/grid/lattice.mjs); the plates are a snapshot of the claims, so write again after claiming a cell. Debug zone: low look bar.";
  return `${[head, how, ...framePlan(states).flatMap((e) => e.lines)].join("\n")}\n`;
}

export const frameClaimText = (created) => `${JSON.stringify({ slug: FRAME_SLUG, cell: FRAME_CELL, layer: FRAME_LAYER, rows: 0, created }, null, 2)}\n`;

// ------------------------------------------------------------------ the tool (reads the tree)

async function stateOnDisk(root) {
  const Z = await import("../lib/world-zones.mjs");
  const { loadWorld } = await import("../../agent-box/layers.mjs");
  const { gridLayerDirs } = await import("../lib/worlds.mjs");
  const dirs = gridLayerDirs(root).filter((d) => existsSync(d));
  const ents = dirs.length ? loadWorld(root, dirs) : [];
  const claims = Z.readCells(root);
  const blocked = new Set(allCells().filter((c) => !claims.some((x) => x.cell === c.id) && Z.blockers(c, ents).length).map((c) => c.id));
  return { Z, claims, states: cellStates(claims, blocked) };
}

export async function main(argv, { root }) {
  const [sub = "list"] = argv;
  const { Z, claims, states } = await stateOnDisk(root);
  const layerFile = join(root, Z.GRID_LAYERS, FRAME_LAYER);
  const claimFile = join(root, Z.CELLS_DIR, `${FRAME_SLUG}.json`);
  const text = frameLayerText(states);
  if (sub === "write") {
    mkdirSync(dirname(layerFile), { recursive: true });
    mkdirSync(dirname(claimFile), { recursive: true });
    writeFileSync(layerFile, text);
    const old = claims.find(isFrameClaim);
    if (!existsSync(claimFile)) writeFileSync(claimFile, frameClaimText(old?.created ?? new Date().toISOString().slice(0, 10)));
    console.log(`lattice: wrote ${Z.GRID_LAYERS}/${FRAME_LAYER} (${framePlan(states).length} entities) and ${Z.CELLS_DIR}/${FRAME_SLUG}.json`);
    return 0;
  }
  if (sub === "check") {
    const now = existsSync(layerFile) ? readFileSync(layerFile, "utf8") : null;
    if (now === text) {
      console.log("lattice: the frame matches the claims");
      return 0;
    }
    console.log(`lattice: ${FRAME_LAYER} is ${now == null ? "missing" : "out of date"}; run node tools/grid/lattice.mjs write`);
    return 1;
  }
  states.forEach((s) => console.log(`${s.id.padEnd(5)} ${s.label.padEnd(28)} ${markerName(s.col, s.row)}`));
  console.log(boardLabel(states));
  return 0;
}

const isMain = process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1];
if (isMain) {
  const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  main(process.argv.slice(2), { root }).then((code) => process.exit(code));
}
