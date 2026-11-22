import { arrive, clientBox, sessionDir } from "../session.mjs";
import { armAndDraw } from "./fuel-robbery-proof.mjs";

// Map robbery alarm, two clients on the Everon world (`up --mission island --profile full --clients 2`):
// client 1 robs the Saint-Philippe fuel till through the attendant; client 2, on the other side of the
// square, opens the map on the fuel station and photographs the badge with its pulsing red ring and "!",
// then hovers it for the card ("Robbery in progress (alarm just raised)"). Client 1's map shows the same.
// The server's `[LFDebug:MapAlarm] robbery in progress at FuelTill:saint-philippe.fuel` line is the proof
// the alarm rang; the pictures are the proof it reached the other client.
//   node agent-box/session.mjs map-alarm-shots [--name <prefix>]
const CLERK = "Fit_saint_philippe_fuel_clerk";
const TILL = "Fit_saint_philippe_fuel_till";
// The FuelTill place on Place_saint-philippe_fuel.layer (a runtime view centre for the picture only).
const FUEL = { x: 4616.6, z: 10658.7 };
const ZOOM = 5.5;

async function runMapAlarmShots() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const at = process.argv.indexOf("--name");
  const prefix = at >= 0 && process.argv[at + 1] ? process.argv[at + 1] : "mapalarm";
  const shot = (n, name) => on(n, { verb: "capture", mode: "ui", name: `${prefix}_${name}` }, 30000);
  const scene = (n, arg, value) => on(n, { verb: "dialogue", arg, value }, 10000).detail;
  const map = (n, ...words) => on(n, { verb: "map", words }, 15000);
  const fails = [];
  const expect = (cond, what) => { if (!cond) fails.push(what); };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  on(1, { verb: "resetcrime" }, 10000);
  on(1, { verb: "stand", entityName: TILL, side: "s" });
  wait(2500);

  // Before: client 2's map on the fuel station, quiet.
  map(2, "open");
  wait(2500);
  map(2, "pan", String(FUEL.x), String(FUEL.z), String(ZOOM));
  wait(2000);
  shot(2, "0_before");
  map(2, "close");

  armAndDraw(on, 1, wait);
  on(1, { verb: "use", className: "LF_DialogueTalkAction", entityName: CLERK }, 10000);
  wait(1500);
  scene(1, "skip");
  wait(800);
  scene(1, "pick", "2"); // "This is a robbery."
  wait(1500);
  scene(1, "skip");
  wait(800);
  scene(1, "pick", "0"); // the demand starts the hold and the dispatch
  wait(3000);
  const hold = on(1, { verb: "robhold" }, 10000).detail;
  expect(/^running/.test(hold), `the hold runs on client 1 (got: ${hold})`);

  // After: client 2's map shows the ring; hover the badge for the card.
  const opened = map(2, "open");
  expect(opened.status === "ok", `client 2's map opens (got ${opened.detail})`);
  wait(2500);
  map(2, "pan", String(FUEL.x), String(FUEL.z), String(ZOOM));
  wait(2000);
  shot(2, "1_ring");
  wait(500);
  shot(2, "2_ring_pulse");
  const hover = map(2, "hover", String(FUEL.x), String(FUEL.z));
  expect(hover.status === "ok", `the hover reaches the fuel badge (got ${hover.detail})`);
  wait(1200);
  shot(2, "3_card");
  map(2, "close");

  map(1, "open");
  wait(2500);
  map(1, "pan", String(FUEL.x), String(FUEL.z), String(ZOOM));
  wait(2000);
  shot(1, "4_robber_map");
  map(1, "close");

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "map alarm shots: FAIL" : "map alarm shots: PASS (read the shots; grep the server log for '[LFDebug:MapAlarm]')");
  return fails.length ? 1 : 0;
}

export default {
  name: "map-alarm-shots",
  usage: "map-alarm-shots [--name <prefix>]   (up --mission island --profile full --clients 2)",
  run: () => runMapAlarmShots(),
};
