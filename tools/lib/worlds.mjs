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
// Vanilla Everon with only the core game mode, a spawn point, the test runner and an SCR_AIWorld (navmesh,
// roads): where vanilla AI walks. session --mission everon boots it (#2174).
export const EVERON_WORLD = "Worlds/CoreOnEveron/CoreOnEveron.ent";
// The island's header: the pack on MainWorld (vanilla Everon, its AI world and navmesh). It inherits
// LF_Everon1989.conf and is hidden from Scenarios; session --mission island boots it (#2204).
export const ISLAND_MISSION = "{5E2A9C41B7D08F36}Missions/LF_EveronIsland.conf";
// The pack world that is a SubScene of PLAYTEST_WORLD and holds the pack town's layers.
export const PLAYTEST_PACK_WORLD = "Worlds/Everon1989Town/Everon1989Town.ent";

// The mission header that boots GRID_WORLD (session --mission grid, from W3 on).
export const GRID_MISSION = "{7C1F4A92D03E6B58}Missions/LF_GridWorld.conf";

// The pack test leg's header: GridWorld under the flagship's header, so the pack's config overlays
// apply. It inherits LF_Everon1989.conf (one overlay list) and is hidden from Scenarios.
export const PACK_TEST_MISSION = "{3626CFA069A75D46}Missions/LF_Everon1989_Test.conf";
// Where a pack keeps its tests, and the registry `regen-tests` writes there for them.
export const PACK_TESTS_DIR = "Scripts/Game/Tests";
export const PACK_TEST_REGISTRY = "LF_PackTestRegistrations.generated.c";

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
