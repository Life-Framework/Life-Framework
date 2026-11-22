import { arrive, clientBox, sessionDir } from "../session.mjs";

// The gathering-job desk on the island: a fresh character walks up to the miner desk at Coalman's Brow, the F key
// offers "Take the job: Miner", using it puts the job on them, and the desk then offers "Quit the job: Miner".
//   node agent-box/session.mjs up --mission island --profile full
//   node agent-box/session.mjs job-desk-proof
//   node agent-box/session.mjs down
// Server line to read after: `[LFDebug:Jobs] <uid> took MINER at the desk`.
const DESK = "Fit_coalmans_brow_mine_mine_desk";
const ACTION = "LF_WorkplaceJobAction";

async function runJobDeskProof() {
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

  arrive(on, 1, wait);
  wait(6000);
  on(1, { verb: "guide", words: ["close"] });
  wait(500);
  must(on(1, { verb: "stand", entityName: DESK }), "stand at the miner desk");
  wait(2500);
  on(1, { verb: "guide", words: ["close"] });
  wait(800);
  const look = on(1, { verb: "look" });
  if (!look.detail.includes(ACTION)) fails.push(`the desk does not offer ${ACTION}: ${look.detail.slice(0, 300)}`);
  on(1, { verb: "capture", mode: "ui", name: "job_desk_take" }, 25000);
  must(on(1, { verb: "use", className: ACTION, entityName: DESK }, 10000), "take the miner job");
  wait(3000);
  on(1, { verb: "capture", mode: "ui", name: "job_desk_taken" }, 25000);

  if (fails.length) {
    console.log("job-desk-proof: FAIL");
    for (const f of fails) console.log(`  ${f}`);
    return 1;
  }
  console.log("job-desk-proof: OK (read the shots and the [LFDebug:Jobs] line)");
  return 0;
}

export default { name: "job-desk-proof", usage: "job-desk-proof", run: () => runJobDeskProof() };
