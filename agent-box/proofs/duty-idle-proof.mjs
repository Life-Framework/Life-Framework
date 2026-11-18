import { readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, newestLog, sessionDir, sessionPaths } from "../session.mjs";

// #3154: the duty idle pause (#3120, LF_DutyIdle.c) on a DEDICATED server. The sampler reads the
// server's copy of the officer's aim (ReadPose); a listen host reads its own, so only a remote client
// shows whether the turn of a head reaches the server. Client 1 (on the police roster) clocks in at
// the annex police desk (a fixture, not the town) and stands still in two phases, judged on the server log:
//   1. turning: it pitches its view 10 degrees every 5 s (a sawtooth, so no two samples 30 s apart
//      see the same aim) for 360 s, longer than the 300 s window plus one 30 s sample: no
//      "duty clock paused" line may appear.
//   2. control: it stops turning and keeps still; the pause line must appear within 400 s.
//   tools\cli session up --roster 1
//   node agent-box/session.mjs duty-idle-proof
//   tools\cli session down
// Runs about 13 minutes; the harness commands of both phases keep the session lane alive.
const DESK = "Annex_Police_Desk";
const PAUSED = /\[LFDebug:DutyIdle\][^\n]*duty clock paused/g;
const TURN_MS = 360000;
const CONTROL_MS = 400000;
const STEP_MS = 5000;

export function countPauses(text) {
  return (text.match(PAUSED) ?? []).length;
}

async function runDutyIdleProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 200)}`);
    return r;
  };
  const quiet = (spec, timeout) => {
    h.setBoxes([clientBox(dir, 1)]);
    return h.sendCommand(spec, timeout);
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const serverLog = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    return f ? readFileSync(f, "utf8") : "";
  };
  const fails = [];

  arrive(on, 1, wait);
  wait(8000);
  on(1, { verb: "stand", entityName: DESK, side: "s" });
  wait(2500);
  on(1, { verb: "look" });
  const clockIn = on(1, { verb: "use", className: "LF_DutyAction", entityName: DESK }, 10000);
  wait(3000);
  const st = on(1, { verb: "state" }, 10000).detail;
  if (clockIn.status !== "ok" || !/duty=1/.test(st)) {
    console.log(`duty-idle-proof: FAIL (not on duty: clock-in ${clockIn.status} ${clockIn.detail}; ${st}; is client 1 on the --roster?)`);
    return 1;
  }

  // Phase 1: turn the view only. Sawtooth pitch -40..+40 in 10 degree steps.
  const before = countPauses(serverLog());
  let pitch = 0;
  let turns = 0;
  const turnEnd = Date.now() + TURN_MS;
  while (Date.now() < turnEnd) {
    let step = 10;
    if (pitch >= 40) step = -80;
    const r = quiet({ verb: "input", action: "CharacterTurnUp", value: String(step) }, 10000);
    if (r.status === "ok") { pitch += step; turns++; }
    wait(STEP_MS);
  }
  const afterTurn = countPauses(serverLog());
  console.log(`phase 1 (turning ${TURN_MS / 1000} s, ${turns} turns): pause lines ${afterTurn - before}`);
  if (turns < 10) fails.push(`the turn input ran only ${turns} times`);
  if (afterTurn > before) fails.push(`the server paused the duty clock while the officer turned their view (${afterTurn - before} line(s)): the server does not see a remote client's aim`);

  // Phase 2 (control): stand still; the window runs out and the server pauses the clock.
  const controlEnd = Date.now() + CONTROL_MS;
  let paused = false;
  while (Date.now() < controlEnd) {
    wait(20000);
    quiet({ verb: "state" }, 10000);
    if (countPauses(serverLog()) > afterTurn) { paused = true; break; }
  }
  console.log(`phase 2 (still): paused=${paused}`);
  if (!paused) fails.push(`no duty clock pause within ${CONTROL_MS / 1000} s of standing still: the control did not pause, so phase 1 proves nothing`);

  if (fails.length) {
    console.log("duty-idle-proof: FAIL");
    for (const f of fails) console.log(`  ${f}`);
    return 1;
  }
  console.log("duty-idle-proof: PASS (turning the view keeps duty running on a dedicated server; standing still pauses it)");
  return 0;
}

export default {
  name: "duty-idle-proof",
  usage: "duty-idle-proof   (up --roster 1, one client; about 13 min)",
  run: () => runDutyIdleProof(),
};
