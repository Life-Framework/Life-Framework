// Every island layer row holds its y in the frame its prefab reads it in.
// inputs: addons/**, tools/lib/height-frame.mjs, tools/validation/height-frame-known/**, agent-box/layers.mjs
//
// The engine reads a top-level entity's layer y over the terrain when its flags carry EntityFlags.RELATIVE_Y (0x80000:
// the row's own `Flags`, else the nearest up its prefab chain), and as a world height otherwise (tools/lib/height-frame.mjs).
// A row written in the other frame stands somewhere else than meant: the Saint-Philippe fuel pumps stood 19 m over the
// forecourt (world 18.8 in a terrain-frame row), the police street bins sank 17 m (0 in a world-frame row; #3938).
//
// Checked on the island's layers (MainWorld: real terrain; the town and the grid stand on flat terrain at 0, where both
// frames agree):
//   terrain frame  y above TERRAIN_FRAME_MAX_Y (3 m) fails: a world height copied into a row that counts from the ground;
//   world frame    y under 3 m while the world-frame rows within NEAR_M stand at least GAP_M higher (their median) fails:
//                  a height over the ground in a row that counts from the world's zero.
// A row nested in another entity's block is in its parent's frame and is not checked.
//
// Vanilla prefab chains are read from the game paks; with no game install (the GitHub runner) the check says so and
// passes: every dev box, `wt ship`'s preflight and the main guard run it with the paks.
//
// Known rows: tools/validation/height-frame-known/<layer>.txt lists row names still wrong on main when this check landed;
// they print as KNOWN and do not fail. The PR that corrects a layer deletes its file (one file per layer, so slot PRs never
// touch the same file). Nothing is added there: a new row in the wrong frame is fixed by its writer.
//
// LF_HEIGHT_FRAME_DIR points the check at another layer folder (the selftest).
// red-proof: 2026-11-21, on the island before the slot fixes (no known files yet) it failed 39 rows: the 32 the audit called displaced
// (lamps, woodlot and sawmill fences, depot, fuel pumps, weighbridge), woodlot fence_sw3 (which it counted seated) and the 6 sunk ones (police street decor, the
// farm-supply and timber boards); tools/validation/height-frame-selftest.mjs plants one of each.
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { loadWorld } from "../../agent-box/layers.mjs";
import { annotateFrames, gameVfsSync, TERRAIN_FRAME_MAX_Y } from "../lib/height-frame.mjs";
import { islandLayersDir } from "../lib/worlds.mjs";

export const NEAR_M = 40;
export const GAP_M = 5;
const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const KNOWN_DIR = join(ROOT, "tools", "validation", "height-frame-known");

const median = (a) => {
  const s = [...a].sort((p, q) => p - q);
  return s.length ? s[Math.floor(s.length / 2)] : null;
};

// Pure: the wrong-frame rows of a set of records that already carry `frame` (annotateFrames).
export function wrongFrameRows(records) {
  const top = records.filter((e) => !e.nested && e.name);
  const world = top.filter((e) => e.frame === "world");
  const out = [];
  for (const e of top) {
    if (e.frame === "terrain" && e.y > TERRAIN_FRAME_MAX_Y) {
      out.push({ e, why: `its prefab flags carry RELATIVE_Y, so the engine reads y ${e.y} over the terrain: it stands ${e.y} m up. Write the height over the ground (height-frame.mjs layerY)` });
    } else if (e.frame === "world" && e.y < TERRAIN_FRAME_MAX_Y) {
      const near = world.filter((o) => o !== e && Math.hypot(o.x - e.x, o.z - e.z) <= NEAR_M).map((o) => o.y);
      const m = median(near);
      if (m !== null && m - e.y >= GAP_M) out.push({ e, why: `its prefab reads y as a world height (no RELATIVE_Y), and y ${e.y} lies ${Math.round((m - e.y) * 10) / 10} m under the rows around it (median ${m}): a height over the ground written into a world-frame row` });
    }
  }
  return out;
}

export function knownRows(dir = KNOWN_DIR) {
  const known = new Set();
  if (!existsSync(dir)) return known;
  for (const f of readdirSafe(dir)) {
    if (!f.endsWith(".txt")) continue;
    const layer = f.replace(/\.txt$/, "");
    for (const line of readFileSync(join(dir, f), "utf8").split(/\r?\n/)) if (line.trim() && !line.startsWith("#")) known.add(`${layer}|${line.trim()}`);
  }
  return known;
}

function readdirSafe(d) {
  try {
    return readdirSync(d);
  } catch {
    return [];
  }
}

export function main() {
  const vfs = gameVfsSync();
  if (!vfs) {
    console.log("validate-layer-height-frame: skipped, no game install here to read the vanilla prefab chains (the main guard and every dev box run it)");
    return 0;
  }
  const dir = process.env.LF_HEIGHT_FRAME_DIR || islandLayersDir(ROOT);
  const records = annotateFrames(ROOT, loadWorld(ROOT, dir), vfs);
  const known = process.env.LF_HEIGHT_FRAME_DIR ? new Set() : knownRows();
  const wrong = wrongFrameRows(records);
  let fails = 0;
  let kept = 0;
  for (const { e, why } of wrong) {
    if (known.has(`${e.layer.replace(/\.layer$/, "")}|${e.name}`)) {
      kept++;
      continue;
    }
    fails++;
    console.log(`FAIL ${e.layer} ${e.name} @${e.x},${e.y},${e.z}: ${why}`);
  }
  const unknown = records.filter((e) => e.frame === "unknown").length;
  console.log(`validate-layer-height-frame: ${records.length} row(s), ${fails} in the wrong frame${kept ? `, ${kept} known (tools/validation/height-frame-known/)` : ""}${unknown ? `, ${unknown} with an unreadable prefab chain (not judged)` : ""}`);
  return fails ? 1 : 0;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) process.exit(main());
