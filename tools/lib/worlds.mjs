// tools/lib/worlds.mjs: the three worlds and where their layers live (docs/architecture/three-worlds.md).
//
//   TEST_WORLD      Worlds/TestLogic/TestLogic.ent   script tests, no terrain (#1254)
//   GRID_WORLD      Worlds/GridWorld/GridWorld.ent   the flat debug terrain, the annex and every Grid_* layer
//   PLAYTEST_WORLD  Worlds/DebugWorld/DebugWorld.ent the village, plus the pack SubScene Everon1989Town
//
// One table of world paths for the tools that spell them out today (addons.mjs CORE_WORLD,
// world-zones.mjs DEBUG_LAYERS, place.mjs, place-audit.mjs, look.mjs). Nobody imports it yet (W1, #1803).
import { existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const REPO = join(dirname(fileURLToPath(import.meta.url)), "..", "..");

export const CORE_ADDON = "LifeFramework";
export const PACK_ADDON = "LF_Everon1989";

export const TEST_WORLD = "Worlds/TestLogic/TestLogic.ent";
export const GRID_WORLD = "Worlds/GridWorld/GridWorld.ent";
export const PLAYTEST_WORLD = "Worlds/DebugWorld/DebugWorld.ent";
// The pack world that is a SubScene of PLAYTEST_WORLD and holds the pack town's layers.
export const PLAYTEST_PACK_WORLD = "Worlds/Everon1989Town/Everon1989Town.ent";

// The mission header that boots GRID_WORLD (session --mission grid, from W3 on).
export const GRID_MISSION = "{7C1F4A92D03E6B58}Missions/LF_GridWorld.conf";

// "Worlds/X/X.ent" -> "Worlds/X/X_Layers"
export const layersOf = (world) => world.replace(/\/([^/]+)\.ent$/, "/$1_Layers");

// The GridWorld layer folder.
export function gridLayersDir(root = REPO) {
  return join(root, "addons", CORE_ADDON, ...layersOf(GRID_WORLD).split("/"));
}

// Every layer folder of the playtest world, core first: DebugWorld's, then the pack town's when the
// pack is in the checkout. Layer file names are unique across the two.
export function playtestLayerDirs(root = REPO) {
  const dirs = [join(root, "addons", CORE_ADDON, ...layersOf(PLAYTEST_WORLD).split("/"))];
  const pack = join(root, "addons", PACK_ADDON, ...layersOf(PLAYTEST_PACK_WORLD).split("/"));
  if (existsSync(pack)) dirs.push(pack);
  return dirs;
}
