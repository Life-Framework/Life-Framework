// Trailer edit (v3): builds the main cut, the 15 s cut and the 6 s bumper (16:9 and 9:16) from the takes in takes/.
//   node video/trailer-v1/cut.mjs [main|cut15|bumper6|all] [--vertical] [--no-vo]
// One ffmpeg filter graph per cut: hard cuts or crossfades per edit, a title card, an end card over footage,
// burned-in captions (ASS), Piper voice-over, the music bed ducked under the voice, two-pass loudness to -14 LUFS.
import { spawnSync } from "node:child_process";
import { copyFileSync, existsSync, mkdirSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = resolve(HERE, "..", "..");
const P = await import(pathToFileURL(join(ROOT, "tools/film/post.mjs")).href);
const FF = join(ROOT, "tools/film/.bin/ffmpeg.exe");
const TAKES = join(ROOT, "tmp", "trailer-v1", "takes"); // shoot.mjs writes them (git-ignored)
const OUT = join(ROOT, "tmp", "trailer-v3"); // the cuts (git-ignored)
const MUSIC = join(ROOT, "video/music/hitman.mp3");
const LOGO = join(ROOT, "website/public/brand/logo-1024.png");
const FPS = 30;
const VOICE = {}; // en_US-ljspeech (high), public domain: tools/film/post.mjs VOICE_MODEL

// ------------------------------------------------------------------ the edit
// clip: take, in (take seconds), dur, xf (crossfade into this segment, s), game (dB of the take's own sound),
// cap [[from, to, text]] (segment seconds, lower third; burned in and the caption .srt),
// vo [[at, spoken, shown]] (Piper reads spoken; shown goes to the voice-over transcript),
// crop [x, y, w, h] (take pixels at 1920x1080: zoom into that part), panel [x, y, w, h, heightShare] (lift a UI
// panel out of the frame, enlarged, onto a blurred copy of it), vcx (0..1: where the 9:16 crop sits).
const T = (take, inn, dur, o = {}) => ({ kind: "clip", take, in: inn, dur, ...o });
const CREDITS = ["Built on Everon Life by Arkensor", "Code from Overthrow and LNGCLifeRP_Public", "Game assets by Bohemia Interactive", "Music: \"Hitman\" by Kevin MacLeod (incompetech.com), CC BY 4.0", "Voice: Piper en_US-ljspeech (LJ Speech, public domain)"];
const MAIN = {
  name: "life-framework-trailer-v3",
  musicStart: 15,
  musicDb: -9,
  segs: [
    // B1 cold open: the robber is in frame from the first cut; the sacks spill; the alarm sits on the box (#738)
    T("O3", 2.4, 4.0, { fadeIn: 0.4, game: -6, vo: [[0.15, "Everon. Nineteen eighty-nine.", "Everon. 1989."], [2.4, "The cash van keeps a schedule.", "The cash van keeps a schedule."]], cap: [[0.15, 2.4, "Everon. 1989."], [2.4, 4.0, "The cash van keeps a schedule."]] }),
    T("S03", 2.3, 2.9, { game: -6, cap: [[0.0, 0.55, "The cash van keeps a schedule."], [0.6, 2.9, "Somebody always finds a payphone."]], vo: [[0.6, "Somebody always finds a payphone.", "Somebody always finds a payphone."]] }),
    T("S04", 2.4, 2.6, {}),
    { kind: "title", dur: 2.4, lines: ["LIFE FRAMEWORK"] },
    // B3 what it is
    T("S05", 2.6, 5.0, { xf: 0.3, vo: [[0.4, "This is Life Framework. Open-source role-play for Arma Reforger.", "This is Life Framework. Open-source roleplay for Arma Reforger."]], cap: [[0.5, 5.0, "Open-source roleplay for Arma Reforger"]] }),
    T("S06", 3.2, 2.2, { panel: [238, 78, 564, 640, 0.76], vo: [[0.1, "Arrive. Sign the register.", "Arrive. Sign the register."]], cap: [[0.1, 2.2, "Sign the register"]] }),
    T("S07", 2.6, 5.0, { vo: [[0.3, "Any server can run it. Ours is Everon. Nineteen eighty-nine.", "Any server can run it. Ours is Everon, 1989."]], cap: [[0.3, 5.0, "An open framework\\N+ our 1989 Everon server"]] }),
    // B4 montage: each shot cut to the moment its line names
    T("S09", 2.6, 1.9, { vo: [[0.2, "Work the fields.", "Work the fields."]], cap: [[0.1, 1.9, "Jobs and chains"]] }),
    T("R4", 3.4, 1.6, { cap: [[0.0, 1.6, "Jobs and chains"]] }),
    T("S11", 5.0, 1.75, { game: -4, vo: [[0.2, "Swing a pickaxe.", "Swing a pickaxe."]], cap: [[0.1, 1.75, "Mining and smelting"]] }),
    T("M2", 4.1, 2.2, { game: -6, crop: [380, 400, 1040, 585], vo: [[0.1, "Smelt what you dig up.", "Smelt what you dig up."]], cap: [[0.0, 2.3, "Mining and smelting"]] }),
    T("T1", 3.0, 3.0, { vo: [[0.2, "Bank it at the teller. Or risk it.", "Bank it at the teller. Or risk it."]], cap: [[0.1, 3.0, "The bank teller"]] }),
    T("V3", 4.5, 3.2, { game: -8, vo: [[0.2, "Or skip the job. Bring a drill.", "Or skip the job. Bring a drill."]], cap: [[0.1, 3.2, "Heists: drill the bank"]] }),
    T("P1", 2.6, 3.4, { vo: [[0.2, "On duty, police read the wanted board.", "On duty, police read the wanted board."]], cap: [[0.1, 3.4, "Police and the wanted board"]] }),
    T("S17", 2.7, 3.9, { game: -6, vo: [[0.2, "Go down, not out. A medic brings you back.", "Go down, not out. A medic brings you back."]], cap: [[0.1, 3.9, "Downed, not dead"]] }),
    T("C2", 2.3, 2.9, { game: -8, vo: [[0.2, "Stolen cars go to the chop shop.", "Stolen cars go to the chop shop."]], cap: [[0.1, 2.9, "The chop shop"]] }),
    T("G2", 3.3, 1.3, { panel: [244, 84, 552, 560, 0.76], vo: [[0.1, "Lost? The globe on the square takes you anywhere in town.", "Lost? The globe on the square takes you anywhere in town."]], cap: [[0.0, 1.9, "Playtest travel globe"]] }),
    T("G2", 5.6, 2.5, { cap: [[0.0, 2.5, "Playtest travel globe"]] }),
    // B5 join the playtest
    T("S23", 3.3, 9.8, { game: -10, vo: [[0.4, "The playtest town is open. Download it. Play it. And tell us what broke.", "The playtest town is open. Download it, play it, and tell us what broke."], [5.9, "Life framework, dot org, slash playtest.", "life-framework.org/playtest"]], cap: [[0.3, 5.8, "Playtest build 3 is live"], [5.8, 9.8, "life-framework.org/playtest\\NDiscord: discord.gg/MjMexJteqz"]] }),
    // B6 road to v1 and the roadmap
    T("R1", 2.4, 5.6, { xf: 0.5, vo: [[0.4, "Next stop, version one. The framework release, for server owners.", "Next stop, version one. The framework release, for server owners."]], cap: [[0.4, 5.6, "Coming: v1, the framework release\\NTwo addons, one config file, owner docs"]] }),
    T("R2", 2.8, 3.0, { xf: 0.4, vo: [[0.3, "Then our Everon server goes live.", "Then our Everon server goes live."]], cap: [[0.3, 3.0, "Coming: our 1989 Everon server, live"]] }),
    T("R3", 2.8, 3.1, { xf: 0.4, vo: [[0.3, "After that, a story of its own.", "After that, a story of its own."]], cap: [[0.3, 3.1, "Coming: the flagship story"]] }),
    T("R4", 2.4, 5.2, { xf: 0.4, vo: [[0.3, "Then homes and land. No dates. It ships when it plays.", "Then homes and land. No dates. It ships when it plays."]], cap: [[0.3, 5.2, "Coming: homes and land"]] }),
    // B7 end card: fully visible from 1.6 s into the shot until the 1 s fade at the end (5.4 s)
    T("S28", 2.6, 8.0, { xf: 0.5, end: true, fadeOut: 1.0, vo: [[0.6, "Life Framework. Come break it, with us.", "Life Framework. Come break it with us."]],
      lines: ["life-framework.org/playtest", "Discord: discord.gg/MjMexJteqz", "Open source, MIT.", "Built in the open with AI agents."],
      small: CREDITS }),
  ],
};

// cutdowns.md: the 15 s cut (muted autoplay, every line also a caption) and the 6 s bumper.
const OSR = "Open-source roleplay for Arma Reforger";
const CUT15 = {
  name: "life-framework-trailer-v3-15s",
  musicStart: 40,
  musicDb: -8,
  segs: [
    T("O3", 2.6, 2.0, { fadeIn: 0.3, game: -6, vo: [[0.2, "Everon. Nineteen eighty-nine.", "Everon, 1989"]], cap: [[0.1, 2.0, "Everon, 1989"]] }),
    T("O3", 5.0, 1.0, { game: -6 }),
    T("S04", 3.0, 1.0, {}),
    { kind: "title", dur: 0.9, lines: ["LIFE FRAMEWORK"], noFade: true },
    T("S09", 3.0, 0.9, { vo: [[0.1, "Open-source role-play for Arma Reforger.", OSR]], cap: [[0.0, 0.9, OSR]] }),
    T("S11", 5.3, 0.9, { game: -4, cap: [[0.0, 0.9, OSR]] }),
    T("T1", 3.6, 0.9, { vcx: 0.42, cap: [[0.0, 0.9, OSR]] }),
    T("S17", 3.6, 0.9, { game: -6, cap: [[0.0, 0.9, OSR]] }),
    T("C2", 2.6, 0.9, { game: -8, cap: [[0.0, 0.9, OSR]] }),
    T("S23", 3.6, 5.6, { fadeOut: 0.5, endAt: 2.0, game: -10, vo: [[0.1, "The playtest is open. Life framework, dot org, slash playtest.", "The playtest is open. life-framework.org/playtest", 0.93]], cap: [[0.0, 2.0, "Join the playtest"]],
      lines: ["life-framework.org/playtest", "Join the playtest"], small: [] }),
  ],
};
const BUMPER6 = {
  name: "life-framework-trailer-v3-6s",
  musicStart: 54,
  musicDb: -8,
  noMusicFadeOut: true,
  segs: [
    T("O3", 2.8, 2.0, { game: -4, vo: [[0.2, "Rob the van.", "Rob the van."]], cap: [[0.1, 2.0, "Rob the van."]] }),
    T("S03", 2.6, 1.5, { game: -6, vo: [[0.1, "Or call it in.", "Or call it in."]], cap: [[0.0, 1.5, "Or call it in."]] }),
    T("S04", 2.6, 0.5, { cap: [[0.0, 0.5, "Or call it in."]] }),
    { kind: "title", dur: 2.0, lines: ["LIFE FRAMEWORK", "life-framework.org/playtest"], noFade: true, vo: [[0.1, "The playtest is open.", "The playtest is open."]] },
  ],
};

// ------------------------------------------------------------------ build
const run = (args, cwd) => spawnSync(FF, args, { cwd, encoding: "utf8", windowsHide: true, maxBuffer: 64 << 20 });
const esc = (s) => s.replace(/\\/g, "\\\\").replace(/'/g, "\u2019").replace(/:/g, "\\:").replace(/%/g, "\\%");

function build(cut, { vertical = false, noVo = false } = {}) {
  const W = vertical ? 1080 : 1920;
  const H = vertical ? 1920 : 1080;
  const name = cut.name + (vertical ? "-9x16" : "");
  const work = join(OUT, "work", name);
  mkdirSync(work, { recursive: true });
  copyFileSync("C:/Windows/Fonts/segoeui.ttf", join(work, "segoeui.ttf"));
  copyFileSync("C:/Windows/Fonts/segoeuib.ttf", join(work, "segoeuib.ttf"));
  copyFileSync(LOGO, join(work, "logo.png"));
  // timeline
  let t = 0;
  const segs = cut.segs.map((s, i) => {
    const xf = i === 0 ? 0 : s.xf || 0;
    const start = i === 0 ? 0 : t - xf;
    t = start + s.dur;
    return { ...s, xf, start };
  });
  const total = t;
  const inputs = [];
  const addIn = (...a) => {
    inputs.push(...a);
    return inputs.filter((x) => x === "-i").length - 1;
  };
  const f = [];
  const fit = vertical ? `scale=-2:${H}:flags=lanczos,crop=${W}:${H}` : `scale=${W}:${H}:flags=lanczos`;
  const fs = (k) => Math.round((vertical ? 1.0 : 1.0) * k);
  let txt = 0;
  const textFile = (s) => {
    const n = `t${txt++}.txt`;
    writeFileSync(join(work, n), s);
    return n;
  };
  const draw = (file, size, y, { bold = false, color = "white", alpha = 1 } = {}) =>
    `drawtext=fontfile=${bold ? "segoeuib.ttf" : "segoeui.ttf"}:textfile=${file}:fontsize=${size}:fontcolor=${color}@${alpha}:x=(w-text_w)/2:y=${y}:shadowcolor=black@0.6:shadowx=2:shadowy=2`;
  segs.forEach((s, i) => {
    const v = `v${i}`;
    const a = `a${i}`;
    if (s.kind === "title") {
      const li = addIn("-loop", "1", "-t", String(s.dur), "-i", "logo.png");
      const logoH = Math.round((vertical ? 0.28 : 0.42) * H);
      const ty = Math.round((vertical ? 0.62 : 0.66) * H);
      const draws = s.lines.map((l, k) => draw(textFile(l), k === 0 ? fs(vertical ? 92 : 84) : fs(vertical ? 54 : 46), ty + k * Math.round((vertical ? 130 : 110)), { bold: k === 0 })).join(",");
      f.push(`[${li}:v]scale=-2:${logoH},format=rgba,colorkey=0x0a0d12:0.12:0.05[lg${i}]`);
      f.push(`color=c=0x0a0d12:s=${W}x${H}:r=${FPS}:d=${s.dur}[bg${i}]`);
      f.push(`[bg${i}][lg${i}]overlay=(W-w)/2:${Math.round((vertical ? 0.24 : 0.14) * H)},${draws},format=yuv420p${s.noFade ? "" : `,fade=t=in:st=0:d=0.15,fade=t=out:st=${(s.dur - 0.3).toFixed(2)}:d=0.3`}[${v}]`);
      f.push(`anullsrc=r=48000:cl=stereo,atrim=duration=${s.dur}[${a}]`);
      return;
    }
    const src = join(TAKES, `${s.take}.mp4`);
    const ii = addIn("-i", src);
    // 9:16: the centre third by default; vcx (0..1) slides the crop for a subject off centre.
    const fitS = vertical && s.vcx !== undefined ? `scale=-2:${H}:flags=lanczos,crop=${W}:${H}:(iw-${W})*${s.vcx}:0` : fit;
    const zoom = s.crop ? `scale=1920:1080,crop=${s.crop[2]}:${s.crop[3]}:${s.crop[0]}:${s.crop[1]},scale=1920:1080:flags=lanczos,` : "";
    let chain = `[${ii}:v]trim=start=${s.in}:duration=${s.dur},setpts=PTS-STARTPTS,fps=${FPS},${zoom}${fitS},setsar=1,format=yuv420p`;
    if (s.panel) {
      // A UI panel (the arrival form, the travel list) is small on a busy frame; lift it, enlarged, onto a blurred,
      // darkened copy of the same frame so it reads, centred.
      const [cx, cy, cw, ch, share = 0.9] = s.panel;
      f.push(`[${ii}:v]trim=start=${s.in}:duration=${s.dur},setpts=PTS-STARTPTS,fps=${FPS},scale=1920:1080,setsar=1,split=2[fa${i}][fb${i}]`);
      f.push(`[fa${i}]scale=-2:${H},crop=${W}:${H},boxblur=30:2,eq=brightness=-0.18:saturation=0.6[fbg${i}]`);
      const fh = Math.min(Math.round(H * (vertical ? share * 0.6 : share)), Math.round((W * 0.94 * ch) / cw));
      f.push(`[fb${i}]crop=${cw}:${ch}:${cx}:${cy},scale=-2:${fh}:flags=lanczos[ffg${i}]`);
      chain = `[fbg${i}][ffg${i}]overlay=(W-w)/2:${Math.round(H * 0.04)},format=yuv420p`; // top: the caption band below stays clear
    }
    if (s.fadeIn) chain += `,fade=t=in:st=0:d=${s.fadeIn}`;
    if (s.lines) {
      // End card over footage: darken, logo, big lines, small credits.
      const from = s.endAt ?? 1.0;
      const li = addIn("-loop", "1", "-t", String(s.dur), "-i", "logo.png");
      const logoH = Math.round((vertical ? 0.22 : 0.3) * H);
      f.push(`[${li}:v]scale=-2:${logoH},format=rgba,colorkey=0x0a0d12:0.12:0.05,fade=t=in:st=${from}:d=0.6:alpha=1[lg${i}]`);
      let y = Math.round((vertical ? 0.42 : 0.42) * H);
      const big = s.lines.map((l, k) => {
        const d = draw(textFile(l), k === 0 ? fs(vertical ? 66 : 58) : fs(vertical ? 50 : 42), y, { bold: k === 0 });
        y += k === 0 ? (vertical ? 100 : 78) : vertical ? 76 : 60;
        return d + `:enable='gte(t,${from + 0.3})'`;
      });
      // Credits at 32 px or more, stacked up from the bottom edge.
      const step = vertical ? 50 : 42;
      let ys = H - Math.round(H * 0.035) - step * (s.small || []).length;
      const small = (s.small || []).map((l) => {
        const d = draw(textFile(l), fs(vertical ? 34 : 32), ys, { alpha: 0.9 });
        ys += step;
        return d + `:enable='gte(t,${from + 0.6})'`;
      });
      f.push(chain + `,drawbox=x=0:y=0:w=iw:h=ih:color=black@0.55:t=fill:enable='gte(t,${from})'[ec${i}]`);
      f.push(`[ec${i}][lg${i}]overlay=(W-w)/2:${Math.round((vertical ? 0.12 : 0.07) * H)}:enable='gte(t,${from})',${[...big, ...small].join(",")}${s.fadeOut ? `,fade=t=out:st=${(s.dur - s.fadeOut).toFixed(2)}:d=${s.fadeOut}` : ""}[${v}]`);
    } else {
      if (s.fadeOut) chain += `,fade=t=out:st=${(s.dur - s.fadeOut).toFixed(2)}:d=${s.fadeOut}`;
      f.push(chain + `[${v}]`);
    }
    if (s.game !== undefined && hasAudio(src)) f.push(`[${ii}:a]atrim=start=${s.in}:duration=${s.dur},asetpts=PTS-STARTPTS,aresample=48000,aformat=channel_layouts=stereo,volume=${s.game}dB,apad,atrim=duration=${s.dur},afade=t=in:d=0.05,afade=t=out:st=${(s.dur - 0.08).toFixed(2)}:d=0.08[${a}]`);
    else f.push(`anullsrc=r=48000:cl=stereo,atrim=duration=${s.dur}[${a}]`);
  });
  // join: crossfade where xf > 0, else a hard cut
  let vp = "v0";
  let ap = "a0";
  let acc = segs[0].dur;
  for (let i = 1; i < segs.length; i++) {
    const s = segs[i];
    const vn = `vj${i}`;
    const an = `aj${i}`;
    if (s.xf > 0) {
      f.push(`[${vp}]settb=AVTB,fps=${FPS}[vpt${i}]`);
      f.push(`[v${i}]settb=AVTB,fps=${FPS}[vit${i}]`);
      f.push(`[vpt${i}][vit${i}]xfade=transition=fade:duration=${s.xf}:offset=${(acc - s.xf).toFixed(3)}[${vn}]`);
      f.push(`[${ap}][a${i}]acrossfade=d=${s.xf}[${an}]`);
      acc += s.dur - s.xf;
    } else {
      f.push(`[${vp}][v${i}]concat=n=2:v=1:a=0[${vn}]`);
      f.push(`[${ap}][a${i}]concat=n=2:v=0:a=1[${an}]`);
      acc += s.dur;
    }
    vp = vn;
    ap = an;
  }
  // captions
  const cues = [];
  const srt = [];
  segs.forEach((s) => {
    for (const c of s.cap || []) cues.push({ from: s.start + c[0], to: Math.min(s.start + c[1], s.start + s.dur), text: c[2] });
  });
  const ass = P.toAss(oneAtATime(cues, total), { w: W, h: H, family: "Segoe UI" });
  writeFileSync(join(work, "captions.ass"), vertical ? ass.replace(/,2,(\d+),(\d+),(\d+),1$/m, (m, l, r, v) => `,2,60,60,${Math.round(H * 0.16)},1`) : ass);
  f.push(`[${vp}]subtitles=captions.ass:fontsdir=.[vout]`);
  // voice
  const vo = [];
  if (!noVo) segs.forEach((s) => (s.vo || []).forEach((x) => vo.push({ at: s.start + x[0], spoken: x[1], shown: x[2], pace: x[3] })));
  vo.forEach((x, k) => {
    const wav = join(work, `vo${k}.wav`);
    P.synthVo(x.spoken, wav, x.pace ? { ...VOICE, lengthScale: x.pace } : VOICE);
    x.in = addIn("-i", wav);
    x.len = durOf(wav);
    srt.push({ from: x.at, to: x.at + x.len, text: x.shown });
  });
  // Timing check: a line must end before the next starts and inside the picture.
  vo.forEach((x, k) => {
    const next = vo[k + 1];
    if (next && x.at + x.len > next.at - 0.1) console.log('VO OVERLAP ' + x.shown + ' ends ' + (x.at + x.len).toFixed(2) + ', next at ' + next.at.toFixed(2));
    if (x.at + x.len > total - 0.3) console.log('VO PAST END ' + x.shown);
  });
  const mi = addIn("-ss", String(cut.musicStart), "-i", MUSIC);
  const vl = vo.map((x, k) => {
    f.push(`[${x.in}:a]aresample=48000,aformat=channel_layouts=stereo,adelay=${Math.round(x.at * 1000)}:all=1,apad=whole_dur=${total.toFixed(3)}[vo${k}]`);
    return `[vo${k}]`;
  });
  if (vl.length) f.push(`${vl.join("")}amix=inputs=${vl.length}:normalize=0:duration=longest,atrim=duration=${total.toFixed(3)},asplit=2[vokey][vomix]`);
  else f.push(`anullsrc=r=48000:cl=stereo,atrim=duration=${total.toFixed(3)},asplit=2[vokey][vomix]`);
  const fo = cut.noMusicFadeOut ? "" : `,afade=t=out:st=${(total - 2.5).toFixed(3)}:d=2.5`;
  f.push(`[${mi}:a]aresample=48000,aformat=channel_layouts=stereo,atrim=duration=${total.toFixed(3)},asetpts=PTS-STARTPTS,volume=${cut.musicDb}dB,afade=t=in:d=0.8${fo}[mus]`);
  f.push(`[mus][vokey]sidechaincompress=threshold=0.03:ratio=6:attack=15:release=350:makeup=1[musd]`);
  f.push(`[${ap}][musd][vomix]amix=inputs=3:normalize=0:duration=first,acompressor=threshold=0.15:ratio=2.5:attack=5:release=150:makeup=1[premix]`);
  const graph = (measured) => [...f, `[premix]${P.loudnormArgs(measured).replace("limit=0.84", "limit=0.7")}${cut.noMusicFadeOut ? "" : `,afade=t=out:st=${(total - 0.4).toFixed(3)}:d=0.4`}[aout]`].join(";\n");
  writeFileSync(join(work, "g1.txt"), graph(null).replace("subtitles=captions.ass:fontsdir=.[vout]", "nullsink"));
  const p1 = run(["-y", "-hide_banner", ...inputs, "-filter_complex_script", "g1.txt", "-map", "[aout]", "-t", total.toFixed(3), "-f", "null", "-"], work);
  const m = P.parseLoudnorm(p1.stderr);
  if (!m) throw new Error(`loudness pass failed: ${(p1.stderr || "").slice(-1500)}`);
  writeFileSync(join(work, "g2.txt"), graph(m));
  const out = join(OUT, `${name}.mp4`);
  const r = run(["-y", "-hide_banner", "-loglevel", "error", ...inputs, "-filter_complex_script", "g2.txt", "-map", "[vout]", "-map", "[aout]", "-t", total.toFixed(3), "-c:v", "libx264", "-preset", "slow", "-crf", "17", "-pix_fmt", "yuv420p", "-r", String(FPS), "-c:a", "aac", "-b:a", "192k", "-movflags", "+faststart", out], work);
  if (r.status !== 0) throw new Error(`encode failed: ${(r.stderr || "").slice(-1500)}`);
  // <name>.srt: the caption track, exactly what is burned in, plus the end card's text over the card; one cue at a
  // time. <name>.vo.srt: the voice-over transcript, kept apart.
  const capTrack = cues.map((c) => ({ ...c, text: c.text.replace(/\\N/g, "\n") }));
  const card = segs[segs.length - 1];
  const cardFrom = card.kind === "title" ? card.start : card.start + (card.endAt ?? 1.0) + 0.3;
  if (card.lines && card.lines.length) capTrack.push({ from: cardFrom, to: total, text: card.lines.slice(0, 2).join("\n") });
  writeFileSync(out.replace(/\.mp4$/, ".srt"), P.toSrt(oneAtATime(capTrack, total)));
  writeFileSync(out.replace(/\.mp4$/, ".vo.srt"), P.toSrt(oneAtATime(srt, total)));
  console.log(`${name}: ${total.toFixed(2)} s, ${segs.length} segments, ${vo.length} voice lines, ${cues.length} captions, loudness in ${m.input_i} LUFS -> ${out}`);
  return { out, total, segs };
}

// Sorted cues, each ended before the next one starts (a crossfade lets two segments' captions touch).
export function oneAtATime(list, total) {
  const out = [...list].sort((a, b) => a.from - b.from).map((c) => ({ ...c, to: Math.min(c.to, total) }));
  for (let i = 0; i < out.length - 1; i++) out[i].to = Math.min(out[i].to, out[i + 1].from - 0.04);
  return out.filter((c) => c.to - c.from >= 0.3);
}

function hasAudio(src) {
  const r = run(["-hide_banner", "-i", src]);
  return /Stream #.*Audio:/.test(r.stderr || "");
}
function durOf(file) {
  const r = run(["-hide_banner", "-i", file]);
  const d = /Duration:\s*(\d+):(\d+):([\d.]+)/.exec(r.stderr || "");
  return d ? +d[1] * 3600 + +d[2] * 60 + +d[3] : 2;
}

// Segment start times on the final timeline (for the contact sheet and the quality-gate notes).
export function timelineOf(cut) {
  let t = 0;
  return cut.segs.map((s, i) => {
    const xf = i === 0 ? 0 : s.xf || 0;
    const start = i === 0 ? 0 : t - xf;
    t = start + s.dur;
    return { id: s.take || "title", start, end: t, in: s.in };
  });
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const which = process.argv[2] || "main";
  const opts = { vertical: process.argv.includes("--vertical"), noVo: process.argv.includes("--no-vo") };
  const cuts = { main: MAIN, cut15: CUT15, bumper6: BUMPER6 };
  for (const k of which === "all" ? Object.keys(cuts) : [which]) build(cuts[k], opts);
}
export { MAIN, CUT15, BUMPER6 };
