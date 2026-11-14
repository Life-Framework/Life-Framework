// Proves the trigger logic of the gate's pre-move save step, offline (the e2e run is stubbed).
// inputs: tools/**, addons/**, docs/**
// red-proof: with the Persistence/ rule dropped from preMoveRelevant this printed "a Persistence/ diff did not run the e2e".
import { PRE_MOVE_REF, preMoveRelevant, runPreMoveGate } from "../lib/pre-move-gate.mjs";

const fails = [];
const check = (ok, msg) => { if (!ok) fails.push(msg); };
const quiet = () => {};

async function run(files, exit) {
  let ran = null;
  const code = await runPreMoveGate(files, async (o) => { ran = o; return exit; }, quiet);
  return { ran, code };
}

let r = await run(["addons/LifeFramework/Scripts/Game/Persistence/LF_SaveStamp.c"], 0);
check(r.ran && r.ran.preMove === PRE_MOVE_REF && r.ran.mission === "playtest" && r.code === 0, "a Persistence/ diff did not run the e2e");
r = await run(["addons/LF_Everon1989/Missions/LF_PlaytestTown.conf"], 1);
check(r.ran !== null && r.code === 1, "a failing e2e did not exit non-zero");
r = await run(["docs/features.md", "website/src/a.tsx", "addons/LifeFramework/Prefabs/Props/Crate.et"], 1);
check(r.ran === null && r.code === 0, "an unrelated diff ran the e2e");
check(preMoveRelevant(["addons", "LF_Everon1989", "LF_Everon1989.gproj"].join(String.fromCharCode(92))), "a pack .gproj not a trigger (backslash path)");
check(preMoveRelevant("addons/LifeFramework/Scripts/Game/Feature/Bank/LF_BankPersistence.c"), "a *Persist*.c script not a trigger");
check(preMoveRelevant("tools/lib/addons.mjs"), "addons.mjs not a trigger");
check(!preMoveRelevant("addons/LifeFramework/Scripts/Game/Tests/LF_Test_Shop.c"), "an unrelated test triggered");
check(/^[0-9a-f]{7,40}$/.test(PRE_MOVE_REF), "PRE_MOVE_REF is not a commit hash");

if (fails.length) {
  console.log("pre-move-gate-selftest: FAIL");
  for (const f of fails) console.log("  " + f);
  process.exit(1);
}
console.log("pre-move-gate-selftest: OK (persistence/mission/boundary triggers, unrelated skips, failure exits 1)");
