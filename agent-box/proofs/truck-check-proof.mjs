import { readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, newestLog, parseState, readSession, sessionDir } from "../session.mjs";

// The truck licence check at the north-east compound (#4697), one client on the island:
//   node agent-box/session.mjs up --mission island --profile full --wait
//   node agent-box/session.mjs truck-check-proof
//   node agent-box/session.mjs down
// Stands at the examiner, takes the check ($300), then stands at the start spot and at each gate for a picture.
// The harness cannot drive: gates 0 to 3 are crossed by hand (the PR's manual steps); `LF_Test_TruckCheckGate`
// covers the gate order in script.
const EXAMINER = "Fit_ne_compound_truckyard_examiner";
const START = "Fit_ne_compound_truckyard_check_start";
const GATES = [0, 1, 2, 3].map((i) => `Fit_ne_compound_truckyard_check_gate_${i}`);
const ACTION = "LF_TruckCheckExaminerAction";

async function run() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 300)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const expect = (cond, what) => { console.log(`${cond ? "PASS" : "FAIL"}  ${what}`); if (!cond) fails.push(what); };
  const serverLog = () => {
    const s = readSession(dir);
    const log = s ? newestLog(join(s.serverProfile, "logs")) : null;
    return log ? readFileSync(log, "utf8") : "";
  };
  const stateOf = () => {
    let r = on(1, { verb: "state" }, 10000);
    for (let i = 0; i < 8 && r.status !== "ok"; i++) { wait(2500); r = on(1, { verb: "state" }, 10000); }
    return parseState(r.detail);
  };

  arrive(on, 1, wait);
  stateOf();
  if (/WelcomeGuideDismissButton/.test(String(on(1, { verb: "ui" }).detail))) on(1, { verb: "click", widget: "WelcomeGuideDismissButton" });
  wait(1000);
  on(1, { verb: "stand", entityName: EXAMINER });
  wait(2500);
  on(1, { verb: "look" });
  on(1, { verb: "capture", mode: "ui", name: "truck_check_examiner" }, 20000);
  const take = on(1, { verb: "use", className: ACTION, entityName: EXAMINER }, 10000);
  expect(take.status === "ok", `the examiner's action runs (${take.status})`);
  wait(2500);
  on(1, { verb: "capture", mode: "ui", name: "truck_check_taken" }, 20000);
  const log = serverLog();
  console.log((log.match(/.*truck check.*/gi) || []).slice(-8).join("\n"));
  expect(/truck check/i.test(log), "the server logged the truck check");

  on(1, { verb: "stand", entityName: START, side: "w" });
  wait(3000);
  on(1, { verb: "look" });
  on(1, { verb: "capture", mode: "ui", name: "truck_check_start" }, 20000);
  for (const gate of GATES) {
    on(1, { verb: "stand", entityName: gate, side: "w" });
    wait(2000);
    on(1, { verb: "look" });
    on(1, { verb: "capture", mode: "ui", name: `truck_check_${gate.replace(/^.*_check_/, "")}` }, 20000);
  }

  console.log(fails.length ? `truck-check-proof: FAIL (${fails.length})` : "truck-check-proof: PASS");
  return fails.length ? 1 : 0;
}

export default {
  name: "truck-check-proof",
  usage: "truck-check-proof",
  run: () => run(),
};
