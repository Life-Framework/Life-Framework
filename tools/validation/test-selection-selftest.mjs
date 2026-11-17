// tools/validation/test-selection-selftest.mjs
// inputs: tools/lib/test-world.mjs, tools/lib/worlds.mjs, tools/validation/test-selection-selftest.mjs
//
// Proves the split of an --only / --red selection between the core leg and the pack leg
// (tools/lib/test-world.mjs planLegs + splitSelection; docs/architecture/three-worlds.md "Pack test leg"):
//   core only   -> one core leg naming exactly those tests, no pack leg
//   pack only   -> the pack leg alone
//   mixed       -> a core leg of only the core names (LOGIC or WORLD, pinned or not) and a pack leg of only the pack names
//   unknown     -> splitSelection reports it, so `test` refuses instead of running everything
// 2026-11-17 (#2276): a mixed --only left the core leg at classes null, which cmdTest runs as the whole tier.
// Does not boot the game.
//
// red-proof: 2026-11-17, run against the tree before the change: FAIL "a mixed --only (LOGIC core test): the
// core leg names only the core test" and the WORLD and pinned variants; splitSelection was missing, so the
// unknown-name checks failed too. Green with the change: test-selection-selftest: OK.

import { join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const here = fileURLToPath(new URL(".", import.meta.url));
const load = async (p) => {
  try {
    return await import(pathToFileURL(join(here, p)).href);
  } catch {
    return {};
  }
};
const tw = await load("../lib/test-world.mjs");
const worlds = await load("../lib/worlds.mjs");
const failures = [];
const check = (ok, what) => {
  if (!ok) failures.push(what);
};
const fn = (name) => (typeof tw[name] === "function" ? tw[name] : () => undefined);

const tiers = { LF_Test_Logic: "LOGIC", LF_Test_World: "WORLD", LF_Test_Pinned: "WORLD", LF_Test_EveronA: "WORLD", LF_Test_EveronB: "PERSISTENCE" };
const names = { "logic/name": "LF_Test_Logic" };
const registry = [
  { cls: "LF_Test_Logic", tier: "LOGIC" },
  { cls: "LF_Test_World", tier: "WORLD" },
  { cls: "LF_Test_Pinned", tier: "WORLD" },
];
const pack = [
  { cls: "LF_Test_EveronA", tier: "WORLD" },
  { cls: "LF_Test_EveronB", tier: "PERSISTENCE" },
];
const MISSION = worlds.PACK_TEST_MISSION;
const plan = (o) => fn("planLegs")({ tier: "all", tiers, names, registry, pins: [], town: [], pack, env: {}, ...o }) ?? [];
const split = (only) => fn("splitSelection")({ only, names, tiers, registry, pack }) ?? { core: ["?"], pack: ["?"], unknown: ["?"] };
const list = (l) => (l?.classes ?? []).join(",");

// core only: one leg, the names as asked (classes null means "--only as asked") or named, never a pack leg
for (const [label, only, pins] of [["LOGIC", ["LF_Test_Logic"], []], ["WORLD", ["LF_Test_World"], []], ["pinned", ["LF_Test_World", "LF_Test_Pinned"], [{ name: "LF_Test_Pinned" }]]]) {
  const legs = plan({ only, pins });
  check(!legs.some((l) => l.world === MISSION), `core only (${label}): no pack leg`);
  const ran = legs.flatMap((l) => (l.classes ? l.classes : only));
  check(ran.join(",") === only.join(",") || [...ran].sort().join(",") === [...only].sort().join(","), `core only (${label}): legs run exactly ${only.join(",")}, got ${ran.join(",")}`);
}

// pack only
{
  const legs = plan({ only: ["LF_Test_EveronA"] });
  check(legs.length === 1 && legs[0].world === MISSION && list(legs[0]) === "LF_Test_EveronA", "pack only: the pack leg alone, with the named test");
}

// mixed: the core leg names ONLY the core tests, whichever world it lands on
for (const [label, core, pins] of [["LOGIC core test", "LF_Test_Logic", []], ["WORLD core test", "LF_Test_World", []], ["pinned core test", "LF_Test_Pinned", [{ name: "LF_Test_Pinned" }]]]) {
  for (const mode of ["only", "red"]) {
    const legs = plan({ only: [core, "LF_Test_EveronA"], pins });
    const coreLegs = legs.filter((l) => l.world !== MISSION);
    const packLegs = legs.filter((l) => l.world === MISSION);
    check(coreLegs.length >= 1 && coreLegs.every((l) => l.classes), `a mixed --${mode} (${label}): every core leg names its classes (a null leg runs the whole tier)`);
    check(coreLegs.flatMap((l) => l.classes ?? []).join(",") === core, `a mixed --${mode} (${label}): the core leg names only the core test`);
    check(packLegs.length === 1 && list(packLegs[0]) === "LF_Test_EveronA", `a mixed --${mode} (${label}): the pack leg names only the pack test`);
  }
}
{
  // a GetName() alias resolves, and two pack tests stay together
  const legs = plan({ only: ["logic/name", "LF_Test_EveronA", "LF_Test_EveronB"] });
  check(legs.filter((l) => l.world !== MISSION).flatMap((l) => l.classes ?? []).join(",") === "LF_Test_Logic", "a mixed --only by GetName(): the core leg holds the resolved class");
  check(list(legs.find((l) => l.world === MISSION)) === "LF_Test_EveronA,LF_Test_EveronB", "a mixed --only: both pack tests ride the pack leg");
}
// --core-only has no pack loaded: the pack name is not split away, nothing runs it
check(!plan({ only: ["LF_Test_Logic", "LF_Test_EveronA"], coreOnly: true }).some((l) => l.world === MISSION), "--core-only: no pack leg");

// splitSelection
{
  const s = split(["LF_Test_Logic", "logic/name", "LF_Test_EveronB", "LF_Test_World"]);
  check(s.core.join(",") === "LF_Test_Logic,LF_Test_World" && s.pack.join(",") === "LF_Test_EveronB" && s.unknown.length === 0, "splitSelection sorts core and pack names and folds a GetName() alias");
  check(split(["LF_Test_Nope"]).unknown.join(",") === "LF_Test_Nope", "an unknown name is reported, not dropped");
  const m = split(["LF_Test_Logic", "LF_Test_Nope", "LF_Test_EveronA"]);
  check(m.unknown.join(",") === "LF_Test_Nope" && m.core.join(",") === "LF_Test_Logic" && m.pack.join(",") === "LF_Test_EveronA", "an unknown name among known ones is still reported");
}

if (failures.length) {
  for (const f of failures) console.log(`FAIL  ${f}`);
  console.log(`test-selection-selftest: ${failures.length} failure(s)`);
  process.exit(1);
}
console.log("test-selection-selftest: OK");
