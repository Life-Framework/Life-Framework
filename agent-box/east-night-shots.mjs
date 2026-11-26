// agent-box/east-night-shots.mjs - the east industry sites at night (sweep 2026-11-27), one run per `<tag>`:
//   node agent-box/east-night-shots.mjs <before|after> [client] [site,site,...]
// Needs a live island session with the client signed in (`harness flow island-outpost-night` signs in).
//   node agent-box/session.mjs up --mission island --profile full --wait
//   node agent-box/harness.mjs --client 1 flow island-outpost-night
//   node agent-box/east-night-shots.mjs before
//   node agent-box/session.mjs down
// Each stop stands the character at the site (so the terrain and props stream in), then takes `capture world` shots at 22:00: the camera
// feet on the probed ground at (cx, cz), the lens aimed at (tx, ty, tz). Prints the png path of each shot.
import { clientBox, sessionDir } from "./session.mjs";

const h = await import("./harness.mjs");
const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);

// site: stand entity, then shots [name, [tx, ty, tz], [cx, cz]]
export const SITES = {
  smelter: {
    stand: "Fit_eastcompound_smelter_smelter_furnace",
    shots: [
      ["furnace", [6442.5, 163.7, 6489.8], [6447, 6484]],
      ["furnace2", [6442.5, 163.7, 6489.8], [6438, 6496]],
      ["sign", [6437.6, 164.3, 6495.4], [6441, 6490]],
      ["yard", [6443, 163.6, 6494], [6452, 6486]],
      ["lamp", [6443.7, 163, 6493.5], [6447, 6488]],
    ],
  },
  eastrise: {
    stand: "Fit_eastrise_mine_mine_sign",
    shots: [
      ["sign", [6684, 151.4, 7915], [6684, 7909]],
      ["sign2", [6684, 151.4, 7915], [6684, 7921]],
      ["ore", [6685, 150.2, 7929.5], [6685, 7921]],
      ["pick", [6678.5, 150.5, 7920], [6682, 7913]],
      ["lamp", [6679.8, 150.3, 7926.4], [6679, 7920]],
    ],
  },
  highstone: {
    stand: "Fit_highstone_mine_mine_sign",
    shots: [
      ["sign", [4959, 115.6, 8460], [4959, 8454]],
      ["sign2", [4959, 115.6, 8460], [4959, 8466]],
      ["ore", [4958, 116.9, 8473.5], [4958, 8465]],
      ["pick", [4953, 116, 8465.5], [4957, 8460]],
      ["lamp", [4954.8, 116.4, 8471.4], [4954, 8465]],
    ],
  },
  coalmans: {
    stand: "Fit_coalmans_brow_mine_mine_desk",
    shots: [
      ["desk", [4340, 35.7, 10407.4], [4346, 10401]],
      ["desk2", [4340, 35.7, 10407.4], [4334, 10401]],
      ["furnace", [4340, 34.5, 10398], [4346, 10392]],
      ["sign", [4343, 36.2, 10413], [4343, 10407]],
      ["board", [4326.8, 36.4, 10403], [4333, 10407]],
      ["ore", [4333, 36, 10412], [4333, 10405]],
      ["lamp", [4334.4, 35.8, 10410.4], [4334, 10404]],
    ],
  },
  powerplant: {
    stand: "Fit_powerplant_buyer_coal_clerk",
    shots: [
      ["buyer", [5872, 5.8, 9800.5], [5872, 9808]],
      ["buyer2", [5872, 5.8, 9800.5], [5866, 9806]],
      ["sign", [5866.5, 6.5, 9807.5], [5866, 9813]],
      ["yard", [5878, 5, 9805], [5884, 9798]],
    ],
  },
  truckyard: {
    stand: "Fit_ne_compound_truckyard_examiner",
    shots: [
      ["examiner", [5076, 28.9, 10840], [5081, 10835]],
      ["hut", [5086.6, 28.6, 10844.3], [5086, 10836]],
      ["gatesign", [5024.1, 29.6, 10848.3], [5028, 10843]],
      ["trucks", [5044, 29, 10841.8], [5050, 10835]],
    ],
  },
};
export const HOUR = 22;

const tag = process.argv[2];
if (!/^[a-z]+$/.test(tag ?? "")) {
  console.error("usage: node agent-box/east-night-shots.mjs <before|after> [client] [site,site,...]");
  process.exit(2);
}
const client = Number(process.argv[3] ?? 1);
const only = process.argv[4] ? process.argv[4].split(",") : Object.keys(SITES);
h.setBoxes([clientBox(sessionDir(), client)]);
const kv = (text) => Object.fromEntries([...String(text).matchAll(/(\w+)=(\S+)/g)].map((m) => [m[1], m[2]]));
let bad = 0;
const first = h.sendCommand({ verb: "time", hour: HOUR }, 15000);
console.log(`time ${HOUR} -> ${first.status} ${first.detail}`);
wait(35000);
for (const site of only) {
  const def = SITES[site];
  if (!def) continue;
  const st = h.sendCommand({ verb: "stand", entityName: def.stand }, 30000);
  console.log(`stand ${def.stand} -> ${st.status} ${String(st.detail).replace(/\s+/g, " ").slice(0, 120)}`);
  wait(6000);
  h.sendCommand({ verb: "time", hour: HOUR }, 15000);
  wait(3000);
  for (const [spot, [tx, ty, tz], [cx, cz]] of def.shots) {
    const g = h.sendCommand({ verb: "probe", mode: "ground", x: cx, z: cz }, 8000);
    const gy = Number(kv(g.detail).y);
    if (g.status !== "ok" || Number.isNaN(gy)) {
      console.log(`${site}_${spot}: ground probe failed ${g.status} ${g.detail}`);
      bad++;
      continue;
    }
    const name = `east_${tag}_${site}_${spot}`;
    const r = h.sendCommand({ verb: "capture", mode: "world", name, x: tx, y: ty, z: tz, ox: cx - tx, oy: gy - ty, oz: cz - tz, hour: HOUR }, 60000);
    console.log(`${name} (ground ${gy}) -> ${r.status} ${String(r.detail).replace(/\s+/g, " ").slice(0, 200)}`);
    if (r.status !== "ok") bad++;
    wait(1500);
  }
}
process.exit(bad ? 1 : 0);
