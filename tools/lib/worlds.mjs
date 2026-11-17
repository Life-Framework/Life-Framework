// tools/lib/worlds.mjs: the three worlds and where their layers live (docs/architecture/three-worlds.md).
//
//   TEST_WORLD      Worlds/TestLogic/TestLogic.ent   script tests, no terrain (#1254)
//   GRID_WORLD      Worlds/GridWorld/GridWorld.ent   the flat debug terrain, the annex and every Grid_* layer
//   PLAYTEST_WORLD  Worlds/DebugWorld/DebugWorld.ent the village, plus the pack SubScene Everon1989Town
//
// One table of world paths for the tools that spell them out today (addons.mjs CORE_WORLD,
// world-zones.mjs DEBUG_LAYERS, place.mjs, place-audit.mjs, look.mjs). Nobody imports it yet (W1, #1803).
import { existsSync, readdirSync } from "node:fs";
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
// The pack's island world itself (a SubScene of vanilla Eden): MainWorld_Layers holds the Place_<place>_<slot>.layer
// files `town fit` writes. `look place:<place>/<slot>` opens it (docs/visual-testing.md "Island slots").
export const ISLAND_WORLD = "Worlds/MainWorld/MainWorld.ent";
export const islandLayersDir = (root = REPO) => join(root, "addons", PACK_ADDON, ...layersOf(ISLAND_WORLD).split("/"));
// The pack world that is a SubScene of PLAYTEST_WORLD and holds the pack town's layers.
export const PLAYTEST_PACK_WORLD = "Worlds/Everon1989Town/Everon1989Town.ent";

// The mission header that boots GRID_WORLD (session --mission grid, from W3 on).
export const GRID_MISSION = "{7C1F4A92D03E6B58}Missions/LF_GridWorld.conf";

// The pack's own grid world: a SubScene of GRID_WORLD (the way Everon1989Town is one of DebugWorld) that holds the
// Grid_<slug>.layer files whose prefabs are pack content (docs/architecture/world-zones.md "Pack cells"). Booted
// by the pack headers only; core-only runs never see it.
export const PACK_GRID_WORLD = "Worlds/Everon1989Grid/Everon1989Grid.ent";

// The pack test leg's header: GridWorld under the flagship's header, so the pack's config overlays
// apply. It inherits LF_Everon1989.conf (one overlay list) and is hidden from Scenarios.
export const PACK_TEST_MISSION = "{3626CFA069A75D46}Missions/LF_Everon1989_Test.conf";
// The header `session --mission grid` boots when the pack's grid layers exist: the same header as the pack test
// leg, whose World is PACK_GRID_WORLD (the grid plus the pack's cells, under the flagship's overlays).
export const PACK_GRID_MISSION = PACK_TEST_MISSION;
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

// The pack's grid layer folder (it may not exist: a core-only checkout, or no pack cell yet).
export function packGridLayersDir(root = REPO) {
  return join(root, "addons", PACK_ADDON, ...layersOf(PACK_GRID_WORLD).split("/"));
}

// True when the pack holds at least one Grid_*.layer, so the pack grid world has something GridWorld lacks.
export function hasPackGridLayers(root = REPO) {
  const dir = packGridLayersDir(root);
  return existsSync(dir) && readdirSync(dir).some((f) => /^Grid_.*\.layer$/.test(f));
}

// Every grid layer folder, core first, then the pack's when it exists: the layers the grid tools read.
export function gridLayerDirs(root = REPO) {
  const dirs = [gridLayersDir(root)];
  const pack = packGridLayersDir(root);
  if (existsSync(pack)) dirs.push(pack);
  return dirs;
}

// The world a grid boot opens: the pack's grid world when it holds a cell, else GridWorld. `pack` forces the
// pack world (a look at a pack cell) when the caller already knows the target is a pack claim.
export function gridWorldFor(root = REPO, { pack = false } = {}) {
  return pack || hasPackGridLayers(root) ? PACK_GRID_WORLD : GRID_WORLD;
}
