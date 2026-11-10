import { arrive, clientBox, sessionDir } from "../session.mjs";
import { makeClientSees } from "./_clientSees.mjs";

// M2 #415 on a running `up --clients 3 --mission playtest --wait` session with client 2 on the roster
// (`roster 2` before `up`): the whole loop. Client 1 owns the starter S105; client 3 hotwires it; client 1
// reports it; the officer (client 2) reads it on the board and checks the plate; the thief drives it to
// the chop shop yard (the harness teleports the seated thief with the car), takes it apart and is paid in
// cash; the board then shows the chop shop crime and no stolen car.
async function runChopProof() {
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
  const sees = makeClientSees({ on, wait, dir });
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what} returned ${r.status} ${r.detail}`);
    return r;
  };
  const cash = (n) => Number(/cash=(\d+)/.exec(on(n, { verb: "state" }, 10000).detail)?.[1] ?? -1);
  for (const n of [1, 2, 3]) arrive(on, n, wait);
  wait(8000);

  on(2, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  must(on(2, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000), "clock in");
  wait(2500);

  on(1, { verb: "stand", entityName: "Roadmap_Garage" });
  wait(2500);
  must(on(1, { verb: "use", className: "LF_VehicleRetrieveAction", entityName: "Roadmap_Garage" }, 10000), "retrieve the car");
  wait(4000);
  on(3, { verb: "stand", entityName: "Roadmap_Garage" });
  wait(2500);
  must(on(3, { verb: "engine", entityName: "car" }, 10000), "seat the thief");
  wait(4000);
  let started;
  for (let attempt = 1; attempt <= 5; attempt++) {
    must(on(3, { verb: "use", className: "LF_HotwireAction" }, 10000), "hotwire");
    wait(17000);
    on(3, { verb: "engine" }, 10000);
    wait(4000);
    started = on(3, { verb: "engine" }, 10000);
    if (started.status === "ok" && started.detail.includes("on=1")) break;
  }
  if (!started || !started.detail.includes("on=1")) fails.push(`the stolen car did not run: ${started && started.detail}`);

  on(1, { verb: "look" });
  must(on(1, { verb: "use", className: "LF_ReportStolenAction" }, 10000), "report the car stolen");
  wait(1500);
  on(2, { verb: "stand", entityName: "Roadmap_Garage" });
  wait(2500);
  must(on(2, { verb: "use", className: "LF_CheckPlateAction" }, 10000), "check the plate");
  wait(1500);
  on(2, { verb: "capture", mode: "ui", name: "m2_415_plate_check_client2" }, 25000);

  // The thief drives (teleports) the car into the yard and gets out.
  const before = cash(3);
  on(3, { verb: "stand", entityName: "Roadmap_ChopShop" });
  wait(3000);
  on(3, { verb: "engine", entityName: "out" }, 10000);
  wait(3000);
  on(3, { verb: "look" });
  on(3, { verb: "capture", mode: "ui", name: "m2_415_yard_before_client3" }, 25000);
  must(on(3, { verb: "use", className: "LF_ChopAction", entityName: "Roadmap_ChopShop" }, 10000), "load the car");
  wait(1500);
  for (let i = 0; i < 6; i++) {
    must(on(3, { verb: "use", className: "LF_ChopAction", entityName: "Roadmap_ChopShop" }, 10000), `work ${i + 1}`);
    wait(1600);
  }
  on(3, { verb: "capture", mode: "ui", name: "m2_415_yard_stripped_client3" }, 25000);
  must(on(3, { verb: "use", className: "LF_ChopAction", entityName: "Roadmap_ChopShop" }, 10000), "collect");
  wait(2500);
  const after = cash(3);
  on(3, { verb: "capture", mode: "ui", name: "m2_415_paid_client3" }, 25000);
  if (after - before !== 300) fails.push(`the payout was ${after - before}, expected 300 (30 percent of 1000)`);

  on(2, { verb: "stand", entityName: "Roadmap_Police_WantedBoard" });
  wait(2500);
  on(2, { verb: "look" });
  on(2, { verb: "use", className: "LF_WantedBoardReadAction", entityName: "Roadmap_Police_WantedBoard" }, 10000);
  wait(2500);
  // #522: the officer's own screen carries the chop shop crime (the board is read on client 2, not the server).
  sees.expectText(2, "Chop shop", fails, "board after the chop");
  on(2, { verb: "capture", mode: "ui", name: "m2_415_board_after_client2" }, 25000);
  on(2, { verb: "ui" });
  on(2, { verb: "click", widget: "Close" });

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "chop proof: FAIL" : "chop proof: PASS (stolen, reported, plate checked, chopped, paid 300; confirm the server log for the scrap, the part items and the evidence report)");
  return fails.length ? 1 : 0;
}

export default {
  name: "chop-proof",
  usage: "chop-proof",
  run: (ctx) => runChopProof(),
};
