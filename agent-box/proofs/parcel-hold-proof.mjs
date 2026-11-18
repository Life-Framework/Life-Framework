import { mkdirSync } from "node:fs";
import { join, resolve } from "node:path";
import { bmpFileToPng } from "../bmp.mjs";
import { arrive, clientBox, parseState, sessionDir } from "../session.mjs";

// How the courier parcel is held, on `up --clients 2 --profile full`. Client 1 signs up at the
// post counter and takes a parcel run, so the parcel is in its hands. Then, on open ground south of
// the police desk (facing north, +z):
//   hold_first_stand / hold_first_walk   client 1's own view, standing and walking
//   hold_third_stand / hold_third_walk   client 2 beside the carrier, standing and walking
//   hold_third_front                     client 2 in front of the carrier, looking back at the chest
// `parcel-hold-proof [--name <prefix>] [--out <dir>]`: PNGs land in the out folder (default
// tmp/parcel-hold) as <prefix>_<shot>.png. Run it before and after a change with two prefixes.
async function runParcelHoldProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const flag = (name, dflt) => { const i = process.argv.indexOf(name); return i > 0 ? process.argv[i + 1] : dflt; };
  const prefix = flag("--name", "parcel");
  const out = resolve(flag("--out", "tmp/parcel-hold"));
  mkdirSync(out, { recursive: true });
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    if (spec.verb === "capture" && r.status === "ok") {
      const shot = h.resolveCapture(r.detail);
      if (shot) console.log(`  saved ${bmpFileToPng(shot, join(out, `${spec.name}.png`))}`);
      else console.log(`  no file for ${spec.name}`);
    }
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const at = (n) => String(parseState(on(n, { verb: "state" }, 10000).detail).pos ?? "0,0,0").split(",").map(Number);
  const fails = [];

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  on(1, { verb: "stand", entityName: "Annex_Job_Depot" });
  wait(1500);
  on(1, { verb: "use", className: "LF_JobSignUpAction", entityName: "Annex_Job_Depot" }, 10000);
  wait(1500);
  const run = on(1, { verb: "use", className: "LF_JobStartRunAction", entityName: "Annex_Job_Depot" }, 10000);
  if (run.status !== "ok") fails.push(`start run returned ${run.status} ${run.detail}`);
  wait(3500);
  // Open ground south of the police desk, facing north, as the asset states proof stands.
  on(1, { verb: "stand", entityName: "Annex_Police_Desk", side: "s22" });
  wait(3000);
  on(1, { verb: "heldxf" }, 10000);
  const shot = (n, spec) => on(n, { verb: "capture", hour: 12, ...spec }, 30000);

  // Standing.
  on(1, { verb: "capture", mode: "ui", name: `${prefix}_first_stand` }, 30000);
  let [x, y, z] = at(1);
  shot(2, { mode: "world", name: `${prefix}_third_stand`, x, y: y + 1, z, ox: -2.4, oy: 0.3, oz: 0.4 });
  shot(2, { mode: "world", name: `${prefix}_third_front`, x, y: y + 1, z, ox: 0.6, oy: 0.3, oz: 2.2 });

  // Walking: client 2's camera goes to the carrier's left, a little ahead of where it will be about
  // two seconds in, then client 1 shoots its own view; the walk lasts long enough for both.
  on(1, { verb: "walk", words: ["6", "1"] }, 10000);
  wait(1500);
  [x, y, z] = at(1);
  shot(2, { mode: "world", name: `${prefix}_third_walk`, x, y: y + 1, z: z + 3, ox: -2.8, oy: 0.2, oz: 0.2 });
  on(1, { verb: "capture", mode: "ui", name: `${prefix}_first_walk` }, 30000);
  wait(4000);

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "parcel hold proof: FAIL" : `parcel hold proof: done (read every ${prefix}_* picture in ${out})`);
  return fails.length ? 1 : 0;
}

export default {
  name: "parcel-hold-proof",
  usage: "parcel-hold-proof [--name <prefix>] [--out <dir>]",
  run: () => runParcelHoldProof(),
};
