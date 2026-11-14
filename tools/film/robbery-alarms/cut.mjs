// Robbery alarms video (#740): the edit. Reads edit.json and the takes, writes the finished mp4, a contact sheet
// beside it, and review frames every 0.5 s for the quality gate.
//   node tools/film/robbery-alarms/cut.mjs [--takes <dir>] [--out <file.mp4>]
// Steps: one filter graph cuts and joins the segments (cuts on action, fade in, fade to black, end card) into a
// near-lossless master; loudnorm measures the master's audio; the final encode is two-pass H.264 at the bitrate that
// fits maxBytes, with the measured loudness correction (linear) to loudnessLufs.
import { copyFileSync, existsSync, mkdirSync, readFileSync, rmSync, statSync, writeFileSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { dirname, extname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = resolve(HERE, "..", "..", "..");
const { resolveFfmpeg } = await import(pathToFileURL(join(ROOT, "tools/film/film.mjs")).href);

const arg = (name, dflt) => {
  const i = process.argv.indexOf(name);
  return i > 0 ? process.argv[i + 1] : dflt;
};
const EDIT = JSON.parse(readFileSync(join(HERE, "edit.json"), "utf8"));
const TAKES = resolve(arg("--takes", join(ROOT, "tmp", "robbery-video", "takes")));
const OUT = resolve(arg("--out", join(ROOT, "tmp", "robbery-video", "robbery-alarms.mp4")));
const WORK = join(dirname(OUT), "cut-work");
mkdirSync(WORK, { recursive: true });
const ff = resolveFfmpeg();
if (!ff) throw new Error("ffmpeg not found (tools\\cli film-make --setup, or FFMPEG_PATH)");
const run = (args, label) => {
  const r = spawnSync(ff.path, ["-hide_banner", "-nostats", "-y", ...args], { windowsHide: true, encoding: "utf8", cwd: WORK, maxBuffer: 1 << 26 });
  if (r.status !== 0) throw new Error(`${label} failed:\n${(r.stderr || "").slice(-2000)}`);
  return r.stderr || "";
};
const n3 = (x) => Number(x).toFixed(3);

// ---------------------------------------------------------------- the timeline
export function plan(edit) {
  const segs = edit.segments.map((s) => {
    const a = s.anchor ? edit.anchors[s.anchor][s.take] : 0;
    const from = s.anchor ? a + s.rel[0] : s.from;
    const to = s.anchor ? a + s.rel[1] : s.to;
    const au = s.audio;
    const audioFrom = au.anchor ? edit.anchors[au.anchor][au.take] + (au.rel ?? s.rel[0]) : au.from;
    return { ...s, from, to, dur: to - from, audioTake: au.take, audioFrom, gainDb: au.gainDb ?? 0 };
  });
  const takes = [...new Set(segs.flatMap((s) => [s.take, s.audioTake]))];
  const cutSeconds = segs.reduce((t, s) => t + s.dur, 0);
  return { segs, takes, cutSeconds, total: cutSeconds + edit.black + edit.endCard.seconds };
}

function graph(edit, p, markIn) {
  const f = [];
  const idx = (t) => p.takes.indexOf(t);
  const [W, H] = edit.size;
  p.segs.forEach((s, i) => {
    let v = `[${idx(s.take)}:v]trim=start=${n3(s.from)}:end=${n3(s.to)},setpts=PTS-STARTPTS,fps=${edit.fps},scale=${W}:${H},setsar=1,format=yuv420p`;
    if (s.fadeIn) v += `,fade=t=in:st=0:d=${s.fadeIn}`;
    if (s.fadeOut) v += `,fade=t=out:st=${n3(s.dur - s.fadeOut)}:d=${s.fadeOut}`;
    f.push(`${v}[v${i}]`);
    const tail = s.audioTail ?? 0;
    const adur = s.dur + tail;
    let a = `[${idx(s.audioTake)}:a]atrim=start=${n3(s.audioFrom)}:end=${n3(s.audioFrom + adur)},asetpts=PTS-STARTPTS,aresample=48000,aformat=channel_layouts=stereo`;
    if (s.gainDb) a += `,volume=${s.gainDb}dB`;
    a += `,afade=t=in:st=0:d=${s.fadeIn ?? 0.02}`;
    a += `,afade=t=out:st=${n3(adur - (tail ? 2.4 : 0.02))}:d=${tail ? 2.4 : 0.02}`;
    f.push(`${a}[a${i}]`);
  });
  f.push(`color=c=black:s=${W}x${H}:r=${edit.fps}:d=${edit.black},format=yuv420p[vblack]`);
  const c = edit.endCard;
  f.push(`color=c=black:s=${W}x${H}:r=${edit.fps}:d=${c.seconds},format=yuv420p[cbg]`);
  f.push(`[${markIn}:v]scale=-1:150,format=rgba[mark]`);
  let card = `[cbg][mark]overlay=(W-w)/2:${Math.round(H * 0.25)}:shortest=1`;
  let y = Math.round(H * 0.25) + 150 + 60;
  c.lines.forEach((l, k) => {
    const file = `card-${k}.txt`;
    writeFileSync(join(WORK, file), l.text);
    // drawtext cannot take a drive-letter path cleanly; the font is copied next to the graph.
    const font = `font-${k}${extname(l.font)}`;
    copyFileSync(l.font, join(WORK, font));
    card += `,drawtext=fontfile=${font}:textfile=${file}:fontcolor=${l.color ?? "white"}:fontsize=${l.size}:x=(w-text_w)/2:y=${y}`;
    y += Math.round(l.size * 1.5) + (k === 0 ? 10 : 0);
  });
  card += `,format=yuv420p,fade=t=in:st=0:d=${c.fadeIn},fade=t=out:st=${n3(c.seconds - c.fadeOut)}:d=${c.fadeOut}[vcard]`;
  f.push(card);
  const vs = p.segs.map((_, i) => `[v${i}]`).join("") + "[vblack][vcard]";
  f.push(`${vs}concat=n=${p.segs.length + 2}:v=1:a=0[vout]`);
  f.push(`${p.segs.map((_, i) => `[a${i}]`).join("")}concat=n=${p.segs.length}:v=0:a=1[aout]`);
  return f.join(";\n");
}

function main() {
  const p = plan(EDIT);
  for (const t of p.takes) if (!existsSync(join(TAKES, `${t}.mp4`))) throw new Error(`missing take ${t}.mp4 in ${TAKES}`);
  const inputs = p.takes.flatMap((t) => ["-i", join(TAKES, `${t}.mp4`)]);
  const markIn = p.takes.length;
  inputs.push("-loop", "1", "-t", String(EDIT.endCard.seconds), "-i", join(ROOT, EDIT.endCard.mark));
  writeFileSync(join(WORK, "graph.txt"), graph(EDIT, p, markIn));
  const master = join(WORK, "master.mkv");
  console.log(`cut: ${p.segs.length} shots, ${p.cutSeconds.toFixed(2)} s of picture + ${EDIT.black} s black + ${EDIT.endCard.seconds} s card = ${p.total.toFixed(2)} s`);
  run([...inputs, "-/filter_complex", "graph.txt", "-map", "[vout]", "-map", "[aout]", "-c:v", "libx264", "-preset", "fast", "-crf", "12", "-c:a", "pcm_s16le", master], "master");

  // Loudness: measure, then apply as one linear gain (no pumping) with a true-peak ceiling.
  const L = EDIT.loudnessLufs, TP = EDIT.truePeakDb;
  const meas = run(["-i", master, "-vn", "-af", `loudnorm=I=${L}:TP=${TP}:LRA=11:print_format=json`, "-f", "null", "-"], "loudness measure");
  const m = JSON.parse(meas.slice(meas.lastIndexOf("{"), meas.lastIndexOf("}") + 1));
  const ln = `loudnorm=I=${L}:TP=${TP}:LRA=11:measured_I=${m.input_i}:measured_TP=${m.input_tp}:measured_LRA=${m.input_lra}:measured_thresh=${m.input_thresh}:offset=${m.target_offset}:linear=true,aresample=48000,alimiter=limit=0.79:attack=2:release=60:level=false`; // the limiter keeps the true peak under -1 dBFS (quality gate)

  // Two-pass H.264 sized to fit maxBytes with a 5 % margin.
  const audioKbps = 128;
  const videoKbps = Math.floor(((EDIT.maxBytes * 8) / p.total / 1000 - audioKbps) * 0.95);
  const x264 = ["-c:v", "libx264", "-preset", "slow", "-b:v", `${videoKbps}k`, "-maxrate", `${Math.round(videoKbps * 1.6)}k`, "-bufsize", `${videoKbps * 3}k`, "-pix_fmt", "yuv420p", "-profile:v", "high", "-g", "60"];
  run(["-i", master, ...x264, "-pass", "1", "-passlogfile", "x264", "-an", "-f", "mp4", "NUL"], "encode pass 1");
  mkdirSync(dirname(OUT), { recursive: true });
  run(["-i", master, ...x264, "-pass", "2", "-passlogfile", "x264", "-af", ln, "-c:a", "aac", "-b:a", `${audioKbps}k`, "-movflags", "+faststart", OUT], "encode pass 2");

  const check = run(["-i", OUT, "-vn", "-af", `loudnorm=I=${L}:TP=${TP}:print_format=json`, "-f", "null", "-"], "loudness check");
  const c = JSON.parse(check.slice(check.lastIndexOf("{"), check.lastIndexOf("}") + 1));
  const bytes = statSync(OUT).size;

  // Contact sheet (one frame a second) and the reviewer's frames (every 0.5 s).
  const base = OUT.replace(/\.mp4$/i, "");
  run(["-i", OUT, "-vf", "fps=1,scale=480:-1,tile=6x7:padding=4:margin=4", "-frames:v", "1", "-q:v", "3", `${base}-contact.jpg`], "contact sheet");
  const frames = `${base}-frames`;
  rmSync(frames, { recursive: true, force: true });
  mkdirSync(frames, { recursive: true });
  run(["-i", OUT, "-vf", "fps=2,scale=1280:-1", "-q:v", "3", join(frames, "f%03d.jpg")], "review frames");
  const summary = { out: OUT, bytes, seconds: Number(p.total.toFixed(2)), videoKbps, loudnessLufs: Number(c.input_i), truePeakDb: Number(c.input_tp), contact: `${base}-contact.jpg`, frames };
  writeFileSync(`${base}.json`, JSON.stringify(summary, null, 2));
  console.log(JSON.stringify(summary, null, 2));
  if (bytes > EDIT.maxBytes) throw new Error(`the video is ${bytes} bytes, over ${EDIT.maxBytes}`);
}

if (import.meta.url === pathToFileURL(process.argv[1] ?? "").href) main();
