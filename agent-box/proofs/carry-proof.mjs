import { spawnSync } from "node:child_process";
import { existsSync } from "node:fs";
import { join } from "node:path";
import { ROOT, arrive, clientBox, clientSeesPeer, parseState, sessionDir } from "../session.mjs";

// #261, load only (drag and carry were cut 2026-11-15): player 1 downs player 2 beside the annex
// escort car and loads the body straight into a cargo seat. Both clients, and a late joiner, read
// the body seated; a world shot at the car shows the seated body (its head included) for the PR.
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
  const at = (n) => String(parseState(state(n)).pos ?? "0,0,0").split(",").map(Number);
  const fails = [];
  const need = (cond, text) => { if (!cond) fails.push(text); };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  // Each player takes its own side of the car so the two capsules never overlap (#522).
  on(2, { verb: "stand", entityName: "Annex_Escort_Car", side: "n" });
  wait(2500);
  on(1, { verb: "stand", entityName: "Annex_Escort_Car", side: "s" });
  wait(2500);
  on(1, { verb: "shoot", playerId: 2 }, 10000);
  wait(2500);
  const patient = `Player${parseState(state(2)).player}`;
  need(parseState(state(2)).downed === "1", "client 2 was not downed");

  on(1, { verb: "stand", entityName: patient });
  wait(2500);
  on(1, { verb: "look" });
  const [cx, cy, cz] = at(1);
  const load = on(1, { verb: "use", className: "LF_LoadPatientAction", entityName: patient }, 10000);
  need(load.status === "ok", `load returned ${load.status} ${load.detail}`);

  // The seat is the owner's to take: the server asks client 2 to get in (MoveInVehicleOwner), the
  // client seats itself and the compartment state replicates back out. Read each client until it
  // sees the body seated, and print how long that took.
  const seated = {};
  for (const n of [1, 2]) {
    const r = clientSeesPeer(on, wait, n, 2, (p) => (p.seat === "1" ? [] : [`seat=${p.seat}`]), { label: "seated", budgetMs: 15000 });
    seated[n] = r.line;
    console.log(`client ${n}: body ${r.fails.length ? "not " : ""}seated after ${r.seconds.toFixed(1)} s`);
    for (const f of r.fails) fails.push(`seat, ${f}`);
  }
  on(1, { verb: "capture", mode: "world", name: "m2_261_loaded_world", x: cx, y: cy + 0.8, z: cz, ox: 3.5, oy: 1.6, oz: 0 }, 25000);

  // A late joiner reads the seated body too.
  if (!process.argv.includes("--no-late-join")) {
    const already = existsSync(clientBox(dir, 3)) && on(3, { verb: "state" }, 8000).status === "ok";
    const joined = already ? { status: 0 } : spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "join", "3"], { windowsHide: true, stdio: "inherit" });
    if (joined.status !== 0) {
      fails.push("client 3 did not join late");
    } else {
      arrive(on, 3, wait);
      wait(8000);
      const r3 = clientSeesPeer(on, wait, 3, 2, (p) => (p.seat === "1" ? [] : [`seat=${p.seat}`]), { label: "seated", budgetMs: 15000 });
      seated[3] = r3.line;
      for (const f of r3.fails) fails.push(`seat, late joiner: ${f}`);
    }
  }

  console.log(`seated ${JSON.stringify(seated)}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "carry proof: FAIL" : "carry proof: PASS (load; seen by both clients and a late joiner)");
  return fails.length ? 1 : 0;
}

export default {
  name: "carry-proof",
  usage: "carry-proof [--no-late-join]",
  run: (ctx) => runCarryProof(),
};
