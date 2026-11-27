// agent-box/outpost-approach-night-shots.mjs - the north outpost's last 230 m at 22:00 (night-drive round, #5089 follow-up), one run per `<tag>`:
//   node agent-box/outpost-approach-night-shots.mjs <before|after> [client] [shot,shot,...]
// Needs a live island session with the client signed in (`harness flow island-outpost-night` signs in), as east-night-shots.mjs.
// Stands the character at the gate lamp (so the terrain and the lights stream in), pins 22:00, then takes `capture world` shots from the
// approach: the camera feet on the probed ground at (cx, cz), the lens aimed at (tx, ty, tz).
import { clientBox, sessionDir } from "./session.mjs";

const h = await import("./harness.mjs");
const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);

export const HOUR = 22;
export const STAND = "Fit_outpost_north_camp_outpost_lamp_gate_s";
// [name, [tx, ty, tz], [cx, cz]]
export const SHOTS = [
  // Where the AI road graph ends (the night-drive car stopped here, 230 m from the gate), looking at the gate.
  ["roadend", [7662, 18, 8389], [7452, 8286]],
  // The same spot, aimed at the first beacon (14 m on).
  ["roadend_beacon", [7466, 46, 8292], [7447, 8283]],
  // From the road end, aimed at the middle beacon (120 m on).
  ["roadend_mid", [7560, 34, 8344], [7452, 8286]],
  // On the slope, 100 m from the gate.
  ["slope100", [7662, 18, 8389], [7568, 8345]],
  // At the gate from the south, as the first night-drive round's picture.
  ["gate", [7664, 18, 8389], [7664, 8362]],
];

const tag = process.argv[2];
if (!/^[a-z]+$/.test(tag ?? "")) {
  console.error("usage: node agent-box/outpost-approach-night-shots.mjs <before|after> [client] [shot,shot,...]");
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
  const name = `outpost_${tag}_${spot}`;
  const r = h.sendCommand({ verb: "capture", mode: "world", name, x: tx, y: ty, z: tz, ox: cx - tx, oy: gy - ty, oz: cz - tz, hour: HOUR }, 60000);
  console.log(`${name} (ground ${gy}) -> ${r.status} ${String(r.detail).replace(/\s+/g, " ").slice(0, 200)}`);
  if (r.status !== "ok") bad++;
  wait(1500);
}
process.exit(bad ? 1 : 0);
