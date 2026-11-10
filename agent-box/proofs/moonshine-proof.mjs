import { arrive, clientBox, sessionDir } from "../session.mjs";

// M2 #418/#420 on a running `up --clients 3` session with client 3 on the roster (`session.mjs roster 3`
// before up): client 3 clocks in at the police desk, then waits at the hideout door, outside the smell
// range. Clients 1 and 2 run the moonshine chain at the hidden site (Moonshine_Site): client 1 is handed
// one mash of goods, loads Moonshine_Barrel_2, both mash it, client 2 takes the wash, loads
// Moonshine_Still and both distil. Halfway through the distil the officer walks up to the site; the next
// 5 s sweep gives the smell cue (state 3 reads the toast). Shots: the still smoking (client 3) and the
// workers (client 1). Confirm `[LFDebug:Chains] ... smelled site Moonshine_Site` and `smell lead for
// site Moonshine_Site` in the server log, and that the first read (outside range) had no smell toast.
async function runMoonshineProof() {
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
  const barrel = { verb: "use", className: "LF_ChainStationAction", entityName: "Moonshine_Barrel_2" };
  const still = { verb: "use", className: "LF_ChainStationAction", entityName: "Moonshine_Still" };
  for (const n of [1, 2, 3]) arrive(on, n, wait);
  wait(8000);
  const give = on(1, { verb: "moonshine" }, 10000);
  if (give.status !== "ok" || !give.detail.includes("plums=4") || !give.detail.includes("cabbages=2")) fails.push(`goods not given: ${give.status} ${give.detail}`);
  on(3, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  on(3, { verb: "look" });
  const clockIn = on(3, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000);
  if (clockIn.status !== "ok") fails.push(`clock in returned ${clockIn.status} ${clockIn.detail}`);
  wait(3000);
  on(3, { verb: "stand", entityName: "Roadmap_Hideout_Door" });
  on(1, { verb: "stand", entityName: "Moonshine_Barrel_2", side: "e" });
  on(2, { verb: "stand", entityName: "Moonshine_Barrel_2", side: "w" });
  wait(2500);
  on(1, { verb: "look" });
  const load = on(1, barrel, 10000);
  if (load.status !== "ok") fails.push(`mash load returned ${load.status} ${load.detail}`);
  wait(1500);
  const rounds = [];
  for (let round = 1; round <= 6; round++) {
    rounds.push(on(1, barrel, 10000), on(2, barrel, 10000));
    wait(1500);
  }
  for (const r of rounds) if (r.status !== "ok") fails.push(`a mash unit returned ${r.status} ${r.detail}`);
  const collect = on(2, barrel, 10000);
  if (collect.status !== "ok") fails.push(`wash collect returned ${collect.status} ${collect.detail}`);
  wait(1500);
  on(1, { verb: "stand", entityName: "Moonshine_Still", side: "e" });
  on(2, { verb: "stand", entityName: "Moonshine_Still", side: "w" });
  wait(2500);
  const loadStill = on(2, still, 10000);
  if (loadStill.status !== "ok") fails.push(`still load returned ${loadStill.status} ${loadStill.detail}`);
  wait(1500);
  const before = on(3, { verb: "state" }, 10000).detail;
  if (/toast=#LF-Chain_Smell/.test(before)) fails.push(`the officer smelled the site from the hideout door: ${before}`);
  for (let round = 1; round <= 8; round++) {
    rounds.push(on(1, still, 10000), on(2, still, 10000));
    if (round === 3) on(3, { verb: "stand", entityName: "Moonshine_Barrel_3", side: "e" });
    wait(1500);
  }
  wait(6000);
  on(3, { verb: "look" });
  on(3, { verb: "capture", mode: "ui", name: "m2_418_still_smoke_officer_client3" }, 25000);
  on(1, { verb: "look" });
  on(1, { verb: "capture", mode: "ui", name: "m2_420_moonshine_workers_client1" }, 25000);
  const after = on(3, { verb: "state" }, 10000).detail;
  if (!/toast=#LF-Chain_Smell_Officer/.test(after)) fails.push(`the officer got no smell cue at the site: ${after}`);
  const take = on(2, still, 10000);
  if (take.status !== "ok") fails.push(`crate collect returned ${take.status} ${take.detail}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "moonshine proof: FAIL" : "moonshine proof: PASS (mash by two workers, distil, officer smells it from beside the barrels; confirm the server log for the smelled site and smell lead lines)");
  return fails.length ? 1 : 0;
}

export default {
  name: "moonshine-proof",
  usage: "moonshine-proof",
  run: (ctx) => runMoonshineProof(),
};
