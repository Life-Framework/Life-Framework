import { arrive, clientBox, sessionDir } from "../session.mjs";

// M2 #417 on a running `up --clients 2` session (bare DebugWorld): client 1 is handed 8 apples (dev
// gate), stands at Chains_Barrel_1, loads it, and works it. Client 2 joins the same barrel and the two
// alternate, one unit each per round. Eight units from two hands finish the barrel in four rounds,
// where one worker needs eight. Client 2 takes the product. Screenshots: the barrel part way (client 2)
// and ready (client 1). Confirm `[LFDebug:Chains] station Chains_Barrel_1 ready` after 8 units and the
// loaded, worked and collected lines in the server log.
async function runChainProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const action = { verb: "use", className: "LF_ChainStationAction", entityName: "Chains_Barrel_1" };
  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  const give = on(1, { verb: "apples" }, 10000);
  if (give.status !== "ok" || !give.detail.includes("added=8")) fails.push(`apples not given: ${give.status} ${give.detail}`);
  on(1, { verb: "stand", entityName: "Chains_Barrel_1", side: "e" });
  on(2, { verb: "stand", entityName: "Chains_Barrel_1", side: "w" });
  wait(2500);
  on(1, { verb: "look" });
  const load = on(1, action, 10000);
  if (load.status !== "ok") fails.push(`load returned ${load.status} ${load.detail}`);
  wait(1500);
  const rounds = [];
  for (let round = 1; round <= 4; round++) {
    rounds.push(on(1, action, 10000), on(2, action, 10000));
    if (round === 2) {
      wait(500);
      on(2, { verb: "look" });
      on(2, { verb: "capture", mode: "ui", name: "m2_417_barrel_half_client2" }, 25000);
    }
    wait(1500);
  }
  for (const r of rounds) if (r.status !== "ok") fails.push(`a work unit returned ${r.status} ${r.detail}`);
  wait(1000);
  on(1, { verb: "look" });
  on(1, { verb: "capture", mode: "ui", name: "m2_417_barrel_ready_client1" }, 25000);
  const collect = on(2, action, 10000);
  if (collect.status !== "ok") fails.push(`collect returned ${collect.status} ${collect.detail}`);
  wait(1500);
  const again = on(2, action, 10000);
  if (again.status === "ok") fails.push(`an empty barrel with no apples still offered an action: ${again.detail}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "chain proof: FAIL" : "chain proof: PASS (load, 8 units from two workers in 4 rounds, collect; confirm the server log shows ready after the 8th unit and one collect)");
  return fails.length ? 1 : 0;
}

export default {
  name: "chain-proof",
  usage: "chain-proof",
  run: (ctx) => runChainProof(),
};
