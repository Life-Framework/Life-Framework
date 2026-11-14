import { arrive, clientBox, parseState, sessionDir } from "../session.mjs";

// Dropped-parcel demo (#920). Client 1 takes a parcel run, shots the carry, drops the parcel on
// purpose and shots the glow on the ground and the toast; then reads the log for the toast line.
// `parcel-drop-proof [--name <shot prefix>]`
async function run() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const flag = (name, dflt) => { const i = process.argv.indexOf(name); return i > 0 ? process.argv[i + 1] : dflt; };
  const prefix = flag("--name", "parcel_920");
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  arrive(on, 1, wait);
  wait(8000);
  on(1, { verb: "stand", entityName: "Roadmap_Job_Depot" });
  wait(1500);
  on(1, { verb: "use", className: "LF_JobSignUpAction", entityName: "Roadmap_Job_Depot" }, 10000);
  wait(1500);
  const runStart = on(1, { verb: "use", className: "LF_JobStartRunAction", entityName: "Roadmap_Job_Depot" }, 10000);
  if (runStart.status !== "ok") fails.push(`start run ${runStart.status} ${runStart.detail}`);
  wait(2000);
  const [x, y, z] = String(parseState(on(1, { verb: "state" }, 10000).detail).pos ?? "0,0,0").split(",").map(Number);
  const drop = on(1, { verb: "parceldrop" }, 10000);
  if (drop.status !== "ok") fails.push(`parcel drop ${drop.status} ${drop.detail}`);
  wait(2500);
  // The reply names where the carrier stood; the parcel lands within a metre of it.
  const at = /at=<?\s*([-\d.]+)[ ,]+([-\d.]+)[ ,]+([-\d.]+)/.exec(drop.detail);
  const [px, py, pz] = at ? [Number(at[1]), Number(at[2]), Number(at[3])] : [x, y, z];
  on(1, { verb: "time", hour: 21 });
  wait(1500);
  on(1, { verb: "capture", mode: "world", name: `${prefix}_dropped`, x: px, y: py + 0.2, z: pz, ox: 1.4, oy: 1.2, oz: 1.4, hour: 21 }, 25000);
  on(1, { verb: "capture", mode: "ui", name: `${prefix}_toast` }, 25000);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "parcel drop proof: FAIL" : "parcel drop proof: done (read the captures)");
  return fails.length ? 1 : 0;
}

export default { name: "parcel-drop-proof", usage: "parcel-drop-proof [--name <shot prefix>]", run };
