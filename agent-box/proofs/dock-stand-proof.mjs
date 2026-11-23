import { existsSync, mkdirSync, readdirSync, readFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { bmpFileToPng } from "../bmp.mjs";
import { arrive, clientBox, sessionDir } from "../session.mjs";

// #4646: the dock cast's fishmonger and chandler stand at their stalls in work hours. Run in a `session up --mission island
// --profile full` session:
//
//   dock-stand-proof <fishmonger|chandler> [out]
//
// The clock is set to a work hour (10:00, inside both timetables), the player is put beside the resident (`stand
// Resident_<id>`, found by id, no coordinate typed here), the server console must say where the body is, and pictures are
// taken from four sides at the body's spot. PASS needs a body found within NEAR_M of the resident's `counter` anchor (read
// from the anchors' layer name), so the resident stands behind the stall and not at the place origin.
const WHO = {
  fishmonger: { id: "dock_fishmonger", anchor: "Fit_saint_philippe_market_anchor_fish_counter", label: "fishmonger" },
  chandler: { id: "dock_chandler", anchor: "Fit_saint_philippe_dock_wholesale_anchor_chandler_counter", label: "chandler" },
};
const NEAR_M = 2.5;
const HOUR = 10.0;

// Pure: where `stand Resident_<id>` put the player, the last time, or null.
export function residentAt(consoleText, id) {
  const re = new RegExp(`stand beside Resident_${id} at <([-\\d.]+),([-\\d.]+),([-\\d.]+)>`, "g");
  let last = null;
  for (const m of String(consoleText).matchAll(re)) last = { x: Number(m[1]), y: Number(m[2]), z: Number(m[3]) };
  return last;
}

// Pure: the layer coords of a named entity in the text of any layer of the folder, or null.
export function coordsOf(text, name) {
  const m = new RegExp(`^\\w+ ${name}\\b[^\\n]*\\{[\\s\\S]*?\\n coords ([-\\d.]+) ([-\\d.]+) ([-\\d.]+)`, "m").exec(text);
  return m ? { x: Number(m[1]), y: Number(m[2]), z: Number(m[3]) } : null;
}

export function flatDistance(a, b) {
  return Math.hypot(a.x - b.x, a.z - b.z);
}

function serverConsole() {
  const dir = join(sessionDir(), "server", "logs");
  const runs = existsSync(dir) ? readdirSync(dir).sort() : [];
  const f = runs.length ? join(dir, runs[runs.length - 1], "console.log") : "";
  return f && existsSync(f) ? readFileSync(f, "utf8") : "";
}

function anchorCoords(name) {
  const dir = join(process.cwd(), "addons", "LF_Everon1989", "Worlds", "MainWorld", "MainWorld_Layers");
  for (const f of readdirSync(dir).filter((n) => n.endsWith(".layer"))) {
    const c = coordsOf(readFileSync(join(dir, f), "utf8"), name);
    if (c) return c;
  }
  return null;
}

async function main(args) {
  if (args.includes("--selftest")) return selftest();
  const who = WHO[String(args[0] || "")];
  if (!who) {
    console.log("usage: dock-stand-proof <fishmonger|chandler> [out]");
    return 2;
  }
  const out = args[1] ? resolve(String(args[1])) : "";
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
  // The welcome sheet and any menu are closed first so the pictures show the world.
  for (let i = 0; i < 3; i++) {
    on({ verb: "input", action: "MenuBack" }, 8000);
    wait(800);
  }

  on({ verb: "time", hour: 4.0 }, 15000);
  wait(4000);
  on({ verb: "time", hour: HOUR }, 15000);
  wait(14000);
  const anchor = anchorCoords(who.anchor);
  let pos = null;
  for (let i = 0; i < 6 && !pos; i++) {
    on({ verb: "stand", entityName: `Resident_${who.id}` }, 20000);
    wait(2500);
    pos = residentAt(serverConsole(), who.id);
  }
  const d = pos && anchor ? flatDistance(pos, anchor) : null;
  console.log(`  ${who.label} at ${HOUR}:00: ${pos ? `${pos.x},${pos.y},${pos.z}` : "not found"}; ${d === null ? "?" : d.toFixed(1)} m from the counter anchor`);
  if (pos) {
    const sides = [["a", 3.2, 0], ["b", 0, 3.2], ["c", -3.2, 0], ["d", 0, -3.2]];
    for (const [tag, ox, oz] of sides) {
      on({ verb: "capture", mode: "world", name: `${who.label}_${tag}`, x: pos.x, y: pos.y + 1, z: pos.z, ox, oy: 0.9, oz, hour: HOUR, free: true }, 25000);
    }
  }
  const ok = pos !== null && d !== null && d <= NEAR_M;
  console.log(ok ? `dock-stand ${who.label}: PASS (the resident stands ${d.toFixed(1)} m from the counter anchor at ${HOUR}:00)` : `dock-stand ${who.label}: FAIL (${pos ? `${d === null ? "anchor not found" : d.toFixed(1) + " m from the anchor"}` : "the resident was not found"})`);
  return ok ? 0 : 1;
}

function selftest() {
  const log = "x stand beside Resident_dock_fishmonger at <4479.5,13.9,10716.2>\nx stand beside Resident_dock_fishmonger at <4480,13.9,10716>";
  const layer = "GenericEntity Fit_a_anchor {\n components {\n }\n coords 4480 13.9 10715.1\n angleY 1\n}\n";
  const at = residentAt(log, "dock_fishmonger");
  const c = coordsOf(layer, "Fit_a_anchor");
  const ok = at && at.x === 4480 && residentAt("", "x") === null && c && c.z === 10715.1 && flatDistance({ x: 0, z: 0 }, { x: 3, z: 4 }) === 5 && coordsOf(layer, "nope") === null;
  console.log(ok ? "dock-stand-proof selftest: OK" : "dock-stand-proof selftest: FAIL");
  return ok ? 0 : 1;
}

export default {
  name: "dock-stand-proof",
  usage: "dock-stand-proof <fishmonger|chandler> [out]   (run in a --mission island session; --selftest runs the pure parts)",
  run: (ctx) => main(ctx?.args ?? process.argv.slice(2)),
};
