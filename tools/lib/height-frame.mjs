// tools/lib/height-frame.mjs - which frame a layer row's `coords` y is in: the world's zero, or the terrain under it.
//
// The rule is the engine's, not a list of classes. A top-level entity whose flags carry EntityFlags.RELATIVE_Y reads its
// layer y as the height over the terrain at its x and z; every other entity reads it as a world height. The flags are the
// entity's `Flags` value: the row's own `Flags` line when the layer gives one, else the nearest `Flags` up the prefab chain
// (the row's prefab, its parent, ... the vanilla base), else none (0). The Workbench script that saves an entity's
// position does the same subtraction (scripts/WorkbenchCommon/ViewOrientationTool.c: `if (e.GetFlags() &
// EntityFlags.RELATIVE_Y) pos[1] -= terrain y`).
//
// RELATIVE_Y is 0x80000. The generated EntityFlags.c lists the names without values; the island showed the bit
// (audit of 2026-11-21, #3938): every row the engine stood at terrain + y has it (Lamps_Base 0x80403, Wall_Base 0x80003,
// Building_Base 0x80403, FuelStation_E_01_tankpistol_base 0x80000, Tree 0x80103), every row it stood at the written y
// lacks it (DestructibleMultiPhase_Props_Base 0x403: benches, bins, flowerpots, sign boards).
//
// A child entity (a row nested in another's block) is in its parent's frame, never the terrain's: the rule is for the
// top-level rows of a layer.
//
//   RELATIVE_Y                              0x80000
//   rowFlags(blockText)                     -> number | null       a row's own `Flags` line (first value), else null
//   prefabFlags(root, vfs, path)            -> { flags, from }     the nearest `Flags` up the chain; flags null when none
//   frameOf(root, vfs, prefab, own = null)  -> "terrain" | "world" | "unknown"   unknown: the chain could not be read
//   layerY(worldY, terrainY, frame)         the y to write for an entity whose base should stand at worldY
//   worldYOf(layerY, terrainY, frame)       where the engine puts it
import { createRequire } from "node:module";
import { join } from "node:path";
import { readText } from "./game-prefab.mjs";
import { findMcpDir, gamePath } from "./install-api.mjs";

export const RELATIVE_Y = 0x80000;

// A terrain-frame row is legitimately a step or two over the ground (a fence on a plinth, a pump on its slab); more than
// this is a world height written into a row that counts from the ground.
export const TERRAIN_FRAME_MAX_Y = 3;

const GUIDREF = /^\{[0-9A-Fa-f]{16}\}/;
const parentOf = (t) => /^[^\n]*?:\s*"\{[0-9A-Fa-f]{16}\}([^"]+\.et)"/.exec(t)?.[1] ?? null;
const num = (s) => {
  const n = Number(s);
  return Number.isFinite(n) ? n : null;
};

// The root block's own `Flags` (indent one space, as Workbench writes a prefab or a layer row).
export function rowFlags(text) {
  const m = /^ Flags\s+(\S+)/m.exec(String(text ?? ""));
  return m ? num(m[1]) : null;
}

const cache = new Map();
export function prefabFlags(root, vfs, path) {
  const start = String(path ?? "").replace(GUIDREF, "");
  if (!start) return { flags: null, from: null, read: true };
  const key = `${root}|${start.toLowerCase()}`;
  if (cache.has(key)) return cache.get(key);
  let res = { flags: null, from: null, read: true };
  const seen = new Set();
  for (let p = start; p && !seen.has(p.toLowerCase()); ) {
    seen.add(p.toLowerCase());
    const t = readText(root, vfs, p);
    if (t == null) {
      res = { flags: null, from: null, read: false, missing: p };
      break;
    }
    const f = rowFlags(t);
    if (f !== null) {
      res = { flags: f, from: p, read: true };
      break;
    }
    p = parentOf(t);
  }
  cache.set(key, res);
  return res;
}

export function frameOf(root, vfs, prefab, own = null) {
  if (own !== null && own !== undefined) return own & RELATIVE_Y ? "terrain" : "world";
  if (!prefab) return "world"; // a bare GenericEntity: no flags at all
  const f = prefabFlags(root, vfs, prefab);
  if (!f.read) return "unknown";
  return f.flags !== null && f.flags & RELATIVE_Y ? "terrain" : "world";
}

export const layerY = (worldY, terrainY, frame) => (frame === "terrain" ? worldY - terrainY : worldY);
export const worldYOf = (y, terrainY, frame) => (frame === "terrain" ? y + terrainY : y);

// The game paks, opened once and synchronously (Node loads the MCP's ES module through require), or null when this
// machine has no game install or MCP build (the GitHub runner): a vanilla ancestor is then unreadable and the frame
// "unknown". LF_HEIGHT_FRAME_NO_GAME=1 forces that (selftests).
let vfsSync;
export function gameVfsSync() {
  if (vfsSync !== undefined) return vfsSync;
  vfsSync = null;
  if (process.env.LF_HEIGHT_FRAME_NO_GAME === "1") return vfsSync;
  try {
    const mcp = findMcpDir();
    if (mcp) vfsSync = createRequire(import.meta.url)(join(mcp, "dist", "pak", "vfs.js")).PakVirtualFS.get(gamePath()) ?? null;
  } catch {
    vfsSync = null;
  }
  return vfsSync;
}

// Give each layer record (agent-box/layers.mjs: prefab, own `flags`, `nested`) its `frame`. Mutates and returns them.
export function annotateFrames(root, records, vfs = gameVfsSync()) {
  for (const e of records) e.frame = e.nested ? "parent" : frameOf(root, vfs, e.prefab, e.flags ?? null);
  return records;
}
