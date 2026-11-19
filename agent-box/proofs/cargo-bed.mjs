import { arrive, clientBox, sessionDir } from "../session.mjs";

// Truck cargo bed (#3265) on a running session, driven the way a player does it.
//   node agent-box/session.mjs up --profile full --clients 2
//   node agent-box/session.mjs cargo-bed
//   node agent-box/session.mjs down
// Placed in the slice-3265 debug-grid cell: a log named CargoBed_Log and the truck named CargoBed_Truck.
// Player 1 hoists the log, loads it at the tailgate, unloads it again; with LF_CARGO_SECOND=1 player 2 stands
// by the truck and must see the stack ("seen"). A late joiner joins after the load and runs the same look.
const LOG = "CargoBed_Log";
const TRUCK = "CargoBed_Truck";
const HOIST = "LF_ShoulderHoistAction";
const BED = "LF_CargoBedAction";

async function runCargoBedProof() {
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

  arrive(on, 1, wait);
  must(on(1, { verb: "stand", entityName: LOG }), "stand at the log");
  wait(2500);
  must(on(1, { verb: "use", className: HOIST, entityName: LOG }, 10000), "hoist the log");
  wait(1500);
  must(on(1, { verb: "stand", entityName: TRUCK }), "stand at the tailgate");
  wait(2500);
  const atTruck = must(on(1, { verb: "look" }), "look at the truck");
  if (!atTruck.detail.includes(BED)) fails.push(`the F key does not offer ${BED} at ${TRUCK}: ${atTruck.detail.slice(0, 200)}`);
  must(on(1, { verb: "use", className: BED, entityName: TRUCK }, 10000), "load the log");
  wait(1500);
  if (process.env.LF_CARGO_SECOND === "1") must(on(2, { verb: "seen" }), "player 2 sees the stack");
  must(on(1, { verb: "use", className: BED, entityName: TRUCK }, 10000), "unload the log");
  wait(1500);

  if (fails.length) {
    console.log("cargo-bed: FAIL");
    for (const f of fails) console.log(`  ${f}`);
    return 1;
  }
  console.log("cargo-bed: OK");
  return 0;
}

export default { name: "cargo-bed", usage: "cargo-bed", run: () => runCargoBedProof() };
