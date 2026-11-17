import { copyFileSync, existsSync, mkdirSync, readFileSync } from "node:fs";
import { basename, dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { bmpFileToPng } from "../bmp.mjs";
import { arrive, clientBox, newestLog, parseState, sessionDir, sessionPaths } from "../session.mjs";

// A generated building works for a player (tools\cli building, docs/building-pipeline/README.md).
// Run it on a full-quality session: a building's check is how its materials, glass and doors look,
// which the lowspec profile's lowest textures would hide. `tools\cli building proof <slug>` runs the
// whole thing (session up, this verb with --slug, down) and is the normal way in.
//   node agent-box/session.mjs building-proof --slug supermarket [--entity Village_Supermarket] [--out <dir>] [--box x0,x1,z0,z1,h] [--no-scenario]
//   node agent-box/session.mjs building-proof [--entity Village_Supermarket] [--side s9] [--shot]   (front door only)
// With --slug, for every door set of <Name>_Base.et: stand 1.4 m from it, the F key offers the door, the
// server opens it, a character-wide gap through the open doorway traces clear at knee, waist and head
// height, and it closes again; pictures of each door shut and open, the four sides, the street corner and
// the hall from inside; then the building's own scenario, tools/buildings/<slug>/proof.json.
const DOOR_ACTION = "SCR_DoorUserAction";
const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");

function flag(args, name, fallback) {
  const i = args.indexOf(name);
  return i >= 0 && args[i + 1] ? args[i + 1] : fallback;
}

const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);

async function runBuildingProof(args = []) {
  if (flag(args, "--slug")) return runSlug(args);
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const entity = flag(args, "--entity", "Village_Supermarket");
  const side = flag(args, "--side", "s9");
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 300)}`);
    return r;
  };
  const fails = [];
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what} returned ${r.status} ${r.detail}`);
    return r;
  };
  const shot = (name) => {
    if (!args.includes("--shot")) return;
    h.setBoxes([clientBox(dir, 1)]);
    const s = h.sendCommand({ verb: "capture", mode: "ui", name }, 25000);
    wait(3000);
    console.log(`client 1: shot ${name} -> ${h.resolveCapture(s.detail) || "not written"}`);
  };

  arrive(on, 1, wait);
  must(on(1, { verb: "stand", entityName: entity, side }), `stand on the ${side} side of ${entity}`);
  wait(3000);
  const look = must(on(1, { verb: "look" }), `look at ${entity}`);
  if (!look.detail.includes(DOOR_ACTION)) fails.push(`the F key does not offer ${DOOR_ACTION} at ${entity}: ${look.detail.slice(0, 200)}`);
  shot(`building_${entity}_shut`);

  must(on(1, { verb: "use", className: DOOR_ACTION }, 10000), "open the door");
  wait(2500);
  shot(`building_${entity}_open`);
  must(on(1, { verb: "use", className: DOOR_ACTION }, 10000), "close the door");
  wait(1500);

  if (fails.length) {
    console.log("building-proof: FAIL");
    for (const f of fails) console.log(`  ${f}`);
    return 1;
  }
  console.log("building-proof: OK");
  return 0;
}

// Pure: where to stand for a door, and the traces through it, in building space. A door set's +Z is its
// normal; an outside door is approached from outside (away from the building's centre), an inside one
// from the side nearer the centre.
export function doorPlan(door) {
  const r = (door.yaw * Math.PI) / 180;
  const n = [Math.sin(r), 0, Math.cos(r)];
  const across = [Math.cos(r), 0, -Math.sin(r)];
  const out = n[0] * door.at[0] + n[2] * door.at[2] > 0 ? 1 : -1;
  const side = /_EXT/.test(door.name) ? out : -out;
  const at = (k, lat, y) => [door.at[0] + n[0] * k * side + across[0] * lat, door.at[1] + y, door.at[2] + n[2] * k * side + across[2] * lat].map((v) => +v.toFixed(3));
  const look = at(0, 0, 1.2);
  const stand = at(1.4, 0, 0);
  const lanes = [-0.4, 0, 0.4].map((lat) => [0.5, 1.0, 1.6].map((y) => ({ from: at(1.2, lat, y), to: at(-1.2, lat, y) })));
  return { look, from: stand.map((v, i) => +(v - look[i]).toFixed(3)), lanes };
}

// Pure: the doorway is walkable when one lane (a strip a character wide) is clear at every height.
export function walkable(laneResults) {
  return laneResults.some((lane) => lane.every((t) => t.d >= t.total - 0.05));
}

async function runSlug(args) {
  const h = await import("../harness.mjs");
  const { loadWorld } = await import("../layers.mjs");
  const { buildingFiles, readChildren } = await import("../../tools/buildings/pipeline/store-slots.mjs");
  const { loadScenario, runScenario, scenarioProblems, toWorld } = await import("../../tools/buildings/pipeline/scenario.mjs");
  const slug = flag(args, "--slug");
  const spec = JSON.parse(readFileSync(join(ROOT, "tools", "buildings", slug, "spec.json"), "utf8"));
  const addonDir = join(ROOT, "addons", spec.addon || "LifeFramework");
  const files = buildingFiles(spec);
  const prefabs = [files.store, files.building].filter(Boolean);
  const named = flag(args, "--entity");
  const match = (e) => (named ? e.name === named : prefabs.some((p) => (e.prefab || "").endsWith(p)));
  // A kit in a grid claim (core GridWorld or the pack's Everon1989Grid) is not in the default world's layers.
  const { gridLayerDirs } = await import("../../tools/lib/worlds.mjs");
  const place = loadWorld(ROOT).find(match) || loadWorld(ROOT, gridLayerDirs(ROOT)).find(match);
  if (!place) {
    console.log(`building-proof: FAIL  ${named || prefabs.join(" or ")} is not placed in any world layer (tools\\cli place ground)`);
    return 1;
  }
  const entity = place.name;
  // A building made from a vanilla one (spec.vanilla) hangs its door sets on the model's sockets, with no
  // coords to walk to; they are the base game's own doors, so the door walk is skipped and the scenario proves the rest.
  if (spec.vanilla) console.log(`doors: skipped, ${basename(files.building)} keeps the vanilla door sets on their sockets`);
  const doors = spec.vanilla ? [] : readChildren(readFileSync(join(addonDir, files.base), "utf8")).filter((c) => /^DoorSet/.test(c.name));
  const box = (flag(args, "--box") || "").split(",").map(Number);
  const [fw, fd] = spec.budget?.footprint_m || [12, 10];
  const [x0, x1, z0, z1, ht] = box.length === 5 && box.every((v) => !Number.isNaN(v)) ? box : [-fw / 2, fw / 2, -fd / 2, fd / 2, spec.budget?.height_m || 5];
  const out = resolve(flag(args, "--out", join(ROOT, "tmp", "buildings", slug, "proof")));
  // A capture name takes letters, digits and underscores only; a slug can have dashes (gun-store).
  const prefix = `bp_${slug.replace(/\W/g, "_")}`;
  mkdirSync(out, { recursive: true });
  const scenario = args.includes("--no-scenario") ? null : loadScenario(ROOT, slug);
  if (scenario) {
    const p = scenarioProblems(scenario);
    if (p.length) {
      for (const f of p) console.log(`FAIL  proof.json ${f}`);
      return 1;
    }
  }

  const dir = sessionDir();
  const on = (spec2, timeout = 15000) => {
    h.setBoxes([clientBox(dir, 1)]);
    const r = h.sendCommand(spec2, timeout);
    console.log(`${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 300)}`);
    return r;
  };
  const pictures = [];
  const keep = (name, r) => {
    for (let i = 0; i < 10; i++) {
      const f = h.resolveCapture(r.detail);
      if (f && existsSync(f)) {
        // A capture is a .bmp: keep a .png, which a PR, the evidence command and an image reader take.
        const bmp = /.bmp$/i.test(f);
        const to = join(out, `${name}${bmp ? ".png" : f.slice(f.lastIndexOf("."))}`);
        if (bmp) bmpFileToPng(f, to);
        else copyFileSync(f, to);
        pictures.push(to);
        return;
      }
      wait(500);
    }
    console.log(`  picture ${name} was not written`);
  };
  const capture = (name, look, from) => {
    const [x, y, z] = toWorld(place, look);
    const [ox, oy, oz] = toWorld(place, from, true);
    const r = on({ verb: "capture", mode: "world", name: `${prefix}_${name}`, x, y, z, ox, oy, oz }, 30000);
    keep(name, r);
    return r;
  };
  const fails = [];
  const results = [];
  arrive((n, s, t) => on(s, t), 1, wait);
  wait(3000);
  // The first frames after a teleport draw the building before it streams in: one throwaway shot.
  capture("warmup", [0, 2.5, z0], [(x0 - x1) / 2, -0.8, -12]);
  wait(5000);

  // 1. Doors: the F key offers each, it opens, the doorway is walkable, it closes.
  for (const [i, door] of doors.entries()) {
    const dp = doorPlan(door);
    const label = `door${i}_${door.name}`;
    capture(`${label}_shut`, dp.look, dp.from);
    wait(1500);
    const look = on({ verb: "look" });
    const trace = (t) => {
      const [a, b] = [toWorld(place, t.from), toWorld(place, t.to)];
      const r = on({ verb: "probe", mode: "trace", x: a[0], y: a[1], z: a[2], x2: b[0], y2: b[1], z2: b[2] }, 10000);
      const kv = parseState(r.detail);
      return { d: Number(kv.d), total: Number(kv.total), ent: kv.ent };
    };
    const shutBlocked = !walkable(dp.lanes.map((lane) => lane.map(trace)));
    const opened = on({ verb: "use", className: DOOR_ACTION }, 10000);
    // Offered: `look` lists it, or `use` passed the action's own shown and can-perform checks (what the F key
    // offers). `look` lists at most 12 actions within 4 m, so a stocked shelf beside a door can crowd it out.
    const offered = look.detail.includes(DOOR_ACTION) || opened.status === "ok";
    wait(2500);
    const lanes = dp.lanes.map((lane) => lane.map(trace));
    const clear = walkable(lanes);
    capture(`${label}_open`, dp.look, dp.from);
    const closed = on({ verb: "use", className: DOOR_ACTION }, 10000);
    wait(1500);
    results.push({ door: label, offered, opened: opened.status === "ok", walkable: clear, closed: closed.status === "ok", shutBlocked });
    if (!offered) fails.push(`${label}: the F key does not offer ${DOOR_ACTION}: ${look.detail.slice(0, 160)}`);
    if (opened.status !== "ok") fails.push(`${label}: open returned ${opened.status} ${opened.detail}`);
    if (!clear) fails.push(`${label}: the open doorway is not walkable (no lane a character wide clear at 0.5, 1.0, 1.6 m: ${lanes.map((l) => l.map((t) => t.ent || "-").join("/")).join(" | ")})`);
    if (closed.status !== "ok") fails.push(`${label}: close returned ${closed.status} ${closed.detail}`);
    if (!shutBlocked) console.log(`  note ${label}: the shut door did not block the traces (a glass or open frame), so walkable proves less here`);
  }

  // 2. The picture set (look + from is where the feet stand; the eye is 1.6 m up): four sides, the street corner, the hall from inside the entrance.
  const cx = (x0 + x1) / 2;
  const cz = (z0 + z1) / 2;
  const far = Math.max(x1 - x0, z1 - z0) * 0.9;
  const mid = [cx, Math.min(ht, 6) / 2, cz];
  capture("side_front", mid, [0, 0.1 - mid[1], z0 - cz - far]);
  capture("side_back", mid, [0, 0.1 - mid[1], z1 - cz + far]);
  capture("side_left", mid, [x0 - cx - far, 0.1 - mid[1], 0]);
  capture("side_right", mid, [x1 - cx + far, 0.1 - mid[1], 0]);
  capture("street_corner", [cx, 2.6, z0], [-(x1 - x0) * 0.55, -0.9, -9]);
  // Inside: 1.4 m in from the street door (the mirror of where its door check stood), facing the back wall.
  // The model bounds would put it outside: a canopy or a sign reaches past the front wall.
  const street = doors.find((d) => /_EXT/.test(d.name)) || doors[0];
  if (street) {
    const sp = doorPlan(street);
    const feet = sp.look.map((v, i) => 2 * street.at[i] - (v + sp.from[i]));
    const look = [cx, 1.4, cz + (cz - feet[2]) * 0.6];
    capture("inside", look, [feet[0] - look[0], street.at[1] - look[1], feet[2] - look[2]]);
  }

  // 3. The building's own scenario.
  let scen = null;
  if (scenario) {
    const serverLog = () => {
      const f = newestLog(join(sessionPaths(dir).server, "logs"));
      return f ? readFileSync(f, "utf8") : "";
    };
    scen = runScenario(scenario, { on, wait, parseState, serverLog, shot: keep, place, entity, prefix });
    fails.push(...scen.fails);
  }

  console.log(`doors: ${results.map((r) => `${r.door} offered=${+r.offered} open=${+r.opened} walkable=${+r.walkable} close=${+r.closed}`).join("; ") || "none in the base prefab"}`);
  console.log(`pictures (${pictures.length}) in ${out}: ${pictures.map((p) => basename(p)).join(", ")}`);
  if (scenario) console.log(`scenario: ${scenario.steps.length} step(s), ${(scenario.checks || []).length} check(s), ${(scenario.log || []).length} log line(s)`);
  if (!doors.length && !spec.vanilla) fails.push(`${basename(files.base)} has no door sets`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "building-proof: FAIL" : `building-proof: PASS (${doors.length} door(s) open and walkable, ${pictures.length} pictures${scenario ? ", the scenario passed" : ""})`);
  return fails.length ? 1 : 0;
}

export default {
  name: "building-proof",
  usage: "building-proof --slug <slug> [--entity E] [--out dir] [--box x0,x1,z0,z1,h] [--no-scenario] | [--entity E] [--side s9] [--shot]   (tools\\cli building proof <slug> runs it on a full session)",
  run: (ctx) => runBuildingProof(ctx.args),
};
