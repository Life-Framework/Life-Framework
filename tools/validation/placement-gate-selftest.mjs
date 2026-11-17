// Proves the trigger logic of the gate's placement step, offline (the audit run is stubbed).
// inputs: tools/**, agent-box/**, .githooks/**, addons/**, docs/**, website/**, video/**
// red-proof: with `.layer` dropped from placementRelevant this printed "a .layer diff did not run the audit"; the grid step
// (2026-11-17, #1814) was written before gridRelevant existed: the import failed, then the grid checks below passed.
import { gridRelevant, placementRelevant, runPlacementGate } from "../lib/placement-gate.mjs";

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

if (fails.length) {
  console.log("placement-gate-selftest: FAIL");
  for (const f of fails) console.log("  " + f);
  process.exit(1);
}
console.log("placement-gate-selftest: OK (layer triggers, docs skips, strict failure exits 1, grid step)");
