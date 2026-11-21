// Proves the trigger logic of the gate's placement step, offline (the audit run is stubbed).
// inputs: tools/**, agent-box/**, .githooks/**, addons/**, docs/**, website/**, video/**
// red-proof: with `.layer` dropped from placementRelevant this printed "a .layer diff did not run the audit"; the grid step
// (2026-11-17, #1814) was written before gridRelevant existed: the import failed, then the grid checks below passed.
// The island gate (#3888) was written first: the import of islandRelevant failed, then the island checks passed.
import { spawnSync } from "node:child_process";
import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { ISLAND_BASELINE, baselineGrowth, baselineText, findingKeys, gridRelevant, islandRelevant, judgeIsland, placementRelevant, readBaseline, runIslandGate, runPlacementGate, shrinkBaseline } from "../lib/placement-gate.mjs";

const fails = [];
const check = (ok, msg) => { if (!ok) fails.push(msg); };
const quiet = () => {};

async function run(files, exit) {
  let ran = null;
  const code = await runPlacementGate(files, async (a) => { ran = a; return exit; }, quiet);
  return { ran, code };
}

let r = await run(["addons/LifeFramework/Worlds/DebugWorld/DebugWorld_Layers/Street.layer"], 0);
check(r.ran && r.ran.includes("--strict") && r.code === 0, "a .layer diff did not run the audit");
r = await run(["docs/features.md", "website/src/a.tsx", "tools/cli.mjs"], 1);
check(r.ran === null && r.code === 0, "a docs-only diff ran the audit");
r = await run(["addons/LifeFramework/Prefabs/Props/Crate.et"], 1);
check(r.ran !== null && r.code === 1, "a strict failure did not exit non-zero");
check(placementRelevant("addons/LifeFramework/Scripts/Game/Core/LF_Utils.c"), "LF_Utils.c not a trigger");
check(!placementRelevant("addons/LifeFramework/Configs/a.conf"), "a .conf triggered");
check(!placementRelevant("addons/LifeFramework/Scripts/Game/Tests/LF_Test_X.c"), "a test triggered");

// The grid step: a GridWorld layer or a prefab runs `--grid --strict` and its failure fails the gate; a town layer does not.
async function runAll(files, exit) {
  const calls = [];
  const code = await runPlacementGate(files, async (a) => { calls.push(a); return exit(a); }, quiet);
  return { calls, code };
}
const GRID = "addons/LifeFramework/Worlds/GridWorld/GridWorld_Layers/Grid_x.layer";
const TOWN = "addons/LifeFramework/Worlds/DebugWorld/DebugWorld_Layers/Street.layer";
r = await runAll([GRID], () => 0);
check(r.calls.length === 2 && r.calls[1].includes("--grid") && r.calls[1].includes("--strict") && r.code === 0, "a GridWorld layer did not run the grid audit");
r = await runAll([GRID], (a) => (a.includes("--grid") ? 1 : 0));
check(r.code === 1, "a grid audit failure did not fail the gate");
r = await runAll([TOWN], () => 0);
check(r.calls.length === 1 && !r.calls[0].includes("--grid"), "a town layer ran the grid audit");
r = await runAll(["addons/LifeFramework/Prefabs/Props/Crate.et"], () => 0);
check(r.calls.length === 2 && r.calls[1].includes("--grid"), "a prefab did not run the grid audit");
check(gridRelevant("addons/LifeFramework/Worlds/GridWorld/GridWorld.ent") && !gridRelevant("addons/LifeFramework/Worlds/GridWorld/GridTerrain/x.layer") && !gridRelevant("docs/a.md"), "gridRelevant");

// The island gate (#3888): the trigger set, the hard and warn split, and a baseline that only shrinks.
const ISL = "addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Place_saint_philippe_bank.layer";
check(islandRelevant(ISL) === "layer", "an island layer is not an island trigger");
check(islandRelevant("tools/town/kits/bank.json") === "whole" && islandRelevant("tools/town/homes.mjs") === "whole", "a kit or generator does not audit the whole island");
check(islandRelevant("addons/LifeFramework/Prefabs/Systems/Vehicles/LF_ParkedCarMarker.et") === "whole" && islandRelevant("addons/LF_Everon1989/Scripts/Game/X/LF_CarSpawner.c") === "whole", "a marker or spawner does not audit the whole island");
check(islandRelevant("tools/town/homes.selftest.mjs") === null && islandRelevant("docs/placement-audit.md") === null && islandRelevant("addons/LifeFramework/Prefabs/Props/Crate.et") === null, "a selftest, doc or plain prefab triggers the island audit");
check(!placementRelevant(ISL), "an island layer runs the playtest-town audit");
r = await runAll([ISL], () => 0);
check(r.calls.length === 1 && r.calls[0].includes("MainWorld") && r.calls[0].includes("--baseline") && r.calls[0][r.calls[0].indexOf("--layer") + 1] === "Place_saint_philippe_bank.layer", `an island layer change did not run the island audit on its layer: ${JSON.stringify(r.calls)}`);
r = await runAll([ISL, "tools/town/kits/bank.json"], () => 0);
check(r.calls.length === 1 && !r.calls[0].includes("--layer"), "a kit change did not audit the whole island (runtime sweep)");
r = await runAll([ISL], (a) => (a.includes("MainWorld") ? 1 : 0));
check(r.code === 1, "an island audit failure did not fail the gate");
let isl = null;
check((await runIslandGate(["docs/a.md"], async (a) => { isl = a; return 1; }, quiet)) === 0 && isl === null, "a docs diff ran the island audit");

const row = (name, kind, layer = "Place_saint_philippe_bank.layer") => ({ name, layer, x: 1, y: 2, z: 3, problems: [{ kind, text: `${kind} text` }] });
const baseline = [findingKeys([row("Old_Lamp", "floating")])[0].key];
let j = judgeIsland([row("Old_Lamp", "floating")], [], baseline);
check(!j.fail && j.warn.length === 1 && j.lines.some((l) => l.startsWith("WARN") && l.includes("Old_Lamp")), "a baseline row did not warn");
j = judgeIsland([row("Old_Lamp", "floating"), row("New_Crate", "floating")], [], baseline);
check(j.fail && j.hard.length === 1 && j.lines.some((l) => l.startsWith("FAIL") && l.includes("New_Crate")), "a new floating row did not fail");
j = judgeIsland([row("Old_Lamp", "clip")], [], baseline);
check(j.fail, "a new problem kind on a baseline prop did not fail");
j = judgeIsland([], [{ kind: "duplicate", text: "duplicate A [x.layer] and B [x.layer]" }], []);
check(j.fail, "a new world finding did not fail");
check(baselineGrowth(["a", "b"], ["a", "b", "c"]).join() === "c" && baselineGrowth(["a", "b"], ["a"]).length === 0, "baselineGrowth");
check(shrinkBaseline(["a", "b"], ["b", "z"]).join() === "b", "shrinkBaseline adds or keeps a fixed row");
check(JSON.parse(baselineText(["b", "a", "a"])).rows.join() === "a,b", "baselineText sorts and dedupes");

// The committed baseline exists, parses, and holds no row origin/main lacks (it may only shrink).
const here = dirname(dirname(dirname(fileURLToPath(import.meta.url))));
const basePath = join(here, ...ISLAND_BASELINE.split("/"));
if (!existsSync(basePath)) fails.push(`${ISLAND_BASELINE} missing`);
else {
  const now = readBaseline(readFileSync(basePath, "utf8"));
  const main = spawnSync("git", ["show", `origin/main:${ISLAND_BASELINE}`], { cwd: here, encoding: "utf8", windowsHide: true });
  if (main.status === 0) {
    const grew = baselineGrowth(readBaseline(main.stdout), now);
    if (grew.length) fails.push(`${ISLAND_BASELINE} grew by ${grew.length} row(s) (${grew.slice(0, 3).join("; ")}): the baseline may only shrink; fix the new finding instead`);
  }
}

if (fails.length) {
  console.log("placement-gate-selftest: FAIL");
  for (const f of fails) console.log("  " + f);
  process.exit(1);
}
console.log("placement-gate-selftest: OK (layer triggers, docs skips, strict failure exits 1, grid step, island step, baseline only shrinks)");
