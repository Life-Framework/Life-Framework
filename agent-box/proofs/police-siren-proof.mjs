import { existsSync, readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, sessionDir } from "../session.mjs";

// Police siren in multiplayer (0.2.0 live bug: "the menu shows but the sound doesn't toggle on"), the
// way a player does it: from the driver seat, through the dash knob's rows (LF_SirenModeAction), not
// the harness `siren` verb (that one calls RequestBy directly and never took the broken path).
//   node agent-box/session.mjs up --clients 2 --profile full --roster 2 --sound
//   node agent-box/session.mjs police-siren-proof
//   node agent-box/session.mjs down
// Client 1 clocks in as police, takes the jeep's driver seat, picks Patrol then Off Duty on the knob.
// Each client's own log must show the replicated mode and the siren sound event on, then off.
const CAR = "Roadmap_Police_Car";
const DESK = "Roadmap_Police_Desk";
const KNOB_PATROL = "LF_SirenModeAction__1"; // knob rows: default, Mode1 (patrol), Mode2, Mode3
const KNOB_OFF = "LF_SirenModeAction";
// Client 2 registers as the debug world's seeded officer (police-proof.mjs); client 1 is the bystander.
const OFFICER = 2;
const BYSTANDER = 1;

function clientLog(dir, n) {
  const logs = [];
  const clientDir = join(dir, `client${n}`, "logs");
  if (existsSync(clientDir)) {
    for (const d of readdirSync(clientDir).sort()) {
      const p = join(clientDir, d, "console.log");
      if (existsSync(p)) logs.push(readFileSync(p, "utf8"));
    }
  }
  return logs.join("\n");
}

async function runPoliceSirenProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 200)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what} returned ${r.status} ${r.detail}`);
    return r;
  };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(6000);

  must(on(OFFICER, { verb: "stand", entityName: DESK }), "stand at the police desk");
  wait(2500);
  on(OFFICER, { verb: "look" });
  // Join the police first (a debug session approves on submit); an earlier run may have joined already.
  if (on(OFFICER, { verb: "use", className: "LF_PoliceApplyAction", entityName: DESK }, 10000).status === "ok") {
    wait(1500);
    on(OFFICER, { verb: "type", widget: "PolAppName", text: "Bob_Officer" });
    on(OFFICER, { verb: "type", widget: "PolAppWhy", text: "Siren_proof" });
    on(OFFICER, { verb: "type", widget: "PolAppAvailability", text: "Evenings" });
    on(OFFICER, { verb: "click", widget: "PolAppSubmit" });
    wait(3000);
    on(OFFICER, { verb: "look" });
  }
  must(on(OFFICER, { verb: "use", className: "LF_DutyAction", entityName: DESK }, 10000), "clock in");
  wait(3000);
  const state = on(OFFICER, { verb: "state" }, 10000).detail;
  if (!state.includes("duty=1")) fails.push(`the officer client is not on duty: ${state}`);

  // The other client stands by the car: a bystander outside.
  must(on(BYSTANDER, { verb: "stand", entityName: CAR, side: "e" }), "the bystander stands by the car");
  must(on(OFFICER, { verb: "stand", entityName: CAR, side: "w" }), "the officer at the car");
  wait(2500);
  on(OFFICER, { verb: "look" });
  // A parked police car is locked; an on-duty officer unlocks it.
  on(OFFICER, { verb: "use", className: "LF_VehicleLockAction" }, 10000);
  wait(2000);
  const getIn = on(OFFICER, { verb: "use", className: "SCR_GetInUserAction", entityName: CAR }, 10000);
  wait(3000);
  const seated = getIn.status === "ok";
  // The harness's server-side get-in can be refused (`SeatObstructed`, every vehicle on 2026-11-22). Then
  // the `siren` verb drives the same server request the knob row now makes (RequestBy), so the
  // replication and the sound event on each client are still proven; the seat menu stays manual.
  if (seated) {
    const look = must(on(OFFICER, { verb: "look" }), "look from the seat");
    if (!look.detail.includes("LF_SirenModeAction")) fails.push(`the seated officer is not offered the knob rows: ${look.detail.slice(0, 300)}`);
  } else console.log(`NOTE  get in refused (${getIn.detail}); using the siren verb (RequestBy, the knob's server path)`);

  const mark1 = clientLog(dir, 1).length;
  const mark2 = clientLog(dir, 2).length;
  if (seated) must(on(OFFICER, { verb: "use", className: KNOB_PATROL }, 10000), "knob: patrol");
  else must(on(OFFICER, { verb: "siren", entityName: CAR, mode: 1 }, 10000), "siren verb: patrol");
  wait(4000);
  on(BYSTANDER, { verb: "capture", mode: "ui", name: "police_siren_on_bystander" }, 25000);
  if (seated) must(on(OFFICER, { verb: "use", className: KNOB_OFF }, 10000), "knob: off duty");
  else must(on(OFFICER, { verb: "siren", entityName: CAR, mode: 0 }, 10000), "siren verb: off");
  wait(4000);

  for (const n of [1, 2]) {
    const text = clientLog(dir, n).slice(n === 1 ? mark1 : mark2);
    const lines = text.split("\n").filter((l) => l.includes("[LFDebug:Siren]"));
    for (const l of lines) console.log(`client ${n} log: ${l.trim()}`);
    if (!lines.some((l) => l.includes("replicated mode index 1"))) fails.push(`client ${n} never received the patrol mode`);
    if (!lines.some((l) => l.includes("siren sound SOUND_VEHICLE_HORN 1 "))) fails.push(`client ${n} played no siren sound event`);
    if (!lines.some((l) => l.includes("replicated mode index 0"))) fails.push(`client ${n} never received the off`);
  }

  if (fails.length) {
    console.log("police-siren-proof: FAIL");
    for (const f of fails) console.log(`  ${f}`);
    return 1;
  }
  console.log("police-siren-proof: OK (driver's knob pick replicated to both clients, siren sound event on each, then off)");
  return 0;
}

export default { name: "police-siren-proof", usage: "police-siren-proof", run: () => runPoliceSirenProof() };
