import { existsSync, mkdirSync, readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { bmpFileToPng } from "../bmp.mjs";
import { arrive, clientBox, parseState, sessionDir } from "../session.mjs";

// #2174: the relocated families walk a route on a world with a navmesh. Run on a
// `session up --mission everon --profile full` session (CoreOnEveron: vanilla Everon, SCR_AIWorld_Eden).
//   relocated-walk-proof survey [x,z] [radius] [out] [step]   ground heights on a grid around x,z (default
//                                                      the spawn) and two overview pictures (top: north up), to lay a route
//   relocated-walk-proof [out]                         eventscene start with the pack fixture at 06:00,
//                                                      then status every 3 s; pictures mid-walk and at
//                                                      the target; PASS when every walker arrived.
//                                                      --control adds a vanilla civilian on the same route
// The route is the scene markers in Worlds/CoreOnEveron/CoreOnEveron_Layers/RelocatedWalk.layer.
const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const LAYER = join(ROOT, "addons", "LifeFramework", "Worlds", "CoreOnEveron", "CoreOnEveron_Layers", "RelocatedWalk.layer");
const FIXTURE = "{6C3B1E8A4F2D9705}Configs/Tests/LF_Test_RelocatedWalkFixture.conf";
const SPAWN = [4471.854, 10535.136];
const WALK_BUDGET_S = 330;
// The control walker: vanilla, CIV faction, no LF prefab in between.
const CONTROL_PREFAB = "{11EB9A0D2A5899EA}Prefabs/Characters/Factions/CIV/GenericCivilians/Character_CIV_DenimJacket_1.et";

// Scene markers of the layer: role (Waypoint|Spawn|Target), order, position.
export function readMarkers(text) {
  const out = [];
  const re = /GenericEntity\s+(\w+)\s*:\s*"[^"]*LF_EventMarker_(Waypoint|Spawn|Target)\.et"\s*\{([\s\S]*?)\n\}/g;
  let m;
  while ((m = re.exec(text))) {
    const order = /m_iOrder\s+(\d+)/.exec(m[3]);
    const c = /coords\s+([-\d.]+)\s+([-\d.]+)\s+([-\d.]+)/.exec(m[3]);
    if (!c) continue;
    out.push({ name: m[1], role: m[2], order: order ? Number(order[1]) : 0, x: Number(c[1]), y: Number(c[2]), z: Number(c[3]) });
  }
  return out;
}

async function setup(out) {
  if (out) mkdirSync(out, { recursive: true });
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (spec, timeout) => {
    h.setBoxes([clientBox(dir, 1)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client 1: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    if (spec.verb === "capture" && r.status === "ok") {
      const shot = h.resolveCapture(r.detail);
      if (shot && out) console.log(`  saved ${bmpFileToPng(shot, join(out, `${spec.name}.png`))}`);
      else if (shot) console.log(`  image: ${shot}`);
      else console.log(`  no file for ${spec.name}`);
    }
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  arrive((n, spec, t) => on(spec, t), 1, wait);
  wait(6000);
  return { on, wait };
}

async function survey(args) {
  const at = args[0] && args[0].includes(",") ? args[0].split(",").map(Number) : SPAWN;
  const radius = Number(args[1] ?? 80);
  const out = args[2] ? resolve(String(args[2])) : "";
  const { on } = await setup(out);
  const ground = (x, z) => Number(/y=([-\d.]+)/.exec(on({ verb: "probe", mode: "ground", x, z }).detail)?.[1]);
  const step = Number(args[3] ?? 20);
  const rows = [];
  for (let dz = radius; dz >= -radius; dz -= step) {
    const row = [];
    for (let dx = -radius; dx <= radius; dx += step) row.push(ground(at[0] + dx, at[1] + dz).toFixed(1));
    rows.push(`z ${(at[1] + dz).toFixed(0)}: ${row.join(" ")}`);
  }
  console.log(`ground heights, x from ${at[0] - radius} to ${at[0] + radius} step ${step}:`);
  for (const r of rows) console.log(r);
  const y = ground(at[0], at[1]);
  on({ verb: "capture", mode: "world", name: "survey_top", x: at[0], y, z: at[1], ox: 0, oy: Math.max(40, radius * 1.4), oz: -0.1, hour: 12, free: true }, 25000);
  on({ verb: "capture", mode: "world", name: "survey_low", x: at[0], y: y + 1, z: at[1], ox: -40, oy: 25, oz: -40, hour: 12 }, 25000);
  return 0;
}

async function walk(args) {
  args = [...args];
  const control = args.includes("--control");
  args = args.filter((a) => a !== "--control");
  const out = args[0] ? resolve(String(args[0])) : "";
  if (!existsSync(LAYER)) {
    console.log(`relocated-walk: no route layer at ${LAYER}`);
    return 2;
  }
  const markers = readMarkers(readFileSync(LAYER, "utf8"));
  const route = markers.filter((m) => m.role === "Waypoint").sort((a, b) => a.order - b.order);
  const spawn = markers.find((m) => m.role === "Spawn");
  const target = markers.find((m) => m.role === "Target");
  if (!route.length || !spawn || !target) {
    console.log(`relocated-walk: the layer needs waypoints, a spawn and a target (found ${markers.map((m) => m.name).join(", ")})`);
    return 2;
  }

  const { on, wait } = await setup(out);
  // Each marker against the ground under it: a marker in the ground buries a walker, one high drops it.
  for (const m of markers) {
    const y = Number(/y=([-\d.]+)/.exec(on({ verb: "probe", mode: "ground", x: m.x, z: m.z }).detail)?.[1]);
    console.log(`marker ${m.name}: y ${m.y}, ground ${y.toFixed(2)}, ${(m.y - y).toFixed(2)} m above`);
  }
  // Stand away from the spawn: the scene spawns out of sight (LF_EventSceneSpawn sight check).
  const start = on({ verb: "eventscene", words: ["start", FIXTURE, "6"] }, 15000);
  if (start.status !== "ok") {
    console.log(`relocated-walk: FAIL eventscene start: ${start.detail}`);
    return 1;
  }

  const shots = { mid: false, target: false };
  const t0 = Date.now();
  let last = "";
  let arrived = 0;
  let spawned = 0;
  while ((Date.now() - t0) / 1000 < WALK_BUDGET_S) {
    wait(3000);
    const s = on({ verb: "eventscene", words: ["status", "relocated_arrival"] }, 10000).detail;
    last = s;
    spawned = Number(/(\d+) spawned/.exec(s)?.[1] ?? 0);
    const walking = Number(/walking (\d+)/.exec(s)?.[1] ?? 0);
    arrived = Number(/arrived (\d+)/.exec(s)?.[1] ?? 0);
    const elapsed = Number(/, ([\d.]+) s(;|$)/.exec(s)?.[1] ?? 0);
    if (!shots.spawn && spawned >= 6) {
      on({ verb: "capture", mode: "world", name: "walk_spawn", x: spawn.x, y: spawn.y + 1, z: spawn.z, ox: 7, oy: 1.8, oz: 7, hour: 12 }, 25000);
      shots.spawn = true;
    }
    // --control: a stock vanilla civilian on the same route in its own group, once.
    if (control && !shots.control && spawned > 0) {
      on({ verb: "eventscene", words: ["control", CONTROL_PREFAB] }, 10000);
      shots.control = true;
    }
    // The route goes out once all six have spawned (about 15 s), so mid-route is about 25 s later.
    if (!shots.mid && walking > 0 && elapsed >= 40) {
      // The middle of the route, from the side, a little above eye height.
      const mid = route[Math.floor(route.length / 2)];
      on({ verb: "capture", mode: "world", name: "walk_mid", x: mid.x, y: mid.y + 1, z: mid.z, ox: 14, oy: 4, oz: 10, hour: 12 }, 25000);
      on({ verb: "capture", mode: "world", name: "walk_mid_close", x: mid.x, y: mid.y + 1, z: mid.z, ox: 6, oy: 1.6, oz: 4, hour: 12 }, 25000);
      shots.mid = true;
    }
    // The walk stops the moment it is over; the status then carries its result, and the family
    // still stands at the target until the despawn delay, so the arrival pictures go now.
    const over = /walk over: (\d+) reached the target, (\d+) timed out/.exec(s);
    if (over) {
      arrived = Number(over[1]);
      spawned = arrived + Number(over[2]);
      break;
    }
    if (spawned > 0 && arrived >= spawned && walking === 0) break;
    if (/no scene running/.test(s) && (Date.now() - t0) / 1000 > 60) break;
  }
  // From the south-west, across the open lawn: the north-east side of the target is a bush.
  on({ verb: "capture", mode: "world", name: "walk_target", x: target.x, y: target.y + 1, z: target.z, ox: -9, oy: 3.5, oz: -11, hour: 12 }, 25000);
  on({ verb: "capture", mode: "world", name: "walk_target_close", x: target.x, y: target.y + 1, z: target.z, ox: -5, oy: 1.7, oz: -6, hour: 12 }, 25000);
  // Top view over the target (image up is north-west): where the family stands when the walk ends.
  // Last: the free camera carries over to the next capture.
  on({ verb: "capture", mode: "world", name: "walk_target_top", x: target.x, y: target.y, z: target.z, ox: 0, oy: 35, oz: -0.1, hour: 12, free: true }, 25000);
  shots.target = true;

  console.log(`relocated-walk: last status: ${last}`);
  const ok = spawned > 0 && arrived >= spawned;
  console.log(ok ? `relocated-walk: PASS (${arrived}/${spawned} reached the target)` : `relocated-walk: FAIL (${arrived}/${spawned} reached the target)`);
  return ok ? 0 : 1;
}

export default {
  name: "relocated-walk-proof",
  usage: "relocated-walk-proof [out] [--control] | survey [x,z] [radius] [out]   (run in a --mission everon --profile full session)",
  run: (ctx) => {
    const args = ctx?.args ?? [];
    return args[0] === "survey" ? survey(args.slice(1)) : walk(args);
  },
};
