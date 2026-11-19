import { arrive, clientBox, sessionDir } from "../session.mjs";

// Shoulder carry (#3263) on a running session, driven the way a player does it.
//   node agent-box/session.mjs up --profile full     (one client; add --clients 2 only for a second player)
//   node agent-box/session.mjs shoulder-carry
//   node agent-box/session.mjs down
// The load is a ShoulderCarryItem_Base placed in the slice-3263 debug-grid cell and named ShoulderCarry_Log.
// Player 1 hoists it; with --clients 2 player 2 stands beside and must see the carrier (late joiner C: join after the hoist).
const SITE = "ShoulderCarry_Log";
const ACTION = "LF_ShoulderHoistAction";

async function runShoulderCarryProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 200)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what} returned ${r.status} ${r.detail}`);
    return r;
  };

  arrive(on, 1, wait);
  must(on(1, { verb: "stand", entityName: SITE }), "stand at the site");
  wait(2500);
  const look = must(on(1, { verb: "look" }), "look");
  if (!look.detail.includes(ACTION)) fails.push(`the F key does not offer ${ACTION} at ${SITE}: ${look.detail.slice(0, 200)}`);
  must(on(1, { verb: "use", className: ACTION, entityName: SITE }, 10000), "use the action");
  wait(1500);
  const after = must(on(1, { verb: "look" }), "look after the hoist");
  if (after.detail.includes(ACTION)) fails.push("the hoist action is still offered while the load is on the shoulder");
  if (process.env.LF_SHOULDER_SECOND === "1") must(on(2, { verb: "seen" }), "player 2 sees the carrier");

  if (fails.length) {
    console.log(`shoulder-carry: FAIL`);
    for (const f of fails) console.log(`  ${f}`);
    return 1;
  }
  console.log("shoulder-carry: OK");
  return 0;
}

export default { name: "shoulder-carry", usage: "shoulder-carry", run: () => runShoulderCarryProof() };
