// agent-box/outpost-night-shots.mjs - the north outpost camp pictures of #4978, one run per `<tag>`:
//   node agent-box/outpost-night-shots.mjs <before|after> [client]
// Runs on a live island session after `harness flow island-outpost-night` signed in. Each spot is a `capture world` (feet at target + offset,
// the target aimed at, the clock pinned to the hour), at 23:00 and at noon, so the pairs line up. Prints the png path of each shot.
import { clientBox, sessionDir } from "./session.mjs";

const h = await import("./harness.mjs");
const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);

// name, target x y z, camera offset ox oy oz (oy is the feet below the aimed point: the ground sits about 1.4 m under an NPC's face)
export const SPOTS = [
  ["quartermaster", 7657.9, 17.3, 8399.2, 3.2, -1.4, -4.2],
  ["board", 7656.8, 17.0, 8392.6, 2.6, -1.0, -4.0],
  ["fire", 7661.5, 16.5, 8395.6, -1.5, -0.6, -5.5],
  ["gate", 7660.2, 17.2, 8388.7, 0.0, -1.2, -7.0],
  ["camp", 7660.5, 17.0, 8396.0, 0.0, -1.0, -13.0],
];
export const HOURS = [23, 12];

const tag = process.argv[2];
if (!/^[a-z]+$/.test(tag ?? "")) {
  console.error("usage: node agent-box/outpost-night-shots.mjs <before|after> [client]");
  process.exit(2);
}
const client = Number(process.argv[3] ?? 1);
h.setBoxes([clientBox(sessionDir(), client)]);
const only = process.argv[4] ? process.argv[4].split(",") : null;
let bad = 0;
for (const hour of HOURS) {
  const first = h.sendCommand({ verb: "time", hour }, 15000);
  console.log(`time ${hour} -> ${first.status} ${first.detail}`);
  wait(3000);
  for (const [spot, x, y, z, ox, oy, oz] of SPOTS) {
    if (only && !only.includes(spot)) continue;
    const name = `outpost_${tag}_${spot}_${hour}`;
    const r = h.sendCommand({ verb: "capture", mode: "world", name, x, y, z, ox, oy, oz, hour }, 60000);
    console.log(`${name} -> ${r.status} ${String(r.detail).replace(/\s+/g, " ").slice(0, 200)}`);
    if (r.status !== "ok") bad++;
    wait(1500);
  }
}
process.exit(bad ? 1 : 0);
