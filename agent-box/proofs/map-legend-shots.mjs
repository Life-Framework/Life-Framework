import { mkdirSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { bmpFileToPng } from "../bmp.mjs";
import { arrive, clientBox, sessionDir } from "../session.mjs";

// Everon map, final proof (#2244, epic #2223, card M16): the legend, the search and a category filter on the OPEN
// in-game map. FINAL PROOF ONLY: the rules are LF_Test_MapSearch; this is the one in-game look.
//
//   map-legend-shots [--out <dir>] [--place <island|saint-philippe|square>] [--word <search word>] [--category <word>]
//
//   node agent-box/session.mjs up --mission everon --profile full
//   node agent-box/session.mjs map-legend-shots
//   node agent-box/session.mjs down
// Writes <out>/legend-open.png (the Map key sheet), legend-search.png (a search with its matches), legend-filtered.png
// (the sheet closed, the category hidden) and legend-restored.png (shown again), default tmp/map-legend-shots.
const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const KNOWN = { island: { x: 6400, z: 6400, zoom: 0 }, "saint-philippe": { x: 4503, z: 10772, zoom: 3 }, square: { x: 4592, z: 10818, zoom: 4 } };
const OPEN_TRIES = 10;

function opt(args, name) {
  const i = args.indexOf(`--${name}`);
  return i < 0 ? undefined : args[i + 1];
}

function parseDetail(detail) {
  const out = {};
  for (const m of String(detail).matchAll(/(\w+)=([^\s]*)/g)) out[m[1]] = m[2];
  return out;
}

async function run(args) {
  const out = resolve(opt(args, "out") || join(ROOT, "tmp", "map-legend-shots"));
  const placeName = opt(args, "place");
  const place = placeName ? { ...KNOWN[placeName] } : undefined;
  if (placeName && !KNOWN[placeName]) throw new Error("--place island|saint-philippe|square");
  const word = opt(args, "word") || "bank";
  const category = opt(args, "category") || "civic";
  mkdirSync(out, { recursive: true });

  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (spec, timeout = 15000) => {
    h.setBoxes([clientBox(dir, 1)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fail = (why) => {
    console.log(`SUMMARY map-legend-shots fail ${why}`);
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

  let status = {};
  for (let i = 0; i < 20; i++) {
    wait(1000);
    status = parseDetail(on({ verb: "map", words: ["status"] }).detail);
    if (status.open === "1" && Number(status.view_m) > 0) break;
  }
  if (status.open !== "1") return fail("the map did not open after map open");

  // No world holds placed POIs yet, so the proof adds four stand-in places around the view's centre (the harness-only
  // `maplegend demo`); a world that has its own known places keeps them and adds none.
  const legend = (...words) => on({ verb: "maplegend", words });
  const centre = place || { ...KNOWN["saint-philippe"], zoom: 4 };
  let known = parseDetail(legend("status").detail);
  if (!(Number(known.known) > 0)) {
    legend("demo", String(centre.x), String(centre.z));
    known = parseDetail(legend("status").detail);
  }
  if (!(Number(known.known) > 0)) return fail("no places to list after maplegend demo");
  on({ verb: "map", words: ["pan", centre.x.toFixed(1), centre.z.toFixed(1), centre.zoom.toFixed(3)] });
  wait(2500);

  const shots = [];
  const shoot = (name) => {
    wait(1500);
    const shot = on({ verb: "capture", mode: "ui", name: `maplegend_${name}` }, 30000);
    if (shot.status !== "ok") return `capture ${name}: ${shot.detail}`;
    const bmp = h.resolveCapture(shot.detail);
    if (!bmp) return `no capture file for ${name}`;
    shots.push(bmpFileToPng(bmp, join(out, `legend-${name}.png`)));
    return "";
  };

  // The sheet, with a line per category the player's map shows.
  const sheet = legend("open");
  if (sheet.status !== "ok") return fail(`maplegend open: ${sheet.detail}`);
  let problem = shoot("open");
  if (problem) return fail(problem);

  // A search: the matches are only places this player knows.
  const found = legend("search", word);
  if (found.status !== "ok") return fail(`maplegend search: ${found.detail}`);
  problem = shoot("search");
  if (problem) return fail(problem);

  // Hide a category, close the sheet: its icons leave the map.
  legend("toggle", category);
  legend("close");
  const hidden = parseDetail(legend("status").detail);
  if (!String(hidden.hidden).includes(category)) return fail(`the category ${category} is not hidden after the toggle`);
  problem = shoot("filtered");
  if (problem) return fail(problem);

  // And back.
  legend("open");
  legend("toggle", category);
  legend("close");
  problem = shoot("restored");
  if (problem) return fail(problem);

  on({ verb: "map", words: ["close"] });
  console.log(`SUMMARY map-legend-shots ok ${shots.length} picture(s) in ${out}: ${shots.join(" ")}`);
  return 0;
}

export default {
  name: "map-legend-shots",
  usage: "map-legend-shots [--out <dir>] [--place <name>] [--word <word>] [--category <word>]   (up --mission everon --profile full; final proof only)",
  run: (ctx) => run(ctx?.args || []),
};
