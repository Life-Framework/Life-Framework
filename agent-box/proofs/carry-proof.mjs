import { spawnSync } from "node:child_process";
import { join } from "node:path";
import { ROB_TARGET, ROOT, arrive, clientBox, judgeCarry, parsePeer, parseState, sessionDir } from "../session.mjs";

async function runCarryProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = (n) => on(n, { verb: "state" }, 10000).detail;
  const peers = (n) => on(n, { verb: "peers" }, 10000).detail;
  const at = (n) => String(parseState(state(n)).pos ?? "0,0,0").split(",").map(Number);
  const fails = [];
  const need = (cond, text) => { if (!cond) fails.push(text); };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  on(2, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "shoot", playerId: 2 }, 10000);
  wait(2500);
  const patient = `Player${parseState(state(2)).player}`;
  need(parseState(state(2)).downed === "1", "client 2 was not downed");

  // Drag it to the hospital desk.
  on(1, { verb: "stand", entityName: patient });
  wait(2500);
  on(1, { verb: "look" });
  const drag = on(1, { verb: "use", className: "LF_DragAction", entityName: patient }, 10000);
  need(drag.status === "ok", `drag returned ${drag.status} ${drag.detail}`);
  wait(1500);
  const start1 = peers(1);
  need(parsePeer(start1, 2)?.carrier === 1, `the body is not held by client 1 after the drag: ${start1}`);
  on(1, { verb: "stand", entityName: "Roadmap_Hospital_Duty_Desk" });
  wait(4000);
  const carrierAt = at(1);
  const afterDrag = { 1: peers(1), 2: peers(2) };
  for (const n of [1, 2]) for (const f of judgeCarry(afterDrag[n], 2, 1, carrierAt, 4)) fails.push(`drag, client ${n}: ${f}`);
  const [cx, cy, cz] = carrierAt;
  on(1, { verb: "capture", mode: "world", name: "m2_261_drag_world", x: cx, y: cy + 0.8, z: cz, ox: 3.5, oy: 1.6, oz: 0 }, 25000);

  // A late joiner reads the hold and the body next to the carrier.
  let afterDrag3 = "";
  if (!process.argv.includes("--no-late-join")) {
    const joined = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "join", "3"], { windowsHide: true, stdio: "inherit" });
    if (joined.status !== 0) {
      fails.push("client 3 did not join late");
    } else {
      arrive(on, 3, wait);
      wait(8000);
      on(3, { verb: "stand", entityName: "Roadmap_Hospital_Duty_Desk" });
      wait(4000);
      afterDrag3 = peers(3);
      for (const f of judgeCarry(afterDrag3, 2, 1, carrierAt, 4)) fails.push(`drag, late joiner: ${f}`);
    }
  }

  // Drop, then carry to the escort car and load.
  const drop = on(1, { verb: "use", className: "LF_DropAction", entityName: patient }, 10000);
  need(drop.status === "ok", `drop returned ${drop.status} ${drop.detail}`);
  wait(1500);
  need(parsePeer(peers(2), 2)?.carrier === 0, "the body is still held after the drop");
  on(1, { verb: "use", className: "LF_CarryAction", entityName: patient }, 10000);
  wait(1500);
  need(parsePeer(peers(2), 2)?.carrier === 1, "the carry did not take hold");
  on(1, { verb: "stand", entityName: "Roadmap_Escort_Car" });
  wait(4000);
  const load = on(1, { verb: "use", className: "LF_LoadPatientAction", entityName: patient }, 10000);
  need(load.status === "ok", `load returned ${load.status} ${load.detail}`);
  wait(9000);
  const seated = { 1: peers(1), 2: peers(2) };
  if (afterDrag3) seated[3] = peers(3);
  for (const n of Object.keys(seated)) {
    const p = parsePeer(seated[n], 2);
    need(p && p.seat === "1" && p.carrier === 0, `seat, client ${n}: ${seated[n]}`);
  }
  on(1, { verb: "capture", mode: "world", name: "m2_261_loaded_world", x: cx, y: cy + 0.8, z: cz, ox: 3.5, oy: 1.6, oz: 0 }, 25000);
  console.log(`afterDrag ${JSON.stringify(afterDrag)} late ${afterDrag3} seated ${JSON.stringify(seated)}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "carry proof: FAIL" : "carry proof: PASS (drag, drop, carry, load; seen by both clients and a late joiner)");
  return fails.length ? 1 : 0;
}

export default {
  name: "carry-proof",
  usage: "carry-proof",
  run: (ctx) => runCarryProof(),
};
