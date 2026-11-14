#!/usr/bin/env node
// tools/visual/look.mjs - pictures of a DebugWorld area with no game session.
//
//   tools\cli look <area | x,z> [more areas]     shots, contact.jpg and index.json per area in tmp/look/<area>/
//   tools\cli look all                           every area of agent-box/areas.json in one Workbench run, plus overview.jpg
//   tools\cli look selftest                      the Node checks (plan, status, PNG stats, flags); no Workbench
//   flags: --edit (the World Editor view instead of play mode)  --close N (close-ups per area, default 12; 0 = none)
//          --radius m  --hour h (local solar time, default 14)  --settle ms  --out dir  --measure (peak RAM)
//          --keep (leave Workbench's raw output)  --force (skip the RAM gate)  --wait (queue behind a Workbench build)
//
// One hidden Workbench (LF-Agents desktop) opens DebugWorld, enters play mode with a free
// camera, takes every shot and exits: about 10 s of start-up and 0.5 s per shot, no server,
// no client, no session lane. tools/visual/wb/LF_VisualShotsPlugin.c does the engine side;
// docs/visual-testing.md says what this does and does not prove.

import { cpSync, existsSync, mkdirSync, readFileSync, readdirSync, renameSync, rmSync, writeFileSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { freemem } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { deflateSync, inflateSync } from "node:zlib";
import { quietSpawn } from "../lib/quiet-launch.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const PLUGIN = join(ROOT, "tools", "visual", "wb", "LF_VisualShotsPlugin.c");
const WORLD = "Worlds/DebugWorld/DebugWorld.ent";
const WORKBENCH_MB = 1900; // peak working set of the run, measured 2026-11-11 (docs/visual-testing.md)
const RAM_MARGIN_MB = 300;
const BOOT_BUDGET_S = 90;

export const DEFAULTS = { hour: 14, settle: 350, first: 3000, close: 12 };

// ------------------------------------------------------------------ args

export function parseArgs(argv) {
  const flags = {};
  const pos = [];
  const valued = new Set(["close", "radius", "hour", "settle", "first", "out", "states", "at"]);
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (!a.startsWith("--")) pos.push(a);
    else if (valued.has(a.slice(2))) flags[a.slice(2)] = argv[++i];
    else flags[a.slice(2)] = true;
  }
  return { pos, flags };
}

const num = (v, d) => (v === undefined || Number.isNaN(Number(v)) ? d : Number(v));
const r1 = (v) => Math.round(v * 10) / 10;
const safe = (s) => String(s).replace(/[^A-Za-z0-9_-]/g, "_");

// "hospital" -> the areas.json entry; "262,255" -> an ad-hoc area.
export function resolveArea(arg, areas, radius) {
  const hit = areas.find((a) => a.name === arg);
  if (hit) return { ...hit, radius: radius ?? hit.radius };
  const m = /^(-?\d+(?:\.\d+)?)[, ](-?\d+(?:\.\d+)?)$/.exec(String(arg));
  if (!m) throw new Error(`unknown area "${arg}". Known: ${[...new Set(areas.map((a) => a.name))].join(", ")}; or pass x,z`);
  const x = Number(m[1]);
  const z = Number(m[2]);
  return { name: `xz-${Math.round(x)}-${Math.round(z)}`, x, y: 0, z, radius: radius ?? 20, front: 180 };
}

// ------------------------------------------------------------------ plan

const SKIP_NAMES = /^(Points|RoadEntity|SplinePoints|GodRays|HBAO|HDR|PPAA|SSDO|UnderWater)$/;
const SKIP_LAYERS = /^(Village_Roads|default|Roadmap|Tests|Gamemode)\.layer$/;
// Too big or too many for a close-up; the wide, the obliques and the top view show them.
const SKIP_PREFABS = /\/(Vegetation|Roads|Rocks|Walls|Houses|Military|Industrial|Commercial|Agriculture|Infrastructure\/Roads)\//i;

const prefabOf = (e) => e.prefab || e.anonPrefab || "";
const baseOf = (e) => prefabOf(e).split("/").pop().replace(/\.et$/, "");

// The entities of an area worth a close-up: interactables first, then named props, then the rest, nearest first.
export function closeSet(world, area, limit) {
  const near = world
    .filter((e) => prefabOf(e) && !SKIP_NAMES.test(e.name) && !SKIP_LAYERS.test(e.layer) && !SKIP_PREFABS.test(prefabOf(e)))
    .map((e) => ({ ...e, dist: Math.hypot(e.x - area.x, e.z - area.z) }))
    .filter((e) => e.dist <= area.radius);
  const rank = (e) => (e.actions?.length ? 0 : e.name ? 1 : 2);
  near.sort((a, b) => rank(a) - rank(b) || a.dist - b.dist);
  const out = [];
  for (const e of near) {
    if (out.length >= limit) break;
    if (out.some((o) => Math.hypot(o.x - e.x, o.z - e.z) < 0.6 && Math.abs(o.y - e.y) < 0.6)) continue;
    out.push(e);
  }
  return out;
}

const cand = (az, dist, up) => `${r1(az)}:${r1(dist)}:${r1(up)}`;

// The shots of one area: [{ name, kind, label, line, entity? }]. `line` is the plugin directive.
export function shotsFor(area, world, opts = {}) {
  const slug = safe(area.name);
  const R = area.radius;
  const front = area.front ?? 180;
  const out = [];
  const add = (kind, label, line, extra = {}) => out.push({ name: `${slug}__${kind}`, area: area.name, kind, label, line: line.replace("NAME", `${slug}__${kind}`), ...extra });

  // Wide: from the side players approach, eye level first, then higher and to the sides.
  const wide = [];
  if (area.wideCands) for (const w of area.wideCands) wide.push(cand(w.az, w.dist, w.up));
  for (const up of [2.2, 5, 9]) for (const off of [0, -25, 25, -50, 50]) for (const k of [1, 1.4, 0.7]) wide.push(cand(front + off, Math.max(6, R * k), up));
  add("wide", "wide", `shot NAME ${area.x} - ${area.z} 1.2 0.85 ${wide.join(",")}`);

  // Obliques from the other three sides, steeper when something stands in the way.
  for (const off of [90, 180, 270]) {
    const az = (front + off) % 360;
    const list = [cand(az, R, R * 0.6), cand(az, R * 0.8, R * 0.95), cand(az, R * 0.55, R * 1.3)];
    add(`ob${String(az).padStart(3, "0")}`, `from ${az} deg`, `shot NAME ${area.x} - ${area.z} 1 0.7 ${list.join(",")}`);
  }

  add("top", "top (north up)", `shot NAME ${area.x} - ${area.z} 0 0 ${cand(180, 0.3, Math.max(30, R * 2.2))}`);

  const closes = closeSet(world, area, opts.close ?? DEFAULTS.close);
  closes.forEach((e, i) => {
    // The camera starts on the side that faces the area centre (the front side for something at the centre).
    const az = e.dist > 2 ? (Math.atan2(area.x - e.x, area.z - e.z) * 180) / Math.PI : front;
    const label = e.name || baseOf(e);
    add(`c${String(i + 1).padStart(2, "0")}_${safe(label).slice(0, 40)}`, label, `close NAME ${e.name || "-"} ${e.x} ${e.y} ${e.z} ${r1(az)}`, { entity: { name: e.name, prefab: prefabOf(e), layer: e.layer, x: e.x, y: e.y, z: e.z, scale: e.scale, actions: e.actions?.length ?? 0 } });
  });
  return out;
}

// plan.txt for the plugin. The park spot is where the play-mode body spawns: far from every area.
export function planText(shots, areas, opts = {}) {
  const minX = Math.min(...areas.map((a) => a.x - a.radius));
  const minZ = Math.min(...areas.map((a) => a.z - a.radius));
  const lines = [
    `world ${WORLD}`,
    `mode ${opts.edit ? "edit" : "play"}`,
    `hour ${opts.hour ?? DEFAULTS.hour}`,
    `settle ${opts.settle ?? DEFAULTS.settle}`,
    `first ${opts.first ?? DEFAULTS.first}`,
    `park ${r1(minX - 150)} ${r1(minZ - 150)}`,
    ...shots.map((s) => s.line),
  ];
  return lines.join("\n") + "\n";
}

// ------------------------------------------------------------------ status

// status.txt -> { world, done, errors: [..], shots: { name: { file, ms, clear, pick, found, size:[x,y,z], cam:[x,y,z] } } }
export function parseStatus(text) {
  const out = { world: null, play: false, done: null, errors: [], warnings: [], shots: {}, readyMs: null };
  for (const raw of String(text).split(/\r?\n/)) {
    const m = /^(\d+) (ok|error|warn|done) ?(.*)$/.exec(raw.trim());
    if (!m) continue;
    const ms = Number(m[1]);
    const rest = m[3];
    if (m[2] === "done") {
      const [shots, failed] = rest.split(" ").map(Number);
      out.done = { ms, shots, failed };
    } else if (m[2] === "error") out.errors.push(rest);
    else if (m[2] === "warn") out.warnings.push(rest);
    else if (rest.startsWith("world ")) {
      out.world = rest.split(" ")[1];
      out.readyMs = ms;
    } else if (rest.startsWith("play ")) {
      out.play = true;
      out.readyMs = ms;
    } else if (rest.startsWith("shot ")) {
      const parts = rest.split(" ");
      const kv = Object.fromEntries(parts.slice(3).map((p) => p.split("=")).filter((p) => p.length === 2));
      const vec = (s) => (s ? s.split(",").map(Number) : null);
      out.shots[parts[1]] = { file: parts[2], ms, clear: kv.clear === undefined ? null : Number(kv.clear), pick: kv.pick === undefined ? null : Number(kv.pick), found: kv.found === undefined ? null : kv.found === "1", size: vec(kv.size), cam: vec(kv.cam) };
    }
  }
  return out;
}

// ------------------------------------------------------------------ PNG (8-bit RGB / RGBA, not interlaced)

export function decodePng(buf) {
  if (buf.length < 33 || buf.readUInt32BE(0) !== 0x89504e47) throw new Error("not a PNG");
  let off = 8;
  let w = 0;
  let h = 0;
  let channels = 0;
  const idat = [];
  while (off + 8 <= buf.length) {
    const len = buf.readUInt32BE(off);
    const type = buf.toString("latin1", off + 4, off + 8);
    const data = buf.subarray(off + 8, off + 8 + len);
    if (type === "IHDR") {
      w = data.readUInt32BE(0);
      h = data.readUInt32BE(4);
      const colour = data[9];
      if (data[8] !== 8 || (colour !== 2 && colour !== 6) || data[12] !== 0) throw new Error(`unsupported PNG (depth ${data[8]}, colour ${colour}, interlace ${data[12]})`);
      channels = colour === 2 ? 3 : 4;
    } else if (type === "IDAT") idat.push(data);
    else if (type === "IEND") break;
    off += 12 + len;
  }
  const raw = inflateSync(Buffer.concat(idat));
  const stride = w * channels;
  const px = Buffer.alloc(stride * h);
  for (let y = 0; y < h; y++) {
    const filter = raw[y * (stride + 1)];
    const src = y * (stride + 1) + 1;
    const dst = y * stride;
    for (let i = 0; i < stride; i++) {
      const a = i >= channels ? px[dst + i - channels] : 0;
      const b = y > 0 ? px[dst + i - stride] : 0;
      const c = y > 0 && i >= channels ? px[dst + i - stride - channels] : 0;
      let v = raw[src + i];
      if (filter === 1) v += a;
      else if (filter === 2) v += b;
      else if (filter === 3) v += (a + b) >> 1;
      else if (filter === 4) {
        const p = a + b - c;
        const pa = Math.abs(p - a);
        const pb = Math.abs(p - b);
        const pc = Math.abs(p - c);
        v += pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
      }
      px[dst + i] = v & 255;
    }
  }
  return { w, h, channels, px };
}

function crc32(buf) {
  let c = ~0;
  for (const byte of buf) {
    c ^= byte;
    for (let k = 0; k < 8; k++) c = (c >>> 1) ^ (0xedb88320 & -(c & 1));
  }
  return ~c >>> 0;
}

// RGB PNG from a pixel function (the selftest's fixtures).
export function encodePng(w, h, at) {
  const raw = Buffer.alloc((w * 3 + 1) * h);
  for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) raw.set(at(x, y), y * (w * 3 + 1) + 1 + x * 3);
  const chunk = (type, data) => {
    const body = Buffer.concat([Buffer.from(type, "latin1"), data]);
    const len = Buffer.alloc(4);
    len.writeUInt32BE(data.length);
    const crc = Buffer.alloc(4);
    crc.writeUInt32BE(crc32(body));
    return Buffer.concat([len, body, crc]);
  };
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0);
  ihdr.writeUInt32BE(h, 4);
  ihdr.set([8, 2, 0, 0, 0], 8);
  return Buffer.concat([Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]), chunk("IHDR", ihdr), chunk("IDAT", deflateSync(raw)), chunk("IEND", Buffer.alloc(0))]);
}

// Coarse numbers over a 96x54 grid of the middle 70% of the frame: mean luma, its spread, the share of
// near-white pixels with no colour, and the share of pure magenta (the engine's missing-texture colour).
export function pixelStats(img) {
  const gx = 96;
  const gy = 54;
  let n = 0;
  let sum = 0;
  let sq = 0;
  let white = 0;
  let magenta = 0;
  for (let j = 0; j < gy; j++) {
    for (let i = 0; i < gx; i++) {
      const x = Math.min(img.w - 1, Math.floor(img.w * (0.15 + (0.7 * (i + 0.5)) / gx)));
      const y = Math.min(img.h - 1, Math.floor(img.h * (0.15 + (0.7 * (j + 0.5)) / gy)));
      const o = (y * img.w + x) * img.channels;
      const r = img.px[o];
      const g = img.px[o + 1];
      const b = img.px[o + 2];
      const l = 0.299 * r + 0.587 * g + 0.114 * b;
      sum += l;
      sq += l * l;
      n++;
      if (Math.min(r, g, b) >= 236 && Math.max(r, g, b) - Math.min(r, g, b) <= 10) white++;
      if (r >= 230 && b >= 230 && g <= 40) magenta++;
    }
  }
  const mean = sum / n;
  return { mean: r1(mean), sd: r1(Math.sqrt(Math.max(0, sq / n - mean * mean))), white: Math.round((white / n) * 100) / 100, magenta: Math.round((magenta / n) * 100) / 100 };
}

// ------------------------------------------------------------------ flags

export const LIMITS = { dark: 30, blown: 222, flat: 7, white: 0.3, magenta: 0.03, blockedArea: 0.2, blockedClose: 0.12 };

// Why a tile deserves a first look. Hints, not verdicts: a white sign trips `white`, a night scene trips `dark`.
export function flagsFor(shot, st, px) {
  const flags = [];
  if (!st) return ["missing"];
  if (shot.kind.startsWith("c") && shot.entity && st.found === false) flags.push("not-found");
  const limit = shot.entity ? LIMITS.blockedClose : LIMITS.blockedArea;
  if (shot.kind !== "top" && st.clear !== null && st.clear < limit) flags.push("blocked");
  if (px) {
    if (px.sd < LIMITS.flat) flags.push("flat");
    else if (px.mean < LIMITS.dark) flags.push("dark");
    else if (px.mean > LIMITS.blown) flags.push("blown");
    if (px.magenta > LIMITS.magenta) flags.push("magenta");
    if (shot.entity && px.white > LIMITS.white) flags.push("white");
  }
  return flags;
}

export function sizeText(size) {
  if (!size) return "";
  return size.map((v) => (v >= 10 ? Math.round(v) : Math.round(v * 10) / 10)).join("x") + "m";
}

export function tileLabel(shot, st, flags) {
  const bits = [shot.label];
  if (shot.entity && st?.size) bits.push(sizeText(st.size));
  if (shot.entity?.scale !== undefined && shot.entity.scale !== 1) bits.push(`scale ${shot.entity.scale}`);
  if (flags.length) bits.push(`[${flags.join(" ").toUpperCase()}]`);
  return bits.join("  ");
}

export function sheetLayout(count) {
  const cols = count <= 6 ? 3 : 4;
  return { cols, rows: Math.ceil(count / cols) };
}

// ------------------------------------------------------------------ run

function steamCommon() {
  return join(process.env["ProgramFiles(x86)"] || "C:\\Program Files (x86)", "Steam", "steamapps", "common");
}

export function workbenchExe() {
  const tools = process.env.ENFUSION_WORKBENCH_PATH || join(steamCommon(), "Arma Reforger Tools");
  return [join(tools, "Workbench", "ArmaReforgerWorkbenchSteamDiag.exe"), join(tools, "ArmaReforgerWorkbenchSteamDiag.exe")].find(existsSync) ?? null;
}

function peakWorkingSetMb(marker) {
  const r = spawnSync("powershell", ["-NoProfile", "-Command", "Get-CimInstance Win32_Process -Filter \"Name like 'ArmaReforgerWorkbench%'\" | ForEach-Object { '{0}|{1}' -f $_.PeakWorkingSetSize, $_.CommandLine }"], { encoding: "utf8", windowsHide: true, timeout: 10000 });
  let peak = 0;
  for (const l of (r.stdout || "").split(/\r?\n/)) {
    const i = l.indexOf("|");
    if (i > 0 && l.includes(marker)) peak = Math.max(peak, Number(l.slice(0, i)) / 1024); // PeakWorkingSetSize is in KB
  }
  return Math.round(peak);
}

// Starts the hidden Workbench on the plan and resolves when it exits or the budget is spent.
export function runWorkbench(exe, profile, budgetS, measure, { plugin = "LF_VisualShotsPlugin" } = {}) {
  // With the pack in the checkout, open its project: it depends on the core, so both load and the pack's
  // string table shows (a sign board reads its own title, not the core's neutral one; #325).
  const packGproj = join(ROOT, "addons", "LF_Everon1989", "LF_Everon1989.gproj");
  const gproj = existsSync(packGproj) ? packGproj : join(ROOT, "addons", "LifeFramework", "LifeFramework.gproj");
  const game = process.env.ENFUSION_GAME_PATH || join(steamCommon(), "Arma Reforger");
  const addonDirs = [join(game, "addons")];
  if (existsSync(packGproj)) addonDirs.push(join(ROOT, "addons"));
  const args = ["-gproj", gproj, "-wbModule=WorldEditor", "-plugin=" + plugin, "-noSplash", "-forceUpdate", "-noSound", "-profile", profile, "-addonsDir", addonDirs.join(",")];
  return new Promise((done) => {
    const started = Date.now();
    let peakMb = 0;
    const child = quietSpawn(exe, args, { cwd: dirname(exe) });
    child.stdout?.resume();
    child.stderr?.resume();
    const sampler = measure ? setInterval(() => { peakMb = Math.max(peakMb, peakWorkingSetMb(profile)); }, 2500) : null;
    const finish = (code, timedOut) => {
      clearTimeout(killer);
      if (sampler) clearInterval(sampler);
      done({ code, timedOut, seconds: (Date.now() - started) / 1000, peakMb });
    };
    const killer = setTimeout(() => {
      child.kill(); // the helper's kill-on-close job ends Workbench
      finish(124, true);
    }, budgetS * 1000);
    child.on("exit", (code) => finish(code ?? 1, false));
    child.on("error", () => finish(1, false));
  });
}

export function latestLog(profile) {
  const logs = join(profile, "logs");
  if (!existsSync(logs)) return null;
  const dirs = readdirSync(logs).filter((d) => d.startsWith("logs_")).sort();
  return dirs.length ? join(logs, dirs[dirs.length - 1], "console.log") : null;
}

export function compileErrors(logFile) {
  if (!logFile || !existsSync(logFile)) return [];
  return readFileSync(logFile, "utf8").split(/\r?\n/).filter((l) => /SCRIPT\s+\(E\)|Can't compile/.test(l)).slice(0, 5);
}

async function run(args) {
  const wt = await import("../wt.mjs");
  if (wt.isMainCheckout(ROOT)) {
    console.log("REFUSE  'look' runs in the main checkout, which is the world-editor copy (it opens a second Workbench on the project).");
    console.log("        create a worktree first:  tools\\cli wt new <feature>");
    return 2;
  }
  const exe = workbenchExe();
  if (!exe) {
    console.log("FAIL  Workbench not found (set ENFUSION_WORKBENCH_PATH to the Arma Reforger Tools folder)");
    return 2;
  }

  const { loadWorld } = await import("../../agent-box/layers.mjs");
  const all = JSON.parse(readFileSync(join(ROOT, "agent-box", "areas.json"), "utf8")).areas;
  const radius = args.flags.radius === undefined ? undefined : num(args.flags.radius, undefined);
  let areas;
  let itemNotes = [];
  let itemPerArea = null;
  try {
    if (args.pos[0] === "item") {
      // look item <prefab> [prefab...] [--states ground,inventory] [--at area|x,z]: items are spawned, so play mode.
      const li = await import("./look-item.mjs");
      const at = resolveArea(args.flags.at || "spawn-square", all, radius);
      if (args.flags.edit) throw new Error("look item spawns the item, so it needs play mode (no --edit)");
      const plan = li.itemPlan(ROOT, args.pos.slice(1), { states: String(args.flags.states || li.ITEM_STATES.join(",")).split(",").filter(Boolean), at });
      if (!plan.areas.length) throw new Error("look item needs a prefab: {GUID}path, a path under addons/<addon>/, or an asset slug");
      areas = plan.areas;
      itemPerArea = plan.perArea;
      itemNotes = plan.notes;
    } else {
      areas = args.pos[0] === "all" ? all.map((a) => ({ ...a })) : args.pos.map((p) => resolveArea(p, all, radius));
    }
  } catch (e) {
    console.log(`FAIL  ${e.message}`);
    return 2;
  }
  const world = itemPerArea ? null : loadWorld(ROOT);
  const opts = { edit: args.flags.edit === true, close: num(args.flags.close, DEFAULTS.close), hour: num(args.flags.hour, DEFAULTS.hour), settle: num(args.flags.settle, DEFAULTS.settle), first: num(args.flags.first, DEFAULTS.first) };
  const perArea = itemPerArea ?? areas.map((a) => ({ area: a, shots: shotsFor(a, world, opts) }));
  for (const n of itemNotes) console.log(`look: ${n}`);
  const shots = perArea.flatMap((p) => p.shots);

  // RAM: Workbench peaks at about 1.9 GB for the 20 s it lives. It has to fit in free RAM with a margin; it is
  // not held against the pool's 1.5 GB floor the way a session's lasting reserve is. Idle sessions are trimmed first.
  if (!args.flags.force) {
    const need = WORKBENCH_MB + RAM_MARGIN_MB;
    const freeMb = () => Math.round(freemem() / 1048576);
    if (freeMb() < need) {
      const pool = await import("../remote/pool.mjs");
      pool.trimIfShort(ROOT, need);
    }
    if (freeMb() < need) {
      console.log(`FAIL  look: ${freeMb()} MB of RAM free, a Workbench needs about ${need} MB. Wait for whatever holds it (a session, an encode), or pass --force.`);
      return 3;
    }
  }

  // One Workbench at a time beside the headless build and the rdb refresh (shared shader cache).
  let locked = false;
  try {
    wt.acquireLock(ROOT, "build", { waitMs: 0 });
    locked = true;
  } catch {
    if (!args.flags.wait) {
      console.log("FAIL  look: a headless Workbench (build, rdb refresh or another look) holds the build lock; pass --wait to queue (up to 10 min)");
      return 3;
    }
    console.log("look: waiting for the build lock (another headless Workbench is running)");
    try {
      wt.acquireLock(ROOT, "build", { waitMs: 600000 });
      locked = true;
    } catch (e) {
      console.log(`FAIL  look: ${e.message}`);
      return 3;
    }
  }

  const profile = join(ROOT, "server", "profile", "visual");
  const raw = join(profile, "profile", "lf-visual");
  const injected = join(ROOT, "addons", "LifeFramework", "Scripts", "WorkbenchGame", "LFVisual");
  const outRoot = resolve(ROOT, args.flags.out || join("tmp", "look"));
  const started = Date.now();
  let res;
  try {
    mkdirSync(raw, { recursive: true });
    for (const f of readdirSync(raw)) rmSync(join(raw, f), { force: true });
    writeFileSync(join(raw, "plan.txt"), planText(shots, areas, opts));
    // Injected like the MCP handlers: Scripts/WorkbenchGame/ is git-ignored, and the copy is removed after the run.
    mkdirSync(injected, { recursive: true });
    cpSync(PLUGIN, join(injected, "LF_VisualShotsPlugin.c"));
    const budgetS = BOOT_BUDGET_S + Math.ceil(shots.length * ((opts.settle + 700) / 1000)) + Math.ceil(opts.first / 1000);
    console.log(`look: ${shots.length} shot(s) of ${areas.length} area(s), ${opts.edit ? "edit" : "play"} mode, hidden Workbench (budget ${budgetS} s)`);
    res = await runWorkbench(exe, profile, budgetS, args.flags.measure === true);
  } finally {
    rmSync(join(injected, "LF_VisualShotsPlugin.c"), { force: true });
    try {
      if (existsSync(injected) && readdirSync(injected).length === 0) rmSync(injected, { recursive: false });
    } catch {}
    if (locked) wt.releaseLock(ROOT, "build");
  }

  const statusFile = join(raw, "status.txt");
  const status = parseStatus(existsSync(statusFile) ? readFileSync(statusFile, "utf8") : "");
  if (res.timedOut) console.log(`TIMEOUT after ${Math.round(res.seconds)}s: Workbench was killed; ${Object.keys(status.shots).length}/${shots.length} shot(s) written`);
  if (!status.world) {
    const log = latestLog(profile);
    const errs = compileErrors(log);
    console.log(`FAIL  look: Workbench exit ${res.code} after ${r1(res.seconds)} s and the plugin never opened the world${status.errors.length ? ` (${status.errors.join("; ")})` : ""}`);
    for (const e of errs) console.log(`      ${e.trim()}`);
    if (log) console.log(`      log: ${log}`);
    return 1;
  }

  // Collect per area: move the PNGs, read their pixels, build the sheet and the index.
  const { image } = await import("../../agent-box/shots.mjs");
  const bootS = res.seconds - (status.done ? status.done.ms : 0) / 1000;
  let flagged = 0;
  let missing = 0;
  const overview = [];
  const summaries = [];
  for (const { area, shots: list } of perArea) {
    const dir = join(outRoot, safe(area.name));
    mkdirSync(dir, { recursive: true });
    for (const f of readdirSync(dir)) if (/\.(png|jpg|json)$/.test(f)) rmSync(join(dir, f), { force: true });
    const rows = [];
    for (const shot of list) {
      const st = status.shots[shot.name] ?? null;
      const file = st ? join(dir, `${shot.kind}.png`) : null;
      let px = null;
      if (st) {
        try {
          renameSync(join(raw, st.file), file);
          px = pixelStats(decodePng(readFileSync(file)));
        } catch (e) {
          px = null;
          if (!existsSync(file)) st.file = null;
        }
      }
      const flags = flagsFor(shot, st && st.file ? st : null, px);
      if (flags.includes("missing")) missing++;
      else if (flags.length) flagged++;
      rows.push({ kind: shot.kind, label: shot.label, file: st && st.file ? file : null, flags, clear: st?.clear ?? null, size: st?.size ?? null, cam: st?.cam ?? null, px, entity: shot.entity ?? null, tile: tileLabel(shot, st, flags) });
    }
    const tiles = rows.map((r) => ({ file: r.file, label: r.tile, marks: [] }));
    const { cols } = sheetLayout(tiles.length);
    const contact = join(dir, "contact.jpg");
    image({ op: "sheet", tiles, out: contact, cols, maxWidth: cols * 480, quality: 74 });
    writeFileSync(join(dir, "index.json"), JSON.stringify({ area: area.name, mode: opts.edit ? "edit" : "play", hour: opts.hour, at: new Date().toISOString(), contact, shots: rows }, null, 2) + "\n");
    const bad = rows.filter((r) => r.flags.length);
    summaries.push({ area: area.name, contact, bad, count: rows.length });
    if (rows[0].file) overview.push({ file: rows[0].file, label: `${area.name}${bad.length ? `  [${bad.length} flagged]` : ""}`, marks: [] });
  }
  if (perArea.length > 1 && overview.length) {
    const { cols } = sheetLayout(overview.length);
    image({ op: "sheet", tiles: overview, out: join(outRoot, "overview.jpg"), cols, maxWidth: cols * 480, quality: 74 });
  }
  if (!args.flags.keep) for (const f of readdirSync(raw)) rmSync(join(raw, f), { force: true });

  const total = (Date.now() - started) / 1000;
  const ram = res.peakMb ? `, Workbench peak ${res.peakMb} MB` : "";
  console.log(`look: ${shots.length - missing}/${shots.length} shot(s) in ${r1(total)} s (Workbench start ${r1(bootS)} s, shots ${r1((status.done?.ms ?? 0) / 1000 - (status.readyMs ?? 0) / 1000)} s${ram}); ${flagged} flagged, ${missing} missing`);
  for (const s of summaries) {
    console.log(`  ${s.area}: ${s.contact}`);
    for (const b of s.bad) console.log(`    ${b.flags.join(",")}: ${b.kind} ${b.label}${b.size ? ` ${sizeText(b.size)}` : ""}`);
  }
  if (perArea.length > 1) console.log(`  overview: ${join(outRoot, "overview.jpg")}`);
  for (const e of status.errors) console.log(`  plugin error: ${e}`);
  for (const w of status.warnings) console.log(`  plugin warning: ${w}`);
  console.log("  read the contact sheet; flags are hints (docs/visual-testing.md). Pictures for a PR still come from `shots area` on a session.");
  return missing || res.timedOut || status.errors.length ? 1 : 0;
}

// ------------------------------------------------------------------ selftest

export function selftest() {
  const failures = [];
  const ok = (cond, what) => {
    if (!cond) failures.push(what);
  };

  // Plan: an area with one interactable, one unnamed prop, a tree and a road.
  const area = { name: "test area", x: 100, y: 0, z: 200, radius: 10, front: 90 };
  const world = [
    { name: "Till_Test", layer: "Shops.layer", actions: ["LF_ShopAction"], x: 101, y: 1, z: 201, prefab: "Prefabs/Props/LF_Till.et" },
    { name: "", layer: "Dressing.layer", actions: [], x: 104, y: 0, z: 200, anonPrefab: "Prefabs/Props/Commercial_Sack.et", scale: 3 },
    { name: "", layer: "Dressing.layer", actions: [], x: 103, y: 0, z: 200, anonPrefab: "Prefabs/Vegetation/Tree/Oak.et" },
    { name: "RoadEntity", layer: "Village_Roads.layer", actions: [], x: 100, y: 0, z: 200, prefab: "Prefabs/Roads/R.et" },
    { name: "Far_Away", layer: "Shops.layer", actions: [], x: 300, y: 0, z: 200, prefab: "Prefabs/Props/LF_Till.et" },
  ];
  const shots = shotsFor(area, world, { close: 12 });
  ok(shots.map((s) => s.kind).join(" ") === "wide ob180 ob270 ob000 top c01_Till_Test c02_Commercial_Sack", `shot kinds: ${shots.map((s) => s.kind).join(" ")}`);
  ok(shots.every((s) => /^[A-Za-z0-9_-]+$/.test(s.name) && s.line.includes(s.name)), "a shot name is not file-safe or not in its line");
  ok(shots[0].line.startsWith("shot test_area__wide 100 - 200 1.2 0.85 90:10:2.2,"), `wide line: ${shots[0].line.slice(0, 60)}`);
  ok(shots[5].line === "close test_area__c01_Till_Test Till_Test 101 1 201 90", `close line: ${shots[5].line}`);
  ok(shots[6].line === "close test_area__c02_Commercial_Sack - 104 0 200 -90", `unnamed close line: ${shots[6].line}`);
  ok(shotsFor(area, world, { close: 0 }).length === 5, "--close 0 still planned a close-up");
  const plan = planText(shots, [area], { hour: 9 });
  ok(/^world Worlds\/DebugWorld\/DebugWorld\.ent\nmode play\nhour 9\nsettle 350\nfirst 3000\npark -60 40\nshot /.test(plan), `plan head: ${plan.slice(0, 110)}`);
  ok(planText(shots, [area], { edit: true }).includes("\nmode edit\n"), "--edit did not reach the plan");
  ok(resolveArea("12.5,40", []).name === "xz-13-40" && resolveArea("bank", [{ name: "bank", x: 1, z: 2, radius: 5 }], 9).radius === 9, "resolveArea");
  let threw = false;
  try {
    resolveArea("nowhere", [{ name: "bank" }]);
  } catch {
    threw = true;
  }
  ok(threw, "an unknown area did not throw");

  // Status: the plugin's lines, a failure, and a run that never finished.
  const status = parseStatus([
    "1018 ok world $LifeFramework:Worlds/DebugWorld/DebugWorld.ent view=1292x714 entities=909",
    "3158 ok play waited=200",
    "3159 ok hour local=14 game=9.27",
    "6504 ok shot test_area__wide test_area__wide.png pick=2 clear=0.445381 cam=188,20,270",
    "7135 ok shot test_area__c01_Till_Test test_area__c01_Till_Test.png clear=0.61 found=1 size=0.4,1.4,0.4 cam=1,2,3",
    "7500 error no_file test_area__top",
    "7600 warn no_clock",
    "7637 done 3 1",
  ].join("\r\n"));
  ok(status.world?.endsWith("DebugWorld.ent") && status.play && status.readyMs === 3158, "status head");
  ok(status.shots.test_area__wide?.pick === 2 && Math.abs(status.shots.test_area__wide.clear - 0.445381) < 1e-9 && status.shots.test_area__wide.cam[1] === 20, "status area shot");
  const closeSt = status.shots.test_area__c01_Till_Test;
  ok(closeSt?.found === true && closeSt.size?.[1] === 1.4, "status close shot");
  ok(status.errors[0] === "no_file test_area__top" && status.warnings[0] === "no_clock" && status.done?.failed === 1, "status errors and done");
  ok(parseStatus("").world === null && parseStatus("12 ok world X view=1x1 entities=0").done === null, "an unfinished status read as done");

  // PNG round trip and the pixel numbers.
  const flat = decodePng(encodePng(64, 36, () => [128, 128, 128]));
  ok(flat.w === 64 && flat.h === 36 && flat.px[0] === 128 && flat.px[flat.px.length - 1] === 128, "PNG round trip");
  const grad = decodePng(encodePng(64, 36, (x, y) => [x * 3, y * 6, (x + y) * 2]));
  ok(grad.px[(10 * 64 + 20) * 3] === 60 && grad.px[(10 * 64 + 20) * 3 + 1] === 60 && grad.px[(10 * 64 + 20) * 3 + 2] === 60, "PNG pixel values");
  let badPng = false;
  try {
    decodePng(Buffer.from("not a png at all, just text padding to pass the length check"));
  } catch {
    badPng = true;
  }
  ok(badPng, "a non-PNG decoded");
  const stFlat = pixelStats(flat);
  ok(stFlat.sd === 0 && stFlat.mean === 128, `flat stats ${JSON.stringify(stFlat)}`);
  const whiteBox = pixelStats(decodePng(encodePng(64, 36, (x, y) => (x > 16 && x < 48 && y > 8 && y < 28 ? [250, 250, 250] : [40 + (x % 7) * 9, 90, 30]))));
  const scene = pixelStats(decodePng(encodePng(64, 36, (x, y) => [60 + ((x * 5) % 120), 80 + ((y * 9) % 90), 50 + ((x + y) % 60)])));
  const magenta = pixelStats(decodePng(encodePng(64, 36, (x) => (x > 24 && x < 40 ? [255, 0, 255] : [60 + x, 90, 70]))));

  // Flags.
  const closeShot = shots[5];
  const wideShot = shots[0];
  const topShot = shots[4];
  const fine = { file: "x.png", clear: 0.6, found: true, size: [0.4, 0.4, 0.4] };
  ok(flagsFor(closeShot, fine, scene).length === 0, `a normal close-up was flagged: ${flagsFor(closeShot, fine, scene)}`);
  ok(flagsFor(closeShot, fine, whiteBox).join() === "white", `white mesh close-up: ${flagsFor(closeShot, fine, whiteBox)} ${JSON.stringify(whiteBox)}`);
  ok(flagsFor(wideShot, { ...fine, found: null }, whiteBox).length === 0, "a wide with a white wall was flagged white");
  ok(flagsFor(closeShot, fine, magenta).includes("magenta"), "missing-texture magenta not flagged");
  ok(flagsFor(closeShot, fine, stFlat).join() === "flat", "a flat frame not flagged");
  ok(flagsFor(closeShot, fine, { mean: 12, sd: 20, white: 0, magenta: 0 }).join() === "dark" && flagsFor(closeShot, fine, { mean: 240, sd: 20, white: 0, magenta: 0 }).join() === "blown", "dark/blown");
  ok(flagsFor(closeShot, { ...fine, found: false }, scene).join() === "not-found", "an entity the plugin did not find was not flagged");
  ok(flagsFor(closeShot, { ...fine, clear: 0.05 }, scene).join() === "blocked" && flagsFor(wideShot, { ...fine, clear: 0.15 }, scene).join() === "blocked", "blocked");
  ok(flagsFor(topShot, { ...fine, clear: 0.01 }, scene).length === 0, "the top view was flagged blocked by the roof it looks at");
  ok(flagsFor(closeShot, null, null).join() === "missing", "a shot with no file is not `missing`");
  ok(tileLabel(shots[6], { size: [0.42, 1.44, 12.4] }, ["white"]) === "Commercial_Sack  0.4x1.4x12m  scale 3  [WHITE]", `tile label: ${tileLabel(shots[6], { size: [0.42, 1.44, 12.4] }, ["white"])}`);
  ok(sheetLayout(5).cols === 3 && sheetLayout(17).cols === 4 && sheetLayout(17).rows === 5, "sheet layout");

  // The plugin: no ternary (a compile error in Enforce Script), the class name the launcher asks for.
  const plugin = readFileSync(PLUGIN, "utf8").replace(/\/\/.*$/gm, "");
  ok(/class LF_VisualShotsPlugin : WorldEditorPlugin/.test(plugin), "plugin class name");
  ok(!/\?[^:\n"]*:/.test(plugin.replace(/"[^"\n]*"/g, '""')), "the plugin uses a ternary");
  for (const d of ["world ", "mode ", "hour ", "settle ", "first ", "park ", "shot ", "close "]) ok(plugin.includes(`StartsWith("${d}")`), `the plugin does not read the '${d.trim()}' directive`);

  // The wide camera's lens fan (#814): a thin pole just off the centre line must be hit by some fan ray.
  // The constants come from the plugin, so loosening the fan there fails here.
  const fanConst = (n) => Number((plugin.match(new RegExp(`static const float ${n} = ([0-9.]+)`)) || [])[1]);
  const [fanStep, fanHalf, fanReach] = ["FAN_STEP_DEG", "FAN_HALF_DEG", "FAN_REACH"].map(fanConst);
  ok(fanStep > 0 && fanHalf > 0 && fanReach > 0, "the plugin lost its lens-fan constants");
  ok(plugin.includes("ClearFan(at, target)") && /fan < free/.test(plugin) && plugin.includes("wide = name.Contains"), "TakeShot does not fold the lens fan into `clear`");
  // The bank wide: camera (188, 242) looking +z, lamp pole at (187, 244.6), 0.2 m across (about 0.1 m radius).
  const fanHits = (camXZ, poleXZ, radius, tgtXZ) => {
    const heading = Math.atan2(tgtXZ[0] - camXZ[0], tgtXZ[1] - camXZ[1]);
    const dx = poleXZ[0] - camXZ[0], dz = poleXZ[1] - camXZ[1];
    const d = Math.hypot(dx, dz);
    for (let deg = -fanHalf; deg <= fanHalf + 0.01; deg += fanStep) {
      const a = heading + (deg * Math.PI) / 180;
      const along = dx * Math.sin(a) + dz * Math.cos(a);
      const off = Math.abs(dx * Math.cos(a) - dz * Math.sin(a));
      if (along > 0 && along <= fanReach + radius && off <= radius) return true;
    }
    return d < 0;
  };
  ok(fanHits([188, 242], [187, 244.6], 0.1, [188, 260]), "the lens fan misses a lamp pole 2.8 m in front of the bank wide camera");
  ok(fanHits([188, 242], [186.5, 244.5], 0.1, [188, 260]) && !fanHits([188, 242], [188, 250], 0.1, [188, 252]), "fan geometry: a pole inside the reach is hit, one beyond it is not");

  if (failures.length) {
    for (const f of failures) console.log(`look selftest FAIL: ${f}`);
    return 1;
  }
  console.log("look selftest: OK (plan, status, PNG stats, flags, plugin directives, lens fan)");
  return 0;
}

// ------------------------------------------------------------------ main

const USAGE = "usage: tools\\cli look <area | x,z | all> [more areas] [--edit] [--close N] [--radius m] [--hour h] [--measure] [--wait] [--force] | look item <prefab|slug> [more] [--states ground,inventory] [--at area|x,z] | look selftest";

export async function main(argv) {
  const args = parseArgs(argv);
  if (args.pos[0] === "selftest") return selftest();
  if (!args.pos.length || args.flags.help) {
    console.log(USAGE);
    return args.flags.help ? 0 : 2;
  }
  // One of this PC's Workbench slots for the whole look (tools/lib/wb-lock.mjs).
  const { acquireWorkbench } = await import("../lib/wb-lock.mjs");
  const wb = await acquireWorkbench(ROOT, { purpose: `look ${args.pos[0]}` });
  try {
    return await run(args);
  } finally {
    wb.release();
  }
}

if (import.meta.url === pathToFileURL(process.argv[1] ?? "").href) {
  main(process.argv.slice(2)).then((code) => process.exit(code), (e) => {
    console.error(e);
    process.exit(1);
  });
}
