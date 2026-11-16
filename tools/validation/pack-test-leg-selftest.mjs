// tools/validation/pack-test-leg-selftest.mjs
// inputs: tools/lib/test-world.mjs, tools/lib/e2e-legs.mjs, tools/lib/worlds.mjs, tools/validation/validate-addon-boundary.mjs, tools/validation/gen-test-registry.ps1, tools/validation/pack-test-leg-selftest.mjs, addons/LF_Everon1989/Scripts, addons/LF_Everon1989/Missions, addons/LifeFramework/Scripts/Game/Tests
//
// Proves the pack test leg (docs/architecture/three-worlds.md "Pack test leg"): tests in the pack's
// Scripts/Game/Tests run in a leg of their own, booted under the pack test header (GridWorld under the
// pack's LF_Everon1989.conf, so its config overlays apply). Checks:
//   1. planLegs: --tier all adds the pack leg and keeps the pack tests out of every core leg; --only of a
//      pack test is the pack leg alone; a mixed --only splits; --core-only, --tier fast and --world add none.
//   2. planE2eLegs: a PERSISTENCE pack test gets an e2e leg on the pack test header.
//   3. validate-addon-boundary: a pack test file and the pack registry pass; a pack test file declaring a
//      modded core class, other pack script, and a core script naming a pack test still fail.
//   4. The repo: the pack header inherits the flagship header and boots GridWorld hidden from Scenarios,
//      the pack registry lists the pack tests, and the core registry names none of them.
// Does not boot the game.
//
// red-proof: 2026-11-16, run against the tree before the change (tools/lib and validate-addon-boundary.mjs
// stashed): 18 FAIL lines, among them "--tier all has one pack leg", "a pack test and the pack registry pass
// the boundary", "e2e: the PERSISTENCE pack test gets a leg on the pack test header" and "the modded LF_TestManager
// is not a pack class". Green with the change: pack-test-leg-selftest: OK.

import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const here = fileURLToPath(new URL(".", import.meta.url));
const root = join(here, "..", "..");
const load = async (p) => {
  try {
    return await import(pathToFileURL(join(here, p)).href);
  } catch {
    return {};
  }
};
const tw = await load("../lib/test-world.mjs");
const e2e = await load("../lib/e2e-legs.mjs");
const worlds = await load("../lib/worlds.mjs");
const boundary = await load("./validate-addon-boundary.mjs");

const failures = [];
const check = (ok, what) => {
  if (!ok) failures.push(what);
};
const fn = (mod, name) => (typeof mod[name] === "function" ? mod[name] : () => undefined);

const MISSION = worlds.PACK_TEST_MISSION;
check(typeof MISSION === "string" && /^\{[0-9A-F]{16}\}Missions\/\w+\.conf$/.test(MISSION), "worlds.mjs exports PACK_TEST_MISSION as a {GUID}Missions/*.conf");

// 1. planLegs
const tiers = { LF_Test_A: "LOGIC", LF_Test_B: "WORLD", LF_Test_EveronX: "WORLD", "everon/x": "WORLD", LF_Test_EveronP: "PERSISTENCE" };
const names = { "everon/x": "LF_Test_EveronX" };
const registry = [
  { cls: "LF_Test_A", tier: "LOGIC" },
  { cls: "LF_Test_B", tier: "WORLD" },
];
const pack = [
  { cls: "LF_Test_EveronX", tier: "WORLD" },
  { cls: "LF_Test_EveronP", tier: "PERSISTENCE" },
];
const plan = (o) => fn(tw, "planLegs")({ tiers, names, registry, pins: [], town: [], pack, env: {}, ...o }) ?? [];
const packLegs = (legs) => legs.filter((l) => l.world === MISSION);
const coreClasses = (legs) => legs.filter((l) => l.world !== MISSION).flatMap((l) => l.classes ?? []);

{
  const legs = plan({ tier: "all" });
  const p = packLegs(legs);
  check(p.length === 1, "--tier all has one pack leg");
  check(p[0]?.classes?.join(",") === "LF_Test_EveronX,LF_Test_EveronP", "the --tier all pack leg holds every pack test");
  check(legs.length === 2, "--tier all is a core leg plus the pack leg");
  check(!coreClasses(legs).some((c) => c.startsWith("LF_Test_Everon")), "no core leg names a pack test");
}
{
  const legs = plan({ tier: "all", only: ["everon/x"] });
  check(legs.length === 1 && legs[0].world === MISSION && legs[0].classes?.join(",") === "LF_Test_EveronX", "--only <pack test GetName()> is the pack leg alone");
}
{
  const legs = plan({ tier: "all", only: ["LF_Test_B", "LF_Test_EveronX"] });
  check(packLegs(legs).length === 1 && packLegs(legs)[0].classes?.join(",") === "LF_Test_EveronX", "a mixed --only puts the pack test in the pack leg");
  const core = legs.filter((l) => l.world !== MISSION);
  check(core.length === 1 && core[0].world === worlds.GRID_WORLD, "a mixed --only keeps the core test on GridWorld");
}
check(packLegs(plan({ tier: "persistence" }))[0]?.classes?.join(",") === "LF_Test_EveronP", "--tier persistence runs the PERSISTENCE pack tests in the pack leg");
check(packLegs(plan({ tier: "all", coreOnly: true })).length === 0, "--core-only has no pack leg");
check(packLegs(plan({ tier: "fast" })).length === 0, "--tier fast has no pack leg");
check(packLegs(plan({ tier: "all", world: worlds.PLAYTEST_WORLD })).length === 0, "--world is never split for the pack");
check(plan({ tier: "all", pack: [] }).length === 1 && plan({ tier: "all", pack: [] })[0].classes === null, "no pack tests: the plan is as before (one leg, classes null)");
check(/worlds=GridWorld\+LF_Everon1989_Test/.test(fn(tw, "legsSummaryLine")("all", { code: 0, passed: 1, failed: 0, total: 1 }, [{ world: worlds.GRID_WORLD }, { world: MISSION ?? "" }]) ?? ""), "the summary line names the pack leg by its header");

// 2. planE2eLegs
{
  const legs = fn(e2e, "planE2eLegs")({ registry: [{ cls: "LF_Test_Save", tier: "PERSISTENCE" }], names: {}, pins: [], town: [], mission: "{X}M.conf", pack, env: {} }) ?? [];
  const p = legs.filter((l) => l.mission === MISSION);
  check(p.length === 1 && p[0].classes?.join(",") === "LF_Test_EveronP", "e2e: the PERSISTENCE pack test gets a leg on the pack test header");
  check(!legs.some((l) => l.mission !== MISSION && (l.classes ?? []).includes("LF_Test_EveronP")), "e2e: no core leg names the pack test");
  const pre = fn(e2e, "planE2eLegs")({ registry: [{ cls: "LF_Test_Save", tier: "PERSISTENCE" }], names: {}, pins: [], town: [], mission: "{X}M.conf", pack, preMove: true, env: {} }) ?? [];
  check(!pre.some((l) => l.mission === MISSION), "e2e --pre-move keeps the playtest leg only");
}

// 3. validate-addon-boundary on fixture trees
function put(base, path, text) {
  const full = join(base, path);
  mkdirSync(dirname(full), { recursive: true });
  writeFileSync(full, text);
}
const CORE = boundary.CORE ?? "LifeFramework";
const PACK = boundary.PACK ?? "LF_Everon1989";
const TEST_OK = `// tier: WORLD\nclass LF_Test_EveronX : LF_Test\n{\n}\n`;
const REG_OK = `modded class LF_TestManager\n{\n\tprotected override void RegisterPackTests()\n\t{\n\t\tRegisterPack(new LF_Test_EveronX(), LF_TestTier.WORLD);\n\t}\n}\n`;
function tree(extra) {
  const r = mkdtempSync(join(tmpdir(), "lf-packleg-"));
  put(r, `${CORE}/Scripts/Game/Tests/LF_TestManager.c`, `class LF_TestManager\n{\n}\n`);
  put(r, `${PACK}/Scripts/Game/Tests/LF_Test_EveronX.c`, TEST_OK);
  put(r, `${PACK}/Scripts/Game/Tests/LF_PackTestRegistrations.generated.c`, REG_OK);
  extra?.(r);
  return r;
}
const roots = [];
const errorsOf = (extra) => {
  const r = tree(extra);
  roots.push(r);
  return fn(boundary, "checkBoundary")(r) ?? ["checkBoundary missing"];
};
try {
  const clean = errorsOf();
  check(clean.length === 0, `a pack test and the pack registry pass the boundary (got: ${clean.join("; ")})`);
  check(errorsOf((r) => put(r, `${PACK}/Scripts/Game/Tests/LF_Test_EveronY.c`, `// tier: WORLD\nclass LF_Test_EveronY : LF_Test\n{\n}\nmodded class SCR_BaseGameMode\n{\n}\n`)).some((e) => /pack test file declares modded SCR_BaseGameMode/.test(e)), "a pack test file that mods a core class fails");
  check(errorsOf((r) => put(r, `${PACK}/Scripts/Game/Tests/LF_Test_EveronZ.c`, `class LFE_Helper\n{\n}\n`)).some((e) => /pack test file declares LFE_Helper/.test(e)), "a pack test file with a non-test class fails");
  check(errorsOf((r) => put(r, `${PACK}/Scripts/Game/Tests/LF_PackTestRegistrations.generated.c`, `class LFE_Sneak\n{\n}\n`)).some((e) => /pack test registry declares LFE_Sneak/.test(e)), "a pack registry declaring anything but the modded LF_TestManager fails");
  check(errorsOf((r) => put(r, `${PACK}/Scripts/Game/LFE_Glue.c`, `class LFE_Glue\n{\n}\n`)).some((e) => /LFE_Glue\.c: script in the pack/.test(e)), "other pack script still fails");
  check(errorsOf((r) => put(r, `${CORE}/Scripts/Game/Tests/LF_Uses.c`, `class LF_Uses\n{\n\tref LF_Test_EveronX m_T;\n}\n`)).some((e) => /uses pack class LF_Test_EveronX/.test(e)), "a core script naming a pack test still fails");
  check(!errorsOf().some((e) => /uses pack class LF_TestManager/.test(e)), "the modded LF_TestManager is not a pack class");
} finally {
  // Fixture trees are plain files under the OS temp dir: no junctions.
  for (const r of roots) rmSync(r, { recursive: true, force: true });
}

// 4. The repo
const packDir = join(root, "addons", "LF_Everon1989");
if (existsSync(packDir)) {
  const header = join(packDir, "Missions", "LF_Everon1989_Test.conf");
  const text = existsSync(header) ? readFileSync(header, "utf8") : "";
  const flagMeta = readFileSync(join(packDir, "Missions", "LF_Everon1989.conf.meta"), "utf8").match(/\{([0-9A-F]{16})\}/)?.[1];
  check(new RegExp(`^LF_MissionHeader : "\\{${flagMeta}\\}Missions/LF_Everon1989\\.conf"`).test(text), "the pack test header inherits LF_Everon1989.conf (one overlay list)");
  check(/World "\{2B35216B7F782445\}Worlds\/GridWorld\/GridWorld\.ent"/.test(text), "the pack test header boots GridWorld (its test runner)");
  check(/m_bShowInScenarioMenu 0/.test(text), "the pack test header is hidden from Scenarios");
  check(!/m_aLF_ConfigOverlays/.test(text), "the pack test header does not copy the overlay list");
  const meta = existsSync(`${header}.meta`) ? readFileSync(`${header}.meta`, "utf8") : "";
  check(MISSION && meta.includes(`Name "${MISSION}"`), "PACK_TEST_MISSION matches the header's .meta");
  const packTests = fn(tw, "readPackTests")(root) ?? [];
  check(packTests.some((t) => t.cls === "LF_Test_EveronDrinksImpair" && t.tier === "WORLD"), "the pack registry lists LF_Test_EveronDrinksImpair (WORLD)");
  const coreReg = readFileSync(join(root, "addons", "LifeFramework", "Scripts", "Game", "Tests", "LF_TestRegistrations.generated.c"), "utf8");
  check(packTests.length > 0 && !packTests.some((t) => coreReg.includes(t.cls)), "the core registry names no pack test");
  const tiersAll = fn(tw, "testTiers")(root) ?? {};
  check(tiersAll["everon/drinks-impair"] === "WORLD", "testTiers resolves a pack test's GetName()");
}

if (failures.length) {
  for (const f of failures) console.log(`FAIL  ${f}`);
  console.log(`pack-test-leg-selftest: FAIL (${failures.length})`);
  process.exit(1);
}
console.log("pack-test-leg-selftest: OK (pack leg planned for test and e2e, boundary allows pack tests only, header inherits the flagship)");
