// agent-box/villages-night-shots.mjs - the villages and rural sites at 22:00 (sweep 2026-11-27), one run per `<tag>`:
//   node agent-box/villages-night-shots.mjs <before|after> [client] [spot,spot,...]
// Runs on a live island session after `harness flow island-villages-night` signed in. Each spot is a `capture world` (feet at target +
// offset, the target aimed at, the clock pinned to 22:00). The camera stands at eye height: the target is about 1.6 m over the ground
// and oy is the feet below it, so the feet land on the ground under the target. Prints the png path of each shot.
import { clientBox, sessionDir } from "./session.mjs";

const h = await import("./harness.mjs");
const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);

// name, target x y z, camera offset ox oy oz
export const SPOTS = [
  // Levie pub (house b03): the street front and the bar from the customers' side.
  ["levie_front", 7447, 139.6, 4719.8, 9, -1.8, 2],
  ["levie_front_n", 7445.5, 139.6, 4721, 3, -1.8, 8],
  ["levie_bar", 7442.4, 139.4, 4719.7, -5, -1.6, 0],
  ["levie_bar2", 7442.9, 139.4, 4719.4, -4.5, -1.6, -2.5],
  // Chotain pub: the fascia from the lane, the bar from the stools.
  ["chotain_front", 7060.7, 121.1, 6005.2, -3.7, -3, 5.9],
  ["chotain_bar", 7063.2, 119.9, 6002.6, 4, -1.6, 1],
  ["chotain_bar2", 7063.2, 119.9, 6002.6, 2, -1.6, 4],
  // Montignac fuel depot.
  ["montignac_load", 4610.9, 159.8, 7002.7, 3, -1.55, 3],
  ["montignac_sign", 4614.4, 160.2, 6993.2, 3, -1.9, 4],
  ["montignac_wide", 4610, 159.8, 6998, 9, -1.5, 9],
  ["montignac_hut", 4600.9, 160, 7002.7, 4, -1.6, 4],
  // Still barn and chop yard (neighbours on the same hillside).
  ["stillbarn_still", 5357.9, 22.2, 9901.1, 3, -1.6, 0],
  ["stillbarn_still2", 5357.9, 22.2, 9901.1, -3, -1.6, 0],
  ["stillbarn_launder", 5360, 22.2, 9894.1, 0, -1.6, 3],
  ["stillbarn_out", 5358, 22, 9900, 0, -1.6, 12],
  ["stillbarn_out2", 5358, 22, 9900, 12, -1.6, 0],
  ["chopyard_bench", 5374.7, 22.2, 9895.9, 3, -1.55, 3],
  ["chopyard_wide", 5372, 22.2, 9893, 8, -1.5, 8],
  // Farm compound.
  ["farm_plate", 6636.15, 117, 5981.1, 0, -1.7, 4.5],
  ["farm_strongbox", 6630.6, 116.8, 5967.5, 3, -1.55, 3],
  ["farm_door", 6633.2, 116.8, 5963.3, 0, -1.55, 4],
  ["farm_wide", 6636, 117, 5972, -9, -1.5, -6],
  // Sawmill.
  ["sawmill_desk", 3092, 119.9, 5252.1, 3.5, -1.4, 0],
  ["sawmill_desk2", 3092, 119.9, 5252.1, 0, -1.4, 3.5],
  ["sawmill_desk3", 3092.64, 119.9, 5250.82, -2.5, -1.45, 0],
  ["sawmill_desk4", 3092.64, 119.9, 5250.82, 0, -1.45, -2.5],
  ["sawmill_desk5", 3092.64, 119.9, 5250.82, 0, -1.45, 2.5],
  ["sawmill_sign", 3084.7, 120.4, 5255.3, 3, -1.9, 3],
  ["sawmill_yard", 3090, 120, 5250, 9, -1.5, 9],
  // Hilltop mast.
  ["mast_strongbox", 9950.9, 316.7, 2280, 3, -1.6, 3],
  ["mast_door", 9948.9, 316.4, 2282.8, -3, -1.6, 0],
  ["mast_wide", 9951, 316.7, 2283, 8, -1.5, 8],
  // Meaux speed trap.
  ["meaux_car", 4562.4, 13.4, 9394, 4, -1.5, 0],
  ["meaux_sign", 4558.1, 13.2, 9380, 3, -1.8, 0],
];
export const HOUR = 22;

const tag = process.argv[2];
if (!/^[a-z]+$/.test(tag ?? "")) {
  console.error("usage: node agent-box/villages-night-shots.mjs <before|after> [client] [spot,spot,...]");
  process.exit(2);
}
const client = Number(process.argv[3] ?? 1);
h.setBoxes([clientBox(sessionDir(), client)]);
const only = process.argv[4] ? process.argv[4].split(",") : null;
let bad = 0;
const first = h.sendCommand({ verb: "time", hour: HOUR }, 15000);
console.log(`time ${HOUR} -> ${first.status} ${first.detail}`);
wait(35000);
let n = 0;
for (const [spot, x, y, z, ox, oy, oz] of SPOTS) {
  if (only && !only.includes(spot)) continue;
  if (n > 0 && n % 6 === 0) {
    h.sendCommand({ verb: "time", hour: HOUR }, 15000);
    wait(2000);
  }
  n++;
  const name = `villages_${tag}_${spot}`;
  const r = h.sendCommand({ verb: "capture", mode: "world", name, x, y, z, ox, oy, oz, hour: HOUR }, 60000);
  console.log(`${name} -> ${r.status} ${String(r.detail).replace(/\s+/g, " ").slice(0, 200)}`);
  if (r.status !== "ok") bad++;
  wait(1500);
}
process.exit(bad ? 1 : 0);
