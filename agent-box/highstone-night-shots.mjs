// agent-box/highstone-night-shots.mjs - the Highstone iron mine seen from its track at 22:00 (#5089), one run per `<tag>`:
//   node agent-box/highstone-night-shots.mjs <before|after> [client] [shot,shot,...]
// Needs a live island session with the client signed in (`harness flow island-outpost-night` signs in), as east-night-shots.mjs.
// Stands the character at the mine sign (so the terrain and the two beacon lamps stream in), pins 22:00, then takes `capture world`
// shots from the road: the camera feet on the probed ground at (cx, cz), the lens aimed at (tx, ty, tz).
import { clientBox, sessionDir } from "./session.mjs";

const h = await import("./harness.mjs");
const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);

export const HOUR = 22;
export const STAND = "Fit_highstone_mine_mine_sign";
// [name, [tx, ty, tz], [cx, cz]]
export const SHOTS = [
  // 90 m from the sign on the track, looking at the mine (the night-drive round's picture, #5089).
  ["road90", [4959, 116, 8460], [5040, 8430]],
  // The same spot, aimed at the turn-off beacon (52 m off, a little left of the mine).
  ["road90_lamp", [4992, 114.6, 8449], [5040, 8430]],
  // 150 m out on the track, looking up the road toward the turn-off.
  ["road150", [4992, 114.6, 8449], [5080, 8390]],
  // On the road at the turn-off, looking at the beacon (after) / the dark gap (before), from the south and from the north.
  ["turnoff_s", [4992, 114.6, 8449], [5001, 8418]],
  ["turnoff_n", [4992, 114.6, 8449], [5008, 8478]],
  // The bend where r05 meets r07, from the track below it.
  ["bend", [5013, 118.6, 8500], [5004, 8466]],
  // From the turn-off toward the clearing: the mine's own lamps beyond.
  ["clearing", [4959, 116, 8460], [4990, 8444]],
];

const tag = process.argv[2];
if (!/^[a-z]+$/.test(tag ?? "")) {
  console.error("usage: node agent-box/highstone-night-shots.mjs <before|after> [client]");
  process.exit(2);
}
const client = Number(process.argv[3] ?? 1);
const only = process.argv[4] ? process.argv[4].split(",") : SHOTS.map((s) => s[0]);
h.setBoxes([clientBox(sessionDir(), client)]);
const kv = (text) => Object.fromEntries([...String(text).matchAll(/(\w+)=(\S+)/g)].map((m) => [m[1], m[2]]));
let bad = 0;
const first = h.sendCommand({ verb: "time", hour: HOUR }, 15000);
console.log(`time ${HOUR} -> ${first.status} ${first.detail}`);
wait(35000);
const st = h.sendCommand({ verb: "stand", entityName: STAND }, 30000);
console.log(`stand ${STAND} -> ${st.status} ${String(st.detail).replace(/\s+/g, " ").slice(0, 120)}`);
wait(6000);
h.sendCommand({ verb: "time", hour: HOUR }, 15000);
wait(35000);
for (const [spot, [tx, ty, tz], [cx, cz]] of SHOTS) {
  if (!only.includes(spot)) continue;
  const g = h.sendCommand({ verb: "probe", mode: "ground", x: cx, z: cz }, 8000);
  const gy = Number(kv(g.detail).y);
  if (g.status !== "ok" || Number.isNaN(gy)) {
    console.log(`${spot}: ground probe failed ${g.status} ${g.detail}`);
    bad++;
    continue;
  }
  const name = `highstone_${tag}_${spot}`;
  const r = h.sendCommand({ verb: "capture", mode: "world", name, x: tx, y: ty, z: tz, ox: cx - tx, oy: gy - ty, oz: cz - tz, hour: HOUR }, 60000);
  console.log(`${name} (ground ${gy}) -> ${r.status} ${String(r.detail).replace(/\s+/g, " ").slice(0, 200)}`);
  if (r.status !== "ok") bad++;
  wait(1500);
}
process.exit(bad ? 1 : 0);
