// Pins the world plan of `tools\cli wt e2e` (#1993, tools/lib/e2e-legs.mjs): the PERSISTENCE tier is split
// by world, so the grid-fixture tests (store-stock, store-unpaid) never ride the DebugWorld boot that
// cannot resolve their counter, no test is dropped, and the town tests keep their playtest pair.
// inputs: tools/lib/e2e-legs.mjs, tools/lib/test-shards.mjs, tools/lib/test-world.mjs, tools/lib/worlds.mjs, tools/test/world-pins.json, tools/test/town-tests.json, tools/validation/e2e-legs-selftest.mjs, addons/LifeFramework/Scripts/Game/Tests
//
// red-proof (#1993), 2026-11-16: run against main, where e2e had no plan (every persistence test in the one
// DebugWorld pair): LF_E2E_LEGS_MODULE=<main's e2e-legs.mjs absent> reads as no planE2eLegs and every check
// below FAILs, among them "the grid-fixture store tests run on GridWorld, not on the playtest leg".
import { join } from "node:path";
import { pathToFileURL, fileURLToPath } from "node:url";

const here = fileURLToPath(new URL(".", import.meta.url));
const root = join(here, "..", "..");
const modPath = process.env.LF_E2E_LEGS_MODULE || join(here, "..", "lib", "e2e-legs.mjs");
const mod = await import(pathToFileURL(modPath).href).catch(() => ({}));
const worlds = await import(pathToFileURL(join(here, "..", "lib", "worlds.mjs")).href);
const tw = await import(pathToFileURL(join(here, "..", "lib", "test-world.mjs")).href);
const shards = await import(pathToFileURL(join(here, "..", "lib", "test-shards.mjs")).href);

const failures = [];
const check = (ok, what) => {
  if (!ok) failures.push(what);
};
const plan = typeof mod.planE2eLegs === "function" ? mod.planE2eLegs : () => [];
const MISSION = "{5A1E5A1E5A1E0002}Missions/EL_DebugTest.conf";

// 1. Synthetic registry: one grid test, one town test, a follower of it, the shared shutdown test, a LOGIC test.
const registry = [
  { cls: "LF_Test_Logic", tier: "LOGIC" },
  { cls: "LF_Test_StorePersist", tier: "PERSISTENCE" },
  { cls: "LF_Test_E2E_SaveStateWrite", tier: "PERSISTENCE" },
  { cls: "LF_Test_E2E_SaveStateVerify", tier: "PERSISTENCE" },
  { cls: "LF_Test_E2E_ShutdownSave", tier: "PERSISTENCE" },
  { cls: "LF_Test_Pinned", tier: "PERSISTENCE" },
];
const names = { "e2e/pass1-write-save": "LF_Test_E2E_SaveStateWrite", "persist/pinned": "LF_Test_Pinned" };
const town = [{ name: "e2e/pass1-write-save" }];
const pins = [{ name: "persist/pinned" }];
const legs = plan({ registry, names, pins, town, mission: MISSION, env: {} });
const grid = legs.find((l) => l.world === worlds.GRID_WORLD);
const play = legs.find((l) => l.world === worlds.PLAYTEST_WORLD);
check(legs.length === 2, `a grid leg and a playtest leg (got ${legs.length})`);
check(grid?.mission === worlds.GRID_MISSION, "the grid leg boots the GridWorld mission header (it watches the host stop file)");
check(play?.mission === MISSION, "the playtest leg boots the e2e mission");
check(grid?.classes?.includes("LF_Test_StorePersist"), "the grid-fixture store tests run on GridWorld, not on the playtest leg");
check(!play?.classes?.includes("LF_Test_StorePersist"), "the playtest leg does not hold a grid test");
check(play?.classes?.includes("LF_Test_E2E_SaveStateWrite") && play?.classes?.includes("LF_Test_Pinned"), "the town test and the pin run on the playtest leg");
check(play?.classes?.includes("LF_Test_E2E_SaveStateVerify"), "the verify test follows its writer into the playtest leg");
check(!grid?.classes?.includes("LF_Test_E2E_SaveStateVerify") && !grid?.classes?.includes("LF_Test_E2E_SaveStateWrite"), "the writer and its verify test are not on the grid leg");
check(play?.classes?.includes("LF_Test_E2E_ShutdownSave") && !grid?.classes?.includes("LF_Test_E2E_ShutdownSave"), "the shutdown marker test follows the save-state writer (its manual save) into the playtest leg");
check(play?.markerWait === true && grid?.markerWait === false, "only the leg with the marker test makes the host stop wait for the marker");
check(![grid, play].some((l) => l?.classes?.includes("LF_Test_Logic")), "a LOGIC test is in no e2e leg");
const all = new Set([...(grid?.classes ?? []), ...(play?.classes ?? [])]);
check(registry.filter((t) => t.tier === "PERSISTENCE").every((t) => all.has(t.cls)), "no persistence test is dropped");

// 2. Rollback, pre-move, and a plan with nothing for the playtest world.
const rb = plan({ registry, names, pins, town, mission: MISSION, env: { LF_TEST_GRID_WORLD: "0" } });
check(rb.length === 1 && rb[0].world === worlds.PLAYTEST_WORLD && rb[0].classes === null, "LF_TEST_GRID_WORLD=0 is one playtest leg of the whole tier");
const pm = plan({ registry, names, pins, town, mission: MISSION, preMove: true, env: {} });
check(pm.length === 1 && pm[0].world === worlds.PLAYTEST_WORLD, "--pre-move keeps the playtest leg only");
const noTown = plan({ registry, names, pins: [], town: [], mission: MISSION, env: {} });
check(noTown.length === 1 && noTown[0].world === worlds.GRID_WORLD, "with no town test or pin there is one grid leg");

// 3. The real tree: the two store tests of #1993 land on the grid leg, the town persistence tests on the playtest
// leg, and every PERSISTENCE test of the registry is in a leg.
const real = plan({ registry: shards.readAllRegistry(root), names: tw.testNames(root), pins: tw.readPins(root), town: tw.readTown(root), mission: MISSION, env: {} });
const rg = real.find((l) => l.world === worlds.GRID_WORLD);
const rp = real.find((l) => l.world === worlds.PLAYTEST_WORLD);
check(rg?.classes?.includes("LF_Test_StorePersist") && rg?.classes?.includes("LF_Test_StoreUnpaidPersist"), "the real plan runs persist/store-stock and persist/store-unpaid on GridWorld");
check(rp?.classes?.includes("LF_Test_E2E_SaveStateWrite") && rp?.classes?.includes("LF_Test_E2E_SaveStateVerify"), "the real plan runs the save-state pair on the playtest world");
const realAll = new Set([...(rg?.classes ?? []), ...(rp?.classes ?? [])]);
const missing = shards.readAllRegistry(root).filter((t) => t.tier === "PERSISTENCE" && !realAll.has(t.cls)).map((t) => t.cls);
check(missing.length === 0, `no real persistence test is dropped (missing: ${missing.slice(0, 3).join(",")})`);

if (failures.length) {
  console.log("e2e-legs-selftest: FAIL");
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}
console.log("e2e-legs-selftest: OK (grid tests on GridWorld, town tests on the playtest world, none dropped)");
