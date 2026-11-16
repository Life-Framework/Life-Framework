// Which world each restart proof of `tools\cli wt e2e` boots (#1993; docs/architecture/three-worlds.md).
//
// e2e runs the PERSISTENCE tier twice (save boot, reload boot). Since the store tests moved to grid
// fixtures (#1891, #1918) that tier no longer fits one world: most tests find their fixtures in
// GridWorld (what `test --tier persistence` boots), the town tests (tools/test/town-tests.json) and
// the pins (tools/test/world-pins.json) only in the playtest world. One pair of boots per world, each
// with only its tests (-Only), so neither pass 1 fails on a fixture its world lacks and pass 2 runs.
//
//   grid leg      GRID_MISSION (GridWorld; its header watches the host stop file)
//                 every PERSISTENCE test that is not a town test or a pin
//   playtest leg  the e2e mission (EL_DebugTest.conf) or --mission playtest
//                 the PERSISTENCE town tests and pins
//
// E2E_FOLLOWS: a test that depends on another one goes where that one goes: the verify test reads what the
// save-state test wrote, and the shutdown marker is deposited after the save-state test's manual save, so the
// host stop waits for the marker (leg.markerWait) only in the leg that holds it.
//   pack leg      PACK_TEST_MISSION (GridWorld under the pack header, so its config overlays apply)
//                 the PERSISTENCE pack tests (addons/LF_Everon1989/Scripts/Game/Tests; readPackTests)
//
// LF_TEST_GRID_WORLD=0 is the rollback: one playtest leg of the whole tier, as before #1993.
// --pre-move boots a tree from before the grid existed in pass 1, so it keeps the playtest leg only.
import { GRID_MISSION, GRID_WORLD, PACK_TEST_MISSION, PLAYTEST_WORLD } from "./worlds.mjs";

export const E2E_FOLLOWS = { LF_Test_E2E_SaveStateVerify: "LF_Test_E2E_SaveStateWrite", LF_Test_E2E_ShutdownSave: "LF_Test_E2E_SaveStateWrite" };
export const SHUTDOWN_MARKER_TEST = "LF_Test_E2E_ShutdownSave";

// registry: [{ cls, tier }] with the town tests in it (test-shards.readAllRegistry); names: testNames();
// pins, town: readPins() and readTown(); mission: the playtest leg's header; pack: readPackTests()
// (the pack tests, which no core leg runs: the runner skips a pack test it was not named for).
// -> [{ world, mission, classes|null, reason }]
export function planE2eLegs({ registry, names, pins, town, mission, pack = [], preMove = false, env = process.env }) {
  const playtest = (classes, reason) => ({ world: PLAYTEST_WORLD, mission, classes, markerWait: !classes || classes.includes(SHUTDOWN_MARKER_TEST), reason });
  const packClasses = preMove ? [] : pack.filter((t) => t.tier === "PERSISTENCE").map((t) => t.cls);
  const packLeg = packClasses.length ? [{ world: GRID_WORLD, mission: PACK_TEST_MISSION, classes: packClasses, markerWait: false, reason: `${packClasses.length} pack persistence test(s) under the pack header` }] : [];
  if (env.LF_TEST_GRID_WORLD === "0") return [playtest(null, "LF_TEST_GRID_WORLD=0: the whole tier on the playtest world"), ...packLeg];
  const clsOf = (n) => (names ?? {})[n] ?? n;
  const tier = (registry ?? []).filter((t) => t.tier === "PERSISTENCE").map((t) => t.cls);
  const inTier = new Set(tier);
  const toPlaytest = new Set([...(pins ?? []), ...(town ?? [])].map((p) => clsOf(p.name)).filter((c) => inTier.has(c)));
  for (const [follower, leader] of Object.entries(E2E_FOLLOWS)) if (toPlaytest.has(leader) && inTier.has(follower)) toPlaytest.add(follower);
  const playClasses = tier.filter((c) => toPlaytest.has(c));
  const gridClasses = tier.filter((c) => !toPlaytest.has(c));
  const legs = [];
  const markerWait = (classes) => classes.includes(SHUTDOWN_MARKER_TEST);
  if (gridClasses.length && !preMove) {
    legs.push({ world: GRID_WORLD, mission: GRID_MISSION, classes: gridClasses, markerWait: markerWait(gridClasses), reason: `${gridClasses.length} persistence test(s) with grid fixtures (as test --tier persistence)` });
  }
  if (toPlaytest.size || preMove || !legs.length) {
    legs.push(playtest(playClasses.length ? playClasses : null, `${toPlaytest.size} town or pinned persistence test(s) (${"tools/test/town-tests.json"}, ${"tools/test/world-pins.json"})${preMove ? "; --pre-move keeps this leg only" : ""}`));
  }
  return [...legs, ...packLeg];
}
