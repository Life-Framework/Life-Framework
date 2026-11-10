import { readFileSync } from "node:fs";
import { join } from "node:path";
import { ROB_TARGET, ROOT, arrive, clientBox, parsePeer, parseState, sessionDir } from "../session.mjs";

async function runBedProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = (n) => parseState(on(n, { verb: "state" }, 10000).detail);
  const peers = (n) => on(n, { verb: "peers" }, 10000).detail;
  const fails = [];
  const need = (cond, text) => { if (!cond) fails.push(text); };
  const medicConf = readFileSync(join(ROOT, "addons", "LifeFramework", "Configs", "Medic", "LF_Medic.conf"), "utf8");
  const billAmount = Number(/m_iReviveBillAmount\s+(\d+)/.exec(medicConf)?.[1] ?? 100);
  const respawnConf = readFileSync(join(ROOT, "addons", "LifeFramework", "Configs", "Spawning", "LF_Respawn.conf"), "utf8");
  const wakeAmount = Number(/m_iBillAmount\s+(\d+)/.exec(respawnConf)?.[1] ?? 150);

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  const start2 = state(2);
  const patientName = `Player${start2.player}`;

  // Down the patient; client 1 clocks in as a medic.
  on(2, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "shoot", playerId: 2 }, 10000);
  wait(2500);
  need(state(2).downed === "1", "client 2 was not downed");
  // The duty action toggles: a repeat run in the same session is already clocked in.
  if (!String(state(1).faction).includes("MEDIC")) {
    on(1, { verb: "stand", entityName: "Roadmap_Hospital_Duty_Desk" });
    wait(2500);
    on(1, { verb: "look" });
    on(1, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Hospital_Duty_Desk" }, 10000);
    wait(2500);
  }
  need(String(state(1).faction).includes("MEDIC"), "client 1 did not clock in as a medic");

  // Carry the patient to the ambulance parked at the hospital and load them.
  on(1, { verb: "stand", entityName: patientName });
  wait(2500);
  on(1, { verb: "look" });
  on(1, { verb: "use", className: "LF_CarryAction", entityName: patientName }, 10000);
  wait(1500);
  need(parsePeer(peers(2), 2)?.carrier === 1, "the carry did not take hold");
  const carryOnly = process.argv.includes("--carry-only");
  let bleedA = 0;
  if (!carryOnly) {
    on(1, { verb: "stand", entityName: "Roadmap_Hospital_Ambulance" });
    wait(4000);
    const load = on(1, { verb: "use", className: "LF_LoadPatientAction", entityName: patientName }, 10000);
    need(load.status === "ok", `load returned ${load.status} ${load.detail}`);
    wait(9000);
    const seated = parsePeer(peers(2), 2);
    need(seated && seated.seat === "1", `the patient is not seated in the ambulance: ${peers(2)}`);
    const amb = parsePeer(peers(1), 2)?.at ?? [266, 0, 250];
    on(1, { verb: "capture", mode: "world", name: "m2_262_loaded_ambulance", x: amb[0], y: amb[1] + 1.2, z: amb[2], ox: 4, oy: 1.8, oz: 0 }, 25000);

    // Stretcher: the bleed-out clock does not run while loaded.
    bleedA = Number(state(2).bleed);
    wait(16000);
    const bleedB = Number(state(2).bleed);
    need(bleedA > 0 && bleedB === bleedA, `the bleed-out ran while loaded: ${bleedA} -> ${bleedB}`);

  }

  // Bed: the medic puts the patient in a bed.
  on(1, { verb: "stand", entityName: "Hospital_Bed_2" });
  wait(3000);
  on(1, { verb: "look" });
  const deliver = on(1, { verb: "use", className: "LF_BedDeliveryAction", entityName: "Hospital_Bed_2" }, 10000);
  need(deliver.status === "ok", `the bed action returned ${deliver.status} ${deliver.detail}`);
  wait(1000);
  on(1, { verb: "capture", mode: "world", name: "m2_262_patient_in_bed", x: 268.6, y: 1.2, z: 261.45, ox: 2.4, oy: 1.4, oz: -1.2 }, 25000);
  wait(3000);
  let woke = state(2);
  for (let i = 0; i < 10 && woke.life !== "ALIVE"; i++) {
    wait(2000);
    woke = state(2);
  }
  need(woke.downed === "0" && woke.life === "ALIVE", `the patient did not wake: downed=${woke.downed} life=${woke.life}`);
  const billsExpected = Number(start2.bills) + 1;
  need(Number(woke.bills) === billsExpected, `the patient carries bills=${woke.bills}, expected ${billsExpected}`);
  need(woke.bank === start2.bank, `the bill touched the bank: ${start2.bank} -> ${woke.bank}`);
  need(parsePeer(peers(1), 2)?.carrier === 0, "the patient is still held");
  on(2, { verb: "capture", mode: "ui", name: "m2_262_woke_client2" }, 25000);

  // The reduced bill: the revive amount, not the wake amount.
  on(2, { verb: "stand", entityName: "Roadmap_Hospital_Duty_Desk" });
  wait(2500);
  on(2, { verb: "look" });
  on(2, { verb: "use", className: "LF_TreatmentBillPayAction", entityName: "Roadmap_Hospital_Duty_Desk" }, 10000);
  wait(2500);
  const paid = state(2);
  need(Number(paid.cash) === Number(woke.cash) - billAmount, `the bill took ${Number(woke.cash) - Number(paid.cash)} cash, expected the reduced ${billAmount} (a wake bill is ${wakeAmount})`);

  console.log("");
  console.log(`start ${JSON.stringify(start2)}`);
  console.log(`woke  ${JSON.stringify(woke)}`);
  console.log(`paid  ${JSON.stringify(paid)}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "bed proof: FAIL" : `bed proof: PASS (${carryOnly ? "carried" : `loaded in the ambulance with the bleed-out paused at ${bleedA}`}, put in a bed, woke on their client, bill ${billAmount} not ${wakeAmount})`);
  return fails.length ? 1 : 0;
}

export default {
  name: "bed-proof",
  usage: "bed-proof",
  run: (ctx) => runBedProof(),
};
