// Proves the test world routing (tools/lib/test-world.mjs): a LOGIC-only selection boots the
// minimal world Worlds/TestLogic, anything else boots DebugWorld, the tier parser agrees with the
// generated registry, the TestLogic runner is flagged, and the chosen world changes the pass
// cache key. Does not boot the game.
// inputs: tools/lib/test-world.mjs, tools/lib/test-shards.mjs, tools/lib/worlds.mjs, tools/test/world-pins.json, tools/test/town-tests.json, tools/lib/test-cache.mjs, tools/validation/test-world-selftest.mjs, addons/LifeFramework/Scripts/Game/Tests, addons/LifeFramework/Worlds/TestLogic
//
// red-proof: 2026-11-14, each break run through LF_TEST_WORLD_MODULE=<broken copy>:
//   the fast-tier branch never taken        -> FAIL "--tier fast boots TestLogic";
//   the not-LOGIC guard on --only removed    -> FAIL "--only with one WORLD test boots DebugWorld" (+3 more);
//   GetName() strings not mapped to a tier   -> FAIL "a real GetName() string resolves (tests/logic-world)".
// red-proof (#1895, town tests leave the suite), 2026-11-16: checks 2e and 2f first ran against the module
// as it stood before the town list (no `town` or `townRun` in planLegs, no readTown, townLeg or
// town-tests.json): 10 FAIL lines, among them
//   --tier all drops a town test from every leg (it was still in the GridWorld leg);
//   --tier all with no pins is one GridWorld leg without the town tests (it was one leg, classes null);
//   --town is one PLAYTEST_WORLD leg with the town tests (it was the ordinary GridWorld + DebugWorld plan);
//   tools/test/town-tests.json reads as a list of 12 (got undefined).
// Green after the change: test-world-selftest: OK.
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { pathToFileURL, fileURLToPath } from "node:url";

const here = fileURLToPath(new URL(".", import.meta.url));
const root = join(here, "..", "..");
const modPath = process.env.LF_TEST_WORLD_MODULE || join(here, "..", "lib", "test-world.mjs");
const tw = await import(pathToFileURL(modPath).href);
const worlds = await import(pathToFileURL(join(here, "..", "lib", "worlds.mjs")).href);
const tc = await import(pathToFileURL(join(here, "..", "lib", "test-cache.mjs")).href);

const failures = [];
// A function the module under test lacks reads as a failure of each check that uses it, not a crash.
const fn = (name) => (typeof tw[name] === "function" ? tw[name] : () => undefined);
const check = (ok, what) => {
  if (!ok) failures.push(what);
};

// 1. The parser: tier lines bind to the class below them, GetName() strings carry the class tier.
const sample = [
  "// tier: LOGIC",
  "class LF_Test_A : LF_Test",
  "{",
  "\toverride string GetName()",
  "\t{",
  '\t\treturn "unit/a";',
  "\t}",
  "}",
  "// tier: WORLD",
  "class LF_Test_B : LF_Test",
  "{",
  "\toverride string GetName()",
  "\t{",
  '\t\treturn "unit/b";',
  "\t}",
  "}",
  "class LF_Helper : LF_Test",
  "{",
  "}",
].join("\n");
const t = tw.parseTiers(sample);
check(t.LF_Test_A === "LOGIC" && t["unit/a"] === "LOGIC", "a LOGIC class and its GetName() parse as LOGIC");
check(t.LF_Test_B === "WORLD" && t["unit/b"] === "WORLD", "a WORLD class and its GetName() parse as WORLD");
check(t.LF_Helper === undefined, "a class with no tier line has no tier");
// 2. The routing: LOGIC to TestLogic, WORLD and PERSISTENCE to GridWorld (#1804), PERF to DebugWorld.
const tiers = { LF_Test_A: "LOGIC", "unit/a": "LOGIC", LF_Test_B: "WORLD", "unit/b": "WORLD", LF_Test_P: "PERSISTENCE", LF_Test_Q: "WORLD", "unit/q": "WORLD" };
const env = {};
const pick = (o) => tw.pickWorld({ tiers, env, ...o }).world;
check(pick({ tier: "fast" }) === tw.LOGIC_WORLD, "--tier fast boots TestLogic");
check(tw.GRID_WORLD === worlds.GRID_WORLD && tw.PLAYTEST_WORLD === worlds.PLAYTEST_WORLD && tw.LOGIC_WORLD === worlds.TEST_WORLD, "the worlds come from tools/lib/worlds.mjs");
check(pick({ tier: "all" }) === worlds.GRID_WORLD, "--tier all boots GridWorld");
check(pick({ tier: "persistence" }) === worlds.GRID_WORLD, "--tier persistence boots GridWorld");
check(pick({ tier: "perf" }) === undefined, "--tier perf boots DebugWorld");
check(pick({ tier: "all", only: ["LF_Test_A", "unit/a"] }) === tw.LOGIC_WORLD, "--only with LOGIC classes and names boots TestLogic");
check(pick({ tier: "all", only: ["LF_Test_A", "LF_Test_B"] }) === worlds.GRID_WORLD, "--only with one WORLD test boots GridWorld");
check(pick({ tier: "all", only: ["LF_Test_P"] }) === worlds.GRID_WORLD, "--only with a PERSISTENCE test boots GridWorld");
check(pick({ tier: "fast", only: ["unit/b"] }) === worlds.GRID_WORLD, "--tier fast --only <WORLD test> boots GridWorld");
check(pick({ tier: "all", only: ["no-such-test"] }) === worlds.GRID_WORLD, "--only with an unknown name boots GridWorld");
check(pick({ tier: "fast", world: "Worlds/X/X.ent" }) === "Worlds/X/X.ent", "an explicit --world wins");
check(tw.pickWorld({ tier: "fast", tiers, env: { LF_TEST_LOGIC_WORLD: "0" } }).world === undefined, "LF_TEST_LOGIC_WORLD=0 turns the LOGIC routing off");
const off = { LF_TEST_GRID_WORLD: "0" };
check(tw.pickWorld({ tier: "all", tiers, env: off }).world === undefined, "LF_TEST_GRID_WORLD=0 sends --tier all back to DebugWorld");
check(tw.pickWorld({ tier: "all", only: ["LF_Test_B"], tiers, env: off }).world === undefined, "LF_TEST_GRID_WORLD=0 sends a WORLD --only back to DebugWorld");
check(tw.pickWorld({ tier: "fast", tiers, env: off }).world === tw.LOGIC_WORLD, "LF_TEST_GRID_WORLD=0 leaves the fast tier on TestLogic");

// 2b. The legs: a pinned test runs in a second boot on DebugWorld; the rest stays on GridWorld.
const registry = [
  { cls: "LF_Test_A", tier: "LOGIC" },
  { cls: "LF_Test_B", tier: "WORLD" },
  { cls: "LF_Test_P", tier: "PERSISTENCE" },
  { cls: "LF_Test_Q", tier: "WORLD" },
];
const names = { "unit/a": "LF_Test_A", "unit/b": "LF_Test_B", "unit/q": "LF_Test_Q" };
const pins = [{ name: "unit/q", reason: "reads the town" }];
const legs = (o) => fn("planLegs")({ tiers, names, registry, pins, env, ...o });
const show = (ls) => JSON.stringify((ls ?? []).map((x) => [x.world ?? "default", x.classes]));
const eq = (a, b) => JSON.stringify(a) === JSON.stringify(b);
let l = legs({ tier: "all" });
check(l?.length === 2 && l[0].world === worlds.GRID_WORLD && eq(l[0].classes, ["LF_Test_A", "LF_Test_B", "LF_Test_P"]) && l[1].world === worlds.PLAYTEST_WORLD && eq(l[1].classes, ["LF_Test_Q"]), `--tier all with a pinned name splits into GridWorld + DebugWorld (got ${show(l)})`);
l = legs({ tier: "persistence" });
check(l?.length === 1 && l[0].world === worlds.GRID_WORLD && l[0].classes === null, `--tier persistence with no pinned PERSISTENCE test is one GridWorld boot (got ${show(l)})`);
l = legs({ tier: "all", only: ["unit/q"] });
check(l?.length === 1 && l[0].world === worlds.PLAYTEST_WORLD && eq(l[0].classes, ["LF_Test_Q"]), `--only <pinned name> is one DebugWorld boot (got ${show(l)})`);
l = legs({ tier: "all", only: ["LF_Test_B", "LF_Test_Q"] });
check(l?.length === 2 && eq(l[0].classes, ["LF_Test_B"]) && eq(l[1].classes, ["LF_Test_Q"]), `--only <WORLD>,<pinned class> splits (got ${show(l)})`);
l = legs({ tier: "all", pins: [] });
check(l?.length === 1 && l[0].world === worlds.GRID_WORLD && l[0].classes === null, `no pins: one GridWorld boot of the whole tier (got ${show(l)})`);
l = legs({ tier: "all", env: off });
check(l?.length === 1 && l[0].world === undefined && l[0].classes === null, `LF_TEST_GRID_WORLD=0: one DebugWorld boot, no split (got ${show(l)})`);
l = legs({ tier: "all", world: "Worlds/X/X.ent" });
check(l?.length === 1 && l[0].world === "Worlds/X/X.ent" && l[0].classes === null, `--world wins over the pins (got ${show(l)})`);
l = legs({ tier: "fast" });
check(l?.length === 1 && l[0].world === tw.LOGIC_WORLD && l[0].classes === null, `--tier fast is one TestLogic boot (got ${show(l)})`);
l = legs({ tier: "perf" });
check(l?.length === 1 && l[0].world === undefined && l[0].classes === null, `--tier perf is one DebugWorld boot (got ${show(l)})`);

// 2c. One verdict from both boots.
const v = fn("mergeLegs")([{ code: 0, passed: 10, failed: 0, total: 10 }, { code: 1, passed: 2, failed: 1, total: 3 }]);
check(v?.code === 1 && v.passed === 12 && v.failed === 1 && v.total === 13, `a failing leg fails the merged verdict (got ${JSON.stringify(v)})`);
const v2 = fn("mergeLegs")([{ code: 0, passed: 10, failed: 0, total: 10 }, { code: 0, passed: 3, failed: 0, total: 3 }]);
check(v2?.code === 0 && v2.passed === 13 && v2.total === 13, `two green legs merge green (got ${JSON.stringify(v2)})`);
check(/^SUMMARY test ok tier=all legs=2 passed=13 failed=0 total=13\b/.test(fn("legsSummaryLine")("all", v2 ?? {}, [{ world: worlds.GRID_WORLD }, { world: worlds.PLAYTEST_WORLD }])), "the merged SUMMARY line reads ok with both counts summed");

// 2d. The real pin list: every pin has a reason and names a registered WORLD or PERSISTENCE test.
const realPins = fn("readPins")(root);
const realNames = fn("testNames")(root) ?? {};
const realTiers = tw.testTiers(root);
check(Array.isArray(realPins), "tools/test/world-pins.json reads as a list (it ends empty when the last test gains grid fixtures)");
for (const p of realPins ?? []) {
  check(typeof p.reason === "string" && p.reason.length > 5, `pin ${p.name} has a reason`);
  const cls = realNames[p.name] ?? (realTiers[p.name] ? p.name : undefined);
  check(!!cls, `pin ${p.name} names a registered test`);
  check(cls && (realTiers[cls] === "WORLD" || realTiers[cls] === "PERSISTENCE"), `pin ${p.name} is a WORLD or PERSISTENCE test (got ${cls && realTiers[cls]})`);
}

// 2e. The town tests (tools/test/town-tests.json) are about the town itself, so no suite run carries
// them; `test --town` runs them in one boot of the playtest world.
const regT = [...registry, { cls: "LF_Test_T", tier: "WORLD" }, { cls: "LF_Test_U", tier: "PERSISTENCE" }];
const namesT = { ...names, "unit/t": "LF_Test_T", "unit/u": "LF_Test_U" };
const tiersT = { ...tiers, LF_Test_T: "WORLD", "unit/t": "WORLD", LF_Test_U: "PERSISTENCE", "unit/u": "PERSISTENCE" };
const town = [{ name: "unit/t", reason: "reads the town" }, { name: "unit/u", reason: "reads the town" }];
const tlegs = (o) => fn("planLegs")({ tiers: tiersT, names: namesT, registry: regT, pins, town, env, ...o });
l = tlegs({ tier: "all" });
check(l?.length === 2 && l[0].world === worlds.GRID_WORLD && eq(l[0].classes, ["LF_Test_A", "LF_Test_B", "LF_Test_P"]) && l[1].world === worlds.PLAYTEST_WORLD && eq(l[1].classes, ["LF_Test_Q"]), `--tier all drops a town test from every leg (got ${show(l)})`);
l = tlegs({ tier: "all", pins: [] });
check(l?.length === 1 && l[0].world === worlds.GRID_WORLD && eq(l[0].classes, ["LF_Test_A", "LF_Test_B", "LF_Test_P", "LF_Test_Q"]), `--tier all with no pins is one GridWorld leg without the town tests, and no DebugWorld leg (got ${show(l)})`);
l = tlegs({ tier: "persistence", pins: [] });
check(l?.length === 1 && l[0].world === worlds.GRID_WORLD && eq(l[0].classes, ["LF_Test_P"]), `--tier persistence drops a PERSISTENCE town test (got ${show(l)})`);
l = tlegs({ tier: "all", town: [], pins: [] });
check(l?.length === 1 && l[0].world === worlds.GRID_WORLD && l[0].classes === null, `an empty town list changes nothing (got ${show(l)})`);
l = tlegs({ tier: "all", townRun: true });
check(l?.length === 1 && l[0].world === worlds.PLAYTEST_WORLD && eq(l[0].classes, ["LF_Test_T", "LF_Test_U"]), `--town is one PLAYTEST_WORLD leg with the town tests, and nothing else (got ${show(l)})`);
l = tlegs({ tier: "all", townRun: true, env: off });
check(l?.length === 1 && l[0].world === worlds.PLAYTEST_WORLD && eq(l[0].classes, ["LF_Test_T", "LF_Test_U"]), `--town ignores LF_TEST_GRID_WORLD=0 (got ${show(l)})`);
l = tlegs({ tier: "all", only: ["unit/t"], pins: [] });
check(l?.length === 1 && l[0].world === worlds.PLAYTEST_WORLD && eq(l[0].classes, ["LF_Test_T"]), `--only <town test> is one PLAYTEST_WORLD leg (got ${show(l)})`);
l = tlegs({ tier: "all", only: ["LF_Test_B", "unit/t"], pins: [] });
check(l?.length === 2 && eq(l[0].classes, ["LF_Test_B"]) && l[1].world === worlds.PLAYTEST_WORLD && eq(l[1].classes, ["LF_Test_T"]), `--only <WORLD>,<town test> splits (got ${show(l)})`);
l = tlegs({ tier: "all", env: off, pins: [] });
check(l?.length === 1 && l[0].world === undefined && l[0].classes === null, `LF_TEST_GRID_WORLD=0 is the rollback: one DebugWorld boot of everything (got ${show(l)})`);
l = tlegs({ tier: "fast", pins: [] });
check(l?.length === 1 && l[0].world === tw.LOGIC_WORLD && l[0].classes === null, `--tier fast is untouched by the town list (got ${show(l)})`);
const tl = fn("townLeg")({ names: namesT, town });
check(tl?.world === worlds.PLAYTEST_WORLD && eq(tl.classes, ["LF_Test_T", "LF_Test_U"]), `townLeg is the PLAYTEST_WORLD leg of the list (got ${JSON.stringify(tl)})`);

// 2f. The real list: each town test is registered, WORLD or PERSISTENCE, not also pinned, and the
// shard registry (what ci, wt gate and a sharded --tier all run) leaves it out.
const realTown = fn("readTown")(root);
const realPinClasses = new Set((realPins ?? []).map((p) => realNames[p.name] ?? p.name));
const wantTown = ["signs/coverage", "e2e/narrative-loop", "e2e/pass1-write-save", "e2e/roadmap-contraband", "e2e/roadmap-jailbreak", "e2e/roadmap-jailing", "roadmap/contraband", "roadmap/escort", "roadmap/hideout", "roadmap/jailbreak", "roadmap/jailing", "roadmap/jailing-save"];
check(Array.isArray(realTown) && realTown.length === 12, `tools/test/town-tests.json reads as a list of 12 (got ${realTown?.length})`);
check(eq((realTown ?? []).map((p) => p.name).sort(), [...wantTown].sort()), "the town list names the 12 tests of slice #1895");
const shardsMod = await import(pathToFileURL(join(here, "..", "lib", "test-shards.mjs")).href);
const suiteRegistry = shardsMod.readRegistry(root);
const suiteClasses = new Set(suiteRegistry.map((x) => x.cls));
for (const p of realTown ?? []) {
  check(typeof p.reason === "string" && p.reason.length > 5, `town test ${p.name} has a reason`);
  const cls = realNames[p.name] ?? (realTiers[p.name] ? p.name : undefined);
  check(!!cls, `town test ${p.name} names a registered test`);
  check(cls && (realTiers[cls] === "WORLD" || realTiers[cls] === "PERSISTENCE"), `town test ${p.name} is a WORLD or PERSISTENCE test (got ${cls && realTiers[cls]})`);
  check(cls && !realPinClasses.has(cls), `town test ${p.name} is not also in world-pins.json`);
  check(cls && !suiteClasses.has(cls), `town test ${p.name} is not in the shard registry that ci and wt gate run`);
}
const townClasses = new Set((realTown ?? []).map((p) => realNames[p.name] ?? p.name));
const realLegs = fn("planLegs")({ tier: "all", tiers: realTiers, names: realNames, registry: suiteRegistry, pins: realPins, town: realTown, env }) ?? [];
// classes null is the whole shard registry (the plan once world-pins.json is empty), which 2f checks holds no town test.
check(realLegs.length > 0 && realLegs.every((x) => x.classes === null || (x.classes.length > 0 && !x.classes.some((c) => townClasses.has(c)))), "the real --tier all plan has no town test in any leg");

// 3. The parser agrees with the generated registry on the real tests.
const real = tw.testTiers(root);
const reg = readFileSync(join(root, "addons/LifeFramework/Scripts/Game/Tests/LF_TestRegistrations.generated.c"), "utf8");
let regCount = 0;
for (const m of reg.matchAll(/Register\(new (\w+)\(\), LF_TestTier\.(LOGIC|WORLD|PERSISTENCE)\)/g)) {
  regCount++;
  check(real[m[1]] === m[2], `the parser gives ${m[1]} tier ${m[2]} like the registry (got ${real[m[1]]})`);
}
check(regCount > 100, `the registry lists the suite (${regCount} registrations)`);
check(real["tests/logic-world"] === "LOGIC", "a real GetName() string resolves (tests/logic-world)");

// 4. The world: the files exist, the runner is flagged, nothing but the allowed entities stands in it.
const worldDir = join(root, "addons/LifeFramework/Worlds/TestLogic");
check(existsSync(join(worldDir, "TestLogic.ent")) && existsSync(join(worldDir, "TestLogic.ent.meta")), "TestLogic.ent and its meta exist");
const layer = join(root, tw.LOGIC_WORLD_LAYER);
check(existsSync(layer) && /LF_TestRunnerComponent "\{[0-9A-F]{16}\}" \{\s*m_bLogicWorld 1/.test(readFileSync(layer, "utf8")), "the TestLogic runner sets m_bLogicWorld 1");
const allowed = new Set(["GenericWorldEntity", "SCR_CameraManager", "TimeAndWeatherManagerEntity", "LF_GameModeRoleplay", "SCR_MPDestructionManager", "GenericEntity"]);
const layersDir = join(worldDir, "TestLogic_Layers");
for (const f of existsSync(layersDir) ? readdirSync(layersDir) : []) {
  for (const m of readFileSync(join(layersDir, f), "utf8").matchAll(/^(\w+)\b/gm)) {
    check(allowed.has(m[1]), `TestLogic ${f} holds only test infrastructure (found ${m[1]})`);
  }
}

// 5. The chosen world is in the pass cache key.
const keyOf = (world) => tc.cacheKey(tc.keyInputs(root, { tier: "fast", world, build: "b", addons: ["LifeFramework"], trees: { addons: "x" }, rdb: { LifeFramework: "y" } }));
check(keyOf(tw.LOGIC_WORLD) !== keyOf(undefined), "a TestLogic run and a DebugWorld run have different pass cache keys");
check(new Set([keyOf(worlds.GRID_WORLD), keyOf(worlds.PLAYTEST_WORLD), keyOf(undefined)]).size === 3, "the GridWorld leg and the DebugWorld leg have their own pass cache keys");

for (const f of failures) console.log(`FAIL  ${f}`);
console.log(failures.length ? `test-world-selftest: ${failures.length} failure(s)` : "test-world-selftest: OK");
process.exit(failures.length ? 1 : 0);
