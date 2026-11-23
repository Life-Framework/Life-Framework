import { existsSync, mkdirSync, readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { bmpFileToPng } from "../bmp.mjs";
import { arrive, clientBox, newestLog, sessionDir, sessionPaths } from "../session.mjs";

// #4586 (from #4479, covers #4403): the square's and the quay's life on the real island. Run in a
// `session up --mission island --profile full` session (the world with an AIWorld, so the flocks fly).
//
//   square-harbor-life-proof [out]
//     1. 12:00 on the square: the pigeons sit on the cobbles (picture from the free camera, farther than
//        their 50 m danger range), then the player walks up to them and they lift off (picture); the client logs
//        the square bed live.
//     2. 23:00: the square bed stops, the village-night bed live.
//     3. 12:00 on the quay: the gull flock on the dock apron (picture) and the harbor day bed live.
//     4. 23:00 on the quay: the harbor night bed live.
//   Evidence is the client's own [LFDebug:Atmosphere] lines; no coordinate is typed here: every spot is a layer entity.
const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const LAYERS = join(ROOT, "addons", "LF_Everon1989", "Worlds", "MainWorld", "MainWorld_Layers");
const SQUARE = { pigeons: "Fit_saint_philippe_square_life_pigeons_a", bed: "Fit_saint_philippe_square_life_bed_square", night: "Fit_saint_philippe_square_life_bed_village_night" };
// Places 60 to 90 m from a flock: the warehouse marker (66 m from the gulls) and the bank marker (91 m from the pigeons).
const FAR = { warehouse: "Fit_saint_philippe_dock_warehouse_Place", bank: "Fit_saint_philippe_bank_Place" };
const QUAY = { gulls: "Fit_saint_philippe_dock_harbor_life_gulls", bed: "Fit_saint_philippe_dock_harbor_life_bed_harbor_day", night: "Fit_saint_philippe_dock_harbor_life_bed_harbor_night" };

// Pure: whether the Atmosphere lines (oldest first) show the named emitter live after index `from`: `start <name> ...` with a sound
// manager, else `<name> is in range ... got no voice` (a session without `--sound`; the mix only asks an emitter for a voice when it is
// in range and inside its hours, so both lines prove the bed was chosen).
export function liveAfter(lines, name, from = 0) {
  return lines.slice(from).some((l) => l.includes(name) && (/^start\b/.test(l) || /is in range at/.test(l)));
}

// Pure: whether a `stop` line follows index `from`.
export function stoppedAfter(lines, from = 0) {
  return lines.slice(from).some((l) => /^stop\b/.test(l));
}

// Pure: the layer entity's top-level coords, or null (a camera target is read from the layer, never typed).
export function coordsOf(text, name) {
  const m = new RegExp(`^\\w+ ${name}\\b[^\\n]*\\{[\\s\\S]*?\\n coords ([-\\d.]+) ([-\\d.]+) ([-\\d.]+)`, "m").exec(text);
  return m ? { x: Number(m[1]), y: Number(m[2]), z: Number(m[3]) } : null;
}

async function main(args) {
  if (args.includes("--selftest")) return selftest();
  const out = args[0] ? resolve(String(args[0])) : "";
  if (out) mkdirSync(out, { recursive: true });
  const square = coordsOf(readFileSync(join(LAYERS, "Place_saint-philippe_square-life.layer"), "utf8"), SQUARE.pigeons);
  const quay = coordsOf(readFileSync(join(LAYERS, "Place_saint-philippe-dock_harbor-life.layer"), "utf8"), QUAY.gulls);
  if (!square || !quay) { console.log(`square-harbor-life: the layers lack the pigeons (${!!square}) or the gulls (${!!quay})`); return 2; }
  const shot = (name, at, ox, oy, oz, hour) => on({ verb: "capture", mode: "world", name, x: at.x, y: at.y + 1, z: at.z, ox, oy, oz, hour, free: true }, 25000);
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (spec, timeout) => {
    h.setBoxes([clientBox(dir, 1)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client 1: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    if (spec.verb === "capture" && r.status === "ok" && out) {
      const shot = h.resolveCapture(r.detail);
      if (shot) console.log(`  saved ${bmpFileToPng(shot, join(out, `${spec.name}.png`))}`);
    }
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const lines = () => {
    const f = newestLog(sessionPaths(dir).clients[0].logs);
    if (!f || !existsSync(f)) return [];
    return readFileSync(f, "utf8").split("\n").filter((l) => l.includes("[LFDebug:Atmosphere]")).map((l) => l.replace(/^.*\[LFDebug:Atmosphere\]\s*/, "").trim());
  };
  const fails = [];
  const expect = (cond, label) => { console.log(`  ${cond ? "ok  " : "FAIL"} ${label}`); if (!cond) fails.push(label); };

  arrive((n, spec, t) => on(spec, t), 1, wait);
  // The first-steps card covers the view of a new character: close it once it is up (the click answers no_session until then).
  for (let i = 0; i < 8; i++) {
    wait(3000);
    if (on({ verb: "click", widget: "WelcomeGuideCloseButton" }, 15000).status === "ok") break;
  }

  // A flock lifts when a player is within 50 m (DangerEventRange), and a client only sees entities its character is near: the perched
  // pictures are taken with the character 60 to 90 m from the flock, the lifted ones right after walking up to it.
  // 1. The quay at noon: gulls perched on the apron from the warehouse's side, then the character beside the harbor bed.
  on({ verb: "time", hour: 12 }, 15000);
  on({ verb: "stand", entityName: FAR.warehouse, side: "e3" }, 20000);
  wait(7000);
  shot("gulls_apron", quay, 3, 1.4, 3, 12);
  on({ verb: "stand", entityName: QUAY.bed, side: "e30" }, 20000);
  shot("gulls_lifted", quay, 7, 4, 7, 12);
  wait(8000);
  expect(liveAfter(lines(), QUAY.bed, 0), "12:00 on the quay: the harbor day bed starts");

  // 2. The quay at night.
  let mark = lines().length;
  on({ verb: "time", hour: 23 }, 15000);
  wait(8000);
  expect(liveAfter(lines(), QUAY.night, mark), "23:00 on the quay: the harbor night bed starts");
  shot("quay_night", quay, 8, 3, 8, 23);

  // 3. The square at noon: pigeons perched from the bank's side, then up to them (within 50 m they lift off).
  on({ verb: "time", hour: 12 }, 15000);
  on({ verb: "stand", entityName: FAR.bank, side: "e3" }, 20000);
  wait(7000);
  shot("pigeons_perched", square, 3, 1.4, 3, 12);
  on({ verb: "stand", entityName: SQUARE.bed, side: "w4" }, 20000);
  shot("pigeons_lifted", square, 7, 4, 7, 12);
  wait(6000);
  expect(liveAfter(lines(), SQUARE.bed, 0), "12:00 on the square: the square bed starts");

  // 4. Night on the square.
  mark = lines().length;
  on({ verb: "time", hour: 23 }, 15000);
  wait(8000);
  const night = lines();
  expect(!liveAfter(night, SQUARE.bed, mark), "23:00: the square bed is silent (outside its hours)");
  expect(liveAfter(night, SQUARE.night, mark), "23:00: the village-night bed starts");

  for (const l of lines().slice(-12)) console.log(`    ${l.slice(0, 200)}`);
  console.log(fails.length ? `square-harbor-life: FAIL (${fails.join("; ")})` : "square-harbor-life: PASS");
  return fails.length ? 1 : 0;
}

function selftest() {
  const lines = ["start Fit_sq at 4 m (radius 34) Atmosphere_Square.acp", "stop Fit_sq (hours)", "Fit_vn is in range at 5 m (radius 54) and got no voice for 5 s: this machine has no sound manager"];
  const layer = "GenericEntity Fit_a {\n components {\n }\n coords 1 2 3\n}\n";
  const ok = liveAfter(lines, "Fit_sq") && !liveAfter(lines, "Fit_sq", 1) && stoppedAfter(lines, 0) && !stoppedAfter(lines, 2)
    && liveAfter(lines, "Fit_vn", 2) && !liveAfter(lines, "Fit_ha") && coordsOf(layer, "Fit_a")?.z === 3 && coordsOf(layer, "Nope") === null;
  console.log(ok ? "square-harbor-life-proof selftest: OK" : "square-harbor-life-proof selftest: FAIL");
  return ok ? 0 : 1;
}

export default {
  name: "square-harbor-life-proof",
  usage: "square-harbor-life-proof [out]   (run in a --mission island --profile full session; --selftest runs the pure parts)",
  run: (ctx) => main(ctx?.args ?? process.argv.slice(2)),
};
