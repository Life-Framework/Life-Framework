import { spawnSync } from "node:child_process";
import { join } from "node:path";
import { ROOT, arrive, clientBox, clientSees, clientSeesPeer, judgeCarry, parsePeer, parseState, sessionDir } from "../session.mjs";

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
  // The proof moves the carrier with `stand`, a 17 m teleport no walking player makes; the hold
  // then moves the body 17 m in one tick. A client applies a teleported character (the carrier
  // or the body) up to several seconds late (QA: client 1 still read the body at the shot spot
  // 4 s on, another run client 2 read the carrier there). Read each client until the body sits
  // by the carrier, and print how long that took; a hold that never follows still fails.
  // Every read is the client's own copy (#522): the carrier id is a replicated prop, the position the
  // streamed body, so a hold the server keeps but never bumps fails here on the client that missed it.
  const settled = (n, carrierAt, budgetMs = 10000) => {
    const r = clientSees(on, wait, n, (line) => judgeCarry(line, 2, 1, carrierAt, 4), { verb: "peers", parse: (line) => line, budgetMs });
    console.log(`client ${n}: body ${r.fails.length ? "not " : ""}beside the carrier after ${r.seconds.toFixed(1)} s more`);
    return { line: r.line, fails: r.fails.map((f) => f.replace(/^client \d+ reads: /, "")) };
  };
  // The body's carrier as client n reads it, polled: the replicated value lands a moment after the server writes it.
  const carrierSeen = (n, want) => clientSeesPeer(on, wait, n, 2, (p) => (p.carrier === want ? [] : [`carrier=${p.carrier}, expected ${want}`]), { label: want ? "carried" : "dropped" });

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  // An annex desk on open ground, not a town till: a till sits on a counter in a closed kiosk, and
  // two characters teleported onto the same spot there overlapped and downed the shooter (#522).
  // Each player takes its own side of the desk so the two capsules never overlap.
  on(2, { verb: "stand", entityName: "Annex_Police_Desk", side: "n" });
  wait(2500);
  on(1, { verb: "stand", entityName: "Annex_Police_Desk", side: "s" });
  wait(2500);
  on(1, { verb: "shoot", playerId: 2 }, 10000);
  wait(2500);
  const patient = `Player${parseState(state(2)).player}`;
  need(parseState(state(2)).downed === "1", "client 2 was not downed");

  // Drag it a few metres across the annex (a 280 m teleport leaves the held body behind).
  on(1, { verb: "stand", entityName: patient });
  wait(2500);
  on(1, { verb: "look" });
  const drag = on(1, { verb: "use", className: "LF_DragAction", entityName: patient }, 10000);
  need(drag.status === "ok", `drag returned ${drag.status} ${drag.detail}`);
  wait(1500);
  for (const n of [1, 2]) fails.push(...carrierSeen(n, 1).fails);
  on(1, { verb: "stand", entityName: "Annex_Police_WantedBoard" });
  wait(4000);
  const carrierAt = at(1);
  const afterDrag = {};
  for (const n of [1, 2]) {
    const r = settled(n, carrierAt);
    afterDrag[n] = r.line;
    for (const f of r.fails) fails.push(`drag, client ${n}: ${f}`);
  }
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
      on(3, { verb: "stand", entityName: "Annex_Police_WantedBoard" });
      wait(4000);
      // Client 3 stands on the carrier's spot and shoves it a metre or two: judgeCarry takes the
      // carrier from client 3's own line.
      const r3 = settled(3, carrierAt);
      afterDrag3 = r3.line;
      for (const f of r3.fails) fails.push(`drag, late joiner: ${f}`);
    }
  }

  // Drop, then carry to the escort car and load.
  const drop = on(1, { verb: "use", className: "LF_DropAction", entityName: patient }, 10000);
  need(drop.status === "ok", `drop returned ${drop.status} ${drop.detail}`);
  wait(1500);
  for (const n of [1, 2]) fails.push(...carrierSeen(n, 0).fails);
  on(1, { verb: "use", className: "LF_CarryAction", entityName: patient }, 10000);
  wait(1500);
  for (const n of [1, 2]) fails.push(...carrierSeen(n, 1).fails);
  on(1, { verb: "stand", entityName: "Annex_Escort_Car" });
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
