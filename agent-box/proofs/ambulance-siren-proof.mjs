import { arrive, clientBox, sessionDir } from "../session.mjs";

// Ambulance siren (#923) on a running session, driven the way a player does it.
//   node agent-box/session.mjs up --profile full     (one client; add --clients 2 only for a second player)
//   node agent-box/session.mjs ambulance-siren-proof
//   node agent-box/session.mjs down
// Stands at the hospital ambulance and checks the F menu offers the siren action (an off-duty
// walker may not see it, so this proves the entity and the action are wired, not the duty gate).
const SITE = "Roadmap_Hospital_Ambulance";
const ACTION = "LF_SirenModeAction";

async function runAmbulanceSirenProof() {
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

  if (fails.length) {
    console.log(`ambulance-siren-proof: FAIL`);
    for (const f of fails) console.log(`  ${f}`);
    return 1;
  }
  console.log("ambulance-siren-proof: OK");
  return 0;
}

export default { name: "ambulance-siren-proof", usage: "ambulance-siren-proof", run: () => runAmbulanceSirenProof() };
