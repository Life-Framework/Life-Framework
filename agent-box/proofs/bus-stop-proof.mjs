import { arrive, clientBox, parseState, sessionDir } from "../session.mjs";

// Bus stop on a running session, driven the way a player does it: at the grid's south stop, open the
// action list (no window), keep a picture, buy the ticket to the north stop with F, keep a picture of the hint, wait
// the bus out and check the fare was taken and the player now stands at the north stop.
//   node agent-box/session.mjs up --profile full
//   node agent-box/session.mjs bus-stop-proof
//   node agent-box/session.mjs down
const FROM = "Grid_bus_stops_R0_1";
const TO = "Grid_bus_stops_R0_2";
const ACTION = "LF_BusStopAction";
const CASH = 100;

async function runBusStopProof() {
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
  const state = () => parseState(on(1, { verb: "state" }, 10000).detail);

  arrive(on, 1, wait);
  // A fresh character is put back at its spawn a few seconds after registering: let that land first.
  wait(15000);
  must(on(1, { verb: "resetplayer", cash: CASH, keepInventory: true }), "give the player cash");
  for (let i = 0; i < 3; i++) {
    must(on(1, { verb: "stand", entityName: FROM }), "stand at the south stop");
    wait(2500);
    const z = Number(String(state().pos ?? "").split(",")[2]);
    if (z > 80 && z < 90) break;
  }
  const look = must(on(1, { verb: "look" }), "look");
  if (!look.detail.includes(ACTION)) fails.push(`the F key does not offer ${ACTION} at ${FROM}: ${look.detail.slice(0, 200)}`);
  // No window: the sign's own action list offers "Bus to <stop>" per other stop; F on the first buys it.
  on(1, { verb: "capture", mode: "ui", name: "bus_actions" }, 25000);
  must(on(1, { verb: "use", className: ACTION, entityName: FROM }, 10000), "buy the ticket from the action list");
  wait(1500);
  if (on(1, { verb: "ui", mode: "text" }, 10000).detail?.includes("BusTo_")) fails.push("a timetable window opened");
  on(1, { verb: "capture", mode: "ui", name: "bus_ticket" }, 25000);
  const paid = Number(state().cash);
  if (!(paid < CASH)) fails.push(`the fare was not taken: cash ${paid}`);

  // The wait (20 to 30 s) plus the fade.
  wait(34000);
  const after = state();
  const pos = String(after.pos ?? "").split(",").map(Number);
  console.log(`after the ride: cash ${after.cash}, pos ${after.pos}`);
  if (Number(after.cash) !== paid) fails.push(`the ride refunded or charged again: ${paid} -> ${after.cash}`);
  if (!(pos.length === 3 && pos[2] > 88)) fails.push(`the player is not at the north stop: pos ${after.pos}`);

  if (fails.length) {
    console.log(`bus-stop-proof: FAIL`);
    for (const f of fails) console.log(`  ${f}`);
    return 1;
  }
  console.log("bus-stop-proof: OK");
  return 0;
}

export default { name: "bus-stop-proof", usage: "bus-stop-proof", run: () => runBusStopProof() };
