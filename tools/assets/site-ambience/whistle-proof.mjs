// Session proof for WK-W23 (#4183): the 06:00 shift whistle. One client stands at the whistle emitter in grid cell
// slice-4183 (a Coalman's Brow stand-in), the clock is set before, inside and after the whistle window, and the
// client's own [LFDebug:Atmosphere] lines must show the whistle start inside the window only, with the site name.
//
//   node tools/cli session up --mission grid ; node tools/assets/site-ambience/whistle-proof.mjs ; node tools/cli session down
import { readFileSync } from "node:fs";
import { arrive, clientBox, newestLog, sessionDir, sessionPaths } from "../../../agent-box/session.mjs";

const EMITTER = "Grid_slice_4183_CoalmansBrow_Whistle";
const PROJECT = "Work_ShiftWhistle.acp";

const h = await import("../../../agent-box/harness.mjs");
const dir = sessionDir();
const on = (n, spec, timeout) => {
  h.setBoxes([clientBox(dir, n)]);
  const r = h.sendCommand(spec, timeout);
  console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
  return r;
};
const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
const lines = () => {
  const f = newestLog(sessionPaths(dir).clients[0].logs);
  return f ? readFileSync(f, "utf8").split("\n").filter((l) => l.includes("[LFDebug:Atmosphere]")).map((l) => l.replace(/^.*\[LFDebug:Atmosphere\]\s*/, "").trim()) : [];
};
let seen = 0;
const step = (label, ms = 6000) => {
  wait(ms);
  const all = lines();
  const fresh = all.slice(seen);
  seen = all.length;
  console.log(`## ${label}`);
  for (const l of fresh) console.log(`    [LFDebug:Atmosphere] ${l}`);
  return fresh;
};
const fails = [];

arrive(on, 1, wait);
wait(6000);
on(1, { verb: "time", hour: 5.9 }, 10000);
on(1, { verb: "stand", entityName: EMITTER });
const before = step("05:54, before the whistle");
if (before.some((l) => l.startsWith("start " + EMITTER))) fails.push("the whistle sounded at 05:54");
on(1, { verb: "time", hour: 6.01 }, 10000);
const inside = step("06:00, the whistle window", 14000);
if (!inside.some((l) => l.startsWith("start " + EMITTER) && l.includes(PROJECT))) fails.push(`no "start ${EMITTER} ... ${PROJECT}" at 06:00`);
if (!inside.some((l) => l.startsWith("voice up " + EMITTER))) fails.push("the engine reported no voice for the whistle");
on(1, { verb: "time", hour: 6.5 }, 10000);
const after = step("06:30, after the window");
if (!after.some((l) => l.startsWith("stop " + EMITTER))) fails.push("the whistle did not stop after its window");
console.log(fails.length ? "whistle proof: FAIL\n  " + fails.join("\n  ") : "whistle proof: PASS (starts inside 06:00 to 06:01 only, engine voice up, stops after, site name in the log)");
process.exit(fails.length ? 1 : 0);
