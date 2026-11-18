import { mkdirSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { bmpFileToPng } from "../bmp.mjs";
import { arrive, clientBox, sessionDir } from "../session.mjs";

// Everon map zoom-end cost (#2397, epic #2223): the in-engine `map probe` on a rendering client, repeated.
//
//   map-probe [--repeats N] [--zoom Z] [--out dir]
//
// Opens the carried paper map, pans to Saint-Philippe at zoom Z (default 3: real areas and places in view), then sends
// the harness `map probe` N times (default 3) and prints each answer: the mean frame ms since the map opened, the
// places layer's zoom-end rebuild for 400 synthetic POIs (alternating two zooms, clustered and solo, with the
// clustering share), a pan reposition, and the areas layer's re-tessellation of 40 synthetic areas (records, with
// canvas commands, with the commit). Needs:
//   node agent-box/session.mjs up --mission everon --profile full
//   node agent-box/session.mjs map-probe
//   node agent-box/session.mjs down
const OPEN_TRIES = 10;
const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");

function opt(args, name) {
  const i = args.indexOf(`--${name}`);
  return i < 0 ? undefined : args[i + 1];
}

async function runMapProbe(args) {
  const repeats = Number(opt(args, "repeats") || 3);
  const zoom = Number(opt(args, "zoom") || 3);
  const out = resolve(opt(args, "out") || join(ROOT, "tmp", "map-probe"));
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (spec, timeout = 30000) => {
    h.setBoxes([clientBox(dir, 1)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fail = (why) => {
    console.log(`SUMMARY map-probe fail ${why}`);
    return 1;
  };

  arrive((n, spec, t) => on(spec, t), 1, wait);
  wait(6000);
  let opened = on({ verb: "map", words: ["open"] });
  for (let i = 1; i < OPEN_TRIES && opened.status !== "ok" && /not_carried|no_character/.test(opened.detail); i++) {
    wait(2000);
    opened = on({ verb: "map", words: ["open"] });
  }
  if (opened.status !== "ok") return fail(`map open: ${opened.detail}`);

  let status = "";
  for (let i = 0; i < 20; i++) {
    wait(1000);
    status = on({ verb: "map", words: ["status"] }).detail;
    if (/open=1/.test(status) && /view_m=[1-9]/.test(status)) break;
  }
  if (!/open=1/.test(status)) return fail("the map did not open");

  on({ verb: "map", words: ["pan", "4503", "10772", String(zoom)] });
  wait(2500);
  const lines = [];
  for (let i = 0; i < repeats; i++) {
    const r = on({ verb: "map", words: ["probe"] }, 60000);
    if (r.status !== "ok") return fail(`map probe: ${r.detail}`);
    lines.push(r.detail);
    wait(1000);
  }

  // The stand-in areas through the layer's own frame slices (no real area in the Everon test world), photographed.
  const demo = on({ verb: "map", words: ["demo"] });
  if (demo.status !== "ok") return fail(`map demo: ${demo.detail}`);
  wait(2500);
  const shot = on({ verb: "capture", mode: "ui", name: "map_probe_demo" }, 30000);
  let picture = "";
  if (shot.status === "ok") {
    const bmp = h.resolveCapture(shot.detail);
    if (bmp) {
      mkdirSync(out, { recursive: true });
      picture = bmpFileToPng(bmp, join(out, "demo-areas.png"));
      console.log(`saved ${picture}`);
    }
  }

  on({ verb: "map", words: ["close"] });
  console.log(`SUMMARY map-probe ok ${lines.length} probe(s) ${picture}`);
  return 0;
}

export default {
  name: "map-probe",
  usage: "map-probe [--repeats N] [--zoom Z] [--out dir]   (up --mission everon --profile full; the zoom-end cost of the map's two layers, #2397)",
  run: (ctx) => runMapProbe(ctx?.args || []),
};
