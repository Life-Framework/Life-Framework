// Which world a test run boots (tools\cli test; docs/architecture/three-worlds.md).
//
// A run whose every selected test is LOGIC boots the minimal world TEST_WORLD (Worlds/TestLogic):
// a world entity, the time manager, the debug game mode, the destruction manager and a test runner
// flagged m_bLogicWorld. No terrain, village, annex, zones or spawn points (#1254). WORLD and
// PERSISTENCE runs boot GRID_WORLD (Worlds/GridWorld: the flat terrain and the annex; #1803, #1804).
// A test listed in tools/test/world-pins.json still reads the town, so it runs in a second boot on
// PLAYTEST_WORLD (DebugWorld), and the two SUMMARY lines merge into one verdict (planLegs, mergeLegs).
// A test listed in tools/test/town-tests.json is about the town itself: no suite run carries it (not
// --tier all, persistence, ci, wt gate or wt ship), and `test --town` runs the whole list in one boot
// of PLAYTEST_WORLD (#1895). Naming one with --only still runs it, on PLAYTEST_WORLD.
//
//   --tier fast (no --only)        LOGIC only by definition (LF_TestManager FAST mode)  -> TestLogic
//   --only / --red, all LOGIC      every name resolves to a LOGIC test                   -> TestLogic
//   --only / --red, any other name WORLD, PERSISTENCE or unknown                         -> GridWorld (+ DebugWorld for pins)
//   --tier all | persistence       -> GridWorld (+ DebugWorld for the pinned tests of that tier)
//   --tier perf                    -> DebugWorld (its budgets were measured there)
//   --town                         -> one PLAYTEST_WORLD boot of the town tests only (townLeg)
//   --world <path>                 always wins, never split. LF_TEST_LOGIC_WORLD=0 turns the LOGIC
//                                  routing off; LF_TEST_GRID_WORLD=0 sends WORLD/PERSISTENCE to DebugWorld
//
// In TestLogic the runner refuses a WORLD or PERSISTENCE test by name and adds a "tier it WORLD"
// hint under a failing LOGIC test (LF_TestManager.LogicWorldRefusal / LogicWorldHint).
// The chosen world goes into the test pass cache key (tools/lib/test-cache.mjs keys on --world),
// so each leg of a split run is cached on its own.

import { existsSync, readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { TEST_WORLD, GRID_WORLD, PLAYTEST_WORLD } from "./worlds.mjs";

export { GRID_WORLD, PLAYTEST_WORLD };
export const LOGIC_WORLD = TEST_WORLD;
export const LOGIC_WORLD_LAYER = "addons/LifeFramework/Worlds/TestLogic/TestLogic_Layers/Tests.layer";
export const PINS_FILE = "tools/test/world-pins.json";
export const TOWN_FILE = "tools/test/town-tests.json";

const TESTS_DIR = ["addons", "LifeFramework", "Scripts", "Game", "Tests"];

// { "<LF_Test_Class>": "LOGIC", "<GetName() string>": "LOGIC", ... } from every test file's
// `// tier:` line above each `class LF_Test_X : LF_Test`, the same source regen-tests reads.
export function parseTiers(text) {
  const tiers = {};
  let pending = null;
  let cls = null;
  let inGetName = false;
  for (const line of text.split(/\r?\n/)) {
    const t = /^\s*\/\/\s*tier:\s*(LOGIC|WORLD|PERSISTENCE)\b/.exec(line);
    if (t) {
      pending = t[1];
      continue;
    }
    const c = /^\s*class\s+(\w+)\s*:\s*LF_Test\b/.exec(line);
    if (c) {
      cls = c[1];
      if (pending) tiers[cls] = pending;
      pending = null;
      inGetName = false;
      continue;
    }
    if (!cls || !tiers[cls]) continue;
    if (/\bstring\s+GetName\s*\(\s*\)/.test(line)) {
      inGetName = true;
      continue;
    }
    if (inGetName) {
      const r = /return\s+"([^"]+)"\s*;/.exec(line);
      if (r) {
        tiers[r[1]] = tiers[cls];
        inGetName = false;
      } else if (/^\s*}\s*$/.test(line)) inGetName = false;
    }
  }
  return tiers;
}

// { "<GetName() string>": "<LF_Test_Class>" }, so a pin or an --only may name a test either way.
export function parseNames(text) {
  const names = {};
  let cls = null;
  let inGetName = false;
  for (const line of text.split(/\r?\n/)) {
    const c = /^\s*class\s+(\w+)\s*:\s*LF_Test\b/.exec(line);
    if (c) {
      cls = c[1];
      inGetName = false;
      continue;
    }
    if (!cls) continue;
    if (/\bstring\s+GetName\s*\(\s*\)/.test(line)) {
      inGetName = true;
      continue;
    }
    if (inGetName) {
      const r = /return\s+"([^"]+)"\s*;/.exec(line);
      if (r) {
        names[r[1]] = cls;
        inGetName = false;
      } else if (/^\s*}\s*$/.test(line)) inGetName = false;
    }
  }
  return names;
}

function eachTestFile(root, parse) {
  const dir = join(root, ...TESTS_DIR);
  const out = {};
  if (!existsSync(dir)) return out;
  for (const f of readdirSync(dir)) {
    if (f.endsWith(".c")) Object.assign(out, parse(readFileSync(join(dir, f), "utf8")));
  }
  return out;
}

export const testTiers = (root) => eachTestFile(root, parseTiers);
export const testNames = (root) => eachTestFile(root, parseNames);

// [{ name, reason }] from tools/test/world-pins.json: the tests that still need the playtest world.
export function readPins(root) {
  const f = join(root, ...PINS_FILE.split("/"));
  if (!existsSync(f)) return [];
  const j = JSON.parse(readFileSync(f, "utf8"));
  return Array.isArray(j.pins) ? j.pins : [];
}

// [{ name, reason }] from tools/test/town-tests.json: the tests that run only under `test --town`.
export function readTown(root) {
  const f = join(root, ...TOWN_FILE.split("/"));
  if (!existsSync(f)) return [];
  const j = JSON.parse(readFileSync(f, "utf8"));
  return Array.isArray(j.tests) ? j.tests : [];
}

// The registry without the town tests: what a suite run (a shard plan, a gate) selects from.
export function withoutTown(registry, { town, names }) {
  const out = new Set((town ?? []).map((p) => (names ?? {})[p.name] ?? p.name));
  return out.size ? registry.filter((t) => !out.has(t.cls)) : registry;
}

// The one boot of `test --town`: every town test, on the playtest world. null when the list is empty.
export function townLeg({ names, town, world }) {
  const classes = [...new Set((town ?? []).map((p) => (names ?? {})[p.name] ?? p.name))];
  if (!classes.length) return null;
  return { world: world ?? PLAYTEST_WORLD, classes, reason: `the town tests (${TOWN_FILE})` };
}

// { world, reason }: world is undefined for DebugWorld (the harness default), so a run that is not
// routed keeps the exact cache key and arguments it had before the routing existed.
export function pickWorld({ tier, only = [], world, env = process.env, tiers }) {
  if (world) return { world, reason: "--world" };
  const grid = (reason) => (env.LF_TEST_GRID_WORLD === "0" ? { world: undefined, reason: `${reason}; LF_TEST_GRID_WORLD=0` } : { world: GRID_WORLD, reason });
  const logic = (reason) => (env.LF_TEST_LOGIC_WORLD === "0" ? { world: undefined, reason: "LF_TEST_LOGIC_WORLD=0" } : { world: LOGIC_WORLD, reason });
  if (only.length) {
    const notLogic = only.filter((n) => tiers[n] !== "LOGIC");
    if (notLogic.length) return grid(`not LOGIC: ${notLogic.slice(0, 3).join(",")}${notLogic.length > 3 ? ",..." : ""}`);
    return logic("every selected test is LOGIC");
  }
  if (tier === "fast") return logic("the fast tier is LOGIC only");
  if (tier === "all" || tier === "persistence") return grid(`tier ${tier}`);
  return { world: undefined, reason: `tier ${tier}` };
}

// The boots of one run: [{ world, classes, reason }]. classes null means "the tier or --only as
// asked" (one boot, nothing split); otherwise it is the exact class list of that leg
// (--only-classes). A run routed to GridWorld whose selection holds a pinned test becomes a
// GridWorld leg without the pins and a DebugWorld leg with only them. A town test
// (tools/test/town-tests.json) never rides a tier run: GridWorld gets the tier's classes without it
// and no leg holds it; named with --only it runs on the playtest world like a pin. townRun is
// `test --town`: one PLAYTEST_WORLD leg of the list (townLeg). registry is [{ cls, tier }]
// (tools/lib/test-shards.mjs readRegistry); names is testNames(); town is readTown().
export function planLegs({ tier, only = [], world, env = process.env, tiers, names, registry, pins, town, townRun = false }) {
  if (townRun) {
    const leg = townLeg({ names, town, world });
    return leg ? [leg] : [];
  }
  const picked = pickWorld({ tier, only, world, env, tiers });
  const one = [{ world: picked.world, classes: null, reason: picked.reason }];
  if (world || picked.world !== GRID_WORLD) return one;
  const clsOf = (n) => (names ?? {})[n] ?? n;
  const pinned = new Set((pins ?? []).map((p) => clsOf(p.name)));
  const townSet = new Set((town ?? []).map((p) => clsOf(p.name)));
  if (!pinned.size && !townSet.size) return one;
  const selection = only.length ? [...new Set(only.map(clsOf))] : registry.filter((t) => tier === "all" || t.tier === tier.toUpperCase()).map((t) => t.cls);
  // A tier run leaves the town tests out; an --only run that names one keeps it for the playtest leg.
  const suite = only.length ? selection : selection.filter((c) => !townSet.has(c));
  const left = selection.length - suite.length;
  const toPlaytest = suite.filter((c) => pinned.has(c) || townSet.has(c));
  if (!toPlaytest.length && !left) return one;
  const toGrid = suite.filter((c) => !pinned.has(c) && !townSet.has(c));
  const legs = [];
  const apart = toPlaytest.length ? `; ${toPlaytest.length} pinned or town test(s) run apart` : "";
  const dropped = left ? `; ${left} town test(s) run only under --town` : "";
  if (toGrid.length) legs.push({ world: GRID_WORLD, classes: toGrid, reason: `${picked.reason}${apart}${dropped}` });
  if (toPlaytest.length) legs.push({ world: PLAYTEST_WORLD, classes: toPlaytest, reason: `pinned to the playtest world (${PINS_FILE}, ${TOWN_FILE})` });
  return legs;
}

// One verdict from the legs: [{ code, passed, failed, total }] -> the same shape, summed.
export function mergeLegs(results) {
  const out = { code: 0, passed: 0, failed: 0, total: 0 };
  for (const r of results) {
    if (r.code !== 0 && out.code === 0) out.code = r.code || 1;
    out.passed += r.passed ?? 0;
    out.failed += r.failed ?? (r.code === 0 ? 0 : 1);
    out.total += r.total ?? 0;
  }
  return out;
}

export function legsSummaryLine(tier, v, legs) {
  const where = legs.map((l) => (l.world ?? PLAYTEST_WORLD).replace(/^.*\/([^/]+)\.ent$/, "$1")).join("+");
  return `SUMMARY test ${v.code === 0 ? "ok" : "fail"} tier=${tier} legs=${legs.length} passed=${v.passed} failed=${v.failed} total=${v.total} worlds=${where}`;
}
