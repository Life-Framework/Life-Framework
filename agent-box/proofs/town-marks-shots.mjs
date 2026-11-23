// Town marks on the F-map (#4655): a fresh player's map shows no town; after studying a road map every live
// Everon town is marked. FINAL PROOF ONLY: one run at the end. Needs a rendering client on the island:
//   node agent-box/session.mjs up --mission island --profile full
//   node agent-box/session.mjs town-marks-shots [--out <dir>] [--close]
//   node agent-box/session.mjs down
// Writes before_z<zoom>.png (the map before the study) and after_z<zoom>.png (after; 0.18 is 0p18) for each frame, default
// tmp/town-marks-shots, and prints the server's MapKnown lines. Fails (exit 1) when the map never opens, the
// road map cannot be given or studied, or a picture is missing.
import { mkdirSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { bmpFileToPng } from "../bmp.mjs";
import { arrive, clientBox, sessionDir } from "../session.mjs";
import { parseStatus } from "./map-shots.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const ROAD_MAP = "{7C3E5A92D41F6B08}Prefabs/Items/Paper/LF_RoadMap.et";
// The island frame holding the five live towns (Saint-Philippe 4480,10715 to Levie 7464,4739), and the zoom steps tried.
const ISLAND = { x: 5970, z: 7730 };
const ZOOMS = [0.18, 0.28, 0.45];
// The five live towns at the map's own positions (docs/everon/places/<slug>/dossier.json), for the close-ups.
const TOWNS = [
  ["saint-philippe", 4480, 10715], ["meaux", 4520, 9468], ["montignac", 4773, 7095], ["chotain", 7086, 6012], ["levie", 7464, 4739],
];

async function run(args) {
  const i = args.indexOf("--out");
  const out = resolve(i >= 0 ? args[i + 1] : join(ROOT, "tmp", "town-marks-shots"));
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
    console.log(`SUMMARY town-marks-shots fail ${why}`);
    return 1;
  };
  const shots = [];
  const shoot = (name) => {
    wait(1500);
    const shot = on({ verb: "capture", mode: "ui", name: `townmarks_${name}` }, 30000);
    const bmp = shot.status === "ok" ? h.resolveCapture(shot.detail) : null;
    if (!bmp) return false;
    shots.push(bmpFileToPng(bmp, join(out, `${name}.png`)));
    return true;
  };
  const openMap = () => {
    let opened = on({ verb: "map", words: ["open"] });
    for (let t = 1; t < 10 && opened.status !== "ok" && /not_carried|no_character/.test(opened.detail); t++) {
      wait(2000);
      opened = on({ verb: "map", words: ["open"] });
    }
    for (let t = 0; t < 30; t++) {
      wait(1000);
      if (parseStatus(on({ verb: "map", words: ["status"] }).detail).open === "1") return true;
      if (t % 6 === 5) on({ verb: "map", words: ["open"] });
    }
    return false;
  };
  const pan = (p, zoom) => {
    on({ verb: "map", words: ["pan", String(p.x), String(p.z), String(zoom)] });
    wait(2500);
  };
  const frames = (prefix) => {
    for (const zoom of ZOOMS) {
      pan(ISLAND, zoom);
      if (!shoot(`${prefix}_z${String(zoom).replace(".", "p")}`)) return false;
    }
    return true;
  };

  // --close: the character has studied the road map already (an earlier run in this session); only the town close-ups.
  if (args.includes("--close")) {
    if (!openMap()) return fail("the map did not open");
    on({ verb: "maplegend", words: ["icons", "open"] });
    for (const [slug, x, z] of TOWNS) {
      pan({ x, z }, 4);
      on({ verb: "map", words: ["hover", String(x), String(z)] });
      if (!shoot(`close_${slug.replace(/-/g, "_")}`)) return fail(`capture close ${slug}`);
    }
    console.log(`SUMMARY town-marks-shots ok ${shots.length} close-up(s) in ${out}`);
    return 0;
  }

  arrive((n, spec, t) => on(spec, t), 1, wait);
  wait(6000);
  if (!openMap()) return fail("the map did not open");
  on({ verb: "maplegend", words: ["icons", "open"] });
  if (!frames("before")) return fail("capture before");
  on({ verb: "map", words: ["close"] });
  wait(1500);

  // Buy-free road map: one in the inventory, dropped within reach, studied through its own action (the server
  // runs LF_RoadMapRules.HearTowns, the same call the shop item makes).
  const given = on({ verb: "itemgive", prefab: ROAD_MAP });
  if (given.status !== "ok") return fail(`itemgive ${given.detail}`);
  wait(1500);
  const dropped = on({ verb: "itemdrop", prefab: ROAD_MAP });
  if (dropped.status !== "ok") return fail(`itemdrop ${dropped.detail}`);
  wait(2000);
  let studied = on({ verb: "use", className: "LF_RoadMapAction" });
  for (let t = 1; t < 5 && studied.status !== "ok"; t++) {
    wait(2000);
    studied = on({ verb: "use", className: "LF_RoadMapAction" });
  }
  if (studied.status !== "ok") return fail(`use LF_RoadMapAction ${studied.detail}`);
  wait(4000);

  if (!openMap()) return fail("the map did not reopen after the study");
  on({ verb: "maplegend", words: ["icons", "open"] });
  if (!frames("after")) return fail("capture after");

  console.log(`SUMMARY town-marks-shots ok ${shots.length} picture(s) in ${out}: ${shots.join(" ")}`);
  return 0;
}

export default {
  name: "town-marks-shots",
  usage: "town-marks-shots [--out <dir>] [--close]   (up --mission island --profile full; final proof only)",
  run: (ctx) => run(ctx?.args || []),
};
