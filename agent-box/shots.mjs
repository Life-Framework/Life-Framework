#!/usr/bin/env node
// tools\cli shots area|crop: the screenshot rig for the area polish pass
// (docs/agent-area-polish.md). One blocking call per area; prints about ten lines.
//
//   shots area <name | x,z> [--radius 30] [--out tmp/shots/<area>] [--machine any]
//              [--hour 17.9] [--annotate] [--pick] [--detail <Entity>] [--keep] [--fov 60] [--wide-only]
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
const MAX_INTERACTABLES = 40;
const TILE_LIMIT = 12;
// Problem thresholds (metres, degrees).
export const LIMITS = { gap: 0.15, tilt: 5, walk: 1.5 };

const add = (a, b) => [a[0] + b[0], a[1] + b[1], a[2] + b[2]];
const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
const mul = (a, k) => [a[0] * k, a[1] * k, a[2] * k];
const len = (a) => Math.hypot(a[0], a[1], a[2]);
const norm = (a) => mul(a, 1 / (len(a) || 1));
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
  return out.slice(0, MAX_INTERACTABLES);
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
    pgap: kv.pgap === undefined ? undefined : Number(kv.pgap),
    tilt: Number(kv.tilt),
    walk: Number(kv.walk),
    clip: kv.clip && kv.clip !== "none" ? kv.clip.split(";") : [],
  };
}

// The numeric checks. wallMounted entities (signs, anything above 1.5 m) skip the ground gap.
// Heading (degrees) the entity faces, from its layer yaw, against the direction from it to the area's approach point
// (a point radius out from the centre along `front`). Over 100 degrees off is "faces away". Vehicles (big bounds) and
// entities without a yaw are exempt, as are pickups (radial contexts have no front).
export const FACING_LIMIT = 100;
export function facingOff(entity, m, area) {
  if (!area || entity.yaw === undefined || area.front === undefined) return null;
  if (m.bmin && m.bmax && (m.bmax[0] - m.bmin[0]) * (m.bmax[1] - m.bmin[1]) * (m.bmax[2] - m.bmin[2]) > 30) return null;
  const acts = entity.actions || [];
  const npc = /Npc|Clerk|Vince/.test(entity.name || "");
  if (!npc && !acts.length) return null;
  if (acts.length && acts.every((a) => /PickUp/.test(a))) return null;
  const fd = dirOf(area.front);
  const to = [area.x + fd[0] * area.radius - entity.x, 0, area.z + fd[2] * area.radius - entity.z];
  if (len(to) < 1.5 || entity.dist > area.radius * 0.65) return null; // a neighbour's entity has its own approach
  const fw = dirOf(entity.yaw);
  const cos = (fw[0] * to[0] + fw[2] * to[2]) / len(to);
  return Math.round((Math.acos(Math.max(-1, Math.min(1, cos))) * 180) / Math.PI);
}

export function problemsFor(entity, m, area) {
  if (m.missing) return ["not in the world"];
  const list = [];
  const mounted = /^Sign_/.test(entity.name) || entity.y > 1.5;
  if (!mounted && m.gap > LIMITS.gap) list.push(`floating ${r2(m.gap)} m`);
  // Sunk: the pivot (the ground-contact point) is under the floor. Mesh or bounds that reach below a pivot that
  // stands on the floor (vehicles, payphone bases) are modelled that way, not buried. Without a pivot reading, bounds.
  const sunk = m.pgap === undefined ? -m.gap : -m.pgap;
  if (!mounted && sunk > LIMITS.gap) list.push(`sunk ${r2(sunk)} m`);
  if (m.tilt > LIMITS.tilt && !/Bag|Sack/.test(entity.name || "")) // sacks are laid down on purpose
    list.push(`tilted ${Math.round(m.tilt)} deg`);
  if (entity.actions && entity.actions.length && m.walk > (/^Sign_/.test(entity.name) ? 3 : LIMITS.walk)) list.push(m.walk >= 99 ? "unreachable (no standing spot within 3 m of its edge)" : `reach ${r2(m.walk)} m from its edge to standing room`);
  const off = facingOff(entity, m, area);
  if (off !== null && off > FACING_LIMIT) list.push(`faces away ${off} deg from the approach`);
  if (m.clip.length) list.push(`clips ${m.clip.join(", ")}`);
  return list;
}

// Camera candidates for a shot kind; the guard takes the first that passes.
export function candidates(kind, area, extra = {}) {
  let c = [area.x, area.y, area.z];
  const R = area.radius;
  const out = [];
  if (kind === "wide") {
    // From the front side (the heading players approach from), eye level and a little above, about the aim point
    // (the focus entities, else the area centre); scoreWideHero chooses among these.
    const front = area.front ?? 180;
    if (area.wideCands) for (const wc of area.wideCands) out.push({ az: wc.az, dist: wc.dist, up: wc.up, mode: "wide" });
    else {
      const aim = extra.aim ?? { x: area.x, z: area.z, frame: R };
      c = [aim.x, area.y, aim.z];
      for (const off of [0, -20, 20, -40, 40, -65, 65]) for (const k of [0.45, 0.7, 1, 1.4, 1.9]) for (const up of [0, 3, 6.5]) out.push({ az: front + off, dist: aim.frame * k, up, mode: "wide" });
    }
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
    const up = sessionTool(["up", "--clients", "1", "--mission", "playtest", "--profile", "full", "--machine", machine, "--wait"]);
    if (up.code !== 0) throw new Error(`session up failed: ${up.text.trim().split("\n").slice(-3).join(" | ")}`);
    started = true;
    if (s.readRemotePointer()) return { h, remote: true, started };
    h.setBoxes([s.clientBox(s.sessionDir(), 1)]);
  }
  for (let i = 0; i < 20 && !alive(); i++) Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, 1500);
  if (!alive()) throw new Error("the session client does not answer the harness (probe ground)");
  s.requireFullProfile("shots");
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
    trace(a, b, all = false) {
      const kv = parseKv(q({ mode: all ? "traceall" : "trace", x: a[0], y: a[1], z: a[2], x2: b[0], y2: b[1], z2: b[2] }));
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
    // Map objects with no collision (pylons) are invisible to the rays: areas.json lists them as avoid [x, z, r].
    if (cand.mode === "wide" && (area.avoid ?? []).some(([ax, az, ar]) => Math.hypot(sx - ax, sz - az) < ar)) continue;
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
    if (extra.collect) {
      if (score >= 0) extra.collect.push(pick);
      continue;
    }
    if (!best || pick.score > best.score) best = pick;
    if (score >= 2) break;
    if (extra.close && score >= 1) break;
  }
  return best;
}

// Tree canopies have no collision, so a ray sails through them. The layers place the trees, so
// treat each as a sphere (9 m radius, centred 10 m up; a camera within 5 m of the trunk below 22 m is in the crown) and count the ones that sit in the near
// 80% of the depth and in the frame (weighted by the share of the frame they cover), or that the camera is inside.
const TREE = /Vegetation\/Tree\//i;
const CANOPY_R = 7;
const CANOPY_UP = 10;
export function treeOcclusion(eye, target, trees, depth, fovV = FOV_V) {
  const f = norm(sub(target, eye));
  const vf = (fovV * Math.PI) / 180;
  const hf = 2 * Math.atan(Math.tan(vf / 2) * (16 / 9));
  let share = 0;
  for (const t of trees) {
    const v = sub([t.x, t.gy + CANOPY_UP, t.z], eye);
    const d = len(v);
    if (Math.hypot(v[0], v[2]) < 5 && eye[1] - t.gy < 22) return 1;
    if (d < CANOPY_R) return 1;
    if (d > depth * 0.8 + CANOPY_R) continue; // the far half of the view is background, not foreground
    const a = Math.asin(CANOPY_R / d);
    const ang = Math.acos(Math.max(-1, Math.min(1, (v[0] * f[0] + v[1] * f[1] + v[2] * f[2]) / d)));
    if (ang > hf / 2 + a) continue;
    share += (Math.PI * a * a) / (hf * vf); // the canopy's share of the frame
  }
  return Math.min(1, share * 1.5);
}

// ------------------------------------------------------------ hero-based wide scoring (#515)
// The old grid scored "rays that land inside the radius", which rewards any wall or canopy and cannot tell a
// facade from a fuel canopy standing in front of it. The new score asks what a player wants from a wide:
// the area's main interactables are in frame, big enough to read, and not hidden; nothing tall that is not
// one of them fills the foreground.
const FOV_V_TAN = Math.tan((FOV_V * Math.PI) / 360);
const FOV_H_TAN = FOV_V_TAN * (16 / 9);
const HERO_MAX = 12;
const HERO_SIZE_FULL = 0.1; // a hero this wide (share of the frame) counts fully
const FG_DEPTH = 0.75; // a non-hero hit nearer than this share of the target depth is foreground

const volOf = (e) => (e.bmin && e.bmax ? Math.max(0, e.bmax[0] - e.bmin[0]) * Math.max(0, e.bmax[1] - e.bmin[1]) * Math.max(0, e.bmax[2] - e.bmin[2]) : 0);

// How much an interactable is "the area's main thing": its actions and its size. Signs and bystander NPCs count for little.
export function importance(e) {
  const acts = e.actions ? e.actions.length : 0;
  let w = Math.min(acts, 4) + 0.7 * Math.cbrt(volOf(e));
  if (/^Sign_/.test(e.name || "")) w *= 0.2;
  else if (/Npc|Clerk|Tomato|Plum|Apple|Farmer/.test(e.name || "")) w *= 0.35;
  return r2(w);
}

// The entities a wide must show: the area's interactables (with measured bounds), plus any named in areas.json `focus`.
export function heroesOf(checked, area) {
  const focus = area.focus ?? [];
  const out = [];
  for (const e of checked) {
    if (!e.bmin || !e.name) continue;
    const isFocus = focus.includes(e.name);
    if (!isFocus && !(e.actions && e.actions.length)) continue;
    const w = importance(e) * (isFocus ? 4 : 1);
    if (w < 0.3) continue;
    // A node with no mesh of its own measures as a point: give it a person-sized box so it can be framed and hit.
    const min = [...e.bmin];
    const max = [...e.bmax];
    for (let i = 0; i < 3; i++) {
      const need = i === 1 ? 1.5 : 1.2;
      if (max[i] - min[i] < need) {
        if (i === 1) max[i] = min[i] + need;
        else {
          const c = (min[i] + max[i]) / 2;
          min[i] = c - need / 2;
          max[i] = c + need / 2;
        }
      }
    }
    out.push({ name: e.name, min, max, w, focus: isFocus });
  }
  // Only the area's own: a neighbour inside the radius (the fuel station seen from the hospital) is not what this wide is for.
  const focusHeroes = out.filter((h) => h.focus);
  const aim = aimOf(area, focusHeroes);
  const reach = focusHeroes.length ? Math.max(12, aim.frame * 1.6) : area.radius * 0.65;
  const own = out.filter((h) => h.focus || Math.hypot((h.min[0] + h.max[0]) / 2 - aim.x, (h.min[2] + h.max[2]) / 2 - aim.z) <= reach);
  own.sort((a, b) => b.w - a.w);
  return own.slice(0, HERO_MAX);
}

// Where a wide looks and how far it should stand: the centre of the focus entities (else the area centre).
export function aimOf(area, heroes) {
  const f = heroes.filter((h) => h.focus);
  if (!f.length) return { x: area.x, z: area.z, frame: area.radius };
  const cx = (h) => (h.min[0] + h.max[0]) / 2;
  const cz = (h) => (h.min[2] + h.max[2]) / 2;
  const x = f.reduce((s, h) => s + cx(h), 0) / f.length;
  const z = f.reduce((s, h) => s + cz(h), 0) / f.length;
  const ext = Math.max(...f.map((h) => Math.hypot(cx(h) - x, cz(h) - z) + Math.hypot(h.max[0] - h.min[0], h.max[2] - h.min[2]) / 2));
  return { x, z, frame: Math.max(7, ext * 1.6) };
}

// Level camera at `eye` facing yaw (degrees). Box corners -> normalised screen rectangle and nearest depth.
export function boxScreen(eye, yawDeg, min, max) {
  const sy = Math.sin((yawDeg * Math.PI) / 180);
  const cy = Math.cos((yawDeg * Math.PI) / 180);
  let x0 = 9;
  let x1 = -9;
  let y0 = 9;
  let y1 = -9;
  let zmin = 1e9;
  for (let i = 0; i < 8; i++) {
    const p = [i & 1 ? max[0] : min[0], i & 2 ? max[1] : min[1], i & 4 ? max[2] : min[2]];
    const v = sub(p, eye);
    const z = v[0] * sy + v[2] * cy;
    if (z < 0.3) return null;
    const x = (v[0] * cy - v[2] * sy) / z / FOV_H_TAN;
    const y = v[1] / z / FOV_V_TAN;
    x0 = Math.min(x0, x);
    x1 = Math.max(x1, x);
    y0 = Math.min(y0, y);
    y1 = Math.max(y1, y);
    zmin = Math.min(zmin, z);
  }
  const w = Math.max(1e-6, x1 - x0);
  const h = Math.max(1e-6, y1 - y0);
  const ox = Math.max(0, Math.min(1, x1) - Math.max(-1, x0));
  const oy = Math.max(0, Math.min(1, y1) - Math.max(-1, y0));
  return { x0, x1, y0, y1, z: zmin, inFrac: (ox * oy) / (w * h), width: w / 2 };
}

const sizeFactor = (s) => Math.max(0.1, Math.min(1, s.width / HERO_SIZE_FULL));

// Pure stage: the share of hero weight that is in frame and big enough to read (no rays). Ranks candidates cheaply.
export function heroFrame(eye, yawDeg, heroes) {
  let total = 0;
  let got = 0;
  for (const hero of heroes) {
    total += hero.w;
    const s = boxScreen(eye, yawDeg, hero.min, hero.max);
    if (s) got += hero.w * s.inFrac * sizeFactor(s);
  }
  return total ? got / total : 0;
}

// Distance along a ray (origin o, unit dir d) at which it enters the box, or null.
export function rayBoxEntry(o, d, min, max) {
  let t0 = 0;
  let t1 = 1e9;
  for (let i = 0; i < 3; i++) {
    if (Math.abs(d[i]) < 1e-9) {
      if (o[i] < min[i] || o[i] > max[i]) return null;
      continue;
    }
    let a = (min[i] - o[i]) / d[i];
    let b = (max[i] - o[i]) / d[i];
    if (a > b) [a, b] = [b, a];
    t0 = Math.max(t0, a);
    t1 = Math.min(t1, b);
    if (t0 > t1) return null;
  }
  return t0;
}

const inBox = (p, hero, pad) => p[0] >= hero.min[0] - pad && p[0] <= hero.max[0] + pad && p[1] >= hero.min[1] - pad && p[1] <= hero.max[1] + pad && p[2] >= hero.min[2] - pad && p[2] <= hero.max[2] + pad;

// Pure: combine the measurements into one score; the breakdown says why.
export function scoreHero(m) {
  const score = 5 * m.heroVis + 1.5 * m.backdrop - 7 * m.foreground - 5 * m.bigNear - 8 * m.treeNear - 8 * m.crowd - 8 * (m.dominance || 0) + m.bonus;
  const out = { score: r2(score) };
  for (const [k, v] of Object.entries(m)) out[k] = typeof v === "number" ? r2(v) : v;
  return out;
}

// Trace the frame: hero visibility (3 sample points each), a 10x5 grid of foreground rays, and short crowding probes.
function scoreWideHero(probe, area, placed, heroes, trees, aim) {
  const { eye, target } = placed;
  const { yaw } = poseOf(eye, target);
  const depth = Math.max(4, Math.hypot(aim.x - eye[0], aim.z - eye[2]));
  const rad = Math.PI / 180;
  const f = [Math.sin(yaw * rad), 0, Math.cos(yaw * rad)];
  const r = [Math.cos(yaw * rad), 0, -Math.sin(yaw * rad)];
  const u = [0, 1, 0];
  const blockers = new Map();
  const bump = (k, w) => blockers.set(k, (blockers.get(k) ?? 0) + w);
  // 1. hero visibility
  let total = 0;
  let got = 0;
  let dom = 0;
  for (const hero of heroes) {
    total += hero.w;
    const s = boxScreen(eye, yaw, hero.min, hero.max);
    if (!s) continue;
    // A hero that fills the frame (the van you are standing next to) is as bad as one you cannot see.
    const share = (Math.max(0, Math.min(1, s.x1) - Math.max(-1, s.x0)) * Math.max(0, Math.min(1, s.y1) - Math.max(-1, s.y0))) / 4;
    if (share > 0.15) dom += (share - 0.15) * hero.w;
    if (s.inFrac < 0.3) continue;
    const c = [(hero.min[0] + hero.max[0]) / 2, (hero.min[1] + hero.max[1]) / 2, (hero.min[2] + hero.max[2]) / 2];
    const hw = (hero.max[0] - hero.min[0]) / 2;
    const side = norm([-(c[2] - eye[2]), 0, c[0] - eye[0]]);
    const pts = [c, add(c, mul(side, Math.min(hw, 0.6)))];
    let seen = 0;
    for (const p of pts) {
      const dir = norm(sub(p, eye));
      const entry = rayBoxEntry(eye, dir, hero.min, hero.max);
      const tr = probe.trace(eye, p, true);
      if (tr.d >= tr.total - 0.05 || (entry !== null && tr.d >= entry - 0.4)) seen++;
      else bump(tr.ent, hero.w);
    }
    got += hero.w * (seen / pts.length) * s.inFrac * sizeFactor(s);
  }
  // 2. foreground: what is hit in the near part of the view that is not a hero
  let cover = 0;
  let fg = 0;
  let big = 0;
  const nx = 8;
  const ny = 4;
  for (let i = 0; i < nx; i++) {
    for (let j = 0; j < ny; j++) {
      const a = ((i - (nx - 1) / 2) / ((nx - 1) / 2)) * 0.92 * FOV_H_TAN;
      const b = ((j - (ny - 1) / 2) / ((ny - 1) / 2)) * 0.92 * FOV_V_TAN;
      const dir = norm(add(add(f, mul(r, a)), mul(u, b)));
      const tr = probe.trace(eye, add(eye, mul(dir, depth * 2)), true);
      if (!(tr.d < tr.total - 0.05)) continue;
      const p = add(eye, mul(dir, tr.d));
      const h = p[1] - probe.ground(p[0], p[2]);
      if (h < 0.3) continue;
      if (Math.hypot(p[0] - aim.x, p[2] - aim.z) <= aim.frame * 1.25 && tr.d >= depth * 0.4) cover++;
      if (heroes.some((x) => inBox(p, x, 0.8))) continue;
      if (tr.d < depth * FG_DEPTH && h > 0.8) {
        fg++;
        bump(tr.ent, 0.2);
        if (h > 2.5) big++;
      }
    }
  }
  // 3. thin poles right in front of the lens
  let crowded = 0;
  for (const yo of [-0.6, 0, 0.6]) {
    for (const po of [-0.4, 0, 0.4]) {
      const dir = norm(add(add(f, mul(r, yo)), mul(u, po)));
      const tr = probe.trace(eye, add(eye, mul(dir, 5)), true);
      if (tr.d < tr.total - 0.05) crowded++;
    }
  }
  const n = nx * ny;
  const off = Math.abs((((placed.cand.az - (area.front ?? 180)) % 360) + 540) % 360 - 180);
  const bonus = (placed.cand.up <= 3 ? 0.6 : 0) + (off >= 20 && off <= 60 ? 0.4 : 0);
  const m = scoreHero({
    heroVis: total ? got / total : 0,
    dominance: total ? dom / total : 0,
    backdrop: cover / n,
    foreground: fg / n,
    bigNear: big / n,
    treeNear: treeOcclusion(eye, [target[0], eye[1], target[2]], trees, depth),
    crowd: crowded / 9,
    bonus,
  });
  const top = [...blockers.entries()].sort((a, b) => b[1] - a[1]).slice(0, 3).map(([k]) => k);
  if (top.length) m.blockedBy = top.join(",");
  return m;
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
  const focusNames = area.focus ?? [];
  for (const e of o.args.flags["wide-only"] ? ents.filter((x) => pads.includes(x) || focusNames.includes(x.name)) : ents) {
    let m;
    try {
      m = probe.ent(e);
    } catch (err) {
      m = { missing: true };
    }
    checked.push({ name: e.name, layer: e.layer, x: e.x, y: e.y, z: e.z, yaw: e.yaw, actions: e.actions, dist: r2(e.dist), ...m, problems: problemsFor(e, m, area) });
  }
  const heroes = heroesOf(checked, area);
  const aim = aimOf(area, heroes);
  // Close-ups: the interactables that matter most (actions and size), not the ones nearest the centre.
  for (const p of pads) {
    const own = checked.find((x) => x.name === p.name);
    p.importance = own ? importance(own) : 0;
  }
  const closeSet = pads.slice().sort((x, y) => y.importance - x.importance).slice(0, MAX_CLOSEUPS);

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
  let widePool = [];
  try {
  const lookAt = (h1) => [area.x, area.y + h1, area.z];
  // Wide: rank every front-side candidate by what a player needs to see (heroes in frame, unhidden, big enough,
  // nothing tall in front), ray-score the best six, shoot the best two (wide = best score).
  const noClose = o.args.flags["wide-only"] || o.args.flags["no-close"];
  console.log(`closeups: ${closeSet.map((p) => `${p.name}(${p.importance})`).join(" ")}`);
  const wides = [];
  const trees = placedMod
    .loadWorld(ROOT)
    .filter((e) => TREE.test(e.prefab || e.anonPrefab || "") && Math.hypot(e.x - aim.x, e.z - aim.z) < Math.max(area.radius, aim.frame) * 4)
    .map((e) => ({ x: e.x, z: e.z, gy: probe.ground(e.x, e.z) }));
  placeCamera(probe, "wide", area, { collect: wides, aim }, () => [aim.x, area.y + 1, aim.z]);
  for (const w of wides) {
    const yaw = poseOf(w.eye, w.target).yaw;
    w.treeNear = treeOcclusion(w.eye, [w.target[0], w.eye[1], w.target[2]], trees, len(sub(w.target, w.eye)));
    w.pre = heroFrame(w.eye, yaw, heroes) * 5 - 8 * w.treeNear + (w.cand.up <= 3 ? 0.6 : 0) + (w.score >= 2 ? 0 : -0.5);
  }
  wides.sort((x, y) => y.pre - x.pre);
  const unscored = wides.splice(6);
  const tw = Date.now();
  for (const w of wides) {
    w.frame = scoreWideHero(probe, area, w, heroes, trees, aim);
    w.frame.score = r2(w.frame.score + (w.score >= 2 ? 0 : -0.5));
  }
  wides.sort((x, y) => y.frame.score - x.frame.score);
  widePool = wides.map((w) => ({ az: r2(w.cand.az), dist: r2(w.cand.dist), up: w.cand.up, pre: r2(w.pre), ...w.frame, viewBlocked: w.score < 2 || undefined })).concat(unscored.slice(0, 20).map((w) => ({ az: r2(w.cand.az), dist: r2(w.cand.dist), up: w.cand.up, pre: r2(w.pre), skipped: true })));
  const bw = wides[0];
  if (bw) console.log(`wide: ${wides.length} scored of ${wides.length + unscored.length} in ${Math.round((Date.now() - tw) / 1000)}s; best az ${r2(bw.cand.az)} dist ${r2(bw.cand.dist)} up ${bw.cand.up}: ${JSON.stringify(bw.frame)}; heroes ${heroes.length}`);
  wides.slice(0, 2).forEach((w, i) => {
    const sh = take(i === 0 ? "wide" : "wide-2", "wide", "wide", null, w);
    sh.frame = w.frame;
  });
  if (!wides.length) take("wide", "wide", "wide", null, placeCamera(probe, "wide", area, { aim }, () => [aim.x, area.y + 1, aim.z]));
  // The character camera cannot pitch, so a top-down needs the (crashing) free camera: opt in with --free-camera.
  if (o.args.flags["free-camera"]) take("top", "top", "top", null, placeCamera(probe, "top", area, {}, () => lookAt(0)));
  if (!noClose) for (const [dirName, az] of [["N", 0], ["E", 90], ["S", 180], ["W", 270]]) {
    // Standing at the radius edge on that side, looking in: the camera sits on the side named.
    take(dirName, "eye", dirName, null, placeCamera(probe, "eye", area, { az }, (cand, stand) => [centre[0], stand[1] + EYE, centre[2]]));
  }
  for (const p of noClose ? [] : closeSet) {
    const floorY = p.y < 1.5 ? p.y : undefined;
    const h1 = p.y > 1.5 ? 0 : 1;
    const placed = placeCamera(probe, "close", area, { x: p.x, z: p.z, yaw: p.yaw, floorY, name: p.name }, () => [p.x, p.y + h1, p.z]);
    const cs = take(`close-${p.name}`, "close", p.name, p.name, placed);
    cs.frame = { score: r2(p.importance * (placed.score >= 2 ? 1 : 0.4)), importance: p.importance, view: placed.score, dist: r2(Math.hypot(p.x - area.x, p.z - area.z)) };
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
    area: { name: area.name, n: area.n, centre, radius: area.radius, front: area.front ?? 180 },
    hour: o.hour,
    clock,
    fovAssumed: o.fov,
    shots,
    wideCandidates: widePool,
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

// The website pair: the best-scoring wide, and the best-scoring close-up (or --detail <Entity>).
// Ties on the close-up go to the interactable nearest the centre (the list is already in that order).
export function bestBy(list) {
  let best = null;
  for (const s of list) if (!best || (s.frame?.score ?? -9) > (best.frame?.score ?? -9)) best = s;
  return best;
}

function pick(area, outDir, shots, pads, detail, checked) {
  const closes = shots.filter((s) => s.kind === "close" && s.status === "ok");
  let want = bestBy(closes);
  if (typeof detail === "string") want = closes.find((s) => s.entity === detail);
  const wide = bestBy(shots.filter((s) => s.kind === "wide" && s.status === "ok"));
  if (!wide || !want) return `pick: nothing copied (${!wide ? "no wide shot" : `no close-up for ${detail ?? "any interactable"}`})`;
  const dir = join(ROOT, "website", "public", "docs-images", `area-${area.n || 0}-${area.slug}`);
  mkdirSync(dir, { recursive: true });
  copyFileSync(join(outDir, wide.file), join(dir, "wide.png"));
  copyFileSync(join(outDir, want.file), join(dir, "detail.png"));
  return `pick: ${wide.id} (score ${wide.frame?.score}) + ${want.entity} (score ${want.frame?.score}) -> ${relative(ROOT, dir)} (override with --detail <Entity>)`;
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
  const fa = { x: 0, z: 0, radius: 10, front: 180 };
  const faceM = parseEnt("bmin=0,0,0 bmax=1,1,1 gap=0 tilt=0 walk=0.5 clip=none");
  if (!problemsFor({ name: "Phone", x: 0, z: 0, y: 0, yaw: 0, actions: ["X"] }, faceM, fa).join().includes("faces away")) fails.push("faces-away misses a back to the approach");
  if (problemsFor({ name: "Phone", x: 0, z: 0, y: 0, yaw: 180, actions: ["X"] }, faceM, fa).join().includes("faces away")) fails.push("faces-away flags a correct facing");
  if (problemsFor({ name: "Pick", x: 0, z: 0, y: 0, yaw: 0, actions: ["SCR_PickUpItemAction"] }, faceM, fa).join().includes("faces away")) fails.push("faces-away flags a pickup");
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
  const wideC = candidates("wide", { x: 0, y: 0, z: 0, radius: 10, front: 270 }).list;
  if (wideC.some((c) => Math.abs(c.az - 270) > 65) || !wideC.some((c) => c.up === 0)) fails.push("wide candidates start at the front heading, eye level");
  // Hero scoring: a hero in front of the camera is seen; behind it is not; a near, big blocker costs.
  const hero = { name: "H", min: [-1, 0, 7], max: [1, 2, 9], w: 3 };
  if (!(heroFrame([0, 1.6, 0], 0, [hero]) > 0.8) || heroFrame([0, 1.6, 0], 180, [hero]) !== 0) fails.push("heroFrame in front/behind");
  if (!(heroFrame([0, 1.6, 0], 0, [hero]) > heroFrame([0, 1.6, -60], 0, [hero]))) fails.push("heroFrame prefers closer");
  const good = scoreHero({ heroVis: 0.9, backdrop: 0.3, foreground: 0, bigNear: 0, treeNear: 0, crowd: 0, bonus: 0 });
  const wall = scoreHero({ heroVis: 0.9, backdrop: 0.3, foreground: 0.33, bigNear: 0.33, treeNear: 0, crowd: 0, bonus: 0 });
  if (!(good.score > wall.score + 3)) fails.push(`scoreHero wall ${good.score} ${wall.score}`);
  if (rayBoxEntry([0, 0, 0], [0, 0, 1], [-1, -1, 5], [1, 1, 7]) !== 5 || rayBoxEntry([5, 0, 0], [0, 0, 1], [-1, -1, 5], [1, 1, 7]) !== null) fails.push("rayBoxEntry");
  const imp = (name, n, v) => importance({ name, actions: new Array(n).fill("a"), bmin: [0, 0, 0], bmax: [v, 1, 1] });
  if (!(imp("Roadmap_Vehicle", 3, 5) > imp("TomatoNpc", 1, 1) * 3) || !(imp("Roadmap_Vehicle", 3, 5) > imp("Sign_Atm", 1, 1) * 3)) fails.push("importance ranks a vehicle over an NPC or a sign");
  const eyeP = [0, 10, 0];
  if (treeOcclusion(eyeP, [0, 1, 30], [{ x: 0, z: 4, gy: 0 }], 30) < 0.5 || treeOcclusion(eyeP, [0, 1, 30], [{ x: 0, z: 40, gy: 0 }], 30) !== 0) fails.push("treeOcclusion");
  // Problems: a pivot above the floor with mesh below it is not sunk; a buried prop is.
  if (problemsFor({ name: "Jeep", y: 0 }, parseEnt("bmin=0,0,0 bmax=2,2,4 gap=-0.4 pgap=0 tilt=0 walk=0.5 clip=none")).join().includes("sunk")) fails.push("sunk ignores bounds below a grounded pivot");
  if (!problemsFor({ name: "Crate", y: 0 }, parseEnt("bmin=0,0,0 bmax=1,1,1 gap=-0.4 pgap=-0.4 tilt=0 walk=0.5 clip=none")).join().includes("sunk")) fails.push("sunk catches a buried prop");
  if (bestBy([{ id: "a", frame: { score: 1 } }, { id: "b", frame: { score: 3 } }]).id !== "b") fails.push("bestBy");
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
