import { existsSync, mkdirSync, readdirSync, readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { bmpFileToPng } from "../bmp.mjs";
import { arrive, clientBox, sessionDir } from "../session.mjs";

// #3001: one resident walks between anchors on the real island. Run in a `session up --mission island --profile full` session.
//
//   resident-island-proof [out]      the teller: their day is the home bench (12:00) then the bank counter (13:00), 70 m apart. The player
//                                    stands between the two (within 50 m of the teller), the clock is set to 12:30 and then 13:01, and the
//                                    server console must log `resident 'teller' walks <n> m ... ` and then `arrived after ... s`.
//                                    Pictures: the middle of the route, the bank counter, the home bench and doorstep.
//
// The markers come from the two island layers, so no coordinate is typed here. The walk is read from this session's server console.
const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const LAYERS = join(ROOT, "addons", "LF_Everon1989", "Worlds", "MainWorld", "MainWorld_Layers");
const RESIDENT = "teller";
const WALK_BUDGET_S = 150;
const MIN_WALK_M = 20;

// Pure: the top-level coords of the named entity in a layer text, or null.
export function coordsOf(text, name) {
  const m = new RegExp(`^\\w+ ${name}\\b[^\\n]*\\{[\\s\\S]*?\\n coords ([-\\d.]+) ([-\\d.]+) ([-\\d.]+)`, "m").exec(text);
  return m ? { x: Number(m[1]), y: Number(m[2]), z: Number(m[3]) } : null;
}

// Pure: the walk and the arrival of one resident in console text -> { walks, arrived, all }. A walk shorter than minM metres (a
// resident already standing at the anchor) is not the proof; `arrived` is the first arrival line after that walk.
export function walkLines(consoleText, id, minM = 0) {
  const lines = String(consoleText).split(/\r?\n/).filter((l) => l.includes("[LFDebug:Residents]") && l.includes(`'${id}'`));
  const walks = lines.find((l) => /walks (\d+) m/.test(l) && Number(/walks (\d+) m/.exec(l)[1]) >= minM) ?? null;
  const at = walks ? lines.indexOf(walks) : -1;
  return { walks, arrived: at >= 0 ? (lines.slice(at + 1).find((l) => /arrived after/.test(l)) ?? null) : null, all: lines.slice(-6) };
}

// Pure: the teller's home place id in the pack's residents conf, or "".
export function tellerHomeId(confText) {
  const block = /m_sId "teller"[\s\S]*?m_sHomePlaceId "([^"]+)"/.exec(String(confText));
  return block ? block[1] : "";
}

// Pure: the name of the first `ResidentBench_*` entity that follows the house marker with this place id in a layer text, or "".
export function homeBenchOf(layerText, placeId) {
  if (!placeId) return "";
  const at = String(layerText).indexOf(`m_sId "${placeId}"`);
  if (at < 0) return "";
  return /GenericEntity (ResidentBench_\w+)/.exec(String(layerText).slice(at))?.[1] ?? "";
}

// The newest server console of this session (the walk is read from it, not from a pool test log).
function serverConsole() {
  const dir = join(sessionDir(), "server", "logs");
  const runs = existsSync(dir) ? readdirSync(dir).sort() : [];
  const f = runs.length ? join(dir, runs[runs.length - 1], "console.log") : "";
  return f && existsSync(f) ? readFileSync(f, "utf8") : "";
}

async function main(args) {
  if (args.includes("--selftest")) return selftest();
  const out = args[0] ? resolve(String(args[0])) : "";
  if (out) mkdirSync(out, { recursive: true });
  const bankText = readFileSync(join(LAYERS, "Place_saint-philippe_bank.layer"), "utf8");
  // The teller's own house (the pack's timetable names its place id) holds their bench, not the shared residents slot
  // (#3001 first stood there; 100 m from the teller's real bench the walk is out of the 50 m watch radius and they are placed).
  const homeText = readFileSync(join(LAYERS, "Homes_saint-philippe_resident.layer"), "utf8");
  const confText = readFileSync(join(ROOT, "addons", "LF_Everon1989", "Configs", "Residents", "LF_Residents_Everon1989.conf"), "utf8");
  const benchName = homeBenchOf(homeText, tellerHomeId(confText));
  const bank = coordsOf(bankText, "Fit_saint_philippe_bank_Place_BankCounter");
  const bench = benchName ? coordsOf(homeText, benchName) : null;
  if (!bank || !bench) {
    console.log(`resident-island: the layers lack the bank counter marker or the home bench anchor (bank ${!!bank}, bench ${!!bench})`);
    return 2;
  }
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
  arrive((n, spec, t) => on(spec, t), 1, wait);
  wait(6000);

  // The walk needs a player within 50 m of the resident when the line changes (LF_ResidentWalk), so the character really
  // stands between the two places (a capture moves only the camera): 30 m west of the home bench, about 40 m from the bank.
  on({ verb: "stand", entityName: benchName, side: "w30" }, 20000);
  on({ verb: "time", hour: 12.5 }, 15000);
  wait(15000);
  on({ verb: "time", hour: 13.02 }, 15000);
  const t0 = Date.now();
  let seen = { walks: null, arrived: null, all: [] };
  let midShot = false;
  while ((Date.now() - t0) / 1000 < WALK_BUDGET_S) {
    wait(2000);
    seen = walkLines(serverConsole(), RESIDENT, MIN_WALK_M);
    if (seen.walks && !midShot) {
      // Mid-route, from the side. The free camera: a capture without it does not move the view once any capture used it.
      const mid = { x: (bank.x + bench.x) / 2, y: (bank.y + bench.y) / 2, z: (bank.z + bench.z) / 2 };
      on({ verb: "capture", mode: "world", name: "walk_mid", x: mid.x, y: mid.y + 1, z: mid.z, ox: 10, oy: 3, oz: 10, hour: 13, free: true }, 25000);
      midShot = true;
    }
    if (seen.arrived) break;
  }
  // At the bank counter, where the teller stands once arrived, and the home bench they left.
  on({ verb: "capture", mode: "world", name: "bank_arrived", x: bank.x, y: bank.y + 1, z: bank.z, ox: 3, oy: 2.2, oz: -3, hour: 13, free: true }, 25000);
  on({ verb: "capture", mode: "world", name: "home_bench", x: bench.x, y: bench.y + 1, z: bench.z, ox: 4, oy: 1.8, oz: 4, hour: 13, free: true }, 25000);
  on({ verb: "capture", mode: "world", name: "home_top", x: bench.x, y: bench.y, z: bench.z, ox: 0, oy: 22, oz: -0.1, hour: 13, free: true }, 25000);
  for (const l of seen.all) console.log(l.slice(0, 200));
  const metres = seen.walks ? /walks (\d+) m/.exec(seen.walks)?.[1] : null;
  const ok = Boolean(seen.walks && seen.arrived);
  console.log(ok ? `resident-island: PASS (${RESIDENT} walked ${metres} m and arrived)` : `resident-island: FAIL (walks ${!!seen.walks}, arrived ${!!seen.arrived})`);
  return ok ? 0 : 1;
}

function selftest() {
  const layer = "GenericEntity Fit_a_Place {\n components {\n }\n coords 1 2 3\n}\nGenericEntity Fit_b {\n coords 4.5 5 -6\n angleY 9\n}\n";
  const a = coordsOf(layer, "Fit_a_Place");
  const b = coordsOf(layer, "Fit_b");
  const text = [
    "x [LFDebug:Residents] resident 'teller' walks 1 m to anchor 'bench' (21 s allowed)",
    "x [LFDebug:Residents] resident 'teller' arrived after 1 s",
    "x [LFDebug:Residents] resident 'teller' walks 70 m to anchor 'counter' (97 s allowed)",
    "x [LFDebug:Residents] resident 'old_man' arrived after 3 s",
    "x [LFDebug:Residents] resident 'teller' arrived after 28 s",
  ].join("\n");
  const w = walkLines(text, "teller", 20);
  const short = walkLines(text.split("\n").slice(0, 2).join("\n"), "teller", 20);
  const ok = a?.z === 3 && b?.x === 4.5 && b?.z === -6 && coordsOf(layer, "Nope") === null && /walks 70 m/.test(w.walks) && /after 28 s/.test(w.arrived) && short.walks === null && short.arrived === null && walkLines("", "teller").walks === null;
  const conf = ["LF_ResidentDef \"{1}\" {", " m_sId \"teller\"", " m_sHomeKind \"House\"", " m_sHomePlaceId \"saint-philippe.home.teller\"", "}"].join("\n");
  const homes = ["x m_sId \"saint-philippe.home.old_man\"", "GenericEntity ResidentBench_a {", "}", "x m_sId \"saint-philippe.home.teller\"", "GenericEntity ResidentBench_b {", "}"].join("\n");
  if (tellerHomeId(conf) !== "saint-philippe.home.teller" || homeBenchOf(homes, "saint-philippe.home.teller") !== "ResidentBench_b" || homeBenchOf(homes, "nope") !== "") {
    console.log("resident-island-proof selftest: FAIL (tellerHomeId / homeBenchOf)");
    return 1;
  }
  console.log(ok ? "resident-island-proof selftest: OK" : "resident-island-proof selftest: FAIL");
  return ok ? 0 : 1;
}

export default {
  name: "resident-island-proof",
  usage: "resident-island-proof [out]   (run in a --mission island --profile full session; --selftest runs the pure parts)",
  run: (ctx) => main(ctx?.args ?? process.argv.slice(2)),
};
