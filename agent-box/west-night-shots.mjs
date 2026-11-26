// agent-box/west-night-shots.mjs - the west and coast sites at 22:00 (sweep 2026-11-27), one run per `<tag>`:
//   node agent-box/session.mjs up --mission island --profile full --wait
//   node agent-box/harness.mjs --client 1 flow island-night-fronts      (signs in)
//   node agent-box/west-night-shots.mjs <before|after> [client] [spot,spot,...]
//   node agent-box/session.mjs down
// Each spot is a `capture world` (feet at target + offset, the target aimed at, the clock pinned to the hour). The target y is the
// ground plus 1.5 m and the offset's y is -1.5, so the camera stands on the ground at eye height. Prints the png path of each shot.
import { clientBox, sessionDir } from "./session.mjs";

const h = await import("./harness.mjs");
const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);

// name, target x y z, camera offset ox oy oz
export const SPOTS = [
  ["dock-crane", 4408.0, 3.1, 11111.9, 6.0, -1.5, -4.0],
  ["wholesale-front", 4462.9, 11.1, 11193.6, -10.0, -5.2, 0.0],
  ["wholesale-buyer", 4467.6, 7.3, 11193.9, 4.0, -1.45, 0.0],
  ["wholesale-iron", 4466.3, 7.3, 11200.8, 4.0, -1.45, 0.0],
  ["wholesale-board", 4467.7, 7.4, 11189.7, 4.0, -1.45, 0.0],
  ["warehouse-board", 4459.0, 3.7, 11090.9, -1.0, -1.5, -5.0],
  ["warehouse-crates", 4461.0, 4.0, 11073.0, 6.0, -1.5, 0.0],
  ["smokehouse-sign", 4455.9, 3.5, 11088.8, -2.0, -1.5, -4.0],
  ["smokehouse-smoker", 4451.7, 5.0, 11076.6, -6.0, -1.5, -3.0],
  ["smokehouse-fire", 4447.9, 3.1, 11070.3, -3.0, -1.5, -3.0],
  ["containers-hut", 4431.0, 3.2, 11100.5, -6.0, -1.5, -4.0],
  ["containers-nine", 4430.8, 3.1, 11118.0, -6.0, -1.5, 0.0],
  ["shipment-lane", 4444.0, 2.8, 11101.5, -9.0, -1.5, 0.0],
  ["raid-lane", 4452.0, 2.8, 11099.0, 0.0, -1.5, 8.0],
  ["cove-spot", 4021.0, 2.4, 10699.0, 3.0, -1.5, -4.0],
  ["cove-bench", 4017.5, 2.5, 10698.0, -3.0, -1.5, -4.0],
  ["cove-sign", 4012.0, 2.7, 10695.5, 4.0, -1.5, -3.0],
  ["lake-spot", 9519.0, 209.95, 2997.0, 3.0, -1.5, -4.0],
  ["lake-bench", 9521.1, 210.26, 2995.6, 3.0, -1.5, -3.0],
  ["lake-sign", 9520.1, 210.1, 2994.1, 4.0, -1.5, -3.0],
  ["scrap-recycler", 5034.5, 29.7, 10858.5, -5.0, -1.5, -4.0],
  ["scrap-dealer", 5013.6, 30.0, 10870.4, 0.0, -1.5, -5.0],
  ["scrap-dump", 5035.0, 29.7, 10888.0, 0.0, -1.5, -9.0],
  ["fuel-sign", 5303.3, 23.0, 9837.3, 4.0, -1.5, -4.0],
  ["fuel-payphone", 5304.3, 23.0, 9834.2, 0.0, -1.5, -4.0],
  ["fuel-drop", 5295.9, 22.8, 9829.0, 3.0, -1.5, -3.0],
  ["farm-sign", 5142.5, 177.8, 7031.2, 4.0, -1.5, -4.0],
  ["farm-payphone", 5146.7, 177.8, 7027.6, 0.0, -1.5, -4.0],
  ["farm-drop", 5130.4, 177.3, 7021.2, 3.0, -1.5, -3.0],
  // second pass: the same targets from a clearer side, and the lanterns head on
  ["scrap-recycler-b", 5034.5, 29.7, 10858.5, 0.0, -1.5, -6.0],
  ["scrap-recycler-c", 5034.5, 29.7, 10858.5, 7.0, -1.5, 2.0],
  ["warehouse-board-b", 4459.0, 3.7, 11090.9, -7.0, -1.5, -3.0],
  ["smokehouse-sign-b", 4455.9, 3.5, 11088.8, -6.0, -1.5, -2.0],
  ["containers-clerk", 4434.6, 3.8, 11101.0, 4.0, -1.5, -4.0],
  ["cove-lantern", 4018.3, 2.4, 10698.0, 2.0, -1.5, -3.0],
  ["lake-lantern", 9521.9, 210.4, 2996.9, 2.0, -1.5, -3.0],
  ["dock-crane-b", 4408.0, 3.1, 11111.9, 0.0, -1.5, 6.0],
  // third pass: the outposts' drops from the open side, and the farm plate from its front
  ["fuel-drop-b", 5295.9, 22.8, 9829.0, -3.5, -1.5, 1.0],
  ["fuel-wide", 5299.2, 23.0, 9830.7, -9.0, -1.5, -8.0],
  ["farm-sign-front", 5142.5, 177.8, 7031.2, 5.0, -1.5, 0.0],
  ["farm-drop-b", 5130.4, 177.3, 7021.2, 4.0, -1.5, 3.0],
  ["farm-drop-c", 5130.4, 177.3, 7021.2, 5.0, -1.5, -2.0],
  ["farm-lane-wide", 5144.6, 177.8, 7029.4, 8.0, -1.5, 8.0],
];
export const HOURS = [22];

const tag = process.argv[2];
if (!/^[a-z]+$/.test(tag ?? "")) {
  console.error("usage: node agent-box/west-night-shots.mjs <before|after> [client] [spot,spot]");
  process.exit(2);
}
const client = Number(process.argv[3] ?? 1);
h.setBoxes([clientBox(sessionDir(), client)]);
const only = process.argv[4] ? process.argv[4].split(",") : null;
let bad = 0;
for (const hour of HOURS) {
  const first = h.sendCommand({ verb: "time", hour }, 15000);
  console.log(`time ${hour} -> ${first.status} ${first.detail}`);
  wait(40000); // the night lights tick every 30 s
  let n = 0;
  for (const [spot, x, y, z, ox, oy, oz] of SPOTS) {
    if (only && !only.includes(spot)) continue;
    if (++n % 6 === 0) {
      h.sendCommand({ verb: "time", hour }, 15000);
      wait(2000);
    }
    const name = `west_${tag}_${spot.replace(/-/g, "_")}_${hour}`;
    const r = h.sendCommand({ verb: "capture", mode: "world", name, x, y, z, ox, oy, oz, hour }, 60000);
    console.log(`${name} -> ${r.status} ${String(r.detail).replace(/\s+/g, " ").slice(0, 200)}`);
    if (r.status !== "ok") bad++;
    wait(1500);
  }
}
process.exit(bad ? 1 : 0);
