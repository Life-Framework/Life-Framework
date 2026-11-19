import { existsSync, mkdirSync, readdirSync, readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { bmpFileToPng } from "../bmp.mjs";
import { arrive, clientBox, sessionDir } from "../session.mjs";

// #3707: the cafe's evening neighbours sit on real seats. Run in a `session up --mission island` session.
//
//   resident-cafe-evening-proof [out]   the player stands beside the cafe's `bench` anchor and the clock is set to 21:00. The server
//                                       console must log two different bodies seated within a few metres of the anchor (`sit: <body>
//                                       sits on <seat> at <pos>`: the old man and Mireille each took a place of the bench, neither idled
//                                       at the place origin), `stand Resident_<id>` must put each beside the anchor, and no `no free
//                                       seat` or `could not take seat` line; then at 22:40 each must walk, move or despawn on their way home.
//
// The anchor is found by its layer name, so no coordinate is typed here. Read from this session's server console.
const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const IDS = ["old_man", "neighbour_mireille"];
const BENCH = "Fit_saint_philippe_cafe_anchor_bench";
const BUDGET_S = 150;
// A resident on a seat at the bench stands within this of the anchor (the player stands a step beside them).
const NEAR_M = 6;

// Pure: the top-level coords of the named entity in a layer text, or null.
export function coordsOf(text, name) {
  const m = new RegExp(`^\\w+ ${name}\\b[^\\n]*\\{[\\s\\S]*?\\n coords ([-\\d.]+) ([-\\d.]+) ([-\\d.]+)`, "m").exec(text);
  return m ? { x: Number(m[1]), y: Number(m[2]), z: Number(m[3]) } : null;
}

// Pure: the bodies seated in console text (`sit: <body> sits on <seat> at <x,y,z>`), and whether a seat was refused or missing.
// With `near` ({x, z}) only seats within NEAR_M of it count; two places of one bench share a seat id, so the bodies are counted.
export function seatsTaken(consoleText, near = null) {
  const lines = String(consoleText).split(/\r?\n/);
  const bodies = new Set();
  for (const l of lines) {
    const m = /\[LFDebug:Emote\] sit: (\S+) sits on (\S+) at <([-\d.]+),([-\d.]+),([-\d.]+)>/.exec(l);
    if (!m) continue;
    if (near && flatDistance({ x: Number(m[3]), z: Number(m[5]) }, near) > NEAR_M) continue;
    bodies.add(m[1]);
  }
  const refused = lines.some((l) => /no free seat is near the anchor|could not take seat/.test(l));
  return { seats: [...bodies], refused };
}

// Pure: for each id, its last line saying it walked, moved, arrived or despawned (the home line at 22:30).
export function leftLines(consoleText, ids) {
  const lines = String(consoleText).split(/\r?\n/).filter((l) => l.includes("[LFDebug:Residents]"));
  const out = {};
  for (const id of ids) {
    out[id] = lines.filter((l) => l.includes(`'${id}'`) && /(walks \d+ m|despawned|placed on the new anchor|arrived after)/.test(l)).pop() ?? null;
  }
  return out;
}

// Pure: where `stand Resident_<id>` put the player beside the resident, the last time (the harness logs
// `stand beside Resident_<id> at <x,y,z>`), or null.
export function residentAt(consoleText, id) {
  const re = new RegExp(`stand beside Resident_${id} at <([-\\d.]+),([-\\d.]+),([-\\d.]+)>`, "g");
  let last = null;
  for (const m of String(consoleText).matchAll(re)) last = { x: Number(m[1]), y: Number(m[2]), z: Number(m[3]) };
  return last;
}

// Pure: horizontal distance between two points, in metres.
export function flatDistance(a, b) {
  return Math.hypot(a.x - b.x, a.z - b.z);
}

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

  const at = coordsOf(readFileSync(join(ROOT, "addons", "LF_Everon1989", "Worlds", "MainWorld", "MainWorld_Layers", "Place_saint-philippe_cafe.layer"), "utf8"), BENCH);
  on({ verb: "stand", entityName: BENCH, side: "n2" }, 20000);
  // A morning first, so the evening lines are new ones and not the residents' first sweep.
  on({ verb: "time", hour: 12.0 }, 15000);
  wait(8000);
  const mark = serverConsole().length;
  on({ verb: "time", hour: 21.0 }, 15000);
  const t0 = Date.now();
  let taken = { seats: [], refused: false };
  while ((Date.now() - t0) / 1000 < BUDGET_S) {
    wait(3000);
    taken = seatsTaken(serverConsole().slice(mark), at);
    if (taken.seats.length >= IDS.length) break;
    // The walker has arrived (or been moved): one more look, then stop waiting.
    if (/neighbour_mireille' (arrived after|had not arrived)/.test(serverConsole().slice(mark))) {
      wait(6000);
      taken = seatsTaken(serverConsole().slice(mark), at);
      break;
    }
  }
  console.log(`  bodies seated at the bench after 21:00: ${taken.seats.join(", ") || "none"}; refused ${taken.refused}`);
  // Where each one really is: `stand Resident_<id>` logs the spot the server put the player beside them.
  const near = [];
  for (const id of IDS) {
    on({ verb: "stand", entityName: `Resident_${id}` }, 20000);
    wait(1500);
    const pos = residentAt(serverConsole(), id);
    const d = pos && at ? flatDistance(pos, at) : null;
    console.log(`  ${id} at 21:00: ${pos ? `${pos.x},${pos.z}` : "not found"}, ${d === null ? "?" : d.toFixed(1)} m from the bench anchor`);
    if (d !== null && d <= NEAR_M) near.push(id);
  }
  on({ verb: "stand", entityName: BENCH, side: "n2" }, 20000);
  // Pictures at dusk (19:42, the lines are the same until 22:30; at 21:00 the island is black): from the street side the
  // bench faces, then a wider one.
  if (at) {
    on({ verb: "time", hour: 19.7 }, 15000);
    wait(6000);
    on({ verb: "capture", mode: "world", name: "cafe_evening_front", x: at.x, y: at.y + 1, z: at.z, ox: -3.2, oy: 1.1, oz: 0.4, hour: 19.7, free: true }, 25000);
    on({ verb: "capture", mode: "world", name: "cafe_evening_wide", x: at.x, y: at.y + 1, z: at.z, ox: -6, oy: 2.6, oz: 5, hour: 19.7, free: true }, 25000);
  }

  const mark2 = serverConsole().length;
  on({ verb: "time", hour: 22.7 }, 15000);
  wait(25000);
  const left = leftLines(serverConsole().slice(mark2), IDS);
  for (const id of IDS) console.log(`  ${id} after 22:30: ${left[id] ? left[id].slice(0, 200) : "no leave line"}`);
  const home = IDS.filter((id) => left[id]);

  const ok = taken.seats.length >= IDS.length && !taken.refused && near.length === IDS.length && home.length === IDS.length;
  console.log(ok ? "resident-cafe-evening: PASS (two seats taken beside the bench anchor at 21:00, both neighbours left after 22:30)" : `resident-cafe-evening: FAIL (seats ${taken.seats.length}/${IDS.length}, refused ${taken.refused}, at the bench ${near.length}/${IDS.length}, left ${home.length}/${IDS.length})`);
  return ok ? 0 : 1;
}

function selftest() {
  const text = [
    "10:36:27.245 SCRIPT       : [LFDebug:Emote] sit: 1537 sits on 967 at <4444.86,13.1,10702.6>",
    "10:36:27.246 SCRIPT       : [LFDebug:Emote] sit: 1538 sits on 967 at <4444.73,13.1,10703.5>",
    "10:36:27.247 SCRIPT       : [LFDebug:Emote] sit: 1538 sits on 967 at <4444.73,13.1,10703.5>",
    "10:36:27.248 SCRIPT       : [LFDebug:Emote] sit: 1539 sits on 2749 at <4472.86,15.4,10647.5>",
    "x [LFDebug:Residents] resident 'old_man' despawned: the timetable moved them on",
    "x [LFDebug:Residents] resident 'neighbour_mireille' walks 35 m to anchor 'door' (59 s allowed)",
  ].join("\n");
  const at = residentAt("x stand beside Resident_old_man at <4450.5,13.1,10703.8>\nx stand beside Resident_old_man at <4473.7,15.3,10646.9>", "old_man");
  if (!at || at.x !== 4473.7 || residentAt("", "old_man") !== null || flatDistance({ x: 0, z: 0 }, { x: 3, z: 4 }) !== 5) {
    console.log("resident-cafe-evening-proof selftest: FAIL (residentAt)");
    return 1;
  }
  const t = seatsTaken(text);
  const l = leftLines(text, IDS);
  const refusedText = "x (W): [LFDebug:Residents] resident should sit but no free seat is near the anchor; the resident idles";
  const bench = seatsTaken(text, { x: 4445, z: 10703 });
  const ok = t.seats.length === 3 && bench.seats.length === 2 && !t.refused && seatsTaken(refusedText).refused && /despawned/.test(l.old_man) && /walks 35 m/.test(l.neighbour_mireille) && leftLines("", IDS).old_man === null && seatsTaken("").seats.length === 0;
  console.log(ok ? "resident-cafe-evening-proof selftest: OK" : "resident-cafe-evening-proof selftest: FAIL");
  return ok ? 0 : 1;
}

export default {
  name: "resident-cafe-evening-proof",
  usage: "resident-cafe-evening-proof [out]   (run in a --mission island session; --selftest runs the pure parts)",
  run: (ctx) => main(ctx?.args ?? process.argv.slice(2)),
};
