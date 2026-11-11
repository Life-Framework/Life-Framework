// Post-production for the film pipeline (#397): turns a storyboard JSON plus footage into one mp4 with title and end
// cards, crossfades, burned-in captions (and an .srt beside it), offline voice-over, and a music bed that ducks under
// game sound and voice. One command:
//   tools\cli film-make video/<name>.json [--out file.mp4] [--keep] [--no-vo]
//   tools\cli film-make --setup          (fetch Piper + the voice into tools/film/.tts, see docs/setup.md)
//   tools\cli film-make --selftest
//
// Storyboard (paths are repo-relative or absolute; see video/README.md):
//   { "title", "size":[1920,1080], "fps":30, "transition":0.6,
//     "titleCard":{ "lines":[...], "seconds":4 }, "endCard":{ "lines":[...], "seconds":5 },
//     "clips":[{ "src", "start":0, "seconds":8, "audio":true, "captions":"cues.txt" | [[from,to,"text"]], "vo":"cues.txt" | [[at,"text"]] }],
//     "music":{ "file", "gainDb":-14, "duckDb":12, "fadeIn":2, "fadeOut":3 }, "voice":{ "speaker":20 } }
// Cue files are plain text, one cue a line: captions "from to text", voice-over "at text" (seconds from the start of the
// take; "#" starts a comment). Caption and voice lines in one file may share lines: a cue with a trailing "|vo" is also spoken.

import { spawnSync } from "node:child_process";
import { copyFileSync, existsSync, mkdirSync, readFileSync, readdirSync, rmSync, statSync, writeFileSync } from "node:fs";
import { dirname, isAbsolute, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { OUT_DIR, resolveFfmpeg } from "./film.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
export const TTS_DIR = join(ROOT, "tools", "film", ".tts");
const FONT_CANDIDATES = ["C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"];
const FONT_FAMILY = { "segoeui.ttf": "Segoe UI", "arial.ttf": "Arial", "DejaVuSans.ttf": "DejaVu Sans" };
const LOGO = join(ROOT, "website", "public", "brand", "logo-1024.png");
const BG = "0x0a0d12";

// ---------------------------------------------------------------- pure parts

// "from to text" lines. kind "vo" reads "at text". Throws with the line number on a malformed line.
export function parseCues(text, kind = "caption") {
  const out = [];
  String(text || "").split(/\r?\n/).forEach((raw, i) => {
    const line = raw.replace(/#.*$/, "").trim();
    if (!line) return;
    const re = kind === "vo" ? /^(\d+(?:\.\d+)?)\s+(.+)$/ : /^(\d+(?:\.\d+)?)\s+(\d+(?:\.\d+)?)\s+(.+)$/;
    const m = re.exec(line);
    if (!m) throw new Error(`cue line ${i + 1} is not "${kind === "vo" ? "at text" : "from to text"}": ${raw}`);
    if (kind === "vo") out.push({ at: Number(m[1]), text: m[2].trim() });
    else {
      const cue = { from: Number(m[1]), to: Number(m[2]), text: m[3].trim() };
      if (cue.to <= cue.from) throw new Error(`cue line ${i + 1}: end ${cue.to} is not after start ${cue.from}`);
      out.push(cue);
    }
  });
  return out;
}

export function normalizeCues(spec, kind, baseDir) {
  if (!spec) return [];
  if (typeof spec === "string") return parseCues(readFileSync(existsSync(abs(spec)) ? abs(spec) : abs(spec, baseDir), "utf8"), kind);
  return spec.map((c) => (kind === "vo" ? { at: Number(c[0]), text: String(c[1]) } : { from: Number(c[0]), to: Number(c[1]), text: String(c[2]) }));
}

function abs(p, base = ROOT) {
  return isAbsolute(p) ? p : resolve(base, p);
}

// Validates a storyboard and names the field that is wrong.
export function validateBoard(b) {
  const bad = (f, why) => {
    throw new Error(`storyboard ${f}: ${why}`);
  };
  if (!b || typeof b !== "object") bad("(root)", "not an object");
  if (!Array.isArray(b.clips) || !b.clips.length) bad("clips", "needs at least one clip");
  b.clips.forEach((c, i) => {
    if (!c.src) bad(`clips[${i}].src`, "missing");
    if (!(c.seconds > 0)) bad(`clips[${i}].seconds`, "must be a positive number");
    if (c.start !== undefined && !(c.start >= 0)) bad(`clips[${i}].start`, "must be >= 0");
  });
  const t = b.transition === undefined ? 0.6 : b.transition;
  if (!(t >= 0)) bad("transition", "must be >= 0");
  const shortest = Math.min(...segmentsOf(b).map((s) => s.seconds));
  if (t * 2 > shortest) bad("transition", `${t}s crossfade needs every segment to be at least ${t * 2}s (shortest is ${shortest}s)`);
  if (b.size && !(b.size.length === 2 && b.size.every((n) => n > 0 && n % 2 === 0))) bad("size", "must be [width, height], both even");
  return true;
}

export function segmentsOf(b) {
  const segs = [];
  if (b.titleCard) segs.push({ kind: "title", seconds: b.titleCard.seconds || 4, lines: b.titleCard.lines || [b.title || "Life Framework"] });
  b.clips.forEach((c, i) => segs.push({ kind: "clip", seconds: c.seconds, clip: c, index: i }));
  if (b.endCard) segs.push({ kind: "end", seconds: b.endCard.seconds || 5, lines: b.endCard.lines || [] });
  return segs;
}

// Start time of each segment on the final timeline, and the total length. Crossfades overlap neighbours by `t`.
export function timeline(segs, t) {
  let at = 0;
  const starts = segs.map((s, i) => {
    const start = i === 0 ? 0 : at;
    at = start + s.seconds - t;
    return start;
  });
  const last = segs.length - 1;
  return { starts, total: starts[last] + segs[last].seconds };
}

export function fmtSrt(sec) {
  const ms = Math.round(sec * 1000);
  const p = (n, w) => String(n).padStart(w, "0");
  return `${p(Math.floor(ms / 3600000), 2)}:${p(Math.floor(ms / 60000) % 60, 2)}:${p(Math.floor(ms / 1000) % 60, 2)},${p(ms % 1000, 3)}`;
}

export function fmtAss(sec) {
  const cs = Math.round(sec * 100);
  const p = (n, w) => String(n).padStart(w, "0");
  return `${Math.floor(cs / 360000)}:${p(Math.floor(cs / 6000) % 60, 2)}:${p(Math.floor(cs / 100) % 60, 2)}.${p(cs % 100, 2)}`;
}

// Final-timeline caption cues from each clip's take-relative cues. A cue is kept inside its clip's visible window.
export function timelineCues(segs, starts, t, baseDir) {
  const out = [];
  segs.forEach((s, i) => {
    if (s.kind !== "clip") return;
    const cues = normalizeCues(s.clip.captions, "caption", baseDir);
    const end = starts[i] + s.seconds - (i < segs.length - 1 ? t : 0);
    for (const c of cues) {
      const from = starts[i] + c.from;
      const to = Math.min(starts[i] + c.to, end);
      if (to > from) out.push({ from, to, text: c.text });
    }
  });
  return out.sort((a, b) => a.from - b.from);
}

export function toSrt(cues) {
  return cues.map((c, i) => `${i + 1}\n${fmtSrt(c.from)} --> ${fmtSrt(c.to)}\n${c.text}\n`).join("\n");
}

// Lower third, sans-serif, white on a semi-transparent black box (BorderStyle 3 draws the box from the outline colour).
export function toAss(cues, { w, h, family }) {
  const fs = Math.round(h * 0.056);
  const head = [
    "[Script Info]",
    "ScriptType: v4.00+",
    `PlayResX: ${w}`,
    `PlayResY: ${h}`,
    "WrapStyle: 0",
    "",
    "[V4+ Styles]",
    "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding",
    `Style: Lower,${family},${fs},&H00FFFFFF,&H00FFFFFF,&H8C000000,&H00000000,0,0,0,0,100,100,0,0,3,${Math.round(h * 0.014)},0,2,${Math.round(w * 0.08)},${Math.round(w * 0.08)},${Math.round(h * 0.085)},1`,
    "",
    "[Events]",
    "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text",
  ];
  const ev = cues.map((c) => `Dialogue: 0,${fmtAss(c.from)},${fmtAss(c.to)},Lower,,0,0,0,,${c.text.replace(/\n/g, "\\N").replace(/[{}]/g, "")}`);
  return [...head, ...ev, ""].join("\n");
}

// Voice-over lines on the final timeline: { at (s), text }.
export function timelineVo(segs, starts, baseDir) {
  const out = [];
  segs.forEach((s, i) => {
    if (s.kind !== "clip") return;
    for (const v of normalizeCues(s.clip.vo, "vo", baseDir)) out.push({ at: starts[i] + v.at, text: v.text });
  });
  return out;
}

// Two-pass level: pass 1 measures integrated loudness (loudnorm in print mode); pass 2 applies the gain to reach -14 LUFS
// and a limiter holds the true peak under -1.5 dB (loudnorm's own linear mode gives up when the peaks would clip).
export function loudnormArgs(m) {
  if (!m) return "loudnorm=I=-14:TP=-1.5:LRA=11:print_format=json";
  return `volume=${(-14 - Number(m.input_i)).toFixed(2)}dB,alimiter=limit=0.84:level=false`;
}

export function parseLoudnorm(stderr) {
  const m = /\{[^{}]*"input_i"[^{}]*\}/.exec(stderr || "");
  if (!m) return null;
  const j = JSON.parse(m[0]);
  return Number.isFinite(Number(j.input_i)) ? j : null;
}

// The one filter graph. Inputs, in order: one per segment (cards: lavfi colour, then the logo), clip audio is part of
// the clip input, then music, then one wav per voice line. `segInputs` tells where each segment's video input is.
export function buildGraph({ segs, starts, total, t, size, fps, music, vo, hasAudio, cardFont, cardTexts, assName, fontsDir, measured = null, audioOnly = false }) {
  const [w, h] = size;
  const f = [];
  const aOut = [];
  segs.forEach((s, i) => {
    const v = `v${i}`;
    const a = `a${i}`;
    if (s.kind === "clip") {
      f.push(`[${s.in}:v]trim=start=${s.clip.start || 0}:duration=${s.seconds},setpts=PTS-STARTPTS,fps=${fps},scale=${w}:${h}:flags=lanczos,setsar=1,format=yuv420p[${v}]`);
      if (hasAudio[i]) f.push(`[${s.in}:a]atrim=start=${s.clip.start || 0}:duration=${s.seconds},asetpts=PTS-STARTPTS,aresample=48000,aformat=channel_layouts=stereo,apad,atrim=duration=${s.seconds}[${a}]`);
      else f.push(`anullsrc=r=48000:cl=stereo,atrim=duration=${s.seconds}[${a}]`);
    } else {
      const sz = Math.round(h * 0.062);
      const lines = cardTexts[i];
      const draws = lines
        .map((file, k) => `drawtext=fontfile=${cardFont}:textfile=${file}:fontcolor=white:fontsize=${k === 0 ? sz : Math.round(sz * 0.62)}:x=(w-text_w)/2:y=${Math.round(h * 0.7) + k * Math.round(sz * 1.35)}`)
        .join(",");
      const fadeOut = s.kind === "end" ? `,fade=t=out:st=${s.seconds - 0.8}:d=0.8` : "";
      const fadeIn = s.kind === "title" ? ",fade=t=in:st=0:d=0.8" : "";
      f.push(`[${s.in}:v]scale=-2:${Math.round(h * 0.42)}[logo${i}]`);
      f.push(`color=c=${BG}:s=${w}x${h}:r=${fps}:d=${s.seconds}[bg${i}]`);
      f.push(`[bg${i}][logo${i}]overlay=(W-w)/2:${Math.round(h * 0.1)},${draws},format=yuv420p${fadeIn}${fadeOut}[${v}]`);
      f.push(`anullsrc=r=48000:cl=stereo,atrim=duration=${s.seconds}[${a}]`);
    }
  });
  // crossfade chain
  let vPrev = "v0";
  let aPrev = "a0";
  let at = segs[0].seconds - t;
  for (let i = 1; i < segs.length; i++) {
    const last = i === segs.length - 1;
    const vn = last ? "vx" : `vc${i}`;
    const an = last ? "ax" : `ac${i}`;
    if (t > 0) {
      f.push(`[${vPrev}][v${i}]xfade=transition=fade:duration=${t}:offset=${at.toFixed(3)}[${vn}]`);
      f.push(`[${aPrev}][a${i}]acrossfade=d=${t}[${an}]`);
    } else {
      f.push(`[${vPrev}][v${i}]concat=n=2:v=1:a=0[${vn}]`);
      f.push(`[${aPrev}][a${i}]concat=n=2:v=0:a=1[${an}]`);
    }
    vPrev = vn;
    aPrev = an;
    at += segs[i].seconds - t;
  }
  if (segs.length === 1) {
    f.push("[v0]null[vx]");
    f.push("[a0]anull[ax]");
  }
  if (audioOnly) f.push("[vx]nullsink");
  else f.push(`[vx]subtitles=${assName}:fontsdir=${fontsDir}[vout]`);
  // audio: game, voice, music
  const keyParts = ["[game]"];
  f.push("[ax]asplit=2[game][gamemix]");
  let voLabel = null;
  if (vo.length) {
    const labels = vo.map((v, k) => {
      f.push(`[${v.in}:a]aresample=48000,aformat=channel_layouts=stereo,adelay=${Math.round(v.at * 1000)}:all=1,apad=whole_dur=${total}[vo${k}]`);
      return `[vo${k}]`;
    });
    f.push(`${labels.join("")}amix=inputs=${labels.length}:normalize=0:duration=longest,volume=1.0[vo]`);
    f.push("[vo]asplit=2[vokey][vomix]");
    voLabel = "[vomix]";
    keyParts.push("[vokey]");
  }
  const gainDb = music ? music.gainDb : 0;
  let mixIns = "[gamemix]";
  if (music) {
    const fin = music.fadeIn ?? 2;
    const fout = music.fadeOut ?? 3;
    f.push(
      `[${music.in}:a]aresample=48000,aformat=channel_layouts=stereo,atrim=duration=${total},asetpts=PTS-STARTPTS,volume=${gainDb}dB,afade=t=in:st=0:d=${fin},afade=t=out:st=${(total - fout).toFixed(3)}:d=${fout}[mus]`,
    );
    f.push(`${keyParts.join("")}amix=inputs=${keyParts.length}:normalize=0[key]`);
    f.push(`[mus][key]sidechaincompress=threshold=0.02:ratio=8:attack=20:release=400:makeup=1[musduck]`);
    mixIns += "[musduck]";
  } else {
    f.push(`${keyParts.slice(0, 1).join("")}anullsink`);
  }
  let n = music ? 2 : 1;
  if (voLabel) {
    mixIns += voLabel;
    n++;
  }
  f.push(`${mixIns}amix=inputs=${n}:normalize=0:duration=first,acompressor=threshold=0.125:ratio=3:attack=5:release=150:makeup=1,${loudnormArgs(measured)},afade=t=out:st=${(total - 0.5).toFixed(3)}:d=0.5[aout]`);
  return f.join(";\n");
}

// ---------------------------------------------------------------- impure parts

function run(cmd, args, opts = {}) {
  const r = spawnSync(cmd, args, { encoding: "utf8", windowsHide: true, maxBuffer: 64 * 1024 * 1024, ...opts });
  return r;
}

function probeAudio(ff, file) {
  const r = run(ff.path, ["-hide_banner", "-i", file]);
  return /Stream #.*Audio:/.test(r.stderr || "");
}

function piperPath() {
  const p = join(TTS_DIR, "piper", process.platform === "win32" ? "piper.exe" : "piper");
  return existsSync(p) ? p : null;
}

export function synthVo(text, wav, { speaker = 20, model = "en_US-libritts_r-medium.onnx" } = {}) {
  const piper = piperPath();
  if (!piper) throw new Error("Piper not installed. Run: tools\\cli film-make --setup");
  const r = run(piper, ["-m", join(TTS_DIR, model), "-s", String(speaker), "--length_scale", "1.08", "-f", wav], { input: text + "\n", cwd: join(TTS_DIR, "piper") });
  if (r.status !== 0 || !existsSync(wav)) throw new Error(`piper failed: ${(r.stderr || "").trim().slice(-300)}`);
}

export function setup() {
  const dl = (url, out) => {
    const r = run("curl", ["-sSL", "--fail", "-o", out, url]);
    if (r.status !== 0) throw new Error(`download failed ${url}: ${r.stderr}`);
  };
  mkdirSync(TTS_DIR, { recursive: true });
  if (!piperPath()) {
    const zip = join(TTS_DIR, "piper.zip");
    dl("https://github.com/rhasspy/piper/releases/download/2023.11.14-2/piper_windows_amd64.zip", zip);
    // Windows' own bsdtar: from Git Bash, PATH finds GNU tar first, which reads "C:\..." as a remote host and fails.
    const sysTar = join(process.env.SystemRoot || "C:\\Windows", "System32", "tar.exe");
    const r = run(process.platform === "win32" && existsSync(sysTar) ? sysTar : "tar", ["-xf", zip, "-C", TTS_DIR]);
    if (r.status !== 0) throw new Error(`unzip failed: ${r.stderr}`);
    rmSync(zip, { force: true });
  }
  const base = "https://huggingface.co/rhasspy/piper-voices/resolve/main/en/en_US/libritts_r/medium";
  for (const f of ["en_US-libritts_r-medium.onnx", "en_US-libritts_r-medium.onnx.json"]) if (!existsSync(join(TTS_DIR, f))) dl(`${base}/${f}`, join(TTS_DIR, f));
  console.log(`Piper and the en_US-libritts_r voice are in ${TTS_DIR}`);
  return 0;
}

function findFont() {
  for (const p of FONT_CANDIDATES) if (existsSync(p)) return p;
  return null;
}

export async function filmMake(argv) {
  const boardPath = argv.find((a) => !a.startsWith("--"));
  const opt = (name) => {
    const i = argv.indexOf(name);
    return i === -1 ? null : argv[i + 1];
  };
  if (!boardPath) throw new Error("usage: tools\\cli film-make <storyboard.json> [--out file.mp4] [--keep] [--no-vo] [--shot ids] [--group name] [--preview] [--client n] [--no-shoot] [--shoot-only]");
  const bp = abs(boardPath);
  const baseDir = dirname(bp);
  let board = JSON.parse(readFileSync(bp, "utf8"));
  if (board.format) {
    // lf-storyboard/1 (tools/film/storyboard.mjs): shoot the shots through the camera rig, then post-produce the takes.
    board = await (await import("./storyboard.mjs")).prepare(bp, argv);
    if (!board) return 0;
  }
  validateBoard(board);
  const ff = resolveFfmpeg();
  if (!ff) throw new Error("ffmpeg not found. Run: node tools/film/film.mjs fetch");
  const fontSrc = findFont();
  if (!fontSrc) throw new Error("no sans-serif font found (segoeui.ttf, arial.ttf)");
  const fontFile = fontSrc.split("/").pop();
  const t = board.transition === undefined ? 0.6 : board.transition;
  const size = board.size || [1920, 1080];
  const fps = board.fps || 30;
  const segs = segmentsOf(board);
  const { starts, total } = timeline(segs, t);
  const cues = timelineCues(segs, starts, t, baseDir);
  const voLines = argv.includes("--no-vo") ? [] : timelineVo(segs, starts, baseDir);

  const name = (board.name || bp.split(/[\\/]/).pop().replace(/\.json$/, "")).replace(/[^A-Za-z0-9_-]+/g, "_");
  const out = abs(opt("--out") || join(OUT_DIR, `${name}.mp4`), process.cwd());
  mkdirSync(dirname(out), { recursive: true });
  const work = join(ROOT, "tmp", `film-make-${name}`);
  rmSync(work, { recursive: true, force: true });
  mkdirSync(work, { recursive: true });
  copyFileSync(fontSrc, join(work, fontFile));
  copyFileSync(LOGO, join(work, "logo.png"));
  const srt = out.replace(/\.mp4$/i, ".srt");
  writeFileSync(srt, toSrt(cues));
  writeFileSync(join(work, "captions.ass"), toAss(cues, { w: size[0], h: size[1], family: FONT_FAMILY[fontFile] || "Arial" }));

  const inputs = [];
  const addIn = (args) => {
    inputs.push(...args);
    return inputs.filter((x) => x === "-i").length - 1;
  };
  const hasAudio = {};
  const cardTexts = {};
  segs.forEach((s, i) => {
    if (s.kind === "clip") {
      const src = abs(s.clip.src);
      if (!existsSync(src)) throw new Error(`clips[${s.index}].src not found: ${src}`);
      s.in = addIn(["-i", src]);
      hasAudio[i] = s.clip.audio !== false && probeAudio(ff, src);
    } else {
      s.in = addIn(["-loop", "1", "-t", String(s.seconds), "-i", "logo.png"]);
      cardTexts[i] = s.lines.map((line, k) => {
        const file = `card${i}_${k}.txt`;
        writeFileSync(join(work, file), line);
        return file;
      });
    }
  });
  let music = null;
  if (board.music && board.music.file) {
    const mf = abs(board.music.file);
    if (!existsSync(mf)) throw new Error(`music.file not found: ${mf}`);
    music = { ...board.music, in: addIn(["-stream_loop", "-1", "-i", mf]) };
  }
  const vo = voLines.map((v, k) => {
    const wav = join(work, `vo${k}.wav`);
    synthVo(v.text, wav, board.voice || {});
    return { ...v, in: addIn(["-i", wav]) };
  });
  const gp = { segs, starts, total, t, size, fps, music, vo, hasAudio, cardFont: fontFile, cardTexts, assName: "captions.ass", fontsDir: "." };
  writeFileSync(join(work, "graph1.txt"), buildGraph({ ...gp, audioOnly: true }));
  const pass1 = run(ff.path, ["-y", "-hide_banner", ...inputs, "-filter_complex_script", "graph1.txt", "-map", "[aout]", "-t", total.toFixed(3), "-f", "null", "-"], { cwd: work });
  let measured = parseLoudnorm(pass1.stderr);
  // A silent cut (no game sound, no music, no voice: the camera test clips) measures -inf; there is nothing to level.
  if (!measured && /"input_i"\s*:\s*"-inf"/.test(pass1.stderr || "")) measured = { input_i: "-14.00", input_tp: "-2.00", input_lra: "0.00", input_thresh: "-24.00", target_offset: "0.00" };
  if (!measured) throw new Error(`loudness pass failed: ${(pass1.stderr || "").trim().slice(-400)}`);
  console.log(`loudness: ${measured.input_i} LUFS in, true peak ${measured.input_tp} dBTP, normalised to -14`);
  writeFileSync(join(work, "graph.txt"), buildGraph({ ...gp, measured }));
  const args = ["-y", "-hide_banner", "-loglevel", "error", ...inputs, "-filter_complex_script", "graph.txt", "-map", "[vout]", "-map", "[aout]", "-t", total.toFixed(3), "-c:v", "libx264", "-preset", "medium", "-crf", "18", "-pix_fmt", "yuv420p", "-r", String(fps), "-c:a", "aac", "-b:a", "192k", "-movflags", "+faststart", out];
  console.log(`film-make: ${segs.length} segments, ${total.toFixed(1)} s, ${cues.length} captions, ${vo.length} voice lines, music ${music ? "on" : "off"}`);
  const r = run(ff.path, args, { cwd: work });
  if (r.status !== 0) throw new Error(`ffmpeg failed: ${(r.stderr || "").trim().slice(-800)}`);
  if (!argv.includes("--keep")) rmSync(work, { recursive: true, force: true });
  const mb = (statSync(out).size / 1048576).toFixed(1);
  console.log(`wrote ${out} (${mb} MB) and ${srt}`);
  return 0;
}

// ---------------------------------------------------------------- selftest

export function selftestPost() {
  const fails = [];
  const ok = (c, m) => {
    if (!c) fails.push(m);
  };
  const cues = parseCues("# c\n0.5 3 Work the farm\n4 6.5 Rob the bank  # note\n");
  ok(cues.length === 2 && cues[1].to === 6.5 && cues[1].text === "Rob the bank", `parseCues ${JSON.stringify(cues)}`);
  try {
    parseCues("1 two nope");
    fails.push("parseCues accepted a bad line");
  } catch (e) {
    ok(/line 1/.test(e.message), `parseCues error ${e.message}`);
  }
  try {
    parseCues("5 5 same");
    fails.push("parseCues accepted an empty span");
  } catch {}
  ok(parseCues("1.5 Hello there", "vo")[0].text === "Hello there", "parseCues vo");
  const board = { clips: [{ src: "a.mp4", seconds: 8, captions: [[1, 4, "One"]] }, { src: "b.mp4", seconds: 6, captions: [[2, 9, "Two"]], vo: [[1, "Spoken"]] }], titleCard: { seconds: 4 }, endCard: { seconds: 5 } };
  validateBoard(board);
  const segs = segmentsOf(board);
  const tl = timeline(segs, 0.5);
  ok(tl.starts.join() === "0,3.5,11,16.5" && tl.total === 21.5, `timeline ${JSON.stringify(tl)}`);
  const tc = timelineCues(segs, tl.starts, 0.5, ROOT);
  ok(tc.length === 2 && tc[0].from === 4.5 && tc[1].to === 16.5, `timelineCues ${JSON.stringify(tc)}`);
  ok(timelineVo(segs, tl.starts, ROOT)[0].at === 12, "timelineVo");
  ok(fmtSrt(3725.5) === "01:02:05,500" && fmtAss(65.25) === "0:01:05.25", "time formats");
  ok(toSrt(tc).startsWith("1\n00:00:04,500 --> 00:00:07,500\nOne\n"), "toSrt");
  ok(/Style: Lower,Arial,\d+,.*,3,/.test(toAss(tc, { w: 1920, h: 1080, family: "Arial" })), "toAss box style");
  for (const [bad, field] of [[{ clips: [] }, "clips"], [{ clips: [{ src: "a", seconds: 0 }] }, "clips[0].seconds"], [{ clips: [{ seconds: 3 }] }, "clips[0].src"], [{ clips: [{ src: "a", seconds: 1 }], transition: 2 }, "transition"]]) {
    try {
      validateBoard(bad);
      fails.push(`validateBoard accepted ${field}`);
    } catch (e) {
      ok(e.message.includes(field), `validateBoard named ${e.message}`);
    }
  }
  segs.forEach((s, i) => (s.in = i));
  const g = buildGraph({ segs, starts: tl.starts, total: tl.total, t: 0.5, size: [1920, 1080], fps: 30, music: { in: 9, gainDb: -14, fadeIn: 2, fadeOut: 3 }, vo: [{ in: 8, at: 12 }], hasAudio: { 1: true }, cardFont: "f.ttf", cardTexts: { 0: ["a.txt"], 3: ["b.txt"] }, assName: "c.ass", fontsDir: "." });
  ok(g.includes("sidechaincompress") && g.includes("acrossfade=d=0.5") && g.includes("xfade=transition=fade:duration=0.5:offset=3.500") && g.includes("loudnorm=I=-14") && g.includes("print_format=json"), "buildGraph parts");
  const ln = parseLoudnorm(`x {"input_i" : "-19.5", "input_tp" : "-1.4", "input_lra" : "10.5", "input_thresh" : "-29.9", "target_offset" : "0.1"}`);
  ok(ln && loudnormArgs(ln).startsWith("volume=5.50dB"), "loudnorm pass 2");
  return fails;
}

export async function main(argv) {
  try {
    if (argv.includes("--selftest")) {
      const f = [...selftestPost(), ...(await import("./storyboard.mjs")).selftestStoryboard()];
      f.forEach((x) => console.error(`  ${x}`));
      console.log(f.length ? "post selftest: FAIL" : "post selftest ok");
      return f.length ? 1 : 0;
    }
    return argv.includes("--setup") ? setup() : await filmMake(argv);
  } catch (e) {
    console.error(`film-make: ${e.message}`);
    return 1;
  }
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) process.exit(await main(process.argv.slice(2)));
