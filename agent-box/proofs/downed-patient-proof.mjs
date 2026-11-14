import { arrive, clientBox, parseState, sessionDir } from "../session.mjs";

// The medic practice patient, solo, on a running `up --profile full` session (one client):
// clock in as a medic, find the downed man in front of the hospital, carry him to the ambulance,
// load him, put him in a bed (he wakes), and wait for the reset to put him back downed on the grass;
// then Revive him where he lies. Shots: the patient lying, loaded in the ambulance, revived (the bed is a toast and a state check: the shot camera cannot look down into the ward).
//   node agent-box/session.mjs up --profile full
//   node agent-box/session.mjs downed-patient-proof
//   node agent-box/session.mjs down
const PATIENT = "Patient"; // the harness name of the practice patient (Roadmap_Hospital_Patient in the layer)
const HOME = [268.2, 0, 254.6];

// `peers` appends "patient downed=N life=X carrier=N seat=N at=x,y,z" for the practice patient.
export function parsePatient(peers) {
  const m = /patient downed=(\d) life=(\w+) carrier=(-?\d+) seat=(\d) at=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)/.exec(String(peers ?? ""));
  if (!m) return null;
  return { downed: m[1], life: m[2], carrier: Number(m[3]), seat: m[4], at: [Number(m[5]), Number(m[6]), Number(m[7])] };
}

async function runDownedPatientProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 220)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const patient = () => parsePatient(on(1, { verb: "peers" }, 10000).detail);
  const fails = [];
  const need = (cond, text) => { if (!cond) fails.push(text); };
  const near = (at, spot, m) => Math.hypot(at[0] - spot[0], at[2] - spot[2]) <= m;
  const shot = (name, ox, oy, oz) => on(1, { verb: "capture", mode: "world", name, x: HOME[0], y: HOME[1] + 0.4, z: HOME[2], ox, oy, oz }, 25000);
  const use = (cls, entity) => {
    on(1, { verb: "stand", entityName: entity });
    wait(2500);
    on(1, { verb: "look" });
    return on(1, { verb: "use", className: cls, entityName: entity }, 10000);
  };

  arrive(on, 1, wait);
  wait(8000);

  // 1. Clock in as a medic (the duty action toggles: skip when already on duty).
  if (!String(parseState(on(1, { verb: "state" }, 10000).detail).faction).includes("MEDIC")) use("LF_DutyAction", "Roadmap_Hospital_Duty_Desk");
  wait(2500);
  need(String(parseState(on(1, { verb: "state" }, 10000).detail).faction).includes("MEDIC"), "did not clock in as a medic");

  // 2. Find the patient: downed, on the grass at his spot.
  on(1, { verb: "stand", entityName: PATIENT });
  wait(3000);
  const lying = patient();
  need(lying && lying.downed === "1", `the patient is not downed: ${JSON.stringify(lying)}`);
  shot("downed_patient_lying", 4, 0, -4);

  // 3. Carry him to the ambulance and load him.
  use("LF_CarryAction", PATIENT);
  wait(1500);
  need(patient()?.carrier === 1 || patient()?.carrier > 0, "the carry did not take hold");
  on(1, { verb: "stand", entityName: "Roadmap_Hospital_Ambulance" });
  wait(4000);
  const load = on(1, { verb: "use", className: "LF_LoadPatientAction", entityName: PATIENT }, 10000);
  need(load.status === "ok", `load returned ${load.status} ${load.detail}`);
  wait(9000);
  const seated = patient();
  need(seated && seated.seat === "1", `the patient is not in the ambulance: ${JSON.stringify(seated)}`);
  on(1, { verb: "capture", mode: "world", name: "downed_patient_loaded", x: 267, y: 1.2, z: 250, ox: 0.5, oy: 0.4, oz: -3 }, 25000);

  // 4. Deliver him to a hospital bed: he wakes there.
  on(1, { verb: "stand", entityName: "Hospital_Bed_2" });
  wait(3000);
  on(1, { verb: "look" });
  const deliver = on(1, { verb: "use", className: "LF_BedDeliveryAction", entityName: "Hospital_Bed_2" }, 10000);
  need(deliver.status === "ok", `the bed action returned ${deliver.status} ${deliver.detail}`);
  wait(1200);
  wait(5000);
  const inBed = patient();
  need(inBed && inBed.downed === "0", `the patient is still downed after the bed: ${JSON.stringify(inBed)}`);

  // 5. The reset: after the config delay he is back on the grass, downed.
  let back = null;
  for (let i = 0; i < 30; i++) {
    wait(4000);
    back = patient();
    if (back && back.downed === "1" && near(back.at, HOME, 1.5)) break;
  }
  need(back && back.downed === "1" && near(back.at, HOME, 1.5), `the patient was not put back downed at his spot: ${JSON.stringify(back)}`);

  // 6. Revive him where he lies.
  const revive = use("LF_ReviveAction", PATIENT);
  need(revive.status === "ok", `revive returned ${revive.status} ${revive.detail}`);
  wait(7000);
  const woke = patient();
  need(woke && woke.downed === "0", `the patient is still downed after the revive: ${JSON.stringify(woke)}`);
  shot("downed_patient_revived", 4, 0, -4);

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "downed-patient-proof: FAIL" : "downed-patient-proof: OK (found, carried, loaded, put in a bed, reset downed on the grass, revived)");
  return fails.length ? 1 : 0;
}

export default { name: "downed-patient-proof", usage: "downed-patient-proof", run: () => runDownedPatientProof() };
