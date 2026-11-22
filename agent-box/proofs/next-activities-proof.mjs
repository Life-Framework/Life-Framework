import { arrive, clientBox, sessionDir } from "../session.mjs";

// What to do next (F2, Next tab) on the island, the way a new player uses it.
//   node agent-box/session.mjs up --mission island --profile full
//   node agent-box/session.mjs next-activities-proof
//   node agent-box/session.mjs down
// The client opens the file on the Next tab, waits (bounded) for the list, shoots it, picks the mining run and presses
// Start: step one ("Take the miner job") puts the waypoint on the miner desk at Coalman's Brow (shot). The client then
// stands at the desk and takes the job; the step passes on the server and step two ("Buy a pickaxe") moves the waypoint
// to the hardware store (shot). Server lines to read after: `[LFDebug:Activities] ... started mining`, `passed mining step
// job`, `is guided to mining step pickaxe`.
const ANSWER_BUDGET_MS = 15000;
const DESK = "Fit_coalmans_brow_mine_mine_desk";

async function runNextActivitiesProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 300)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what} returned ${r.status} ${r.detail}`);
    return r;
  };
  const state = () => on(1, { verb: "activities", words: ["state"] });

  arrive(on, 1, wait);
  wait(6000);
  on(1, { verb: "guide", words: ["close"] });
  wait(500);

  must(on(1, { verb: "playermenu", words: ["open", "next"] }), "open the file on NEXT");
  let rows = "";
  for (let waited = 0; waited < ANSWER_BUDGET_MS && !rows.includes("mining:"); waited += 1000) {
    rows = state().detail;
    if (!rows.includes("mining:")) wait(1000);
  }
  if (!rows.includes("mining:1")) fails.push(`the mining run is not listed as available: ${rows}`);
  if (rows.includes("police:") || rows.includes("medic:")) fails.push(`a civilian is offered duty: ${rows}`);
  wait(800);
  on(1, { verb: "capture", mode: "ui", name: "next_tab" }, 25000);

  const index = rows.replace(/.*ids=/, "").split(",").findIndex((r) => r.startsWith("mining:"));
  must(on(1, { verb: "activities", words: ["pick", String(Math.max(index, 0))] }), "pick the mining run");
  wait(800);
  on(1, { verb: "capture", mode: "ui", name: "next_tab_mining" }, 25000);
  must(on(1, { verb: "activities", words: ["go"] }), "press Start");
  wait(2500);
  if (!state().detail.includes("mining:2")) fails.push("the mining run did not start");
  must(on(1, { verb: "playermenu", words: ["close"] }), "close the file");
  wait(1500);
  on(1, { verb: "capture", mode: "world", name: "mining_step1_waypoint" }, 25000);

  // Step one: take the miner job at the desk; the waypoint moves to the hardware store.
  must(on(1, { verb: "stand", entityName: DESK }), "stand at the miner desk");
  wait(2500);
  const look = on(1, { verb: "look" });
  const action = (look.detail.match(/LF_[A-Za-z]*(Job|SignUp|Workplace|Hire)[A-Za-z]*/) || [])[0];
  if (!action) fails.push(`no job action offered at the desk: ${look.detail.slice(0, 300)}`);
  else must(on(1, { verb: "use", className: action, entityName: DESK }, 10000), "take the miner job");
  wait(4500);
  on(1, { verb: "capture", mode: "world", name: "mining_step2_waypoint" }, 25000);

  if (fails.length) {
    console.log("next-activities-proof: FAIL");
    for (const f of fails) console.log(`  ${f}`);
    return 1;
  }
  console.log("next-activities-proof: OK (read the shots and the [LFDebug:Activities] lines)");
  return 0;
}

export default { name: "next-activities-proof", usage: "next-activities-proof", run: () => runNextActivitiesProof() };
