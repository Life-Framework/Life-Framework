// tools/validation/validate-grid-world.mjs
// inputs: addons/LifeFramework/Worlds/GridWorld/**, tools/grid/**, tools/lib/world-zones.mjs
//
// Every entity in a grid world stands in a claimed cell or the frame (docs/architecture/world-zones.md,
// three-worlds.md; epic #1802).
//
// A GridWorld layer is one of:
//   - system (default, Gamemode, Factions): world settings, no props, not checked;
//   - the frame (Grid_frame.layer): the lattice itself, checked by validate-world-zones;
//   - a Grid_<slug>.layer: one claimed cell, checked by validate-world-zones (entities inside the cell);
//   - a legacy annex layer (Police, Crimes, Jobs, ...): a free-standing station from before the grid.
// This rule covers the last kind. A legacy layer's entities must each stand inside a cell some claim in
// tools/grid/cells/ names, which in practice means the station moved into its own cell and the legacy
// file is deleted. The annex fence (NetFence runs) is the grid's wall, not a station, and is exempt.
//
// PENDING lists the legacy layers whose slice has not landed yet (epic #1802, "One system per cell").
// A slice that moves a station removes its layer from this list in the same change; the list never grows.
// red-proof: 2026-11-16, on main with Police, Crimes, Jobs and NPCs still free-standing, this printed
//   "FAIL ... Police.layer: PoliceStation at 48,50 ..." for all four and exited 1; after the move it prints OK.
// tools/validation/grid-world-selftest.mjs plants a station outside every cell and asserts it is reported.

import { existsSync, readFileSync, readdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { GRID_LAYERS, SYSTEM_LAYERS, cellBounds, parseCellId, readCells } from "../lib/world-zones.mjs";
import { isFrameClaim } from "../grid/lattice.mjs";
import { topLevelBlocks } from "./validate-world-zones.mjs";

// Legacy layers still free-standing. One per line so slices that remove one do not conflict.
export const PENDING = [
  "Annex",
];

// Entities of the legacy layers that stand outside every claimed cell: [{ layer, name, x, z }].
// `layers` is { "<Name>.layer": text }, `claims` the parsed tools/grid/cells/*.json.
export function stationsOutsideCells(layers, claims, pending = PENDING) {
  const cells = claims
    .filter((c) => !c.error && !isFrameClaim(c))
    .map((c) => parseCellId(c.cell))
    .filter(Boolean)
    .map((p) => cellBounds(p.col, p.row));
  const out = [];
  for (const [file, text] of Object.entries(layers)) {
    const name = file.replace(/\.layer$/, "");
    if (/^Grid_/.test(name) || SYSTEM_LAYERS.has(name) || pending.includes(name)) continue;
    for (const b of topLevelBlocks(text)) {
      if (b.x == null || b.z == null || /NetFence/.test(b.text)) continue;
      if (cells.some((c) => b.x >= c.x0 && b.x <= c.x1 && b.z >= c.z0 && b.z <= c.z1)) continue;
      out.push({ layer: file, name: b.name.replace(/\s*\{\s*$/, "").split(/\s+/).pop(), x: b.x, z: b.z });
    }
  }
  return out;
}

const isMain = process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1];
if (isMain) {
  const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const dir = join(root, GRID_LAYERS);
  const layers = {};
  if (existsSync(dir)) for (const f of readdirSync(dir).filter((n) => n.endsWith(".layer"))) layers[f] = readFileSync(join(dir, f), "utf8");
  const bad = stationsOutsideCells(layers, readCells(root));
  if (bad.length) {
    console.log("validate-grid-world: FAIL");
    for (const b of bad) {
      console.log(`  ${GRID_LAYERS}/${b.layer}: ${b.name} at ${b.x},${b.z} stands in no claimed cell. Move the station into its own cell (tools\\cli grid place <slug>, then place --move onto Grid_<slug>.layer) and delete ${b.layer}`);
    }
    process.exit(1);
  }
  console.log(`validate-grid-world: OK (${Object.keys(layers).length} GridWorld layer(s); ${PENDING.length} legacy layer(s) still pending)`);
}
