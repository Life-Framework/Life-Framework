import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { bmpFileToPng } from "../bmp.mjs";
import { arrive, clientBox, sessionDir } from "../session.mjs";

// Everon map, final proof (#2228, epic #2223, card M0b): pictures of the OPEN in-game map, framed like the
// emulator's (`tools\cli map render`), so a visual card's PR shows the emulator and the game side by side.
// FINAL PROOFS ONLY: iterate in the emulator and the LOGIC tests; one run of this at the end.
//
//   map-shots [--out <dir>] [--place <name>] [--bbox x0,z0,x1,z1] [--zoom N]
//
// With no view argument it takes the three the emulator knows: the island, Saint-Philippe and the square.
// --place, --bbox and --zoom mean what they do for `map render` (the view comes from the emulator's own
// resolveView, so the two pictures cannot drift apart). Needs a rendering client on the Everon world:
//   node agent-box/session.mjs up --mission everon --profile full
//   node agent-box/session.mjs map-shots
//   node agent-box/session.mjs down
// Writes <out>/<view>.png (the game) and <out>/<view>-emulator.png (the emulator), default tmp/map-shots.
// Fails (exit 1) when the map never opens or a picture is missing. A view the map cannot reach (its zoom
// limits) is still photographed and the line says how wide the view really was.
const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const DEFAULT_VIEWS = [{ name: "island", place: "island" }, { name: "saint-philippe", place: "saint-philippe" }, { name: "square", place: "square" }];
const OPEN_TRIES = 10;
const PAN_TRIES = 5;
const WIDTH_TOLERANCE = 0.05;

function opt(args, name) {
  const i = args.indexOf(`--${name}`);
  return i < 0 ? undefined : args[i + 1];
}

// The views to photograph: [{ name, place?, bbox?, zoom? }].
export function viewsFor(args) {
  const place = opt(args, "place");
  const bboxArg = opt(args, "bbox");
  const zoomArg = opt(args, "zoom");
  if (!place && !bboxArg && zoomArg === undefined) return DEFAULT_VIEWS;
  const view = { name: bboxArg ? "bbox" : place || "island" };
  if (place && !bboxArg) view.place = place;
  if (bboxArg) {
    view.bbox = bboxArg.split(",").map(Number);
    if (view.bbox.length !== 4 || view.bbox.some((n) => !Number.isFinite(n)) || view.bbox[2] <= view.bbox[0] || view.bbox[3] <= view.bbox[1]) throw new Error("--bbox x0,z0,x1,z1 with x1 > x0 and z1 > z0");
  }
  if (zoomArg !== undefined) {
    view.zoom = Number(zoomArg);
    if (!Number.isFinite(view.zoom)) throw new Error("--zoom N");
    view.name += `-z${view.zoom}`;
  }
  return [view];
}

// `open=1 ppu=0.15 ... view_m=12800 ...` (the harness `map status` detail) to numbers.
export function parseStatus(detail) {
  const out = {};
  for (const m of String(detail).matchAll(/(\w+)=([^\s]+)/g)) out[m[1]] = m[2];
  return out;
}

// True when the open map shows about `wantMetres` across.
export function widthMatches(status, wantMetres) {
  const got = Number(status.view_m);
  return Number.isFinite(got) && got > 0 && Math.abs(got - wantMetres) / wantMetres <= WIDTH_TOLERANCE;
}

// The emulator's own view table (tools/map/render/render.mjs PLACES) and its width rule, for a machine that cannot
// render: { x0, z0, x1, z1, zoom } or undefined. A catalog place name needs the emulator.
const WORLD_M = 12800;
const KNOWN = { island: { x: 6400, z: 6400, zoom: 0 }, "saint-philippe": { x: 4503, z: 10772, zoom: 3 }, square: { x: 4592, z: 10818, zoom: 4 } };
export function fallbackView(view) {
  if (view.bbox) {
    const [x0, z0, x1, z1] = view.bbox;
    return { x0, z0, x1, z1, zoom: view.zoom ?? Math.log2(WORLD_M / (x1 - x0)) };
  }
  const p = KNOWN[view.place || "island"];
  if (!p) return undefined;
  const zoom = view.zoom ?? p.zoom;
  const w = WORLD_M / 2 ** zoom;
  return { x0: p.x - w / 2, x1: p.x + w / 2, z0: p.z - w / 2, z1: p.z + w / 2, zoom };
}

async function runMapShots(args) {
  const out = resolve(opt(args, "out") || join(ROOT, "tmp", "map-shots"));
  const views = viewsFor(args);
  mkdirSync(out, { recursive: true });

  const h = await import("../harness.mjs");
  const { renderMap } = await import("../../tools/map/render/render.mjs");
  const dir = sessionDir();
  const on = (spec, timeout = 15000) => {
    h.setBoxes([clientBox(dir, 1)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fail = (why) => {
    console.log(`SUMMARY map-shots fail ${why}`);
    return 1;
  };

  arrive((n, spec, t) => on(spec, t), 1, wait);
  wait(6000);

  // The welcome gift can land a moment after arrival: retry while the map is not carried yet.
  let opened = on({ verb: "map", words: ["open"] });
  for (let i = 1; i < OPEN_TRIES && opened.status !== "ok" && /not_carried|no_character/.test(opened.detail); i++) {
    wait(2000);
    opened = on({ verb: "map", words: ["open"] });
  }
  if (opened.status !== "ok") return fail(`map open: ${opened.detail}${/unknown_verb/.test(opened.detail) ? " (the harness has no map verb)" : ""}`);

  // Opening takes the gadget's activation delay and a few frames of map start-up.
  let status = {};
  for (let i = 0; i < 20; i++) {
    wait(1000);
    status = parseStatus(on({ verb: "map", words: ["status"] }).detail);
    if (status.open === "1" && Number(status.view_m) > 0) break;
  }
  if (status.open !== "1") return fail("the map did not open after map open (is the paper map carried and the map configured?)");

  const shots = [];
  for (const view of views) {
    // The emulator draws the companion picture and fixes the view; a machine with no MCP clone (the pool's session
    // boxes) cannot render, so the same view comes from the emulator's table (render.mjs PLACES) instead.
    let emu;
    try {
      emu = await renderMap({ root: ROOT, place: view.place, bbox: view.bbox, zoom: view.zoom, log: () => {} });
      writeFileSync(join(out, `${view.name}-emulator.png`), emu.png);
    } catch (e) {
      console.log(`note emulator ${view.name}: ${e.message}; framing from the built-in view table`);
      emu = { view: fallbackView(view) };
      if (!emu.view) return fail(`emulator ${view.name}: ${e.message} and no built-in view for it`);
    }
    const cx = (emu.view.x0 + emu.view.x1) / 2;
    const cz = (emu.view.z0 + emu.view.z1) / 2;
    const wantMetres = emu.view.x1 - emu.view.x0;

    let landed = false;
    for (let i = 0; i < PAN_TRIES && !landed; i++) {
      const pan = on({ verb: "map", words: ["pan", cx.toFixed(1), cz.toFixed(1), emu.view.zoom.toFixed(3)] });
      wait(pan.status === "ok" ? 1500 : 1000);
      status = parseStatus(on({ verb: "map", words: ["status"] }).detail);
      landed = pan.status === "ok" && widthMatches(status, wantMetres);
    }
    if (!landed) console.log(`note ${view.name}: view is ${status.view_m} m wide, wanted ${wantMetres.toFixed(0)} (zoom limits ${status.min}..${status.max} px/m); photographing it as it is`);
    wait(1500); // tiles and labels draw a frame or two after a pan

    const shot = on({ verb: "capture", mode: "ui", name: `map_${view.name.replace(/[^A-Za-z0-9]+/g, "_")}` }, 30000);
    if (shot.status !== "ok") return fail(`capture ${view.name}: ${shot.detail}`);
    const bmp = h.resolveCapture(shot.detail);
    if (!bmp) return fail(`no capture file for ${view.name}`);
    const png = bmpFileToPng(bmp, join(out, `${view.name}.png`));
    shots.push(png);
    console.log(`saved ${png}`);
  }

  on({ verb: "map", words: ["close"] });
  console.log(`SUMMARY map-shots ok ${shots.length} picture(s) in ${out}: ${shots.join(" ")}`);
  return 0;
}

export default {
  name: "map-shots",
  usage: "map-shots [--out <dir>] [--place <name>] [--bbox x0,z0,x1,z1] [--zoom N]   (up --mission everon --profile full; final proofs only)",
  run: (ctx) => runMapShots(ctx?.args || []),
};
