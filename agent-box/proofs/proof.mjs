import { ROB_TARGET, arrive, clientBox, judgeProof, sessionDir } from "../session.mjs";

async function runProof() {
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

  // Arrive: both register and spawn in town as civilians (there is no faction
  // screen). The form is gone once a character exists, so a second proof skips it.
  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  // Client 2 joins the police and goes on duty through the debug menu.
  on(2, { verb: "open", menu: "DebugMenu", mode: "show" });
  wait(800);
  on(2, { verb: "click", widget: "FactionPoliceButton" });
  on(2, { verb: "open", menu: "DebugMenu", mode: "hide" });
  wait(1500);

  const before1 = state(1);
  const before2 = state(2);
  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "look" });
  const rob = on(1, { verb: "use", className: "LF_RobAction", entityName: ROB_TARGET }, 10000);
  wait(2500);
  const after1 = state(1);
  const after2 = state(2);

  console.log("");
  console.log(`before  client 1: ${before1}`);
  console.log(`before  client 2: ${before2}`);
  console.log(`after   client 1: ${after1}`);
  console.log(`after   client 2: ${after2}`);
  const fails = judgeProof(before1, after1, after2);
  if (rob.status !== "ok") fails.unshift(`rob returned ${rob.status} ${rob.detail}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "proof: FAIL" : "proof: PASS (client 1 robbed, client 2 got the alert)");
  return fails.length ? 1 : 0;
}

export default {
  name: "proof",
  usage: "proof",
  film: true,
  run: (ctx) => runProof(),
};
