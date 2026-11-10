import { arrive, clientBox, sessionDir } from "../session.mjs";

// M2 #414 on a running `up --clients 3 --mission playtest --wait` session with client 2 on the roster
// (`roster 2` before `up`): client 1 owns the starter S105 and retrieves it from the garage; client 3
// hotwires it (the starter car comes out unlocked; the pick is #411/#412's proof) (the car is stolen); client 1 reports it; the officer (client 2) reads
// the stolen car on the wanted board, checks the plate, and recovers the car to the owner's garage.
async function runStolenProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what} returned ${r.status} ${r.detail}`);
    return r;
  };
  for (const n of [1, 2, 3]) arrive(on, n, wait);
  wait(8000);

  // The officer clocks in.
  on(2, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  must(on(2, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000), "clock in");
  wait(2500);

  // The owner takes the starter S105 out of the garage; the thief joins them at the car.
  on(1, { verb: "stand", entityName: "Roadmap_Garage" });
  wait(2500);
  must(on(1, { verb: "use", className: "LF_VehicleRetrieveAction", entityName: "Roadmap_Garage" }, 10000), "retrieve the car");
  wait(4000);
  on(3, { verb: "stand", entityName: "Roadmap_Garage" });
  wait(2500);
  on(3, { verb: "look" });
  on(3, { verb: "capture", mode: "ui", name: "m2_414_car_before_client3" }, 25000);
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
  on(3, { verb: "capture", mode: "ui", name: "m2_414_car_stolen_client3" }, 25000);

  // The owner reports it.
  on(1, { verb: "look" });
  must(on(1, { verb: "use", className: "LF_ReportStolenAction" }, 10000), "report the car stolen");
  wait(1500);
  on(1, { verb: "capture", mode: "ui", name: "m2_414_report_client1" }, 25000);

  // The officer reads the board.
  on(2, { verb: "stand", entityName: "Roadmap_Police_WantedBoard" });
  wait(2500);
  on(2, { verb: "look" });
  on(2, { verb: "use", className: "LF_WantedBoardReadAction", entityName: "Roadmap_Police_WantedBoard" }, 10000);
  wait(2500);
  if (!h.waitForText("STOLEN CAR", 10000).found) fails.push("no STOLEN CAR line on the officer's board");
  on(2, { verb: "capture", mode: "ui", name: "m2_414_board_client2" }, 25000);
  on(2, { verb: "ui" });
  on(2, { verb: "click", widget: "Close" });

  // The officer checks the plate at the car.
  on(2, { verb: "stand", entityName: "Roadmap_Garage" });
  wait(2500);
  on(2, { verb: "look" });
  must(on(2, { verb: "use", className: "LF_CheckPlateAction" }, 10000), "check the plate");
  wait(1000);
  on(2, { verb: "capture", mode: "ui", name: "m2_414_plate_check_client2" }, 25000);

  // The thief gets out; the officer recovers the car.
  on(3, { verb: "engine", entityName: "out" }, 10000);
  wait(3000);
  must(on(2, { verb: "use", className: "LF_RecoverVehicleAction" }, 10000), "recover the car");
  wait(2500);
  on(2, { verb: "capture", mode: "ui", name: "m2_414_recovered_client2" }, 25000);

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "stolen proof: FAIL" : "stolen proof: PASS (confirm the server log: reported stolen, plate check STOLEN, recovered)");
  return fails.length ? 1 : 0;
}

export default {
  name: "stolen-proof",
  usage: "stolen-proof",
  run: (ctx) => runStolenProof(),
};
