#!/usr/bin/env node
// tools\cli shots area|crop: the screenshot rig for the area polish pass
// (docs/agent-area-polish.md). One blocking call per area; prints about ten lines.
//
//   shots area <name | x,z> [--radius 30] [--out tmp/shots/<area>] [--machine any]
//              [--hour 17.9] [--annotate] [--pick] [--detail <Entity>] [--keep] [--fov 60]
//   shots crop <shot> <x,y,w,h>        native-resolution crop under 800 px
//   shots selftest
//
// It reuses (or starts) one playtest-mission session with one client, poses that
// client with the harness `capture world` verb (golden-hour clock, clear sky, no HUD),
// and measures placement with the harness `probe` verb. Files written to --out:
//   wide.png top.png N.png E.png S.png W.png close-<Entity>.png   1280 wide, for the website
//   contact.jpg   one agent-sized sheet (<= 1568 px, jpeg q85, label under each tile)
//   index.json    each shot's camera pose + entity, and every entity's numeric checks
import { spawnSync } from "node:child_process";
import { copyFileSync, existsSync, mkdirSync, readFileSync, readdirSync, rmSync, statSync, writeFileSync } from "node:fs";
import { decodePng, pngUnder } from "./bmp.mjs";
import { dirname, join, resolve, basename, relative } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const AREAS_FILE = join(ROOT, "agent-box", "areas.json");
const IMAGE_PS1 = join(ROOT, "agent-box", "shots-image.ps1");

export const GOLDEN_HOUR = 17.9; // local hour (the server clock is 4.73 h behind at this longitude), shared by every shot
export const EYE = 1.6; // metres from the posed body origin to the camera
export const FOV_V = 60; // vertical field of view assumed by --annotate (calibrated against a shot)
const CROP_MAX = 800;
const MAX_KB = 500;
const MAX_CLOSEUPS = 8;
const MAX_ENTITIES = 70;
const TILE_LIMIT = 12;
// Problem thresholds (metres, degrees).
export const LIMITS = { gap: 0.15, tilt: 5, walk: 1.5 };

const add = (a, b) => [a[0] + b[0], a[1] + b[1], a[2] + b[2]];
const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
const mul = (a, k) => [a[0] * k, a[1] * k, a[2] * k];
const len = (a) => Math.hypot(a[0], a[1], a[2]);
const r2 = (n) => Math.round(n * 100) / 100;
const dirOf = (deg) => [Math.sin((deg * Math.PI) / 180), 0, Math.cos((deg * Math.PI) / 180)];

// ---------------------------------------------------------------- pure helpers

export function parseArgs(argv) {
  const out = { _: [], flags: {} };
  const valued = new Set(["radius", "out", "machine", "hour", "detail", "fov"]);
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a.startsWith("--")) {
      const k = a.slice(2);
      if (valued.has(k)) out.flags[k] = argv[++i];
      else out.flags[k] = true;
    } else out._.push(a);
  }
  return out;
}

export function loadAreas() {
  return JSON.parse(readFileSync(AREAS_FILE, "utf8")).areas;
}

// "hospital" -> registry entry; "262,255" -> { name: "xz-262-255", x, z }. y comes from the game.
export function resolveArea(arg, areas, radius) {
  const hit = areas.find((a) => a.name === arg);
  if (hit) return { ...hit, radius: radius ?? hit.radius, slug: hit.name };
  const m = /^(-?\d+(?:\.\d+)?)[, ](-?\d+(?:\.\d+)?)$/.exec(String(arg));
  if (!m) throw new Error(`unknown area "${arg}". Known: ${[...new Set(areas.map((a) => a.name))].join(", ")}; or pass x,z`);
  const x = Number(m[1]);
  const z = Number(m[2]);
  return { n: 0, name: `xz-${Math.round(x)}-${Math.round(z)}`, slug: `xz-${Math.round(x)}-${Math.round(z)}`, x, y: 0, z, radius: radius ?? 30 };
}

// Camera yaw/pitch (degrees) from a position to the point it looks at. The harness flies a free camera at stand + EYE (clean shots).
export function poseOf(stand, target) {
  const d = sub(target, stand);
  const horiz = Math.hypot(d[0], d[2]);
  const yaw = (Math.atan2(d[0], d[2]) * 180) / Math.PI;
  const pitch = horiz > 0.001 ? (Math.atan2(d[1], horiz) * 180) / Math.PI : d[1] > 0 ? 90 : -90;
  return { yaw, pitch };
}

// Screen position (pixels in a w x h frame) of a world point, or null when behind or off-screen.
export function project(stand, target, point, w = 1280, h = 720, fovV = FOV_V) {
  const eye = add(stand, [0, EYE, 0]);
  const { yaw, pitch } = poseOf(eye, target);
  const sy = Math.sin((yaw * Math.PI) / 180);
  const cy = Math.cos((yaw * Math.PI) / 180);
  const sp = Math.sin((pitch * Math.PI) / 180);
  const cp = Math.cos((pitch * Math.PI) / 180);
  const f = [cp * sy, sp, cp * cy];
  const r = [cy, 0, -sy];
  const u = [-sp * sy, cp, -sp * cy];
  const v = sub(point, eye);
  const zc = v[0] * f[0] + v[1] * f[1] + v[2] * f[2];
  if (zc < 0.3) return null;
  const k = h / 2 / Math.tan((fovV * Math.PI) / 360);
  const x = w / 2 + ((v[0] * r[0] + v[1] * r[1] + v[2] * r[2]) / zc) * k;
  const y = h / 2 - ((v[0] * u[0] + v[1] * u[1] + v[2] * u[2]) / zc) * k;
  if (x < 0 || x > w || y < 0 || y > h) return null;
  return { x: Math.round(x), y: Math.round(y) };
}

const SKIP_NAMES = /^(Points|RoadEntity|SplinePoints|GodRays|HBAO|HDR|PPAA|SSDO|UnderWater)$/;
const SKIP_LAYERS = /^(Village_Roads|default|Roadmap|Tests|Gamemode)\.layer$/;

// Placed entities with geometry inside the radius, nearest first.
export function entitiesIn(placed, area) {
  const near = placed
    .filter((e) => e.prefab && !SKIP_NAMES.test(e.name) && !SKIP_LAYERS.test(e.layer))
    .map((e) => ({ ...e, dist: Math.hypot(e.x - area.x, e.z - area.z) }))
    .filter((e) => e.dist <= area.radius)
    .sort((a, b) => a.dist - b.dist);
  return near.slice(0, MAX_ENTITIES);
}

// Interactables (named entities with an action), deduped by position, most central first.
export function interactablesIn(entities) {
  const out = [];
  for (const e of entities) {
    if (!e.actions || !e.actions.length) continue;
    if (out.some((o) => Math.hypot(o.x - e.x, o.z - e.z) < 1.2 && Math.abs(o.y - e.y) < 1.2)) continue;
    out.push(e);
  }
  return out.slice(0, MAX_CLOSEUPS);
}

export function parseKv(text) {
  const out = {};
  for (const part of String(text ?? "").split(" ")) {
    const i = part.indexOf("=");
    if (i > 0) out[part.slice(0, i)] = part.slice(i + 1);
  }
  return out;
}

const triple = (s) => String(s).split(",").map(Number);

// ent probe detail -> numbers. Missing entity -> { missing: true }.
export function parseEnt(detail) {
  const kv = parseKv(detail);
  if (kv.missing) return { missing: true };
  return {
    bmin: triple(kv.bmin),
    bmax: triple(kv.bmax),
    gap: Number(kv.gap),
    tilt: Number(kv.tilt),
    walk: Number(kv.walk),
    clip: kv.clip && kv.clip !== "none" ? kv.clip.split(";") : [],
  };
}

// The numeric checks. wallMounted entities (signs, anything above 1.5 m) skip the ground gap.
export function problemsFor(entity, m) {
  if (m.missing) return ["not in the world"];
  const list = [];
  const mounted = /^Sign_/.test(entity.name) || entity.y > 1.5;
  if (!mounted && m.gap > LIMITS.gap) list.push(`floating ${r2(m.gap)} m`);
  if (!mounted && m.gap < -LIMITS.gap) list.push(`sunk ${r2(-m.gap)} m`);
  if (m.tilt > LIMITS.tilt) list.push(`tilted ${Math.round(m.tilt)} deg`);
  if (entity.actions && entity.actions.length && m.walk > LIMITS.walk) list.push(m.walk >= 99 ? "unreachable (no standing spot within 3 m)" : `reach ${r2(m.walk)} m to standing room`);
  if (m.clip.length) list.push(`clips ${m.clip.join(", ")}`);
  return list;
}

// Camera candidates for a shot kind; the guard takes the first that passes.
export function candidates(kind, area, extra = {}) {
  let c = [area.x, area.y, area.z];
  const R = area.radius;
  const out = [];
  if (kind === "wide") {
    for (const az of [180, 90, 270, 0]) for (const k of [1.5, 1.9, 1.2]) out.push({ az, dist: R * k, up: 12, mode: "wide" });
  } else if (kind === "top") {
    for (const up of [Math.max(30, R * 2.2), Math.max(30, R * 2.2) + 10]) out.push({ az: 0, dist: 0.05, up, mode: "free" });
  } else if (kind === "eye") {
    for (const f of [1, 0.85, 0.7, 0.55]) for (const off of [0, 18, -18]) out.push({ az: extra.az + off, dist: R * f, up: 0, mode: "view" });
  } else if (kind === "close") {
    c = [extra.x, 0, extra.z]; // around the entity; the camera side faces the area centre first
    const toward = (Math.atan2(area.x - extra.x, area.z - extra.z) * 180) / Math.PI;
    const azs = [toward, toward + 45, toward - 45, toward + 90, toward - 90, toward + 180];
    if (extra.yaw !== undefined) azs.splice(1, 0, extra.yaw);
    for (const d of [2.5, 2, 1.6]) for (const az of azs) out.push({ az, dist: d, up: 0, mode: "view", close: true });
  }
  return { c, list: out };
}

export function contactLayout(count) {
  const cols = count <= 9 ? 3 : 4;
  return { cols, rows: Math.ceil(count / cols) };
}

// ------------------------------------------------------------------ image tool

function image(spec) {
  const file = join(process.env.TEMP || process.env.TMP || ROOT, `shots-image-${process.pid}-${Math.random().toString(36).slice(2, 8)}.json`);
  writeFileSync(file, JSON.stringify(spec));
  const r = spawnSync("powershell", ["-NoProfile", "-ExecutionPolicy", "Bypass", "-File", IMAGE_PS1, file], { encoding: "utf8", windowsHide: true });
  rmSync(file, { force: true });
  const text = `${r.stdout ?? ""}${r.stderr ?? ""}`.trim();
  if (r.status !== 0) throw new Error(`image tool failed (${spec.op}): ${text.slice(-300)}`);
  return text;
}

function cropCommand(rest) {
  const [shot, box] = rest;
  const m = /^(\d+)[, ](\d+)[, ](\d+)[, ](\d+)$/.exec(box ?? "");
  if (!shot || !m) {
    console.log("usage: tools\\cli shots crop <shot> <x,y,w,h>   (pixels of the 1280-wide shot; output at most 800 px)");
    return 1;
  }
  let file = resolve(shot);
  if (!existsSync(file)) {
    const base = join(ROOT, "tmp", "shots");
    const name = shot.endsWith(".png") ? shot : `${shot}.png`;
    const hits = existsSync(base)
      ? readdirSync(base).map((d) => join(base, d, name)).filter(existsSync).sort((a, b) => statSync(b).mtimeMs - statSync(a).mtimeMs)
      : [];
    if (!hits.length) {
      console.log(`crop: no such shot ${shot} (pass a path, or a name like wide / close-ATM_Debug from the newest tmp/shots/<area>)`);
      return 1;
    }
    file = hits[0];
  }
  const out = file.replace(/\.png$/i, "") + `-crop-${m[1]}_${m[2]}_${m[3]}_${m[4]}.png`;
  const res = image({ op: "crop", in: file, out, x: +m[1], y: +m[2], w: +m[3], h: +m[4], maxSide: CROP_MAX });
  console.log(`crop: ${out} (${res.replace("ok ", "")})`);
  return 0;
}

// -------------------------------------------------------------------- session

function sessionTool(args) {
  const r = spawnSync(process.execPath, [join(ROOT, "agent-box", "session.mjs"), ...args], { encoding: "utf8", windowsHide: true, cwd: ROOT, env: { ...process.env, LF_CALLER_PID: String(process.pid) } });
  return { code: r.status ?? 1, text: `${r.stdout ?? ""}${r.stderr ?? ""}` };
}

async function openClient(machine) {
  const h = await import("./harness.mjs");
  const s = await import("./session.mjs");
  h.setBoxes([s.clientBox(s.sessionDir(), 1)]);
  const alive = () => {
    const r = h.sendCommand({ verb: "probe", mode: "ground", x: 0, z: 0 }, 4000);
    return r.status === "ok";
  };
  if (s.readRemotePointer()) return { h, remote: true, started: false };
  let started = false;
  if (!alive()) {
    const up = sessionTool(["up", "--clients", "1", "--mission", "playtest", "--machine", machine, "--wait"]);
    if (up.code !== 0) throw new Error(`session up failed: ${up.text.trim().split("\n").slice(-3).join(" | ")}`);
    started = true;
    if (s.readRemotePointer()) return { h, remote: true, started };
    h.setBoxes([s.clientBox(s.sessionDir(), 1)]);
  }
  for (let i = 0; i < 20 && !alive(); i++) Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, 1500);
  if (!alive()) throw new Error("the session client does not answer the harness (probe ground)");
  return { h, remote: false, started };
}

// A fresh playtest client starts on the registration form (R-89); sign it so the player spawns and the world loads around it.
function arrive(h) {
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  for (let i = 0; i < 8; i++) {
    const ui = h.sendCommand({ verb: "ui" });
    if (ui.status !== "ok" || !ui.detail.includes("SignButton")) return;
    for (const [w, t] of [["RegFirstName", "Pat"], ["RegLastName", "Player"], ["RegAge", "30"]]) h.sendCommand({ verb: "type", widget: w, text: t });
    h.sendCommand({ verb: "click", widget: "SignButton" });
    wait(3000);
  }
}

// ------------------------------------------------------------------------ area

function areaCommand(argv) {
  const args = parseArgs(argv);
  const target = args._[0];
  if (!target) {
    console.log("usage: tools\\cli shots area <name | x,z> [--radius 30] [--out dir] [--machine any] [--hour 17.9] [--annotate] [--pick] [--detail <Entity>] [--keep]\n       tools\\cli shots crop <shot> <x,y,w,h>");
    return Promise.resolve(1);
  }
  return runArea(args);
}

async function runArea(args) {
  const f = args.flags;
  const radius = f.radius ? Number(f.radius) : undefined;
  const area = resolveArea(args._[0], loadAreas(), radius);
  const hour = f.hour ? Number(f.hour) : GOLDEN_HOUR;
  const fov = f.fov ? Number(f.fov) : FOV_V;
  const outDir = resolve(f.out ?? join(ROOT, "tmp", "shots", area.slug));
  const machine = f.machine ?? "any";

  if (!f.inner) {
    const { h, remote, started } = await openClient(machine);
    try {
      if (remote) return await runRemote(args, outDir);
      return await capture(h, area, { hour, fov, outDir, args });
    } finally {
      if (started && !f.keep) sessionTool(["down"]);
    }
  }
  const h = await import("./harness.mjs");
  const s = await import("./session.mjs");
  h.setBoxes([s.clientBox(s.sessionDir(), 1)]);
  return capture(h, area, { hour, fov, outDir, args });
}

// The session is on a pool machine: run the same command there (--inner), then copy its out dir back.
async function runRemote(args, outDir) {
  const s = await import("./session.mjs");
  const tb = await import("../tools/remote/testbox.mjs");
  const ptr = s.readRemotePointer();
  const rel = relative(ROOT, outDir).split("\\").join("/");
  const r = spawnSync(process.execPath, [join(ROOT, "agent-box", "session.mjs"), "run", "agent-box/shots.mjs", "area", ...argvOf(args), "--inner", "--out", rel], { stdio: "inherit", windowsHide: true, cwd: ROOT });
  const parent = dirname(outDir);
  mkdirSync(parent, { recursive: true });
  rmSync(outDir, { recursive: true, force: true });
  const remoteDir = String(ptr.dir).split("\\").join("/").replace(/\/+$/, "");
  const c = tb.scp(["-r", `${ptr.host}:${remoteDir}/${rel}`, parent]);
  if (c.status !== 0) console.log(`shots: copying ${rel} from ${ptr.machine} failed: ${(c.stderr || "").trim()}`);
  return r.status ?? 1;
}

function argvOf(args) {
  const out = [args._[0]];
  for (const [k, v] of Object.entries(args.flags)) {
    if (k === "inner" || k === "out") continue;
    out.push(`--${k}`);
    if (v !== true) out.push(String(v));
  }
  return out;
}

function makeProbe(h) {
  const groundCache = new Map();
  const q = (spec, timeout = 8000) => {
    const r = h.sendCommand({ verb: "probe", ...spec }, timeout);
    if (r.status !== "ok") throw new Error(`probe ${spec.mode} -> ${r.status} ${r.detail}`);
    return r.detail;
  };
  return {
    ground(x, z) {
      const key = `${Math.round(x * 2)}:${Math.round(z * 2)}`;
      if (!groundCache.has(key)) groundCache.set(key, Number(parseKv(q({ mode: "ground", x, z })).y));
      return groundCache.get(key);
    },
    trace(a, b) {
      const kv = parseKv(q({ mode: "trace", x: a[0], y: a[1], z: a[2], x2: b[0], y2: b[1], z2: b[2] }));
      return { d: Number(kv.d), total: Number(kv.total), ent: kv.ent };
    },
    cam(p) {
      const kv = parseKv(q({ mode: "cam", x: p[0], y: p[1], z: p[2] }));
      return { wall: Number(kv.wall), ceil: Number(kv.ceil) };
    },
    ent(e) {
      return parseEnt(q({ mode: "ent", name: e.name, x: e.x, y: e.y, z: e.z }, 15000));
    },
  };
}

// Choose the camera for one shot: the first candidate that is not inside geometry and whose view
// line to the target is clear; otherwise the clearest one, flagged. This is the raycast back-off.
function placeCamera(probe, kind, area, extra, targetOf) {
  const { c, list } = candidates(kind, area, extra);
  let best = null;
  for (let i = 0; i < list.length; i++) {
    const cand = list[i];
    const dir = dirOf(cand.az);
    const sx = c[0] + dir[0] * cand.dist;
    const sz = c[2] + dir[2] * cand.dist;
    const gs = probe.ground(sx, sz);
    let sy = gs + cand.up;
    if (cand.up > 0) sy = gs + cand.up - EYE;
    if (cand.close && extra.floorY !== undefined) sy = Math.max(gs, extra.floorY);
    const stand = [sx, sy, sz];
    const eye = add(stand, [0, EYE, 0]);
    const target = targetOf(cand, stand);
    const cam = probe.cam(eye);
    let score = 0;
    let reason = "";
    const inside = cam.wall < 0.3 || (cand.mode === "view" && cam.ceil < 1.9) || (cand.mode !== "view" && cam.ceil < 38);
    if (inside) {
      score = -1;
      reason = `inside geometry (wall ${r2(cam.wall)} m, ceiling ${r2(cam.ceil)} m)`;
    } else if (cand.mode === "view" || cand.mode === "wide") {
      // Canopies and roofs between the camera and the target count as blocked. Look from the target side so the target's own collision does not count: start 1 m out.
      const toEye = sub(eye, target);
      const l = len(toEye);
      const start = add(target, mul(toEye, Math.min(1, 0.7 / l)));
      const tr = probe.trace(start, eye);
      const clear = tr.d >= tr.total - 0.05;
      const near = tr.d >= tr.total - 1.2 || tr.ent === (extra.name ?? "\u0000");
      score = clear ? 2 : near ? 1 : 0.5;
      reason = clear ? "" : `view blocked by ${tr.ent} at ${r2(tr.d)} of ${r2(tr.total)} m`;
    } else score = 2;
    const pick = { stand, target, eye, cand, score, reason, backedOff: i > 0 };
    if (!best || pick.score > best.score) best = pick;
    if (score >= 2) break;
    if (extra.close && score >= 1) break;
  }
  return best;
}

async function capture(h, area, o) {
  const placedMod = await import("./layers.mjs");
  const probe = makeProbe(h);
  const t0 = Date.now();
  arrive(h);
  mkdirSync(o.outDir, { recursive: true });
  for (const f of readdirSync(o.outDir)) if (/\.(png|jpg|json)$/i.test(f)) rmSync(join(o.outDir, f), { force: true });

  const gc = probe.ground(area.x, area.z);
  area = { ...area, y: gc };
  const ents = entitiesIn(placedMod.loadPlaced(ROOT), area);
  const pads = interactablesIn(ents);

  // Numeric placement checks first: cheap, and the annotation uses the entity list.
  const checked = [];
  for (const e of ents) {
    let m;
    try {
      m = probe.ent(e);
    } catch (err) {
      m = { missing: true };
    }
    checked.push({ name: e.name, layer: e.layer, x: e.x, y: e.y, z: e.z, actions: e.actions, dist: r2(e.dist), ...m, problems: problemsFor(e, m) });
  }

  // The clock is the server's: set it (and clear weather) there once, before the free camera exists
  // (setting it again while that camera is active crashed the client). The reply says what it set.
  const t = h.sendCommand({ verb: "time", hour: o.hour }, 10000);
  if (t.status !== "ok") throw new Error(`time ${o.hour} -> ${t.status} ${t.detail}`);
  const clock = t.detail;
  Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, 2500);
  const shots = [];
  const take = (id, kind, label, entity, placed) => {
    const { stand, target, eye } = placed;
    const off = sub(stand, target);
    const name = `shots_${area.slug.replace(/[^A-Za-z0-9]/g, "_")}_${id.replace(/[^A-Za-z0-9]/g, "_")}`;
    const r = h.sendCommand({ verb: "capture", mode: "world", name, x: target[0], y: target[1], z: target[2], ox: off[0], oy: off[1], oz: off[2], hour: o.hour, free: o.args.flags["free-camera"] === true }, 30000);
    const shot = { id, kind, label, entity: entity ?? null, stand: stand.map(r2), target: target.map(r2), ...poseOf(eye, target), eye: eye.map(r2), status: r.status, adjusted: placed.backedOff || undefined, note: placed.reason || undefined };
    const src = r.status === "ok" ? h.resolveCapture(r.detail) : "";
    shot.file = `${id}.png`;
    if (!src) {
      shot.status = `error:${r.status} ${r.detail}`;
    } else {
      const res = image({ op: "norm", in: src, out: join(o.outDir, shot.file), width: 1280 });
      const decoded = decodePng(readFileSync(join(o.outDir, shot.file)));
      const small = pngUnder(decoded.width, decoded.height, decoded.pixels, MAX_KB);
      writeFileSync(join(o.outDir, shot.file), small.png);
      shot.kb = Math.round(small.png.length / 1024);
      if (small.colors) shot.colors = small.colors;
      const [, mean, sd] = res.split(" ");
      shot.luma = Number(mean);
      shot.spread = Number(sd);
      if (shot.spread < 9) shot.note = `${shot.note ? shot.note + "; " : ""}flat image (sky, ground or wall only?)`;
    }
    shots.push(shot);
    return shot;
  };
  const centre = [area.x, area.y, area.z];
  try {
  const lookAt = (h1) => [area.x, area.y + h1, area.z];

  take("wide", "wide", "wide", null, placeCamera(probe, "wide", area, {}, () => lookAt(1)));
  // The character camera cannot pitch, so a top-down needs the (crashing) free camera: opt in with --free-camera.
  if (o.args.flags["free-camera"]) take("top", "top", "top", null, placeCamera(probe, "top", area, {}, () => lookAt(0)));
  for (const [dirName, az] of [["N", 0], ["E", 90], ["S", 180], ["W", 270]]) {
    // Standing at the radius edge on that side, looking in: the camera sits on the side named.
    take(dirName, "eye", dirName, null, placeCamera(probe, "eye", area, { az }, (cand, stand) => [centre[0], stand[1] + EYE, centre[2]]));
  }
  for (const p of pads) {
    const floorY = p.y < 1.5 ? p.y : undefined;
    const h1 = p.y > 1.5 ? 0 : 1;
    const placed = placeCamera(probe, "close", area, { x: p.x, z: p.z, yaw: p.yaw, floorY, name: p.name }, () => [p.x, p.y + h1, p.z]);
    take(`close-${p.name}`, "close", p.name, p.name, placed);
  }

  } finally {
    // Give the player view and HUD back (one free camera served every shot).
    try {
      h.sendCommand({ verb: "clean" }, 8000);
    } catch {}
  }

  // Annotation marks, then the sheet.
  const named = checked.filter((e) => e.name);
  const tiles = shots.slice(0, TILE_LIMIT).map((s) => {
    const own = checked.find((e) => e.name === s.entity);
    const bad = own && own.problems.length ? "!" : "";
    const tile = { file: join(o.outDir, s.file), label: `${s.label}${bad}${s.adjusted ? "~" : ""}` };
    if (o.args.flags.annotate) {
      const marks = [];
      for (const e of named) {
        if (marks.length >= 14) break;
        const pt = e.bmin ? [(e.bmin[0] + e.bmax[0]) / 2, (e.bmin[1] + e.bmax[1]) / 2, (e.bmin[2] + e.bmax[2]) / 2] : [e.x, e.y + 0.5, e.z];
        const px = project(s.stand, s.target, pt, 1280, 720, o.fov);
        if (px) marks.push({ name: e.name, x: px.x, y: px.y });
      }
      tile.marks = marks;
    }
    return tile;
  });
  const lay = contactLayout(tiles.length);
  const sheetRes = image({ op: "sheet", tiles, out: join(o.outDir, "contact.jpg"), cols: lay.cols, maxWidth: 1568, quality: 85 });

  const problems = checked.filter((e) => e.problems.length);
  const index = {
    area: { name: area.name, n: area.n, centre, radius: area.radius },
    hour: o.hour,
    clock,
    fovAssumed: o.fov,
    shots,
    entities: checked,
    problems: problems.map((e) => ({ name: e.name, issues: e.problems })),
  };
  writeFileSync(join(o.outDir, "index.json"), JSON.stringify(index, null, 2));

  let picked = "";
  if (o.args.flags.pick) picked = pick(area, o.outDir, shots, pads, o.args.flags.detail, checked);

  const rel = (p) => relative(ROOT, p);
  const suspect = shots.filter((s) => s.note && /flat|inside|blocked/.test(s.note));
  const secs = Math.round((Date.now() - t0) / 1000);
  console.log(`shots area ${area.name} r=${area.radius} centre ${centre.map(r2).join(",")} hour ${o.hour} (server clock ${clock || "unknown"}): ${shots.length} shots, ${checked.length} entities, ${secs}s`);
  console.log(`contact: ${rel(join(o.outDir, "contact.jpg"))} (${sheetRes.replace("ok ", "")}, ${tiles.length} tiles${shots.length > TILE_LIMIT ? `, +${shots.length - TILE_LIMIT} more files` : ""}${o.args.flags.annotate ? ", annotated" : ""})`);
  const moved = shots.filter((s) => s.adjusted).length;
  console.log(`cameras: ${moved} backed off or turned by the guard${suspect.length ? `; suspect: ${suspect.map((s) => `${s.id} (${s.note.split(";")[0]})`).join(", ")}` : ""}`);
  console.log(problems.length ? `problems: ${problems.length} of ${checked.length} entities` : `problems: none in ${checked.length} entities`);
  for (const e of problems.slice(0, 5)) console.log(`  ${e.name}: ${e.problems.join("; ")}`);
  if (problems.length > 5) console.log(`  +${problems.length - 5} more in index.json`);
  if (picked) console.log(picked);
  console.log(`index: ${rel(join(o.outDir, "index.json"))}`);
  return shots.some((s) => s.status.startsWith("error")) ? 1 : 0;
}

// The website pair: the elevated wide, and the interactable closest to the centre (or --detail).
function pick(area, outDir, shots, pads, detail, checked) {
  const closes = shots.filter((s) => s.kind === "close" && s.status === "ok");
  let want = closes[0];
  if (typeof detail === "string") want = closes.find((s) => s.entity === detail);
  const wide = shots.find((s) => s.id === "wide" && s.status === "ok");
  if (!wide || !want) return `pick: nothing copied (${!wide ? "no wide shot" : `no close-up for ${detail ?? "any interactable"}`})`;
  const dir = join(ROOT, "website", "public", "docs-images", `area-${area.n || 0}-${area.slug}`);
  mkdirSync(dir, { recursive: true });
  copyFileSync(join(outDir, wide.file), join(dir, "wide.png"));
  copyFileSync(join(outDir, want.file), join(dir, "detail.png"));
  return `pick: wide + ${want.entity} -> ${relative(ROOT, dir)} (override with --detail <Entity>)`;
}

// --------------------------------------------------------------------- selftest

function selftest() {
  const fails = [];
  const areas = loadAreas();
  if (areas.length < 17) fails.push(`areas.json has ${areas.length} entries`);
  for (const n of Array.from({ length: 17 }, (_, i) => i + 1)) if (!areas.some((a) => a.n === n)) fails.push(`area ${n} missing`);
  const a = resolveArea("hospital", areas);
  if (a.radius !== 18 || a.slug !== "hospital") fails.push("resolveArea hospital");
  const b = resolveArea("262,255", areas, 12);
  if (b.slug !== "xz-262-255" || b.radius !== 12) fails.push("resolveArea x,z");
  let threw = false;
  try {
    resolveArea("nowhere", areas);
  } catch {
    threw = true;
  }
  if (!threw) fails.push("resolveArea accepted an unknown area");
  // Camera yaw/pitch: stand south of the target and look north (+z): yaw 0, level.
  const p = poseOf([0, 0, -10], [0, 0, 0]);
  if (Math.abs(p.yaw) > 1e-6 || Math.abs(p.pitch) > 1e-6) fails.push(`poseOf north ${JSON.stringify(p)}`);
  const down = poseOf([0, 20, 0.05], [0, 0, 0]);
  if (down.pitch > -89) fails.push("top-down pitch");
  // A point straight ahead projects to the middle; one to the east of a north-facing camera lands right of centre.
  const mid = project([0, 0, -10], [0, 1.6, 0], [0, 1.6, 0]);
  if (!mid || Math.abs(mid.x - 640) > 2 || Math.abs(mid.y - 360) > 2) fails.push(`project centre ${JSON.stringify(mid)}`);
  const east = project([0, 0, -10], [0, 1.6, 0], [3, 1.6, 0]);
  if (!east || east.x <= 640) fails.push(`project east ${JSON.stringify(east)}`);
  if (project([0, 0, -10], [0, 1.6, 0], [0, 1.6, -20])) fails.push("project behind the camera");
  // Problems.
  const ok = parseEnt("bmin=0,0,0 bmax=1,1,1 gap=0.02 tilt=1.5 walk=0.6 clip=none");
  if (problemsFor({ name: "A", y: 0, actions: ["X"] }, ok).length) fails.push("a clean entity has problems");
  const bad = parseEnt("bmin=0,0,0 bmax=1,1,1 gap=0.4 tilt=9 walk=99 clip=Wall_1;Crate");
  const list = problemsFor({ name: "B", y: 0, actions: ["X"] }, bad).join("|");
  for (const w of ["floating", "tilted", "unreachable", "clips Wall_1, Crate"]) if (!list.includes(w)) fails.push(`problems missed ${w}: ${list}`);
  if (!problemsFor({ name: "C", y: 0, actions: [] }, parseEnt("bmin=0,0,0 bmax=1,1,1 gap=-0.3 tilt=0 walk=99 clip=none")).join().includes("sunk")) fails.push("sunk");
  if (problemsFor({ name: "Sign_X", y: 3.3, actions: [] }, parseEnt("bmin=0,3,0 bmax=1,4,1 gap=2.9 tilt=0 walk=99 clip=none")).length) fails.push("a wall sign is flagged floating");
  if (!problemsFor({ name: "D", y: 0 }, { missing: true })[0].includes("not in the world")) fails.push("missing entity");
  // Interactables dedupe by position.
  const pads = interactablesIn([
    { name: "A", x: 0, y: 0, z: 0, actions: ["a"], prefab: "p" },
    { name: "A_Debug", x: 0.2, y: 0, z: 0.1, actions: ["a"], prefab: "p" },
    { name: "B", x: 5, y: 0, z: 0, actions: ["b"], prefab: "p" },
    { name: "C", x: 9, y: 0, z: 0, actions: [], prefab: "p" },
  ]);
  if (pads.length !== 2) fails.push(`interactablesIn kept ${pads.length}`);
  const eye = candidates("eye", { x: 0, y: 0, z: 0, radius: 30 }, { az: 90 });
  if (eye.list[0].dist !== 30 || eye.list[0].up !== 0) fails.push("eye candidates");
  if (contactLayout(12).cols !== 4 || contactLayout(9).cols !== 3) fails.push("contactLayout");
  for (const f of fails) console.log(`  ${f}`);
  console.log(fails.length ? "shots selftest: FAIL" : "shots selftest: OK");
  return fails.length ? 1 : 0;
}

export async function main(argv) {
  const [cmd, ...rest] = argv;
  if (cmd === "area") return areaCommand(rest);
  if (cmd === "crop") return cropCommand(rest);
  if (cmd === "selftest") return selftest();
  console.log("usage: tools\\cli shots area <name | x,z> [--radius N] [--out dir] [--machine any] [--hour H] [--annotate] [--pick] [--detail <Entity>] [--keep]\n       tools\\cli shots crop <shot> <x,y,w,h>\n       tools\\cli shots selftest");
  return cmd ? 1 : 0;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  main(process.argv.slice(2)).then(
    (code) => process.exit(code),
    (err) => {
      console.error(`shots: ${err.message}`);
      process.exit(1);
    },
  );
}
