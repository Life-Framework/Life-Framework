import { arrive, clientBox, parseState, sessionDir } from "../session.mjs";

// Courier parcel demo (#619) on `up --clients 2`. Client 1 signs up at the post counter and
// takes a parcel run, so it carries the parcel. Client 2 orbits a camera on client 1 and
// captures the carry pose from outside. Client 1 then opens the inventory and captures the
// parcel's description. A third capture shows the registry desk props.
// `parcel-proof [--name <shot prefix>]`
async function runParcelProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const flag = (name, dflt) => { const i = process.argv.indexOf(name); return i > 0 ? process.argv[i + 1] : dflt; };
  const prefix = flag("--name", "parcel_619");
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const at = (n) => String(parseState(on(n, { verb: "state" }, 10000).detail).pos ?? "0,0,0").split(",").map(Number);
  const fails = [];

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  on(1, { verb: "stand", entityName: "Roadmap_Job_Depot" });
  wait(1500);
  on(1, { verb: "use", className: "LF_JobSignUpAction", entityName: "Roadmap_Job_Depot" }, 10000);
  wait(1500);
  const run = on(1, { verb: "use", className: "LF_JobStartRunAction", entityName: "Roadmap_Job_Depot" }, 10000);
  if (run.status !== "ok") fails.push(`start run returned ${run.status} ${run.detail}`);
  wait(2000);
  const [x, y, z] = at(1);
  // The pose from outside: client 2 is placed two metres from the carrier, level with the chest.
  on(2, { verb: "capture", mode: "world", name: `${prefix}_carry`, x: x, y: y + 1, z: z, ox: 2.2, oy: 0.4, oz: 0.8 }, 25000);
  wait(1500);
  on(1, { verb: "input", action: "Inventory" });
  wait(2500);
  on(1, { verb: "capture", mode: "ui", name: `${prefix}_inventory` }, 25000);
  on(1, { verb: "input", action: "Inventory" });
  wait(1500);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "parcel proof: FAIL" : "parcel proof: done (read the captures)");
  return fails.length ? 1 : 0;
}

export default {
  name: "parcel-proof",
  usage: "parcel-proof [--name <shot prefix>]",
  run: () => runParcelProof(),
};
