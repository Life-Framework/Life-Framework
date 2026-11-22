// Map rework pictures (0.2.0 playtest: badge icons, names on hover, legend, Shift + click waypoint).
// FINAL PROOFS ONLY: one run at the end. Needs a rendering client on the island:
//   node agent-box/session.mjs up --mission island --profile full
//   node agent-box/session.mjs map-rework-shots [--out <dir>]
//   node agent-box/session.mjs down
// Writes town.png (zoom 4.3), wide.png (zoom 3), square.png (zoom 6), hover.png (a place's card pinned), legend.png (the Legend
// panel), waypoint_map.png (the ring, its tag and the hint strip) and waypoint_hud.png (the map closed: the
// HUD marker and distance), default tmp/map-rework-shots. Fails (exit 1) when the map never opens or a
// picture is missing.
import { mkdirSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { bmpFileToPng } from "../bmp.mjs";
import { arrive, clientBox, sessionDir } from "../session.mjs";
import { parseStatus } from "./map-shots.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
// The Saint-Philippe centre (the town hall square) and the town view, as tools/map/render/render.mjs PLACES frames the town.
const SQUARE = { x: 4470, z: 10715 };
const TOWN = { x: 4503, z: 10772 };

async function run(args) {
  const i = args.indexOf("--out");
  const out = resolve(i >= 0 ? args[i + 1] : join(ROOT, "tmp", "map-rework-shots"));
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
    console.log(`SUMMARY map-rework-shots fail ${why}`);
    return 1;
  };
  const shots = [];
  const shoot = (name) => {
    wait(1500);
    const shot = on({ verb: "capture", mode: "ui", name: `maprework_${name}` }, 30000);
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
      // The paper map can come to hand without switching to map mode (the arrival quest's first seconds on
      // the island): with the map held, `map open` switches the mode on.
      if (t % 6 === 5) on({ verb: "map", words: ["open"] });
    }
    return false;
  };
  const pan = (p, zoom) => {
    on({ verb: "map", words: ["pan", String(p.x), String(p.z), String(zoom)] });
    wait(2500);
  };

  arrive((n, spec, t) => on(spec, t), 1, wait);
  wait(6000);
  if (!openMap()) return fail("the map did not open");

  pan(TOWN, 4.3);
  if (!shoot("town")) return fail("capture town");
  pan(TOWN, 3);
  if (!shoot("wide")) return fail("capture wide");
  pan(SQUARE, 6);
  if (!shoot("square")) return fail("capture square");
  on({ verb: "map", words: ["hover", String(SQUARE.x), String(SQUARE.z)] });
  if (!shoot("hover")) return fail("capture hover");
  on({ verb: "maplegend", words: ["icons"] });
  if (!shoot("legend")) return fail("capture legend");
  on({ verb: "maplegend", words: ["close"] });
  on({ verb: "map", words: ["waypoint", String(SQUARE.x + 60), String(SQUARE.z - 30)] });
  pan(SQUARE, 5.5);
  if (!shoot("waypoint_map")) return fail("capture waypoint on the map");
  on({ verb: "map", words: ["close"] });
  wait(3000);
  if (!shoot("waypoint_hud")) return fail("capture waypoint HUD");

  console.log(`SUMMARY map-rework-shots ok ${shots.length} picture(s) in ${out}: ${shots.join(" ")}`);
  return 0;
}

export default {
  name: "map-rework-shots",
  usage: "map-rework-shots [--out <dir>]   (up --mission island --profile full; final proofs only)",
  run: (ctx) => run(ctx?.args || []),
};
