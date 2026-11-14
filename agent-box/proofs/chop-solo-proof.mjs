import { arrive, clientBox, parseSeen, parseState, clientSees, sessionDir } from "../session.mjs";

// Chop shop for one client (#601, #415, #495), on a running `up --mission playtest --wait` session.
//   node agent-box/session.mjs up --mission playtest --wait
//   node agent-box/session.mjs chop-solo-proof
//   node agent-box/session.mjs down
// chop-proof needs three clients; this is the same yard with one:
//   A. a car that is not stolen (the lot's display UAZ) is refused with the "Only stolen cars" toast
//      (LF-Chop_NotStolen) and stays in the yard untouched (#601).
//   B. the lock-up car is lockpicked and hotwired: the thief is wanted level 1 (#495), the car is
//      driven (the harness teleports the seated thief) into the yard, worked and collected: +300 (#415).
const PAYOUT = 300;

async function runChopSoloProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const st = () => parseState(on(1, { verb: "state" }, 10000).detail);
  const fails = [];
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what} returned ${r.status} ${r.detail}`);
    return r;
  };

  arrive(on, 1, wait);
  wait(6000);

  // B. steal and chop.
  on(1, { verb: "stand", entityName: "Roadmap_Lockpick_Car" });
  wait(2500);
  on(1, { verb: "lockpick" }, 10000);
  wait(1500);
  on(1, { verb: "use", className: "LF_LockpickAction", entityName: "Roadmap_Lockpick_Car" }, 10000);
  wait(23000);
  for (let a = 2; a <= 4; a++) {
    const s = clientSees(on, wait, 1, (v) => (v.veh[0] && v.veh[0].lock === "0" ? [] : ["locked"]), { radius: 8, budgetMs: 0 });
    if (!s.fails.length) break;
    on(1, { verb: "lockpick" }, 10000);
    on(1, { verb: "use", className: "LF_LockpickAction", entityName: "Roadmap_Lockpick_Car" }, 10000);
    wait(23000);
  }
  must(on(1, { verb: "engine", entityName: "Roadmap_Lockpick_Car" }, 10000), "get in the lock-up car");
  wait(4000);
  let started;
  for (let a = 1; a <= 5; a++) {
    on(1, { verb: "use", className: "LF_HotwireAction", entityName: "Roadmap_Lockpick_Car" }, 10000);
    wait(17000);
    on(1, { verb: "engine" }, 10000);
    wait(4000);
    started = on(1, { verb: "engine" }, 10000);
    if (started.status === "ok" && started.detail.includes("on=1")) break;
  }
  if (!started || !started.detail.includes("on=1")) fails.push(`the lock-up car did not run: ${started && started.detail}`);
  const wanted = st().wanted;
  console.log(`wanted level after the hotwire: ${wanted}`);
  if (!(wanted >= 1)) fails.push(`the thief is wanted level ${wanted} after the hotwire, expected at least 1 (#495)`);

  on(1, { verb: "stand", entityName: "Roadmap_ChopShop" });
  wait(3000);
  on(1, { verb: "engine", entityName: "out" }, 10000);
  wait(3000);
  const before = st().cash;
  on(1, { verb: "look" });
  must(on(1, { verb: "use", className: "LF_ChopAction", entityName: "Roadmap_ChopShop" }, 10000), "load the car");
  wait(1500);
  for (let i = 0; i < 6; i++) {
    must(on(1, { verb: "use", className: "LF_ChopAction", entityName: "Roadmap_ChopShop" }, 10000), `work ${i + 1}`);
    wait(1600);
  }
  must(on(1, { verb: "use", className: "LF_ChopAction", entityName: "Roadmap_ChopShop" }, 10000), "collect");
  wait(2500);
  const after = st().cash;
  if (after - before !== PAYOUT) fails.push(`carried cash went ${before} -> ${after}, expected +${PAYOUT}`);

  // A. after the yard is empty again, a car that is not stolen is refused.
  on(1, { verb: "stand", entityName: "Lot_Display_uaz469" });
  wait(2500);
  must(on(1, { verb: "engine", entityName: "Lot_Display_uaz469" }, 10000), "seat in the lot car");
  wait(3000);
  on(1, { verb: "stand", entityName: "Roadmap_ChopShop" });
  wait(3000);
  on(1, { verb: "engine", entityName: "out" }, 10000);
  wait(3000);
  on(1, { verb: "look" });
  on(1, { verb: "use", className: "LF_ChopAction", entityName: "Roadmap_ChopShop" }, 10000);
  wait(2000);
  const refused = st();
  console.log(`refusal toast: ${refused.toast}`);
  if (!/Chop_NotStolen/.test(refused.toast)) fails.push(`toast after chopping a car that is not stolen was ${refused.toast}, expected LF-Chop_NotStolen`);
  const seenA = parseSeen(on(1, { verb: "seen", radius: 10 }, 10000).detail);
  if (!seenA || seenA.chop !== -1) fails.push(`the refused car started a job: chop=${seenA && seenA.chop}`);
  if (seenA && seenA.veh.length < 1) fails.push("the refused car is gone from the yard");

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "chop-solo-proof: FAIL" : `chop-solo-proof: PASS (not-stolen car refused with LF-Chop_NotStolen; hotwire raised wanted to ${wanted}; chopped, +${PAYOUT})`);
  return fails.length ? 1 : 0;
}

export default { name: "chop-solo-proof", usage: "chop-solo-proof", run: () => runChopSoloProof() };
