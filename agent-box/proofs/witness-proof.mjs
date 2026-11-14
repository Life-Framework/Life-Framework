import { rosterHas } from "./scanner-proof.mjs";
import { arrive, clientBox, sessionDir } from "../session.mjs";
import { TILL, armAndDraw, demandTill } from "./fuel-robbery-proof.mjs";

async function runWitnessProof() {
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

  if (!rosterHas(dir, 2)) {
    console.log("witness proof: FAIL  client 2 is not on the police roster; run `session.mjs down`, `session.mjs roster 2`, then `up --clients 2` again");
    return 1;
  }
  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  // Client 2 stands at the fuel till (a witness within a few metres), client 1 robs it through the
  // attendant (#936): the crime is reported when the demand starts the hold, before the till pays.
  on(2, { verb: "stand", entityName: TILL, side: "w" });
  wait(2500);
  on(1, { verb: "stand", entityName: TILL, side: "s" });
  wait(2500);
  on(1, { verb: "look" });
  must(armAndDraw(on, 1, wait), "pistol");
  const robbed = demandTill(on, 1, wait);
  if (!/node=rob_open|closed/.test(robbed.demanded)) fails.push(`the robbery did not start through the attendant (${robbed.demanded})`);
  wait(3000);

  // The witness walks to the payphone and describes the suspect's clothing.
  on(2, { verb: "stand", entityName: "Village_Payphone" });
  wait(2500);
  on(2, { verb: "look" });
  must(on(2, { verb: "use", className: "LF_WitnessPayphoneAction", entityName: "Village_Payphone" }, 10000), "payphone description");
  wait(1000);
  on(2, { verb: "capture", mode: "ui", name: "m2_266_witness_payphone_client2" }, 25000);

  // An officer reads the board: the statement sits under the dispatch log.
  on(2, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  on(2, { verb: "look" });
  must(on(2, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000), "clock in");
  wait(2500);
  // #267: the officer asks the shopkeeper, who stood near the till (a town NPC), what it saw.
  on(2, { verb: "stand", entityName: "Roadmap_Fuel_Clerk" });
  wait(2500);
  on(2, { verb: "look" });
  must(on(2, { verb: "use", className: "LF_WitnessAskAction", entityName: "Roadmap_Fuel_Clerk" }, 10000), "ask the shopkeeper");
  wait(1000);
  on(2, { verb: "capture", mode: "ui", name: "m2_267_ask_shopkeeper_client2" }, 25000);
  on(2, { verb: "stand", entityName: "Roadmap_Police_WantedBoard" });
  wait(2500);
  on(2, { verb: "look" });
  on(2, { verb: "use", className: "LF_WantedBoardReadAction", entityName: "Roadmap_Police_WantedBoard" }, 10000);
  wait(2500);
  must(h.waitForText("WITNESS", 10000).found ? { status: "ok" } : { status: "error", detail: "no WITNESS line on the board" }, "board witness line");
  on(2, { verb: "capture", mode: "ui", name: "m2_266_witness_board_client2" }, 25000);
  on(2, { verb: "ui" });
  on(2, { verb: "click", widget: "Close" });
  // Leave the session as found: an officer left on duty makes the next proof's clock-in a clock-out.
  on(2, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  on(2, { verb: "look" });
  must(on(2, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000), "clock out");
  wait(1500);

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "witness proof: FAIL" : "witness proof: PASS (a witness at the till described the suspect from the payphone; the officer's board shows it)");
  return fails.length ? 1 : 0;
}

export default {
  name: "witness-proof",
  usage: "witness-proof",
  run: (ctx) => runWitnessProof(),
};
