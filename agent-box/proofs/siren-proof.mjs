import { existsSync, readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, sessionDir } from "../session.mjs";

// M2 #263 on a running `up --clients 2` session (debug mission: the dev gate is open): client 1 turns
// the hospital ambulance's siren on through the same rule the driver's key uses, client 2 (a second
// machine) must see and hear the change arrive by replication; then off. Screenshots: lights off,
// lights on from both clients. Replication is read from client 2's own log line.
async function runSirenProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^[0-9]+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const siren = (n, name, mode) => on(n, { verb: "siren", entityName: name, mode }, 10000);
  const AMB = "Roadmap_Hospital_Ambulance";
  const POLICE = "Roadmap_Police_Car";
  const desk = (n) => {
    on(n, { verb: "stand", entityName: "Roadmap_Hospital_Duty_Desk" });
    wait(2500);
    on(n, { verb: "look" });
    const r = on(n, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Hospital_Duty_Desk" }, 10000);
    wait(2500);
    return r;
  };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);

  // #464: the siren is on-duty only. Client 1 stays a civilian; client 2 clocks in as a medic.
  const offDuty = siren(1, AMB, 1);
  if (!offDuty.detail.includes("changed=0 on=0 broadcasts=0")) fails.push(`off-duty client 1 was not refused: ${offDuty.status} ${offDuty.detail}`);
  const clockIn = desk(2);
  const state = on(2, { verb: "state" }, 10000).detail;
  if (clockIn.status !== "ok" || !state.includes("faction=MEDIC") || !state.includes("duty=1")) fails.push(`client 2 did not clock in as a medic: ${clockIn.status} ${state}`);
  const onR = siren(2, AMB, 1);
  if (!onR.detail.includes("changed=1 on=1 broadcasts=1")) fails.push(`on-duty medic ambulance siren: ${onR.status} ${onR.detail}`);
  wait(2500);
  const wrong = siren(2, POLICE, 1);
  if (!wrong.detail.includes("changed=0 on=0 broadcasts=0")) fails.push(`on-duty medic on the police car was not refused: ${wrong.detail}`);
  const again = siren(2, AMB, 1);
  if (!again.detail.includes("changed=0")) fails.push(`the same mode asked twice changed again: ${again.detail}`);
  const offR = siren(2, AMB, 0);
  if (!offR.detail.includes("changed=1 on=0 broadcasts=2")) fails.push(`ambulance siren off: ${offR.status} ${offR.detail}`);
  wait(1500);
  desk(2);

  // Replication: client 1 (the other machine) saw the ambulance change on and off.
  const logs = [];
  const clientDir = join(dir, "client1", "logs");
  if (existsSync(clientDir)) {
    for (const d of readdirSync(clientDir).sort()) {
      const p = join(clientDir, d, "console.log");
      if (existsSync(p)) logs.push(readFileSync(p, "utf8"));
    }
  }
  const modeLines = (logs.join(String.fromCharCode(10)).match(/Siren]? mode -> /g) ?? []).length;
  console.log(`client 1 siren mode lines: ${modeLines}`);
  if (modeLines < 2) fails.push(`client 1 logged ${modeLines} siren mode changes, expected at least 2 (on and off)`);

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "siren proof: FAIL" : "siren proof: PASS (off-duty civilian refused; on-duty medic runs the ambulance siren, is refused on the police car; replicated to the other client)");
  return fails.length ? 1 : 0;
}

export default {
  name: "siren-proof",
  usage: "siren-proof",
  run: (ctx) => runSirenProof(),
};
