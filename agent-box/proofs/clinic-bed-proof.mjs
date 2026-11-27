import { readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, newestLog, parsePeer, parseState, sessionDir, sessionPaths } from "../session.mjs";

// #5153: a patient put in the Saint-Philippe clinic bed lies ON the mattress, then steps down to the floor beside
// the bed (or stays on the mattress when the bed is boxed in), never under the frame.
//   node agent-box/session.mjs up --mission island --clients 2 --profile full --hold 20
//   node agent-box/session.mjs clinic-bed-proof
//   node agent-box/session.mjs down
// Client 2 downs client 1 beside the bed (a harness shot), clocks in at the clinic desk and puts the patient in the bed.
// The patient's height over the medic's feet (the medic stands on the floor beside the bed) is read once the wake
// has run (on the mattress) and again after the step-down; pictures of the bed at both moments.
const DESK = "Fit_saint_philippe_clinic_lf_medic_duty_desk";
const BED = "Fit_saint_philippe_clinic_lf_hospital_bed";
const ON_MATTRESS_MIN = 0.3; // metres over the medic's feet
const ON_FLOOR_MAX = 0.2;

// Pure. The failures for a patient height over the floor: on the mattress after the wake, back on the floor or still on the mattress after the step-down.
export function judgeLay(layHeight, laterHeight, noRoomLogged) {
  const fails = [];
  if (!(layHeight >= ON_MATTRESS_MIN)) fails.push(`lay: the patient is ${layHeight.toFixed(2)} m over the floor after the wake, expected on the mattress (at least ${ON_MATTRESS_MIN} m)`);
  if (!(laterHeight >= -ON_FLOOR_MAX)) fails.push(`step: the patient is ${laterHeight.toFixed(2)} m under the floor after the step-down`);
  if (laterHeight > ON_FLOOR_MAX && !noRoomLogged) fails.push(`step: the patient stayed ${laterHeight.toFixed(2)} m over the floor with no "no room" warning`);
  return fails;
}

async function runClinicBedProof() {
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
  const serverLines = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    return f ? readFileSync(f, "utf8").split("\n") : [];
  };
  const base = serverLines().length;
  const fails = [];
  const need = (cond, text) => { if (!cond) fails.push(text); };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  const patientId = Number(state(1).player);
  const medicId = Number(state(2).player);

  // Both at the bed; the medic downs the patient there, then clocks in.
  on(1, { verb: "stand", entityName: BED });
  wait(2500);
  on(2, { verb: "stand", entityName: BED });
  wait(2500);
  on(2, { verb: "shoot", playerId: patientId }, 10000);
  wait(2500);
  need(state(1).downed === "1", "client 1 was not downed");
  if (!String(state(2).faction).includes("MEDIC")) {
    on(2, { verb: "stand", entityName: DESK });
    wait(2500);
    on(2, { verb: "look" });
    on(2, { verb: "use", className: "LF_DutyAction", entityName: DESK }, 10000);
    wait(2500);
  }
  need(String(state(2).faction).includes("MEDIC"), "client 2 did not clock in as a medic");

  on(2, { verb: "stand", entityName: BED });
  wait(2500);
  on(2, { verb: "look" });
  const floor = parsePeer(peers(2), medicId)?.at;
  need(!!floor, `no peer line for the medic: ${peers(2)}`);
  // A world capture puts the character at target + offset and faces the target: from the medic's own floor spot at the patient.
  const shoot = (name, at) => {
    if (!floor || !at) return;
    on(2, { verb: "capture", mode: "world", name, x: at[0], y: at[1] + 0.2, z: at[2], ox: floor[0] - at[0], oy: floor[1] - (at[1] + 0.2), oz: floor[2] - at[2] }, 25000);
    on(2, { verb: "stand", entityName: BED });
    wait(2500);
  };
  const before = parsePeer(peers(2), patientId)?.at;
  shoot("clinic_bed_before", before);
  console.log(`medic ${JSON.stringify(floor)} patient before ${JSON.stringify(before)}`);
  // A world capture poses the camera and moves the body: stand at the bed again before the action.
  on(2, { verb: "stand", entityName: BED });
  wait(2500);
  on(2, { verb: "look" });

  const deliver = on(2, { verb: "use", className: "LF_BedDeliveryAction", entityName: BED }, 20000);
  need(deliver.status === "ok", `the bed action returned ${deliver.status} ${deliver.detail}`);
  wait(700);
  const lay = parsePeer(peers(2), patientId)?.at;
  // The step-down runs 3 s after the wake: the patient's own picture is the one taken while lying.
  on(1, { verb: "capture", mode: "ui", name: "clinic_bed_patient_lying" }, 25000);
  wait(5000);
  const later = parsePeer(peers(2), patientId)?.at;
  shoot("clinic_bed_stepped_down", later);
  on(1, { verb: "capture", mode: "ui", name: "clinic_bed_patient_view" }, 25000);

  const fresh = serverLines().slice(base);
  const noRoom = fresh.filter((l) => l.includes("no room for a body beside bed"));
  for (const l of noRoom) console.log(`log  ${l.slice(l.indexOf("[LFDebug"))}`);
  need(fresh.some((l) => l.includes("put in a hospital bed by medic")), "the server never logged the bed delivery");
  if (floor && lay && later) {
    const layHeight = lay[1] - floor[1];
    const laterHeight = later[1] - floor[1];
    console.log(`patient height over the medic's feet: lying ${layHeight.toFixed(2)} m, after the step-down ${laterHeight.toFixed(2)} m (${noRoom.length} no-room warning(s))`);
    fails.push(...judgeLay(layHeight, laterHeight, noRoom.length > 0));
  } else {
    fails.push(`no patient position: lay ${JSON.stringify(lay)} later ${JSON.stringify(later)}`);
  }

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "clinic bed proof: FAIL" : "clinic bed proof: PASS (the patient lay on the mattress and stepped down)");
  return fails.length ? 1 : 0;
}

export default {
  name: "clinic-bed-proof",
  usage: "clinic-bed-proof",
  run: () => runClinicBedProof(),
};
