import { arrive, clientBox, clientSees, sessionDir } from "../session.mjs";

// M2 #412 on a running `up --clients 2` session (debug mission): client 1 picks the lock-up car with a
// lockpick, sits at the wheel, fails to start it (the ignition needs its key), hotwires it and starts the
// engine. Client 2 stands by as a witness. The pick takes up to 20 s and the hotwire up to 15 s at level 1.
// Confirm `[LFDebug:Lockpick] ... OPENED`, `[LFDebug:Hotwire] ... success=1` and `marked stolen` in the server log.
// Client side (#522): client 2, the witness, reads the nearest vehicle (the car) locked before the pick,
// unlocked after it, and running and marked stolen after the hotwire, each from its own replicated copy.
async function runHotwireProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  on(2, { verb: "stand", entityName: "Roadmap_Lockpick_Car" });
  on(1, { verb: "stand", entityName: "Roadmap_Lockpick_Car" });
  wait(2500);
  const look = on(1, { verb: "look" });
  const car = (check) => (s) => {
    const v = s.veh[0];
    if (!v) return ["no vehicle within reach"];
    return check(v);
  };
  const lockedBefore = clientSees(on, wait, 2, car((v) => (v.lock === "1" && v.eng === "0" ? [] : [`car lock=${v.lock} eng=${v.eng}, expected locked and off`])), { radius: 8, label: "before the pick" });
  on(1, { verb: "capture", mode: "ui", name: "m2_412_car_before_client1" }, 25000);
  const give = on(1, { verb: "lockpick" }, 10000);
  wait(1500);
  const pick = on(1, { verb: "use", className: "LF_LockpickAction", entityName: "Roadmap_Lockpick_Car" }, 10000);
  wait(23000);
  // A pick can fail or snap (that is the design): work the lock again, up to three more times.
  for (let attempt = 2; attempt <= 4 && clientSees(on, wait, 1, car((v) => (v.lock === "0" ? [] : ["locked"])), { radius: 8, budgetMs: 0 }).fails.length; attempt++) {
    on(1, { verb: "lockpick" }, 10000);
    on(1, { verb: "use", className: "LF_LockpickAction", entityName: "Roadmap_Lockpick_Car" }, 10000);
    wait(23000);
  }
  const unlocked = clientSees(on, wait, 2, car((v) => (v.lock === "0" ? [] : [`car still lock=${v.lock}`])), { radius: 8, label: "after the pick" });
  const getIn = on(1, { verb: "engine", entityName: "Roadmap_Lockpick_Car" }, 10000);
  wait(4000);
  const noKey = on(1, { verb: "engine" }, 10000);
  wait(3000);
  const noKeyState = on(1, { verb: "engine" }, 10000);
  // A hotwire can fail (that is the design): retry up to five times, then ask for the engine.
  let wire;
  let started;
  for (let attempt = 1; attempt <= 5; attempt++) {
    wire = on(1, { verb: "use", className: "LF_HotwireAction", entityName: "Roadmap_Lockpick_Car" }, 10000);
    wait(17000);
    on(1, { verb: "engine" }, 10000);
    wait(4000);
    started = on(1, { verb: "engine" }, 10000);
    if (started.status === "ok" && started.detail.includes("on=1")) break;
  }
  const running = clientSees(on, wait, 2, car((v) => (v.eng === "1" && v.stolen === "1" ? [] : [`car eng=${v.eng} stolen=${v.stolen}, expected running and stolen`])), { radius: 8, label: "after the hotwire" });
  console.log(`client sees: witness before ${lockedBefore.line}`);
  console.log(`client sees: witness after pick ${unlocked.line}`);
  console.log(`client sees: witness after hotwire ${running.line}`);
  fails.push(...lockedBefore.fails, ...unlocked.fails, ...running.fails);
  on(1, { verb: "capture", mode: "ui", name: "m2_412_car_driven_client1" }, 25000);
  on(2, { verb: "capture", mode: "ui", name: "m2_412_car_driven_client2" }, 25000);
  if (give.status !== "ok" || !give.detail.includes("added=5")) fails.push(`lockpick not given: ${give.status} ${give.detail}`);
  if (pick.status !== "ok") fails.push(`pick returned ${pick.status} ${pick.detail}`);
  if (getIn.status !== "ok") fails.push(`seat returned ${getIn.status} ${getIn.detail} (look: ${look.detail})`);
  if (noKeyState.status === "ok" && noKeyState.detail.includes("on=1")) fails.push(`the engine started without a key or a hotwire: ${noKeyState.detail}`);
  if (wire.status !== "ok") fails.push(`hotwire returned ${wire.status} ${wire.detail}`);
  if (started.status !== "ok" || !started.detail.includes("on=1")) fails.push(`the engine did not run after the hotwire: ${started.status} ${started.detail} (a failed roll needs a rerun)`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "hotwire proof: FAIL" : "hotwire proof: PASS (picked, no start without the key, hotwired, engine running; the witness client read the car locked, unlocked, then running and stolen; confirm the server log for the stolen mark)");
  return fails.length ? 1 : 0;
}

export default {
  name: "hotwire-proof",
  usage: "hotwire-proof",
  run: (ctx) => runHotwireProof(),
};
