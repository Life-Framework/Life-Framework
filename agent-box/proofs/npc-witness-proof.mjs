import { arrive, clientBox, sessionDir } from "../session.mjs";

// #610 and #615. Client 1 (first on the police roster) clocks in as the officer and asks a civilian before any crime (a
// noncommittal answer, not "you didn't see anything"). Client 2 kills a civilian beside another
// (murder, a crime in sight), cannot kill the clerk, and the officer asks the second civilian
// and the clerk what they saw: a named answer each time.
const UNSURE = /#LF-Witness_Unsure\d/;
const SAW = /#LF-Witness_(GaveStatement|ForgotSome)/;

async function runNpcWitnessProof() {
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
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what} returned ${r.status} ${r.detail}`);
    return r;
  };
  // The toast key the player last saw (the state line's toast= field).
  const screenText = (n) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand({ verb: "state" }, 10000);
    const m = r.status === "ok" ? /toast=(\S+)/.exec(String(r.detail)) : null;
    return m ? m[1] : "";
  };
  const ask = (target, label) => {
    on(1, { verb: "stand", entityName: target });
    wait(2500);
    on(1, { verb: "look" });
    must(on(1, { verb: "use", className: "LF_WitnessAskAction", entityName: target }, 10000), `ask ${label}`);
    wait(700);
    return screenText(1);
  };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  on(1, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  on(1, { verb: "look" });
  must(on(1, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000), "clock in");
  wait(2500);

  // 1. Nothing has happened yet: a noncommittal line, never "You didn't see anything".
  const before = ask("Roadmap_Civilian_1", "civilian before any crime");
  on(1, { verb: "capture", mode: "ui", name: "npc_witness_unsure_client1_officer" }, 25000);
  if (!UNSURE.test(before)) fails.push("no noncommittal line before any crime");
  if (/NothingSeen/.test(before)) fails.push("the old 'You didn't see anything' line is back");

  // 2. The clerk is protected; a civilian can be killed, which is murder in sight of the other.
  on(2, { verb: "stand", entityName: "Roadmap_Civilian_1" });
  wait(2500);
  const clerkShot = must(on(2, { verb: "shootnpc", entityName: "Roadmap_Fuel_Clerk" }, 10000), "shoot clerk");
  if (!clerkShot.detail.includes("protected=1")) fails.push(`clerk was not protected: ${clerkShot.detail}`);
  const kill = must(on(2, { verb: "shootnpc", entityName: "Roadmap_Civilian_2" }, 10000), "shoot civilian");
  if (!kill.detail.includes("killed=1")) fails.push(`civilian was not killed: ${kill.detail}`);
  wait(3500);
  const state1 = on(2, { verb: "state" }, 10000).detail;
  if (!/wanted=[1-9]/.test(state1)) fails.push(`killer is not wanted: ${state1}`);

  // 3. The officer asks the surviving civilian and the clerk: named answers.
  const civ = ask("Roadmap_Civilian_1", "civilian after the murder");
  on(1, { verb: "capture", mode: "ui", name: "npc_witness_asked_client1_officer" }, 25000);
  const clerk = ask("Roadmap_Fuel_Clerk", "shopkeeper after the murder");
  if (!SAW.test(civ)) fails.push(`the civilian who stood beside the killing did not recall it (toast ${civ})`);
  if (!SAW.test(clerk)) fails.push(`the shopkeeper in sight did not recall it (toast ${clerk})`);

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "npc-witness proof: FAIL" : "npc-witness proof: PASS (a civilian dies, the clerk is protected, witnesses answer by name)");
  return fails.length ? 1 : 0;
}

export default {
  name: "npc-witness-proof",
  usage: "npc-witness-proof",
  run: () => runNpcWitnessProof(),
};
