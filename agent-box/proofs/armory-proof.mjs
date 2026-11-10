import { arrive, clientBox, sessionDir } from "../session.mjs";

// M2 #275 on a running `up --clients 2` session with client 2 on the roster (`session.mjs roster 2`
// before up): client 2 clocks in; client 1 stands at the armory store and is refused without a
// keycard, is handed a stamped keycard (dev gate, standing in for the fence), and breaches it. Three
// serial-stamped weapons lie in front of the store and the loud dispatch reaches the officer.
// Confirm `[LFDebug:Bank] heist Roadmap_Police_ArmoryStore paid 3 weapons` and `loud alarm sounding` in the server log.
async function runArmoryProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  on(2, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  on(2, { verb: "look" });
  const clockIn = on(2, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000);
  wait(3000);
  on(1, { verb: "stand", entityName: "Roadmap_Police_ArmoryStore" });
  wait(2500);
  const noCard = on(1, { verb: "use", className: "LF_HeistRobAction", entityName: "Roadmap_Police_ArmoryStore" }, 10000);
  const give = on(1, { verb: "keycard" }, 10000);
  wait(1500);
  const rob = on(1, { verb: "use", className: "LF_HeistRobAction", entityName: "Roadmap_Police_ArmoryStore" }, 10000);
  wait(3000);
  on(1, { verb: "capture", mode: "ui", name: "m2_275_armory_opened_client1" }, 25000);
  on(2, { verb: "capture", mode: "ui", name: "m2_275_dispatch_client2" }, 25000);
  if (clockIn.status !== "ok") fails.push(`clock in returned ${clockIn.status} ${clockIn.detail}`);
  if (noCard.status === "ok") fails.push(`the store opened without a keycard: ${noCard.detail}`);
  if (give.status !== "ok" || !give.detail.includes("added=1")) fails.push(`keycard not given: ${give.status} ${give.detail}`);
  if (rob.status !== "ok") fails.push(`breach returned ${rob.status} ${rob.detail}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "armory proof: FAIL" : "armory proof: PASS (refused without a keycard, breached with it; confirm the server log for the weapons and the loud alarm)");
  return fails.length ? 1 : 0;
}

export default {
  name: "armory-proof",
  usage: "armory-proof",
  run: (ctx) => runArmoryProof(),
};
