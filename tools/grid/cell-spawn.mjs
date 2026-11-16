// tools/grid/cell-spawn.mjs: the spawn point of a grid cell (#1813, epic #1802).
//
// Every claimed cell has one entity `Grid_<slug>_Spawn` at its gap: a GenericEntity with an
// LF_PlaceComponent of kind GridCell whose id is the slug. `/grid <slug>` and the harness verb
// `teleport grid:<slug>` (Feature/Debug/LF_GridTeleport.c) find it through LF_Places, never by a
// coordinate. The spawn stands in the corridor just outside the gap, 0.7 m past the yard's fence line,
// facing into the yard. The frame (Grid_frame.layer) has one at the gate, in front of the map board.
//
//   node tools/grid/cell-spawn.mjs --backfill   add the spawn to every claimed cell's layer that lacks one
//   node tools/grid/cell-spawn.mjs --check      exit 1 when a claimed cell's layer has no spawn
//
// `grid place` calls `withSpawn` once for a new cell; `lattice.mjs` writes the frame's spawn from
// `frameSpawnLines`. This file imports nothing at load time (lattice.mjs imports it, and
// world-zones.mjs imports lattice.mjs), so the claim and yard maths are loaded on demand.

import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

export const SPAWN_KIND = "GridCell";
// LF_PlaceComponent's GUID as the other layers that place one write it.
export const PLACE_COMPONENT = "{3C7B2E9A41D58F06}";
// How far past the yard's fence line the spawn stands: inside the cell's own 1 m edge strip, clear of the sign.
export const SPAWN_OUT = 0.7;
const FENCE = 5; // lattice CELL.fence: the fence run length a yard side divides into
// A player faces into the yard: the compass heading opposite the gap side (0 = +z, 90 = +x).
const FACE_IN = { north: 180, east: 270, south: 0, west: 90 };
const SIDE_DIR = { south: [1, 0], east: [0, 1], north: [-1, 0], west: [0, -1] };

const num = (v) => {
  const r = Math.round(v * 1000) / 1000;
  return String(Object.is(r, -0) ? 0 : r);
};

export const spawnName = (slug) => `Grid_${slug.replace(/-/g, "_")}_Spawn`;

// The layer text of one spawn entity. Pure.
export function spawnLines(name, id, x, z, yaw) {
  const out = [
    `GenericEntity ${name} {`,
    " components {",
    `  LF_PlaceComponent "${PLACE_COMPONENT}" {`,
    `   m_sKind "${SPAWN_KIND}"`,
    `   m_sId "${id}"`,
    "  }",
    " }",
    ` coords ${num(x)} 0 ${num(z)}`,
  ];
  if (yaw) out.push(` angleY ${num(yaw)}`);
  out.push("}");
  return out;
}

// Where a cell's spawn stands, from its yard plan (`yardPlan(claimRect(claim))` in world-zones.mjs):
// the middle of the gap, SPAWN_OUT past the fence line, facing into the yard. Pure.
export function spawnPoint(plan) {
  const { yard, gap } = plan;
  const dir = SIDE_DIR[gap.side];
  const horizontal = Math.abs(dir[0]) === 1;
  const len = horizontal ? yard.x1 - yard.x0 : yard.z1 - yard.z0;
  const n = Math.max(2, Math.ceil(len / FENCE - 1e-9));
  const open = len >= 20 ? 2 : 1;
  const L = len / n;
  // The side starts at `from` and runs `dir`; the gap is the last `open` runs.
  const from = { south: [yard.x0, yard.z0], east: [yard.x1, yard.z0], north: [yard.x1, yard.z1], west: [yard.x0, yard.z1] }[gap.side];
  const centre = L * (n - open / 2);
  return {
    x: from[0] + dir[0] * centre + gap.out[0] * SPAWN_OUT,
    z: from[1] + dir[1] * centre + gap.out[1] * SPAWN_OUT,
    yaw: FACE_IN[gap.side],
  };
}

// The frame's spawn: at the gate end, in front of the map board (lattice.mjs BOARD_AT), facing it.
export const FRAME_SPAWN_AT = [74, 106];
export function frameSpawnLines() {
  return { name: spawnName("frame"), x: FRAME_SPAWN_AT[0], z: FRAME_SPAWN_AT[1], lines: spawnLines(spawnName("frame"), "frame", FRAME_SPAWN_AT[0], FRAME_SPAWN_AT[1], 180) };
}

// A cell's layer text with its spawn appended when it has none. Pure.
export function withSpawn(text, slug, plan) {
  const name = spawnName(slug);
  if (text.includes(` ${name} `) || text.includes(` ${name}\n`)) return text;
  const p = spawnPoint(plan);
  const body = text.endsWith("\n") || text === "" ? text : `${text}\n`;
  return `${body}${spawnLines(name, slug, p.x, p.z, p.yaw).join("\n")}\n`;
}

// ------------------------------------------------------------------ the tool (reads the tree)

export async function main(argv, { root }) {
  const backfill = argv.includes("--backfill");
  const check = argv.includes("--check");
  if (!backfill && !check) {
    console.log("usage: node tools/grid/cell-spawn.mjs --backfill | --check   (spawn point of every claimed grid cell, #1813)");
    return 1;
  }
  const Z = await import("../lib/world-zones.mjs");
  const { isFrameClaim } = await import("./lattice.mjs");
  let changed = 0;
  const missing = [];
  for (const c of Z.readCells(root)) {
    if (c.error || isFrameClaim(c)) continue;
    const rect = Z.claimRect(c);
    const file = join(root, Z.GRID_LAYERS, Z.layerOfSlug(c.slug));
    if (!rect || !existsSync(file)) continue;
    const text = readFileSync(file, "utf8");
    const next = withSpawn(text, c.slug, Z.yardPlan(rect));
    if (next === text) continue;
    if (backfill) {
      writeFileSync(file, next);
      changed++;
      console.log(`cell-spawn: ${c.slug} (${c.cell}) now has ${spawnName(c.slug)}`);
    } else missing.push(c.slug);
  }
  if (backfill) {
    console.log(`cell-spawn: ${changed} cell(s) backfilled`);
    return 0;
  }
  if (missing.length) {
    console.log(`cell-spawn: no spawn in ${missing.join(", ")}; run node tools/grid/cell-spawn.mjs --backfill`);
    return 1;
  }
  console.log("cell-spawn: every claimed cell has its spawn");
  return 0;
}

const isMain = process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1];
if (isMain) {
  const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  main(process.argv.slice(2), { root }).then((code) => process.exit(code));
}
