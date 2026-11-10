// In-engine recording: the game client screenshots itself at a fixed rate
// (LF_FilmRecorder, harness verb `film`) and this module turns the BMP
// sequence into an mp4. It works on the hidden LF-Agents desktop and on an
// unfocused window, where screen capture (ddagrab, gdigrab) sees nothing new.
//
// The engine writes $logs:el-film/<name>/f<n>.bmp and, on stop, index.txt:
//   # fps=12
//   f1 0
//   f2 84
//   ...
//   # shots=240 seconds=20.1 fps=11.9 gameFps=58.0 frameMs=17.2 shotFrameMs=24.0 maxGapMs=71 stalls=0 failed=0
// Each frame is held until the next one's real time, so the clip plays at
// true speed even where the engine missed a slot.

import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, rmSync, statSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { BUDGET, OUT_DIR, chooseEncoder, encoderArgs, makePreview, resolveFfmpeg, stampName } from "./film.mjs";

// 15 fps at 1280x720 is the measured default (agent-box/README.md, Filming).
export const ENGINE_FPS = 15;
export const ENGINE_MAX_SECONDS = 300;

// ---------------------------------------------------------------- pure parts

export function parseSummary(text) {
  const out = {};
  for (const part of String(text || "").trim().split(/\s+/)) {
    const eq = part.indexOf("=");
    if (eq > 0) out[part.slice(0, eq)] = Number(part.slice(eq + 1));
  }
  return out;
}

export function parseIndex(text) {
  const lines = String(text || "").split(/\r?\n/).map((l) => l.trim()).filter(Boolean);
  const head = lines.find((l) => l.startsWith("# fps="));
  const tail = [...lines].reverse().find((l) => l.startsWith("# shots="));
  const frames = [];
  for (const line of lines) {
    if (line.startsWith("#")) continue;
    const [name, ms] = line.split(/\s+/);
    if (/^f\d+$/.test(name) && Number.isFinite(Number(ms))) frames.push({ name, ms: Number(ms) });
  }
  const slates = lines.filter((l) => l.startsWith("# slate=")).map((l) => Number(l.slice(8)));
  return { fps: head ? Number(head.slice(6)) : 0, frames, slates, summary: parseSummary(tail ? tail.slice(2) : ""), summaryText: tail ? tail.slice(2) : "" };
}

// concat demuxer script: every frame shows until the next frame's time. The
// last frame is listed twice (the demuxer ignores the last duration otherwise).
export function concatList(frames, endMs, fps = ENGINE_FPS) {
  if (!frames.length) throw new Error("no frames to encode");
  const out = ["ffconcat version 1.0"];
  const step = 1000 / (fps || ENGINE_FPS);
  frames.forEach((f, i) => {
    const next = i + 1 < frames.length ? frames[i + 1].ms : Math.max(f.ms + step, endMs || 0);
    out.push(`file '${f.name}.bmp'`, `duration ${((next - f.ms) / 1000).toFixed(3)}`);
  });
  out.push(`file '${frames[frames.length - 1].name}.bmp'`);
  return out.join("\n") + "\n";
}

export function buildEncodeArgs({ list, out, fps = ENGINE_FPS, encoder }) {
  return [
    "-hide_banner", "-nostats", "-loglevel", "error", "-y",
    "-f", "concat", "-safe", "0", "-i", list,
    "-fps_mode", "cfr", "-r", String(fps),
    "-vf", "scale=trunc(iw/2)*2:trunc(ih/2)*2",
    ...encoderArgs(encoder), "-pix_fmt", "yuv420p", "-an", "-movflags", "+faststart", out,
  ];
}

export function buildJoinArgs(list, out) {
  return ["-hide_banner", "-nostats", "-loglevel", "error", "-y", "-f", "concat", "-safe", "0", "-i", list, "-c", "copy", "-movflags", "+faststart", out];
}

// Frames whose BMP landed whole. MakeScreenshot writes asynchronously, and a
// frame cut off by stop can be short; every whole frame has the same size.
export function wholeFrames(frames, sizeOf) {
  const sizes = frames.map((f) => sizeOf(f.name));
  const counts = new Map();
  for (const s of sizes) if (s > 0) counts.set(s, (counts.get(s) || 0) + 1);
  let full = 0;
  for (const [s, c] of counts) if (!full || c > counts.get(full)) full = s;
  return frames.filter((_, i) => sizes[i] === full && full > 0);
}

// ------------------------------------------------------------- environment

function sleepSync(ms) {
  Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
}

function bmpSize(dir) {
  return (name) => {
    const p = join(dir, `${name}.bmp`);
    return existsSync(p) ? statSync(p).size : 0;
  };
}

// Encode one recording directory (holds index.txt and the BMPs) into an mp4
// with a GIF preview. The BMPs are deleted after a good encode unless keep.
export function encodeFrames({ dir, out, fps, encoder, keep = false, label = "engine" }) {
  const ff = resolveFfmpeg();
  if (!ff) return { ok: false, error: "ffmpeg not found. Run: node tools/film/film.mjs fetch" };
  const indexPath = join(dir, "index.txt");
  if (!existsSync(indexPath)) return { ok: false, error: `no index.txt in ${dir}` };
  const index = parseIndex(readFileSync(indexPath, "utf8"));
  if (!index.frames.length) return { ok: false, error: "the recording holds no frames", summary: index.summary };
  // The last shots land a moment after stop.
  const last = index.frames[index.frames.length - 1].name;
  for (let waited = 0; waited < 5000 && !existsSync(join(dir, `${last}.bmp`)); waited += 250) sleepSync(250);
  sleepSync(500);
  const frames = wholeFrames(index.frames, bmpSize(dir));
  if (!frames.length) return { ok: false, error: "no frame landed on disk", summary: index.summary };
  const rate = Number(fps || index.fps || ENGINE_FPS);
  const list = join(dir, "frames.ffconcat");
  writeFileSync(list, concatList(frames, (index.summary.seconds || 0) * 1000, rate));
  const enc = encoder || chooseEncoder(ff.path);
  if (!enc) return { ok: false, error: "no working H.264 encoder" };
  mkdirSync(OUT_DIR, { recursive: true });
  const target = resolve(out || join(OUT_DIR, stampName(label) + ".mp4"));
  mkdirSync(dirname(target), { recursive: true });
  const started = Date.now();
  const run = spawnSync(ff.path, buildEncodeArgs({ list, out: target, fps: rate, encoder: enc }), { windowsHide: true, encoding: "utf8", cwd: dir });
  const encodeMs = Date.now() - started;
  if (run.status !== 0 || !existsSync(target)) return { ok: false, error: (run.stderr || "ffmpeg failed").trim().slice(0, 400), summary: index.summary };
  let preview = "";
  try {
    preview = makePreview(ff.path, target);
  } catch {}
  if (!keep) for (const f of index.frames) rmSync(join(dir, `${f.name}.bmp`), { force: true });
  const sidecar = target.replace(/\.mp4$/i, ".json");
  const result = {
    ok: true,
    out: target,
    preview,
    encoder: enc,
    fps: rate,
    frames: frames.length,
    missing: index.frames.length - frames.length,
    encodeMs,
    bytes: statSync(target).size,
    overBudget: statSync(target).size > BUDGET.mp4Bytes,
    summary: index.summary,
    summaryText: index.summaryText,
  };
  writeFileSync(sidecar, JSON.stringify({ source: dir, ...result }, null, 2) + "\n");
  return result;
}

// Join same-encoder clips (journey steps) into one mp4 without re-encoding.
export function joinClips(clips, out) {
  const ff = resolveFfmpeg();
  if (!ff || !clips.length) return { ok: false, error: "nothing to join" };
  const list = out.replace(/\.mp4$/i, ".join.ffconcat");
  writeFileSync(list, "ffconcat version 1.0\n" + clips.map((c) => `file '${c.replaceAll("\\", "/").replaceAll("'", "'\\''")}'`).join("\n") + "\n");
  const run = spawnSync(ff.path, buildJoinArgs(list, out), { windowsHide: true, encoding: "utf8" });
  rmSync(list, { force: true });
  if (run.status !== 0 || !existsSync(out)) return { ok: false, error: (run.stderr || "join failed").trim().slice(0, 400) };
  let preview = "";
  try {
    preview = makePreview(ff.path, out);
  } catch {}
  return { ok: true, out, preview, bytes: statSync(out).size };
}

// --------------------------------------------------------------- controller

// send(spec, timeoutMs) is agent-box/harness.mjs sendCommand aimed at one
// client; resolve(alias) maps "$logs:el-film/<name>/index.txt" to a file.
export function engineStart(send, name, { fps = ENGINE_FPS, seconds = ENGINE_MAX_SECONDS } = {}) {
  const r = send({ verb: "film", mode: "start", name, fps, seconds }, 8000);
  if (r.status !== "ok") return { ok: false, error: `${r.status} ${r.detail}`.trim() };
  return { ok: true, alias: String(r.detail).trim().split(/\s+/)[0] };
}

export function engineStop(send, resolveAlias, { out, label, keep, fps } = {}) {
  const r = send({ verb: "film", mode: "stop" }, 8000);
  if (r.status !== "ok") return { ok: false, error: `${r.status} ${r.detail}`.trim() };
  const [alias, ...rest] = String(r.detail).trim().split(/\s+/);
  const summary = parseSummary(rest.join(" "));
  // index.txt is written by stop, synchronously, before the result line.
  let index = "";
  for (let waited = 0; waited < 5000 && !(index = resolveAlias(`${alias}/index.txt`)); waited += 250) sleepSync(250);
  if (!index) return { ok: false, error: `index.txt for ${alias} not found`, summary };
  return { ...encodeFrames({ dir: dirname(index), out, label, keep, fps }), alias, summary };
}

export function describe(clip) {
  if (!clip?.ok) return `film failed: ${clip?.error || "unknown"}${clip?.summary ? ` (${JSON.stringify(clip.summary)})` : ""}`;
  const s = clip.summary || {};
  return `${clip.out} (${(clip.bytes / 1048576).toFixed(1)} MB, ${clip.frames} frames, ${s.seconds}s, capture ${s.fps} fps, game ${s.gameFps} fps, frame ${s.frameMs} ms / ${s.shotFrameMs} ms after a shot, max gap ${s.maxGapMs} ms, stalls ${s.stalls}, encode ${clip.encodeMs} ms)`;
}

export function selftestEngine() {
  const fails = [];
  const idx = parseIndex("# fps=12\r\nf1 0\r\nf2 90\r\nf4 260\r\n# shots=4 seconds=0.4 fps=10 gameFps=60 frameMs=16.7 shotFrameMs=20 maxGapMs=40 stalls=0 failed=1\r\n");
  if (idx.fps !== 12 || idx.frames.length !== 3 || idx.frames[2].ms !== 260) fails.push(`parseIndex: ${JSON.stringify(idx)}`);
  if (idx.summary.gameFps !== 60 || idx.summary.failed !== 1) fails.push(`parseSummary: ${JSON.stringify(idx.summary)}`);
  const list = concatList(idx.frames, 400, 12);
  if (!list.includes("file 'f1.bmp'\nduration 0.090") || !list.includes("file 'f4.bmp'\nduration 0.140") || !list.trimEnd().endsWith("file 'f4.bmp'")) fails.push(`concatList:\n${list}`);
  try {
    concatList([], 0);
    fails.push("concatList accepted no frames");
  } catch {}
  const args = buildEncodeArgs({ list: "x.ffconcat", out: "o.mp4", fps: 12, encoder: "h264_nvenc" }).join(" ");
  if (!args.includes("-f concat -safe 0 -i x.ffconcat") || !args.includes("-fps_mode cfr -r 12") || !args.includes("h264_nvenc") || !args.endsWith("o.mp4")) fails.push(`encode args: ${args}`);
  const sizes = { f1: 100, f2: 100, f3: 40, f4: 0 };
  const whole = wholeFrames([{ name: "f1" }, { name: "f2" }, { name: "f3" }, { name: "f4" }], (n) => sizes[n]);
  if (whole.map((f) => f.name).join() !== "f1,f2") fails.push(`wholeFrames kept ${whole.map((f) => f.name).join()}`);
  const sent = [];
  const send = (spec) => {
    sent.push(spec);
    return spec.mode === "start" ? { status: "ok", detail: "$logs:el-film/j_atm" } : { status: "error", detail: "not_recording" };
  };
  const start = engineStart(send, "j_atm", { fps: 12, seconds: 60 });
  if (!start.ok || start.alias !== "$logs:el-film/j_atm" || sent[0].fps !== 12) fails.push(`engineStart: ${JSON.stringify(start)}`);
  const stop = engineStop(send, () => "");
  if (stop.ok || !stop.error.includes("not_recording")) fails.push(`engineStop must surface the engine error: ${JSON.stringify(stop)}`);
  return fails;
}
