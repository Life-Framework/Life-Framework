import { arrive, clientBox, sessionDir } from "../session.mjs";

// M2 #411 on a running `up --clients 2` session: client 1 has no pick and is told so, is handed a
// lockpick (dev gate, standing in for the fence), works the lock-up door and walks in once it gives.
// Client 2 stands by the door as a witness. The attempt takes up to 20 s at level 1. Confirm
// `[LFDebug:Lockpick] ... OPENED` and a `[LFDebug:Wanted] crime report ... Lock picking` line in the server log.
async function runLockpickProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  on(2, { verb: "stand", entityName: "Roadmap_Lockpick_Door" });
  on(1, { verb: "stand", entityName: "Roadmap_Lockpick_Door" });
  wait(2500);
  on(1, { verb: "look" });
  on(1, { verb: "capture", mode: "ui", name: "m2_411_door_before_client1" }, 25000);
  const noPick = on(1, { verb: "use", className: "LF_LockpickAction", entityName: "Roadmap_Lockpick_Door" }, 10000);
  const give = on(1, { verb: "lockpick" }, 10000);
  wait(1500);
  const pick = on(1, { verb: "use", className: "LF_LockpickAction", entityName: "Roadmap_Lockpick_Door" }, 10000);
  wait(23000);
  on(1, { verb: "capture", mode: "ui", name: "m2_411_door_picked_client1" }, 25000);
  on(2, { verb: "capture", mode: "ui", name: "m2_411_door_picked_client2" }, 25000);
  if (noPick.status === "ok") fails.push(`the pick started without a pick: ${noPick.detail}`);
  if (give.status !== "ok" || !give.detail.includes("added=5")) fails.push(`lockpick not given: ${give.status} ${give.detail}`);
  if (pick.status !== "ok") fails.push(`pick returned ${pick.status} ${pick.detail}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "lockpick proof: FAIL" : "lockpick proof: PASS (refused without a pick, picked with one; confirm the server log for the result and the crime report)");
  return fails.length ? 1 : 0;
}

export default {
  name: "lockpick-proof",
  usage: "lockpick-proof",
  run: (ctx) => runLockpickProof(),
};
