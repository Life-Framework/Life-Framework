#!/usr/bin/env node
// Filming harness. Records the rendering game window to mp4 with ffmpeg and
// makes a small looping GIF preview for a PR comment.
//
//   node tools/film/film.mjs fetch [--zip <path>]
//   node tools/film/film.mjs probe
//   node tools/film/film.mjs record (--window <title> [--pid N] | --region x,y,w,h | --desktop)
//        [--seconds 10] [--fps 30] [--method ddagrab|gdigrab] [--encoder <name>]
//        [--out <file.mp4>] [--no-preview]
//   node tools/film/film.mjs flow <agent-box flow> (--window <title> | --region ...)
//        [--pre 1.5] [--post 2] [--out <file.mp4>]
//   node tools/film/film.mjs publish <clip.mp4> --pr <N> [--title text] [--yes]
//   node tools/film/film.mjs selftest
//
// Capture is desktop duplication (ddagrab, GPU) of the game window's rectangle,
// encoded on the GPU (h264_nvenc / qsv / amf, libx264 fallback). gdigrab is
// the fallback when ddagrab is missing or delivers no frame; it can record black frames from a
// flip-model D3D window, so prefer ddagrab for the game. Nothing here talks to
// the engine: the agent-box harness drives the player, this records the screen.

import { spawn, spawnSync } from "node:child_process";
import { createHash } from "node:crypto";
import { closeSync, copyFileSync, existsSync, mkdirSync, openSync, readFileSync, readdirSync, renameSync, rmSync, statSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
export const BIN_DIR = join(ROOT, "tools", "film", ".bin");
export const OUT_DIR = join(ROOT, "agent-box", "reports", "film");

// Pinned portable build. Fetched into tools/film/.bin (git-ignored); never
// installed system-wide. Hash and size re-verified 2026-11-08 (asset uploaded
// 2025-03-12, unchanged). urls are tried in order: the release download link,
// then the same asset by its immutable API id (a different GitHub front end,
// which still served it while releases/download answered 504).
export const FFMPEG_BUILD = {
  version: "7.1.1",
  urls: [
    "https://github.com/GyanD/codexffmpeg/releases/download/7.1.1/ffmpeg-7.1.1-essentials_build.zip",
    "https://api.github.com/repos/GyanD/codexffmpeg/releases/assets/236740574",
  ],
  bytes: 92234348,
  sha256: "04861d3339c5ebe38b56c19a15cf2c0cc97f5de4fa8910e4d47e5e6404e4a2d4",
};

// Preference order. Hardware first: the game is already loading the GPU, and
// NVENC/QSV/AMF encode on a separate block so capture does not cost frames.
export const ENCODERS = ["h264_nvenc", "h264_qsv", "h264_amf", "libx264"];

// PR budget: GitHub renders a GIF inline; keep it small enough to load fast.
export const BUDGET = { mp4Bytes: 25 * 1024 * 1024, previewBytes: 8 * 1024 * 1024 };

// ---------------------------------------------------------------- pure parts

export function parseRegion(text) {
  const parts = String(text || "").split(",").map((n) => Number(n.trim()));
  if (parts.length !== 4 || parts.some((n) => !Number.isFinite(n))) {
    throw new Error(`region must be x,y,w,h: ${text}`);
  }
  const [x, y, w, h] = parts.map((n) => Math.round(n));
  if (w < 16 || h < 16) throw new Error(`region too small: ${w}x${h}`);
  // H.264 4:2:0 needs even dimensions.
  return { x, y, w: w - (w % 2), h: h - (h % 2) };
}

export function pickEncoder(encodersText, wanted) {
  const have = new Set(
    String(encodersText || "")
      .split(/\r?\n/)
      .map((line) => line.trim().split(/\s+/)[1])
      .filter(Boolean),
  );
  if (wanted) {
    if (!have.has(wanted)) throw new Error(`encoder ${wanted} not in this ffmpeg build`);
    return wanted;
  }
  return ENCODERS.find((name) => have.has(name)) || "";
}

export function encoderArgs(encoder) {
  if (encoder === "h264_nvenc") return ["-c:v", "h264_nvenc", "-preset", "p5", "-rc", "vbr", "-cq", "26", "-b:v", "0"];
  if (encoder === "h264_qsv") return ["-c:v", "h264_qsv", "-global_quality", "26"];
  if (encoder === "h264_amf") return ["-c:v", "h264_amf", "-rc", "cqp", "-qp_i", "24", "-qp_p", "26"];
  if (encoder === "libx264") return ["-c:v", "libx264", "-preset", "veryfast", "-crf", "26"];
  throw new Error(`unknown encoder: ${encoder}`);
}

// source: { kind: "region", x, y, w, h } | { kind: "window", title } | { kind: "desktop" }
// ddagrab takes a rectangle on one monitor; a window is resolved to its
// rectangle first (windowRect) so the caller passes a region.
export function captureInputArgs(source, method, fps) {
  if (method === "ddagrab") {
    if (source.kind === "window") throw new Error("ddagrab needs a region; resolve the window rectangle first");
    const opts = [`output_idx=${source.output || 0}`, `framerate=${fps}`, "draw_mouse=0"];
    if (source.kind === "region") {
      opts.push(`offset_x=${source.x}`, `offset_y=${source.y}`, `video_size=${source.w}x${source.h}`);
    }
    return ["-f", "lavfi", "-i", `ddagrab=${opts.join(":")}`];
  }
  if (method === "gdigrab") {
    const base = ["-f", "gdigrab", "-framerate", String(fps), "-draw_mouse", "0"];
    if (source.kind === "window") return [...base, "-i", `title=${source.title}`];
    if (source.kind === "region") {
      return [...base, "-offset_x", String(source.x), "-offset_y", String(source.y), "-video_size", `${source.w}x${source.h}`, "-i", "desktop"];
    }
    return [...base, "-i", "desktop"];
  }
  throw new Error(`capture method must be ddagrab or gdigrab: ${method}`);
}

// ddagrab produces D3D11 frames. NVENC takes them as-is; every other encoder
// needs them downloaded to system memory first.
export function videoFilter(method, encoder, source) {
  const filters = [];
  if (method === "ddagrab" && encoder !== "h264_nvenc") filters.push("hwdownload", "format=bgra");
  if (method === "gdigrab" && source.kind !== "region") filters.push("crop=trunc(iw/2)*2:trunc(ih/2)*2");
  return filters;
}

export function buildRecordArgs({ source, method = "ddagrab", encoder, fps = 30, seconds, out }) {
  if (!out) throw new Error("out path required");
  if (seconds !== undefined && !(Number(seconds) > 0 && Number(seconds) <= 600)) {
    throw new Error(`seconds must be in (0, 600]: ${seconds}`);
  }
  const args = ["-hide_banner", "-nostats", "-loglevel", "warning", "-y", ...captureInputArgs(source, method, fps)];
  if (seconds !== undefined) args.push("-t", String(seconds));
  const filters = videoFilter(method, encoder, source);
  if (filters.length) args.push("-vf", filters.join(","));
  args.push(...encoderArgs(encoder));
  if (encoder !== "h264_nvenc" || method !== "ddagrab") args.push("-pix_fmt", "yuv420p");
  args.push("-an", "-movflags", "+faststart", out);
  return args;
}

export function buildPreviewArgs(input, out, { width = 640, fps = 10, maxSeconds = 12 } = {}) {
  const scale = `fps=${fps},scale=${width}:-2:flags=lanczos`;
  const head = ["-hide_banner", "-nostats", "-loglevel", "warning", "-y", "-t", String(maxSeconds), "-i", input];
  if (out.toLowerCase().endsWith(".gif")) {
    const graph = `${scale},split[a][b];[a]palettegen=max_colors=128:stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=4`;
    return [...head, "-filter_complex", graph, "-loop", "0", out];
  }
  throw new Error(`preview must be .gif: ${out}`);
}

export function stampName(label, now = new Date()) {
  const safe = String(label || "clip").replace(/[^A-Za-z0-9_-]+/g, "_").slice(0, 48) || "clip";
  const iso = now.toISOString().replace(/[-:]/g, "").replace(/\..*$/, "").replace("T", "-");
  return `${safe}-${iso}`;
}

// ------------------------------------------------------------- environment

export function resolveFfmpeg(env = process.env) {
  const candidates = [];
  if (env.FFMPEG_PATH) candidates.push(env.FFMPEG_PATH);
  candidates.push(join(BIN_DIR, process.platform === "win32" ? "ffmpeg.exe" : "ffmpeg"));
  candidates.push("ffmpeg");
  for (const path of candidates) {
    const run = spawnSync(path, ["-hide_banner", "-version"], { windowsHide: true, encoding: "utf8" });
    if (run.status === 0) return { path, version: (run.stdout.split(/\r?\n/)[0] || "").trim() };
  }
  return null;
}

function ffmpegOrThrow() {
  const ff = resolveFfmpeg();
  if (!ff) throw new Error("ffmpeg not found. Run: node tools/film/film.mjs fetch  (or set FFMPEG_PATH)");
  return ff;
}

function listEncoders(ffPath) {
  return spawnSync(ffPath, ["-hide_banner", "-encoders"], { windowsHide: true, encoding: "utf8" }).stdout || "";
}

function hasFilter(ffPath, name) {
  const text = spawnSync(ffPath, ["-hide_banner", "-filters"], { windowsHide: true, encoding: "utf8" }).stdout || "";
  return text.split(/\r?\n/).some((line) => line.trim().split(/\s+/)[1] === name);
}

// Encoders listed by -encoders may still fail at open (no driver/GPU).
// One 1-frame test encode settles it.
function encoderWorks(ffPath, encoder) {
  const run = spawnSync(
    ffPath,
    ["-hide_banner", "-loglevel", "error", "-f", "lavfi", "-i", "color=c=black:s=256x256:d=0.1", "-frames:v", "1", ...encoderArgs(encoder), "-pix_fmt", "yuv420p", "-f", "null", "-"],
    { windowsHide: true, encoding: "utf8" },
  );
  return run.status === 0;
}

export function chooseEncoder(ffPath, wanted) {
  const text = listEncoders(ffPath);
  if (wanted) return pickEncoder(text, wanted);
  for (const name of ENCODERS) {
    if (!pickEncoderSafe(text, name)) continue;
    if (encoderWorks(ffPath, name)) return name;
  }
  return "";
}

function pickEncoderSafe(text, name) {
  try {
    return pickEncoder(text, name);
  } catch {
    return "";
  }
}

// Window rectangle in physical pixels (DPI-aware), plus the monitor index
// ddagrab needs. Windows only; uses user32 through PowerShell.
export function windowRect(title, pid = 0) {
  if (process.platform !== "win32") throw new Error("window lookup is Windows-only; pass --region");
  const script = `
$ErrorActionPreference = 'Stop'
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class FilmWin {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int w, int ht, uint flags);
}
"@
[FilmWin]::SetProcessDPIAware() | Out-Null
Add-Type -AssemblyName System.Windows.Forms
# An exact title wins: "Arma Reforger" must not pick "Arma Reforger Workbench".
$w = Get-Process | Where-Object { $_.MainWindowHandle -ne 0 }
# A session runs two clients with the same title; FILM_PID picks one.
if ($env:FILM_PID -ne '0') { $w = $w | Where-Object { $_.Id -eq [int]$env:FILM_PID } }
$p = $w | Where-Object { $_.MainWindowTitle -eq $env:FILM_EXACT } | Select-Object -First 1
if (-not $p) { $p = $w | Where-Object { $_.MainWindowTitle -like $env:FILM_TITLE } | Select-Object -First 1 }
if (-not $p) { Write-Output 'none'; exit 0 }
$r = New-Object FilmWin+RECT
[FilmWin]::GetClientRect($p.MainWindowHandle, [ref]$r) | Out-Null
$pt = New-Object FilmWin+POINT
[FilmWin]::ClientToScreen($p.MainWindowHandle, [ref]$pt) | Out-Null
$screens = [System.Windows.Forms.Screen]::AllScreens
$idx = 0; $ox = 0; $oy = 0
for ($i = 0; $i -lt $screens.Length; $i++) {
  $b = $screens[$i].Bounds
  if ($pt.X -ge $b.X -and $pt.X -lt ($b.X + $b.Width) -and $pt.Y -ge $b.Y -and $pt.Y -lt ($b.Y + $b.Height)) { $idx = $i; $ox = $b.X; $oy = $b.Y }
}
# Desktop duplication records whatever is on top. Never take focus from the
# user: on a secondary (film) monitor, lift the window in z-order without
# activating it; on the primary monitor leave the user's windows alone.
if (-not $screens[$idx].Primary) { [FilmWin]::SetWindowPos($p.MainWindowHandle, [IntPtr]::Zero, 0, 0, 0, 0, 0x13) | Out-Null }
Write-Output ("{0} {1} {2} {3} {4} {5} {6} {7}" -f ($pt.X - $ox), ($pt.Y - $oy), ($r.R - $r.L), ($r.B - $r.T), $idx, $ox, $oy, $p.MainWindowTitle)
`;
  const run = spawnSync("powershell.exe", ["-NoProfile", "-NonInteractive", "-Command", script], {
    windowsHide: true,
    encoding: "utf8",
    env: { ...process.env, FILM_PID: String(pid || 0), FILM_EXACT: title, FILM_TITLE: title.includes("*") ? title : `*${title}*` },
  });
  const line = (run.stdout || "").trim().split(/\r?\n/).pop() || "";
  if (!line || line === "none") throw new Error(`no visible window matching "${title}"`);
  const [x, y, w, h, output, originX, originY, ...rest] = line.split(" ");
  const region = parseRegion(`${x},${y},${w},${h}`);
  // x,y are relative to the monitor (ddagrab); originX/Y turn them into desktop
  // coordinates for gdigrab.
  return { kind: "region", ...region, output: Number(output), originX: Number(originX), originY: Number(originY), title: rest.join(" ") };
}

// Display-adapter order can differ from Screen.AllScreens order on mixed GPU
// boxes (virtual display drivers). FILM_OUTPUT_IDX overrides when it does.
function applyOutputOverride(source) {
  if (process.env.FILM_OUTPUT_IDX !== undefined && source.kind === "region") {
    return { ...source, output: Number(process.env.FILM_OUTPUT_IDX) };
  }
  return source;
}

// ----------------------------------------------------------------- actions

function parseFlags(argv) {
  const flags = { _: [] };
  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];
    if (!arg.startsWith("--")) {
      flags._.push(arg);
      continue;
    }
    const key = arg.slice(2);
    const next = argv[i + 1];
    if (next === undefined || next.startsWith("--")) flags[key] = true;
    else {
      flags[key] = next;
      i++;
    }
  }
  return flags;
}

function sourceFromFlags(flags) {
  if (flags.region) return { kind: "region", ...parseRegion(flags.region) };
  if (flags.window) return { kind: "window", title: String(flags.window) };
  if (flags.desktop) return { kind: "desktop" };
  throw new Error("pass --window <title>, --region x,y,w,h, or --desktop");
}

// ddagrab can open an output and then never deliver a frame (seen on a box
// with a virtual display adapter beside the real GPU): ffmpeg hangs on the
// first frame. A 1-frame grab with a timeout settles it before recording.
function ddagrabDelivers(ffPath, source) {
  const args = ["-hide_banner", "-loglevel", "error", ...captureInputArgs(source, "ddagrab", 30), "-frames:v", "1", "-vf", "hwdownload,format=bgra", "-f", "null", "-"];
  return spawnSync(ffPath, args, { windowsHide: true, encoding: "utf8", timeout: 5000 }).status === 0;
}

function planCapture(ffPath, flags) {
  let source = sourceFromFlags(flags);
  let method = flags.method || (hasFilter(ffPath, "ddagrab") ? "ddagrab" : "gdigrab");
  if (source.kind === "window") source = windowRect(source.title, Number(flags.pid || 0));
  source = applyOutputOverride(source);
  if (method === "ddagrab" && !flags.method && !ddagrabDelivers(ffPath, source)) {
    console.log("ddagrab delivered no frame in 5 s; falling back to gdigrab");
    method = "gdigrab";
  }
  if (method === "gdigrab" && source.originX !== undefined) {
    source = { ...source, x: source.x + source.originX, y: source.y + source.originY };
  }
  const encoder = chooseEncoder(ffPath, flags.encoder);
  if (!encoder) throw new Error("no working H.264 encoder in this ffmpeg build");
  const fps = Number(flags.fps || 30);
  return { source, method, encoder, fps };
}

export function makePreview(ffPath, mp4, ext = ".gif") {
  const out = mp4.replace(/\.mp4$/i, "") + ".preview" + ext;
  const run = spawnSync(ffPath, buildPreviewArgs(mp4, out), { windowsHide: true, encoding: "utf8" });
  if (run.status !== 0 || !existsSync(out)) throw new Error(`preview failed: ${run.stderr}`);
  return out;
}

function sizeLine(path, budget) {
  const bytes = statSync(path).size;
  const mb = (bytes / 1024 / 1024).toFixed(2);
  const flag = bytes > budget ? "  OVER BUDGET" : "";
  return `${path}  ${mb} MB${flag}`;
}

function probeDuration(ffPath, mp4) {
  const probe = join(dirname(ffPath), process.platform === "win32" ? "ffprobe.exe" : "ffprobe");
  const exe = existsSync(probe) ? probe : "ffprobe";
  const run = spawnSync(exe, ["-v", "error", "-show_entries", "format=duration:stream=width,height,codec_name,nb_frames", "-of", "default=nw=1", mp4], { windowsHide: true, encoding: "utf8" });
  return (run.stdout || "").trim().replace(/\r?\n/g, " ");
}

function record(flags) {
  const ff = ffmpegOrThrow();
  const plan = planCapture(ff.path, flags);
  const seconds = Number(flags.seconds || 10);
  mkdirSync(OUT_DIR, { recursive: true });
  const out = resolve(flags.out || join(OUT_DIR, stampName(flags.label || "clip") + ".mp4"));
  mkdirSync(dirname(out), { recursive: true });
  const args = buildRecordArgs({ ...plan, seconds, out });
  console.log(`ffmpeg: ${ff.path}`);
  console.log(`capture: ${plan.method} ${JSON.stringify(plan.source)} @${plan.fps}fps, encoder ${plan.encoder}`);
  const run = spawnSync(ff.path, args, { windowsHide: true, encoding: "utf8", timeout: (seconds + 30) * 1000 });
  if (run.status !== 0 || !existsSync(out)) {
    console.error(run.stderr || "ffmpeg failed");
    return 1;
  }
  console.log(`clip: ${sizeLine(out, BUDGET.mp4Bytes)}`);
  console.log(`probe: ${probeDuration(ff.path, out)}`);
  if (!flags["no-preview"]) {
    const preview = makePreview(ff.path, out);
    console.log(`preview: ${sizeLine(preview, BUDGET.previewBytes)}`);
  }
  return 0;
}

function sleep(ms) {
  return new Promise((done) => setTimeout(done, ms));
}

// Starts an open-ended recording and returns { out, plan, stop() }. stop()
// sends "q" so ffmpeg finalises the mp4. ffmpeg's log goes to a file, not a
// pipe: a synchronous caller (the journey) blocks the event loop for minutes
// and a full stderr pipe would stall the encoder.
export function startRecording(flags) {
  const ff = ffmpegOrThrow();
  const plan = planCapture(ff.path, flags);
  mkdirSync(OUT_DIR, { recursive: true });
  const out = resolve(flags.out || join(OUT_DIR, stampName(flags.label || "clip") + ".mp4"));
  const logFd = openSync(out.replace(/\.mp4$/i, ".ffmpeg.log"), "w");
  const child = spawn(ff.path, buildRecordArgs({ ...plan, out }), { windowsHide: true, stdio: ["pipe", "ignore", logFd] });
  const exited = new Promise((done) => child.on("exit", (code) => done(code)));
  return {
    out,
    plan,
    async stop() {
      child.stdin.write("q");
      child.stdin.end();
      const code = await Promise.race([exited, sleep(15000).then(() => "timeout")]);
      if (code === "timeout") child.kill();
      closeSync(logFd);
      const ok = code === 0 && existsSync(out);
      let preview = "";
      if (ok) {
        try {
          preview = makePreview(ff.path, out);
        } catch {}
      }
      return { ok, code, out, preview };
    },
  };
}

// Record while an agent-box flow plays. ffmpeg runs open-ended and is stopped
// with "q" on stdin, which finalises the mp4 (moov atom) cleanly.
async function filmFlow(flags) {
  const name = flags._[1];
  if (!name) throw new Error("flow <name> required");
  const harness = await import("../../agent-box/harness.mjs");
  const lines = harness.readFlow(name);
  const ff = ffmpegOrThrow();
  const plan = planCapture(ff.path, flags);
  mkdirSync(OUT_DIR, { recursive: true });
  const out = resolve(flags.out || join(OUT_DIR, stampName(`flow-${name}`) + ".mp4"));
  const args = buildRecordArgs({ ...plan, out });
  console.log(`capture: ${plan.method} ${JSON.stringify(plan.source)} @${plan.fps}fps, encoder ${plan.encoder}`);

  const child = spawn(ff.path, args, { windowsHide: true, stdio: ["pipe", "ignore", "pipe"] });
  let stderr = "";
  child.stderr.on("data", (chunk) => {
    stderr += chunk;
  });
  const exited = new Promise((done) => child.on("exit", (code) => done(code)));

  const started = Date.now();
  await sleep(Number(flags.pre || 1.5) * 1000);
  const played = harness.runFlow(lines); // synchronous: blocks while ffmpeg keeps recording
  const flowMs = Date.now() - started;
  await sleep(Number(flags.post || 2) * 1000);
  child.stdin.write("q");
  child.stdin.end();
  const code = await Promise.race([exited, sleep(15000).then(() => "timeout")]);
  if (code === "timeout") child.kill();

  for (const line of played.lines) console.log(line);
  const sidecar = out.replace(/\.mp4$/i, ".json");
  writeFileSync(
    sidecar,
    JSON.stringify({ flow: name, flowCode: played.code, flowMs, capture: plan, steps: played.lines }, null, 2) + "\n",
  );
  if (code !== 0 || !existsSync(out)) {
    console.error(stderr || `ffmpeg exited ${code}`);
    return 1;
  }
  console.log(`clip: ${sizeLine(out, BUDGET.mp4Bytes)}`);
  const preview = makePreview(ff.path, out);
  console.log(`preview: ${sizeLine(preview, BUDGET.previewBytes)}`);
  console.log(`steps: ${sidecar}`);
  return played.code;
}

function probe() {
  const ff = resolveFfmpeg();
  if (!ff) {
    console.log("ffmpeg: missing (node tools/film/film.mjs fetch)");
    return 1;
  }
  console.log(`ffmpeg: ${ff.path}`);
  console.log(`version: ${ff.version}`);
  const text = listEncoders(ff.path);
  for (const name of ENCODERS) {
    const listed = Boolean(pickEncoderSafe(text, name));
    console.log(`encoder ${name}: ${listed ? (encoderWorks(ff.path, name) ? "works" : "listed, fails to open") : "not built"}`);
  }
  console.log(`ddagrab: ${hasFilter(ff.path, "ddagrab") ? "yes" : "no"}`);
  console.log(`chosen: ${chooseEncoder(ff.path) || "(none)"}`);
  return 0;
}

function sha256(path) {
  return createHash("sha256").update(readFileSync(path)).digest("hex");
}

// curl args for one download attempt. --fail keeps an HTTP error page (a
// GitHub 504 is 92 bytes of HTML) out of the zip; the octet-stream header is
// what the API asset URL needs to redirect to the bytes.
export function curlArgs(url, out) {
  return ["-fsSL", "--retry", "3", "--retry-all-errors", "--retry-delay", "2", "-H", "Accept: application/octet-stream", "-o", out, url];
}

// A zip in hand is only used when it is exactly the pinned build.
function zipOk(path) {
  return existsSync(path) && statSync(path).size === FFMPEG_BUILD.bytes && sha256(path) === FFMPEG_BUILD.sha256;
}

function downloadPinned(zip) {
  const part = `${zip}.part`;
  for (const url of FFMPEG_BUILD.urls) {
    console.log(`downloading ${url}`);
    rmSync(part, { force: true });
    const dl = spawnSync("curl.exe", curlArgs(url, part), { windowsHide: true, stdio: "inherit" });
    if (dl.status === 0 && zipOk(part)) {
      rmSync(zip, { force: true });
      renameSync(part, zip);
      return;
    }
    const got = existsSync(part) ? `${statSync(part).size} bytes, sha256 ${sha256(part)}` : `curl exit ${dl.status}`;
    console.error(`  rejected (${got}); expected ${FFMPEG_BUILD.bytes} bytes, sha256 ${FFMPEG_BUILD.sha256}`);
  }
  rmSync(part, { force: true });
  throw new Error("download failed from every pinned url");
}

function fetchFfmpeg(flags) {
  if (process.platform !== "win32") throw new Error("fetch is Windows-only; install ffmpeg from your package manager");
  mkdirSync(BIN_DIR, { recursive: true });
  const zip = flags.zip ? resolve(String(flags.zip)) : join(tmpdir(), `ffmpeg-${FFMPEG_BUILD.version}.zip`);
  if (flags.zip) {
    if (!zipOk(zip)) throw new Error(`${zip} is not the pinned build (sha256 ${existsSync(zip) ? sha256(zip) : "missing"})`);
  } else if (!zipOk(zip)) {
    // A cached zip that fails the check (an earlier error page) is replaced, not trusted.
    downloadPinned(zip);
  }
  const stage = join(tmpdir(), `film-ffmpeg-${process.pid}`);
  mkdirSync(stage, { recursive: true });
  // System32 bsdtar reads zip; a Git-for-Windows GNU tar earlier on PATH does not.
  const tar = join(process.env.SystemRoot || "C:\\Windows", "System32", "tar.exe");
  const unzip = spawnSync(tar, ["-xf", zip, "-C", stage], { windowsHide: true, stdio: "inherit" });
  if (unzip.status !== 0) throw new Error("extract failed");
  const top = readdirSync(stage).find((name) => name.startsWith("ffmpeg-"));
  for (const exe of ["ffmpeg.exe", "ffprobe.exe"]) {
    copyFileSync(join(stage, top, "bin", exe), join(BIN_DIR, exe));
  }
  console.log(`installed ffmpeg ${FFMPEG_BUILD.version} into ${BIN_DIR} (portable, git-ignored)`);
  rmSync(stage, { recursive: true, force: true });
  return 0;
}

// PR evidence. gh cannot attach a file to a PR comment, so the clip and its
// preview go up as assets of one rolling prerelease (EVIDENCE_TAG) and the
// comment embeds the GIF and links the mp4. The repo is public, so the asset
// URL renders inline for every reader without auth.
export const EVIDENCE_TAG = "film-evidence";

export function publishPlan({ clip, preview, pr, repo, title }) {
  if (!Number.isInteger(Number(pr)) || Number(pr) <= 0) throw new Error(`--pr must be a PR number: ${pr}`);
  if (!/^[\w.-]+\/[\w.-]+$/.test(repo || "")) throw new Error(`repo must be owner/name: ${repo}`);
  const base = (path) => path.replace(/^.*[\\/]/, "");
  const assetName = (path) => `pr${pr}-${base(path)}`;
  const url = (path) => `https://github.com/${repo}/releases/download/${EVIDENCE_TAG}/${assetName(path)}`;
  const files = [clip, preview].filter(Boolean);
  const body = [
    `**Film:** ${title || base(clip)}`,
    "",
    preview ? `![${title || "clip"}](${url(preview)})` : "",
    "",
    `Full clip: [${assetName(clip)}](${url(clip)})`,
  ]
    .filter((line, i, all) => line !== "" || all[i - 1] !== "")
    .join("\n");
  return {
    files: files.map((path) => ({ path, asset: assetName(path) })),
    ensureRelease: ["release", "view", EVIDENCE_TAG, "--repo", repo],
    createRelease: ["release", "create", EVIDENCE_TAG, "--repo", repo, "--prerelease", "--title", "Film evidence", "--notes", "Clips recorded by tools/film for PR evidence. Assets are named pr<N>-<clip>."],
    upload: (staged) => ["release", "upload", EVIDENCE_TAG, ...staged, "--repo", repo, "--clobber"],
    comment: ["pr", "comment", String(pr), "--repo", repo, "--body", body],
    body,
  };
}

function publish(flags) {
  const clip = flags._[1] ? resolve(flags._[1]) : "";
  if (!clip || !existsSync(clip)) throw new Error("publish <clip.mp4> --pr <N> [--repo owner/name] [--title text] [--yes]");
  const guess = clip.replace(/\.mp4$/i, ".preview.gif");
  const preview = flags.preview ? resolve(String(flags.preview)) : existsSync(guess) ? guess : "";
  let repo = flags.repo;
  if (!repo) {
    const view = spawnSync("gh", ["repo", "view", "--json", "nameWithOwner", "-q", ".nameWithOwner"], { windowsHide: true, encoding: "utf8" });
    repo = (view.stdout || "").trim();
  }
  const plan = publishPlan({ clip, preview, pr: flags.pr, repo, title: flags.title });
  for (const file of plan.files) {
    const bytes = statSync(file.path).size;
    const budget = file.path === clip ? BUDGET.mp4Bytes : BUDGET.previewBytes;
    if (bytes > budget) throw new Error(`${file.path} is ${(bytes / 1048576).toFixed(1)} MB, over the ${(budget / 1048576).toFixed(0)} MB budget`);
  }
  if (!flags.yes) {
    console.log("dry run (pass --yes to upload and comment):");
    for (const file of plan.files) console.log(`  upload ${file.path} as ${file.asset}`);
    console.log(`  comment on PR #${flags.pr}:\n${plan.body.replace(/^/gm, "    ")}`);
    return 0;
  }
  const stage = join(tmpdir(), `film-publish-${process.pid}`);
  mkdirSync(stage, { recursive: true });
  const staged = plan.files.map((file) => {
    const target = join(stage, file.asset);
    copyFileSync(file.path, target);
    return target;
  });
  const gh = (args) => {
    const run = spawnSync("gh", args, { windowsHide: true, stdio: "inherit" });
    return run.status === 0;
  };
  if (spawnSync("gh", plan.ensureRelease, { windowsHide: true, stdio: "ignore" }).status !== 0) {
    if (!gh(plan.createRelease)) throw new Error("could not create the evidence release");
  }
  if (!gh(plan.upload(staged))) throw new Error("upload failed");
  rmSync(stage, { recursive: true, force: true });
  if (!gh(plan.comment)) throw new Error("PR comment failed");
  return 0;
}

function selftest() {
  const assert = (cond, msg) => {
    if (!cond) throw new Error(msg);
  };
  const region = parseRegion("10,20,641,361");
  assert(region.w === 640 && region.h === 360, "region rounds to even size");
  let threw = false;
  try {
    parseRegion("1,2,3");
  } catch {
    threw = true;
  }
  assert(threw, "short region must throw");

  const list = " V....D h264_amf  AMD AMF\n V....D libx264  x264\n V....D h264_nvenc  NVIDIA\n";
  assert(pickEncoder(list) === "h264_nvenc", "nvenc wins when built");
  assert(pickEncoder(" V....D libx264  x264\n") === "libx264", "software fallback");
  assert(pickEncoder(" V....D mpeg4  x\n") === "", "no h264 encoder -> empty");

  const nv = buildRecordArgs({ source: { kind: "region", x: 0, y: 0, w: 1920, h: 1080 }, method: "ddagrab", encoder: "h264_nvenc", fps: 30, seconds: 5, out: "a.mp4" });
  const nvText = nv.join(" ");
  assert(nvText.includes("ddagrab=output_idx=0:framerate=30:draw_mouse=0:offset_x=0:offset_y=0:video_size=1920x1080"), nvText);
  assert(!nvText.includes("hwdownload"), "nvenc takes d3d11 frames directly");
  assert(nvText.includes("-t 5") && nvText.endsWith("+faststart a.mp4"), nvText);

  const sw = buildRecordArgs({ source: { kind: "region", x: 5, y: 6, w: 640, h: 360 }, method: "ddagrab", encoder: "libx264", fps: 30, out: "b.mp4" }).join(" ");
  assert(sw.includes("-vf hwdownload,format=bgra") && sw.includes("-pix_fmt yuv420p"), sw);
  assert(!sw.includes(" -t "), "open-ended record has no -t");

  const gw = buildRecordArgs({ source: { kind: "window", title: "Arma Reforger" }, method: "gdigrab", encoder: "libx264", fps: 24, seconds: 3, out: "c.mp4" });
  assert(gw.includes("title=Arma Reforger") && gw.join(" ").includes("crop=trunc(iw/2)*2"), gw.join(" "));
  threw = false;
  try {
    captureInputArgs({ kind: "window", title: "x" }, "ddagrab", 30);
  } catch {
    threw = true;
  }
  assert(threw, "ddagrab refuses an unresolved window");

  const gif = buildPreviewArgs("in.mp4", "in.preview.gif").join(" ");
  assert(gif.includes("palettegen") && gif.includes("paletteuse") && gif.endsWith("in.preview.gif"), gif);
  assert(stampName("flow world-atm", new Date("2026-11-07T10:11:12Z")) === "flow_world-atm-20261107-101112", "stamp name");

  const plan = publishPlan({ clip: "C:\\x\\atm.mp4", preview: "C:\\x\\atm.preview.gif", pr: 75, repo: "Life-Framework/Life-Framework", title: "ATM deposit" });
  const gifUrl = "https://github.com/Life-Framework/Life-Framework/releases/download/film-evidence/pr75-atm.preview.gif";
  assert(plan.body.includes(`![ATM deposit](${gifUrl})`), plan.body);
  assert(plan.body.includes("pr75-atm.mp4"), plan.body);
  assert(plan.files.map((f) => f.asset).join(",") === "pr75-atm.mp4,pr75-atm.preview.gif", "asset names carry the PR number");
  assert(plan.upload(["a"]).includes("--clobber"), "re-recording a clip replaces the asset");
  threw = false;
  try {
    publishPlan({ clip: "a.mp4", pr: "abc", repo: "a/b" });
  } catch {
    threw = true;
  }
  assert(threw, "publish needs a PR number");
  const dlArgs = curlArgs(FFMPEG_BUILD.urls[0], "z.part");
  assert(dlArgs.includes("-fsSL") && dlArgs.includes("--retry"), "fetch fails on an HTTP error page instead of saving it");
  assert(FFMPEG_BUILD.urls.length >= 2 && FFMPEG_BUILD.urls.every((u) => u.startsWith("https://")), "fetch has a pinned fallback url");
  console.log("film selftest ok");
  return 0;
}

function usage() {
  console.log(readFileSync(fileURLToPath(import.meta.url), "utf8").split("\n").slice(1, 13).map((l) => l.replace(/^\/\/ ?/, "")).join("\n"));
}

export async function main(argv) {
  const flags = parseFlags(argv);
  const cmd = flags._[0];
  try {
    if (cmd === "selftest") return selftest();
    if (cmd === "probe") return probe();
    if (cmd === "fetch") return fetchFfmpeg(flags);
    if (cmd === "record") return record(flags);
    if (cmd === "flow") return await filmFlow(flags);
    if (cmd === "publish") return publish(flags);
    usage();
    return cmd ? 1 : 0;
  } catch (err) {
    console.error(err.message);
    return 1;
  }
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  main(process.argv.slice(2)).then((code) => process.exit(code));
}

