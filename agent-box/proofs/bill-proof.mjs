import { readFileSync } from "node:fs";
import { join } from "node:path";
import { ROB_TARGET, ROOT, arrive, clientBox, parseState, sessionDir } from "../session.mjs";

// M2 #260 on a running `up --clients 2 --mission debug` session (debug mission: `shoot` is refused on playtest): client 1 downs client 2, clocks in
// as a medic at the hospital desk and revives them. Client 2 holds a paper treatment bill (the bank
// and carried cash untouched), pays it in cash at the hospital desk, and the medic's share arrives
// at clock-out. Then client 2 dies for real and wakes in hospital: a second paper bill, bank untouched,
// paid at the desk too. Screenshots: the revive toast, the desk payment, the wake bill card.
async function runBillProof() {
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
  const fails = [];
  const deskStep = (n, className) => {
    on(n, { verb: "stand", entityName: "Roadmap_Hospital_Duty_Desk" });
    wait(2500);
    on(n, { verb: "look" });
    const r = on(n, { verb: "use", className, entityName: "Roadmap_Hospital_Duty_Desk" }, 10000);
    wait(2500);
    return r;
  };
  const medicConf = readFileSync(join(ROOT, "addons", "LifeFramework", "Configs", "Medic", "LF_Medic.conf"), "utf8");
  const giveUpDelay = Number(/m_fGiveUpDelaySeconds\s+(\d+)/.exec(medicConf)?.[1] ?? 30);
  const billAmount = Number(/m_iReviveBillAmount\s+(\d+)/.exec(medicConf)?.[1] ?? 100);
  const share = Math.floor(billAmount * Number(/m_fBillMedicShare\s+([\d.]+)/.exec(medicConf)?.[1] ?? 0.5));
  const respawnConf = readFileSync(join(ROOT, "addons", "LifeFramework", "Configs", "Spawning", "LF_Respawn.conf"), "utf8");
  const wakeAmount = Number(/m_iBillAmount\s+(\d+)/.exec(respawnConf)?.[1] ?? 150);

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  const start2 = state(2);
  const medic0 = state(1);
  const patientName = `Player${start2.player}`;

  // Down the patient, then a medic revives.
  on(2, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "shoot", playerId: 2 }, 10000);
  wait(2500);
  if (state(2).downed !== "1") fails.push("client 2 was not downed");
  deskStep(1, "LF_DutyAction");
  if (!String(state(1).faction).includes("MEDIC")) fails.push("client 1 did not clock in as a medic");
  on(1, { verb: "stand", entityName: patientName });
  wait(2500);
  on(1, { verb: "look" });
  const revive = on(1, { verb: "use", className: "LF_ReviveAction", entityName: patientName }, 10000);
  let revived = state(2);
  for (let i = 0; i < 8 && revived.life !== "ALIVE"; i++) {
    wait(2000);
    revived = state(2);
  }
  wait(1500);
  on(2, { verb: "capture", mode: "ui", name: "m2_260_revive_bill_client2" }, 25000);
  revived = state(2);
  if (revive.status !== "ok") fails.push(`revive returned ${revive.status} ${revive.detail}`);
  if (revived.bills !== "1") fails.push(`the revived patient carries bills=${revived.bills}, expected 1`);
  if (revived.cash !== start2.cash) fails.push(`the bill took cash before the desk: ${start2.cash} -> ${revived.cash}`);
  if (revived.bank !== start2.bank) fails.push(`the bill touched the bank: ${start2.bank} -> ${revived.bank}`);

  // The patient pays at the hospital desk, in cash.
  on(2, { verb: "stand", entityName: "Roadmap_Hospital_Duty_Desk" });
  wait(2500);
  on(2, { verb: "look" });
  on(2, { verb: "capture", mode: "ui", name: "m2_260_desk_pay_prompt_client2" }, 25000);
  const pay = on(2, { verb: "use", className: "LF_TreatmentBillPayAction", entityName: "Roadmap_Hospital_Duty_Desk" }, 10000);
  wait(2500);
  const paid = state(2);
  on(2, { verb: "capture", mode: "ui", name: "m2_260_bill_paid_client2" }, 25000);
  if (pay.status !== "ok") fails.push(`the desk payment returned ${pay.status} ${pay.detail}`);
  if (Number(paid.cash) !== Number(start2.cash) - billAmount) fails.push(`paying took ${Number(start2.cash) - Number(paid.cash)} cash, expected ${billAmount}`);
  const payAgain = on(2, { verb: "use", className: "LF_TreatmentBillPayAction", entityName: "Roadmap_Hospital_Duty_Desk" }, 10000);
  wait(1500);
  if (state(2).cash !== paid.cash) fails.push(`a second desk payment changed cash: ${paid.cash} -> ${state(2).cash} (${payAgain.status})`);

  // The medic's share arrives once, at clock-out.
  const medicBefore = state(1);
  deskStep(1, "LF_DutyAction");
  const medicAfter = state(1);
  const gained = Number(medicAfter.cash) - Number(medicBefore.cash);
  if (gained < share) fails.push(`clock-out paid ${gained}, expected at least the ${share} share`);
  on(1, { verb: "capture", mode: "ui", name: "m2_260_medic_share_client1" }, 25000);

  // A real death: the wake writes a second paper bill, the bank stays whole.
  on(2, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "shoot", playerId: 2 }, 10000);
  wait(2500);
  wait((giveUpDelay + 2) * 1000);
  on(2, { verb: "giveup" }, 10000);
  wait(4000);
  on(2, { verb: "click", widget: "RespawnButton" });
  wait(14000);
  on(2, { verb: "capture", mode: "ui", name: "m2_260_wake_bill_card_client2" }, 25000);
  const woke = state(2);
  if (woke.bills !== "1") fails.push(`the wake left bills=${woke.bills}, expected the one wake bill`);
  if (woke.bank !== start2.bank) fails.push(`the wake touched the bank: ${start2.bank} -> ${woke.bank}`);
  on(2, { verb: "click", widget: "Close" });
  const cashWake = Number(woke.cash);
  deskStep(2, "LF_TreatmentBillPayAction");
  const wakePaid = state(2);
  if (Number(wakePaid.cash) !== cashWake - wakeAmount) fails.push(`the wake bill took ${cashWake - Number(wakePaid.cash)} cash, expected ${wakeAmount}`);
  on(2, { verb: "capture", mode: "ui", name: "m2_260_wake_bill_paid_client2" }, 25000);

  console.log("");
  console.log(`start    client 2: ${JSON.stringify(start2)}`);
  console.log(`revived  client 2: ${JSON.stringify(revived)}`);
  console.log(`paid     client 2: ${JSON.stringify(paid)}`);
  console.log(`medic    before ${medicBefore.cash} after clock-out ${medicAfter.cash} (share ${share})`);
  console.log(`woke     client 2: ${JSON.stringify(woke)}; paid the wake bill ${JSON.stringify(wakePaid)}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "bill proof: FAIL" : "bill proof: PASS (revive bill on paper, paid in cash at the hospital desk, medic share at clock-out, wake bill on paper, bank untouched)");
  return fails.length ? 1 : 0;
}

export default {
  name: "bill-proof",
  usage: "bill-proof",
  run: (ctx) => runBillProof(),
};
