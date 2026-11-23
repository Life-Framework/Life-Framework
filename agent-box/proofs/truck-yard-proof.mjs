import { readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, newestLog, parseState, readSession, sessionDir } from "../session.mjs";

// The truck yard at the north-east compound (#4531), one client on the island:
//   node agent-box/session.mjs up --mission island --profile full --wait
//   node agent-box/session.mjs truck-yard-proof
//   node agent-box/session.mjs down
// Stands at the hut counter and opens the sheet (picture: both trucks and their prices; a buyer with $2,000 in the
// bank sees "needs $X" and no Buy button). Then stands at both "Truck Sales" signs and takes a picture of each (the
// sign face text reads only in a session; Workbench never draws it). Not covered here: the truck-licence refusal and
// the spawn at the apron lane. The licence is a progression flag granted by the driving course, which has no harness
// fixture yet, and a Buy button needs $9,500 in the bank; `everon/truck-yard` covers rows, prices and the refusal.
const LOT = "Fit_ne_compound_truckyard_lot";
const SIGNS = ["Fit_ne_compound_truckyard_sign_hut", "Fit_ne_compound_truckyard_sign_gate"];
const BUY_ACTION = "LF_VehicleBuyAction";

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
  // The welcome guide stands over the sheet until it is dismissed.
  if (/WelcomeGuideDismissButton/.test(String(on(1, { verb: "ui" }).detail))) on(1, { verb: "click", widget: "WelcomeGuideDismissButton" });
  wait(1000);
  on(1, { verb: "stand", entityName: LOT });
  wait(2500);
  const open = on(1, { verb: "use", className: BUY_ACTION, entityName: LOT }, 10000);
  expect(open.status === "ok", `the hut counter opens the lot sheet (${open.status})`);
  wait(1500);
  on(1, { verb: "capture", mode: "ui", name: "truck_yard_sheet" }, 20000);
  // A buyer with $2,000 in the bank sees both trucks as "needs $X" rows with no Buy button, so nothing sells.
  expect(!/from the lot sheet for/.test(serverLog()), "no truck was sold to a buyer who cannot pay");
  wait(1000);

  for (const sign of SIGNS) {
    on(1, { verb: "stand", entityName: sign, side: "w" });
    wait(3000);
    on(1, { verb: "look" });
    on(1, { verb: "capture", mode: "ui", name: `truck_yard_${sign.replace(/^.*_sign_/, "sign_")}` }, 20000);
  }

  console.log(fails.length ? `truck-yard-proof: FAIL (${fails.length})` : "truck-yard-proof: PASS");
  return fails.length ? 1 : 0;
}

export default {
  name: "truck-yard-proof",
  usage: "truck-yard-proof",
  run: () => run(),
};
