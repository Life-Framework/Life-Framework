import { join } from "node:path";
import { ROB_TARGET, arrive, clientBox, parseState, sessionDir } from "../session.mjs";

// M2 #427: N revives of a downed client in one session. Client 1 clocks in as a medic once; each
// run downs client 2 with a lethal hit, revives them, and times how long client 2 reads life=ALIVE.
// Every sample logs the life state and resilience/blood/health client 2 reads about itself and
// client 1 reads about client 2 (the server's own lines are `wake of ... +Ns` in the server log).
// `revive-proof [--runs N] [--limit s]`: fails when a run is not ALIVE within the limit.
async function runReviveProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const flag = (name, dflt) => { const i = process.argv.indexOf(name); return i > 0 ? Number(process.argv[i + 1]) : dflt; };
  const runs = flag("--runs", 10);
  const limit = flag("--limit", 12);
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = (n) => on(n, { verb: "state" }, 10000).detail;
  const fails = [];
  const results = [];
  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  on(1, { verb: "stand", entityName: "Roadmap_Hospital_Duty_Desk" });
  wait(2500);
  on(1, { verb: "look" });
  on(1, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Hospital_Duty_Desk" }, 10000);
  wait(2500);
  if (!parseState(state(1)).faction?.includes("MEDIC")) fails.push("client 1 did not clock in as a medic");
  for (let run = 1; run <= runs; run++) {
    on(2, { verb: "stand", entityName: ROB_TARGET });
    wait(2500);
    on(1, { verb: "stand", entityName: ROB_TARGET });
    wait(2500);
    on(1, { verb: "shoot", playerId: 2 }, 10000);
    wait(2500);
    const patient = `Player${parseState(state(2)).player}`;
    const down = parseState(state(2));
    if (down.downed !== "1") { fails.push(`run ${run}: client 2 was not downed`); results.push({ run, ok: false }); continue; }
    on(1, { verb: "stand", entityName: patient });
    wait(2500);
    on(1, { verb: "look" });
    const t0 = Date.now();
    const revive = on(1, { verb: "use", className: "LF_ReviveAction", entityName: patient }, 10000);
    let woke = null;
    const samples = [];
    let relapse = null;
    // Sample until `limit` seconds after the revive, past the wake: a patient who wakes and is
    // downed again seconds later (the #427 relapse) must still fail the run.
    while ((Date.now() - t0) / 1000 < limit) {
      const own = parseState(state(2));
      const t = ((Date.now() - t0) / 1000).toFixed(1);
      samples.push(`${t}s own: life=${own.life} downed=${own.downed} res=${own.res} blood=${own.blood} hp=${own.hp}`);
      if (own.life === "ALIVE" && own.downed === "0" && woke === null) woke = Number(t);
      else if (woke !== null && (own.life !== "ALIVE" || own.downed === "1")) relapse = `${t}s life=${own.life} downed=${own.downed}`;
      wait(1000);
    }
    console.log(`RUN ${run}: revive=${revive.status} woke=${woke === null ? "NEVER" : woke + "s"} | ${samples.join(" | ")}`);
    results.push({ run, ok: woke !== null && !relapse, woke, relapse });
    if (woke === null) fails.push(`run ${run}: never woke within ${limit} s`);
    if (relapse) fails.push(`run ${run}: woke at ${woke} s, then relapsed at ${relapse}`);
    wait(1500);
  }
  console.log(`revive runs: ${JSON.stringify(results)}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "revive proof: FAIL" : `revive proof: PASS (${runs} revives, all awake within ${limit} s)`);
  return fails.length ? 1 : 0;
}

export default {
  name: "revive-proof",
  usage: "revive-proof",
  run: (ctx) => runReviveProof(),
};
