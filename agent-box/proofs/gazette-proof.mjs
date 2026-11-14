import { arrive, clientBox, sessionDir } from "../session.mjs";

// Gazette (#792) on a running session, driven the way a player does it.
//   node agent-box/session.mjs up --profile full     (one client; add --clients 2 only for a second player)
//   node agent-box/session.mjs gazette-proof
//   node agent-box/session.mjs down
// TODO(scaffold): name the placed entity and the action, judge the result from `state` or `seen`,
// and keep the two shots: they are the PR's "after" pictures (tools\cli evidence).
const SITE = "TODO(scaffold)_EntityName";
const ACTION = "LF_GazetteAction";

async function runGazetteProof() {
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
    console.log(`gazette-proof: FAIL`);
    for (const f of fails) console.log(`  ${f}`);
    return 1;
  }
  console.log("gazette-proof: OK");
  return 0;
}

export default { name: "gazette-proof", usage: "gazette-proof", run: () => runGazetteProof() };
