// Proves the test world routing (tools/lib/test-world.mjs): a LOGIC-only selection boots the
// minimal world Worlds/TestLogic, anything else boots DebugWorld, the tier parser agrees with the
// generated registry, the TestLogic runner is flagged, and the chosen world changes the pass
// cache key. Does not boot the game.
// inputs: tools/lib/test-world.mjs, tools/lib/test-cache.mjs, tools/validation/test-world-selftest.mjs, addons/LifeFramework/Scripts/Game/Tests, addons/LifeFramework/Worlds/TestLogic
//
// red-proof: 2026-11-14, each break run through LF_TEST_WORLD_MODULE=<broken copy>:
//   the fast-tier branch never taken        -> FAIL "--tier fast boots TestLogic";
//   the not-LOGIC guard on --only removed    -> FAIL "--only with one WORLD test boots DebugWorld" (+3 more);
//   GetName() strings not mapped to a tier   -> FAIL "a real GetName() string resolves (tests/logic-world)".
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { pathToFileURL, fileURLToPath } from "node:url";

const here = fileURLToPath(new URL(".", import.meta.url));
const root = join(here, "..", "..");
const modPath = process.env.LF_TEST_WORLD_MODULE || join(here, "..", "lib", "test-world.mjs");
const tw = await import(pathToFileURL(modPath).href);
const tc = await import(pathToFileURL(join(here, "..", "lib", "test-cache.mjs")).href);

const failures = [];
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

// 2. The routing.
const tiers = { LF_Test_A: "LOGIC", "unit/a": "LOGIC", LF_Test_B: "WORLD", "unit/b": "WORLD", LF_Test_P: "PERSISTENCE" };
const env = {};
const pick = (o) => tw.pickWorld({ tiers, env, ...o }).world;
check(pick({ tier: "fast" }) === tw.LOGIC_WORLD, "--tier fast boots TestLogic");
check(pick({ tier: "all" }) === undefined, "--tier all boots DebugWorld");
check(pick({ tier: "persistence" }) === undefined, "--tier persistence boots DebugWorld");
check(pick({ tier: "all", only: ["LF_Test_A", "unit/a"] }) === tw.LOGIC_WORLD, "--only with LOGIC classes and names boots TestLogic");
check(pick({ tier: "all", only: ["LF_Test_A", "LF_Test_B"] }) === undefined, "--only with one WORLD test boots DebugWorld");
check(pick({ tier: "all", only: ["LF_Test_P"] }) === undefined, "--only with a PERSISTENCE test boots DebugWorld");
check(pick({ tier: "fast", only: ["unit/b"] }) === undefined, "--tier fast --only <WORLD test> boots DebugWorld");
check(pick({ tier: "all", only: ["no-such-test"] }) === undefined, "--only with an unknown name boots DebugWorld");
check(pick({ tier: "fast", world: "Worlds/X/X.ent" }) === "Worlds/X/X.ent", "an explicit --world wins");
check(tw.pickWorld({ tier: "fast", tiers, env: { LF_TEST_LOGIC_WORLD: "0" } }).world === undefined, "LF_TEST_LOGIC_WORLD=0 turns the routing off");

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

for (const f of failures) console.log(`FAIL  ${f}`);
console.log(failures.length ? `test-world-selftest: ${failures.length} failure(s)` : "test-world-selftest: OK");
process.exit(failures.length ? 1 : 0);
