// tools/livery/spike/session-shots.mjs - livery spike #4226: full-profile session pictures of the spike
// liveries. Needs `node tools/livery/spike/build.mjs --mask tools/livery/spike/police-tiled.json`, the
// spike cell placed (Grid_livery_followup_R0_1 own-mask UAZ469, _R0_2 S1203, the plain police UAZ469 at
// 254,62) and a running session:
//   node agent-box/session.mjs up --mission grid --profile full --wait
//   node tools/livery/spike/session-shots.mjs
//   node agent-box/session.mjs down
// The player takes the S1203's wheel with the engine running, then world captures stand it (and the van
// it sits in) beside each vehicle. There is no throttle: the harness refuses CarThrust on both sides
// (agent-box/harness.mjs MENU_ACTIONS and the in-game input allow list), so nothing is driven under power.
import { arrive, clientBox, sessionDir } from "../../../agent-box/session.mjs";

const h = await import("../../../agent-box/harness.mjs");
const dir = sessionDir();
const CELL = process.env.LF_LIVERY_CELL || "grid:livery-followup";
const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
const on = (spec, timeout = 20000) => {
  h.setBoxes([clientBox(dir, 1)]);
  const r = h.sendCommand(spec, timeout);
  console.log(`${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
  return r;
};

arrive((_n, spec, timeout) => on(spec, timeout), 1, wait);
wait(5000);
on({ verb: "click", widget: "WelcomeGuideDismissButton" });
on({ verb: "teleport", words: [CELL] });
wait(2500);
on({ verb: "stand", entityName: "Grid_livery_followup_R0_2" });
wait(1200);
const seat = on({ verb: "engine", entityName: "Grid_livery_followup_R0_2" });
wait(3500);
const shots = [
  ["livery_uaz_own_side", 258.7, 1.0, 61.0, -1.5, 0.6, -6.5],
  ["livery_uaz_police_side", 254.0, 1.0, 62.0, 1.5, 0.6, -6.5],
  ["livery_s1203_side", 258.5, 1.0, 64.0, -1.0, 0.6, 6.5],
];
for (const [name, x, y, z, ox, oy, oz] of shots) {
  on({ verb: "capture", mode: "world", name, x, y, z, ox, oy, oz, hour: 14 }, 30000);
  wait(800);
}
console.log(`SUMMARY livery-session-shots seat=${seat.status} shots=${shots.length}`);
