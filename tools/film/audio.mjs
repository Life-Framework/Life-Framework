// Game audio for in-engine films (#396). The engine cannot record audio, so a separate process captures what the
// game client process plays (Windows process loopback, tools/film/lf-procaudio.cs) into a WAV, and this module
// lines that WAV up with the frames and muxes an mp4 with a sound track.
//
// Alignment: the harness verb `film slate` plays a UI beep and logs its recording-relative time in index.txt
// ("# slate=<ms>"). The beep is found in the WAV (onset), so video time T = audio time - (onset - slateMs). A slate at
// start and one at stop give the offset at both ends, and their difference is the drift over the clip.
//
// Guard: `checkAudio` fails a clip whose mean volume is under -60 dB or whose slate beep cannot be heard. A silent
// capture means the client has no sound or the capture is on the wrong pid; the host's mute and volume do not matter
// (measured 2026-11-14: endpoint muted at volume 0, the client's slate beeps captured at peak -22.5 dB).

import { spawn, spawnSync } from "node:child_process";
import { existsSync, readFileSync, rmSync, writeFileSync, mkdirSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { OUT_DIR, resolveFfmpeg } from "./film.mjs";
import * as eng from "./engine.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
export const SILENT_DB = -60;
export const RATE = 48000;

// ---------------------------------------------------------------- pure parts

// Slate times (ms) from index.txt text.
export function parseSlates(text) {
  return [...String(text || "").matchAll(/^#\s*slate=(-?\d+)\s*$/gm)].map((m) => Number(m[1])).filter((n) => n >= 0);
}

export function parseVolume(stderrText) {
  const mean = /mean_volume:\s*(-?[\d.]+|-inf)\s*dB/.exec(stderrText);
  const max = /max_volume:\s*(-?[\d.]+|-inf)\s*dB/.exec(stderrText);
  const num = (m) => (m ? (m[1] === "-inf" ? -200 : Number(m[1])) : null);
  return { mean: num(mean), max: num(max) };
}

// Float32 stereo WAV written by lf-procaudio (44-byte header). Returns mono-peak samples (max of |L|,|R|).
export function readWavPeaks(buf) {
  const data = buf.subarray(44);
  const floats = new Float32Array(data.buffer.slice(data.byteOffset, data.byteOffset + data.length - (data.length % 8)));
  const n = floats.length / 2;
  const out = new Float32Array(n);
  for (let i = 0; i < n; i++) out[i] = Math.max(Math.abs(floats[2 * i]), Math.abs(floats[2 * i + 1]));
  return out;
}

// First loud onset within aroundSec +/- spanSec. Level is the peak of 2 ms blocks; the threshold is 18 dB under the
// loudest block in the search window (never under -55 dBFS). Returns seconds, or null when nothing is audible there.
export function findOnset(peaks, aroundSec, spanSec = 1.5, rate = RATE, beforeSec = spanSec) {
  const block = Math.round(rate * 0.002);
  const from = Math.max(0, Math.floor((aroundSec - beforeSec) * rate));
  const to = Math.min(peaks.length, Math.floor((aroundSec + spanSec) * rate));
  const levels = [];
  let best = 0;
  for (let s = from; s + block <= to; s += block) {
    let m = 0;
    for (let i = s; i < s + block; i++) if (peaks[i] > m) m = peaks[i];
    levels.push([s, m]);
    if (m > best) best = m;
  }
  const bestDb = best > 0 ? 20 * Math.log10(best) : -200;
  if (bestDb < -55) return null;
  const thr = Math.pow(10, Math.max(bestDb - 18, -55) / 20);
  for (const [s, m] of levels) if (m >= thr) return s / rate;
  return null;
}

// Offsets between the audio clock and the video clock from each slate: audio onset minus the slate's frame time.
export function slateOffsets(peaks, slateSecs, startGapSec = 0, rate = RATE) {
  return slateSecs.map((t) => {
    // The capture starts startGapSec before the film, so the beep is expected near startGapSec + t, and never
    // before it: a search that also looks back finds an alarm already sounding (the robbery video's police take).
    const onset = findOnset(peaks, startGapSec + t, 1.5, rate, 0.05);
    return { slate: t, onset, offset: onset === null ? null : onset - t };
  });
}

export function muxArgs({ video, wav, out, offsetSec, videoSec }) {
  // offsetSec: audio time that lines up with video time 0. Positive trims the audio start; negative pads it.
  const filters = [];
  if (offsetSec > 0.0005) filters.push(`atrim=start=${offsetSec.toFixed(4)}`, "asetpts=PTS-STARTPTS");
  else if (offsetSec < -0.0005) filters.push(`adelay=${Math.round(-offsetSec * 1000)}|${Math.round(-offsetSec * 1000)}`);
  filters.push("apad");
  return [
    "-hide_banner", "-nostats", "-loglevel", "error", "-y", "-i", video, "-i", wav,
    "-map", "0:v:0", "-map", "1:a:0", "-c:v", "copy", "-af", filters.join(","), "-c:a", "aac", "-b:a", "192k",
    "-t", (videoSec ?? 0).toFixed(3), "-movflags", "+faststart", out,
  ];
}

// ------------------------------------------------------------- environment

export function psRun(args, opts = {}) {
  return spawnSync("powershell", ["-NoProfile", "-File", join(HERE, "lf-procaudio.ps1"), ...args], { windowsHide: true, encoding: "utf8", ...opts });
}

// Default output state, for the log only. Process loopback is taken before the endpoint's volume and mute (measured
// 2026-11-14), so a muted host is fine and a film tool never changes it. #482 was whole-endpoint loopback.
export function endpointGuard() {
  const r = psRun(["--endpoint"]);
  const text = (r.stdout || "").trim();
  const mute = /mute=True/i.test(text);
  const volume = Number((/volume=([\d.]+)/.exec(text) || [])[1] ?? NaN);
  const warnings = [];
  if (r.status !== 0) warnings.push(`cannot read the default audio endpoint: ${(r.stderr || "").trim().slice(0, 200)}`);
  return { text, mute, volume, warnings };
}

export function volumeOf(wavOrMp4) {
  const ff = resolveFfmpeg();
  if (!ff) return null;
  const r = spawnSync(ff.path, ["-hide_banner", "-nostats", "-i", wavOrMp4, "-vn", "-af", "volumedetect", "-f", "null", "-"], { windowsHide: true, encoding: "utf8" });
  return parseVolume(r.stderr || "");
}

// Start capturing one process. Resolves once the capture is running; stop() resolves with the done line.
export function startCapture({ pid, wav }) {
  mkdirSync(dirname(resolve(wav)), { recursive: true });
  const stopFile = wav.replace(/\.wav$/i, ".stop");
  rmSync(stopFile, { force: true });
  const child = spawn("powershell", ["-NoProfile", "-File", join(HERE, "lf-procaudio.ps1"), "--pid", String(pid), "--out", wav, "--stop-file", stopFile], { windowsHide: true, stdio: ["ignore", "pipe", "pipe"] });
  let out = "";
  let err = "";
  child.stdout.on("data", (d) => (out += d));
  child.stderr.on("data", (d) => (err += d));
  const waitFor = (re, ms) =>
    new Promise((res, rej) => {
      const t0 = Date.now();
      const tick = () => {
        const m = re.exec(out);
        if (m) return res(m);
        if (child.exitCode !== null) return rej(new Error(`audio capture exited: ${(err || out).trim().slice(0, 300)}`));
        if (Date.now() - t0 > ms) return rej(new Error(`audio capture timed out: ${(err || out).trim().slice(0, 300)}`));
        setTimeout(tick, 50);
      };
      tick();
    });
  return waitFor(/ready epochMs=(\d+)/, 30000).then((m) => ({
    epochMs: Number(m[1]),
    wav,
    stop: async () => {
      writeFileSync(stopFile, "stop");
      const d = await waitFor(/done frames=(\d+) real=(\d+) peak=(-?[\d.]+)/, 30000);
      rmSync(stopFile, { force: true });
      return { frames: Number(d[1]), real: Number(d[2]), peakDb: Number(d[3]) };
    },
  }));
}

// Mux the film clip with the captured audio. clip is the engineStop result (mp4 path, summary), indexText the
// recording's index.txt. Returns { ok, out, mean, max, offsets, driftMs, warnings } or { ok:false, error }.
export function muxClip({ clip, indexText, wav, out, firstFrameMs = 0, startGapSec = 0, allowSilent = false }) {
  const ff = resolveFfmpeg();
  if (!ff) return { ok: false, error: "ffmpeg not found" };
  const warnings = [];
  const vol = volumeOf(wav);
  if (!vol || vol.mean === null) return { ok: false, error: "could not measure the audio" };
  if (vol.mean < SILENT_DB && !allowSilent) {
    return { ok: false, error: `captured audio is silent (mean_volume ${vol.mean} dB < ${SILENT_DB} dB): the client has no sound (start the session with --sound), or the capture is on another process's pid`, mean: vol.mean };
  }
  const peaks = readWavPeaks(readFileSync(wav));
  const slates = parseSlates(indexText).map((ms) => (ms - firstFrameMs) / 1000);
  const offsets = slateOffsets(peaks, slates, startGapSec);
  const heard = offsets.filter((o) => o.offset !== null);
  if (!slates.length) warnings.push("no slate beep was logged; audio is placed by the start time only");
  else if (!heard.length) {
    if (!allowSilent) return { ok: false, error: "the slate beep is not in the captured audio (wrong process, no sound, or the beep is muted)", mean: vol.mean, offsets };
    warnings.push("slate beep not heard");
  }
  const { offsetSec, driftMs, masked } = chooseOffset(heard.map((o) => o.offset));
  if (masked) warnings.push(`the slates disagree by ${driftMs.toFixed(0)} ms: the end beep was masked by game sound; placed by the start slate`);
  const videoSec = Number(clip?.summary?.seconds) || 0;
  const probeLen = spawnSync(ff.path, ["-hide_banner", "-i", clip.out], { windowsHide: true, encoding: "utf8" }).stderr || "";
  const dm = /Duration:\s*(\d+):(\d+):([\d.]+)/.exec(probeLen);
  const videoLen = dm ? Number(dm[1]) * 3600 + Number(dm[2]) * 60 + Number(dm[3]) : videoSec;
  const run = spawnSync(ff.path, muxArgs({ video: clip.out, wav, out, offsetSec, videoSec: videoLen }), { windowsHide: true, encoding: "utf8" });
  if (run.status !== 0 || !existsSync(out)) return { ok: false, error: (run.stderr || "mux failed").trim().slice(0, 400) };
  const final = volumeOf(out);
  return { ok: true, out, mean: final?.mean ?? vol.mean, max: final?.max ?? vol.max, offsets, offsetSec, driftMs, warnings };
}

// Two slates: use the mean so the error at either end is half the drift. Measured drift is jitter of +-120 ms
// (2026-11-10); a larger gap means one beep was masked (an alarm at -25 dB hides the -22 dB end beep, the robbery
// video), so the start slate, always played in the quiet before the action, places the audio alone.
export const MAX_DRIFT_MS = 500;
export function chooseOffset(offsets) {
  if (!offsets.length) return { offsetSec: 0, driftMs: 0, masked: false };
  const driftMs = offsets.length > 1 ? (offsets[offsets.length - 1] - offsets[0]) * 1000 : 0;
  if (Math.abs(driftMs) > MAX_DRIFT_MS) return { offsetSec: offsets[0], driftMs, masked: true };
  return { offsetSec: offsets.reduce((a, b) => a + b, 0) / offsets.length, driftMs, masked: false };
}

// pid of the game client n of the current session (session.json, written by `tools\cli session up`). The launcher's
// pid can be a wrapper, so a pid that is not an ArmaReforger process falls back to the one carrying -lfSession.
export function clientPid(sessionJson, n = 1) {
  const s = JSON.parse(readFileSync(sessionJson, "utf8"));
  const c = (s.clients || []).find((x) => x.n === n);
  if (!c) throw new Error(`session has no client ${n}`);
  const q = spawnSync("powershell", ["-NoProfile", "-Command", "Get-CimInstance Win32_Process | Where-Object { $_.Name -like 'ArmaReforger*' } | ForEach-Object { [string]$_.ProcessId + [char]9 + $_.CommandLine }"], { windowsHide: true, encoding: "utf8" });
  const rows = (q.stdout || "").split(/\r?\n/).filter(Boolean).map((l) => {
    const i = l.indexOf("\t");
    return { pid: Number(l.slice(0, i)), cmd: l.slice(i + 1) };
  });
  return pickClientPid(rows, { pid: c.pid, sessionId: s.id, n });
}

// Every client of a session carries the same `-lfSession <id>`, so the session id alone matches all of them: the
// old rule took the last match, which recorded client 2's ears for a client 1 take. A client is told apart by its
// own `-profile ...\client<n>` folder; the pid the session wrote wins when it is still a live game client.
export function pickClientPid(rows, { pid, sessionId, n }) {
  const clients = rows.filter((r) => / -client\b/.test(r.cmd) && !/ -server\b/.test(r.cmd));
  if (clients.some((r) => r.pid === pid)) return pid;
  const profileOf = (cmd) => (/ -profile\s+("[^"]+"|\S+)/.exec(cmd)?.[1] ?? "").replace(/"/g, "").replace(/[\\/]+$/, "");
  const hit = clients.find((r) => r.cmd.includes(`-lfSession ${sessionId}`) && /[\\/]client(\d+)$/i.exec(profileOf(r.cmd))?.[1] === String(n));
  return hit ? hit.pid : pid;
}

// One filmed take with a game-audio track. send(spec, ms) talks to the client (harness sendCommand); resolveAlias
// maps "$logs:el-film/<name>/index.txt" to a file; body() runs between the start and end slates.
export async function shootWithAudio({ send, resolveAlias, name, fps = 30, seconds = 120, pid, body, out, label = "film", log = console.log }) {
  const guard = endpointGuard();
  for (const w of guard.warnings) log(`audio: WARNING ${w}`);
  log(`audio: capturing pid ${pid} (host output ${guard.mute ? "muted" : "on"}, which process loopback ignores)`);
  const wav = join(OUT_DIR, `${name}.wav`);
  // The first play of a UI sound is loaded on demand and lands about 0.5 s late (measured 2026-11-10: 0.62 s
  // first, 0.13 s second). Play one beep before the capture starts so both slates have the same latency.
  const warm = send({ verb: "film", mode: "slate" }, 8000);
  log(`audio: warm-up beep -> ${warm.status} ${warm.detail}`);
  await new Promise((r) => setTimeout(r, 1500));
  const cap = await startCapture({ pid, wav });
  const start = eng.engineStart(send, name, { fps, seconds });
  if (!start.ok) {
    await cap.stop();
    return { ok: false, error: `film start failed: ${start.error}` };
  }
  const startGapSec = (Date.now() - cap.epochMs) / 1000;
  await new Promise((r) => setTimeout(r, 300));
  const s1 = send({ verb: "film", mode: "slate" }, 8000);
  log(`audio: start slate -> ${s1.status} ${s1.detail}`);
  await new Promise((r) => setTimeout(r, 1200));
  let bodyError = null;
  try {
    await body();
  } catch (e) {
    bodyError = e;
  }
  await new Promise((r) => setTimeout(r, 800));
  const s2 = send({ verb: "film", mode: "slate" }, 8000);
  log(`audio: end slate -> ${s2.status} ${s2.detail}`);
  await new Promise((r) => setTimeout(r, 1200));
  const clip = eng.engineStop(send, resolveAlias, { label, fps, out: out ? out.replace(/\.mp4$/i, ".silent.mp4") : undefined });
  const capture = await cap.stop();
  log(`audio: capture ${JSON.stringify(capture)} start gap ${startGapSec.toFixed(2)} s`);
  if (bodyError) log(`film body failed: ${bodyError.message}`);
  if (!clip.ok) return { ok: false, error: clip.error, clip };
  const index = resolveAlias(`${clip.alias}/index.txt`);
  const indexText = index ? readFileSync(index, "utf8") : "";
  const firstFrameMs = eng.parseIndex(indexText).frames[0]?.ms ?? 0;
  const target = out || clip.out.replace(/\.mp4$/i, ".audio.mp4");
  const mux = muxClip({ clip, indexText, wav, out: target, firstFrameMs, startGapSec });
  return { ...mux, clip, wav, capture, bodyError: bodyError ? bodyError.message : null };
}

export function selftestAudio() {
  const fails = [];
  const t = "# fps=30\nf1 0\nf2 33\n# slate=120\n# slate=9800\n# shots=2 seconds=10 fps=30\n";
  if (parseSlates(t).join() !== "120,9800") fails.push(`parseSlates: ${parseSlates(t)}`);
  const v = parseVolume("[Parsed_volumedetect_0 @ 0x1] mean_volume: -43.1 dB\n[Parsed_volumedetect_0 @ 0x1] max_volume: -16.2 dB\n");
  if (v.mean !== -43.1 || v.max !== -16.2) fails.push(`parseVolume: ${JSON.stringify(v)}`);
  const sil = parseVolume("mean_volume: -inf dB\nmax_volume: -inf dB");
  if (sil.mean >= SILENT_DB) fails.push("parseVolume -inf must read as silent");
  // 1 s of near silence, a 0.1 s tone from 0.5 s
  const peaks = new Float32Array(RATE * 2).fill(0.0001);
  for (let i = Math.round(RATE * 0.5); i < Math.round(RATE * 0.6); i++) peaks[i] = 0.5;
  const on = findOnset(peaks, 0.5, 0.4);
  if (on === null || Math.abs(on - 0.5) > 0.004) fails.push(`findOnset: ${on}`);
  if (findOnset(new Float32Array(RATE).fill(0.0001), 0.5, 0.4) !== null) fails.push("findOnset heard a beep in silence");
  // Two clients of one session: each n resolves to its own process, never the other client's (the silent HOLD take).
  const rows = [
    { pid: 11, cmd: '"ArmaReforgerServer.exe" -server x -lfSession abc' },
    { pid: 21, cmd: '"ArmaReforgerSteam.exe" -client 127.0.0.1:2011 -profile C:\\s\\client1 -window -lfSession abc' },
    { pid: 22, cmd: '"ArmaReforgerSteam.exe" -client 127.0.0.1:2011 -profile C:\\s\\client2 -window -lfSession abc' },
    { pid: 31, cmd: '"ArmaReforgerSteam.exe" -client 127.0.0.1 -profile C:\\o\\client1 -lfSession zzz' },
  ];
  if (pickClientPid(rows, { pid: 999, sessionId: "abc", n: 1 }) !== 21) fails.push(`pickClientPid n=1: ${pickClientPid(rows, { pid: 999, sessionId: "abc", n: 1 })}`);
  if (pickClientPid(rows, { pid: 999, sessionId: "abc", n: 2 }) !== 22) fails.push("pickClientPid n=2");
  if (pickClientPid(rows, { pid: 21, sessionId: "abc", n: 1 }) !== 21) fails.push("pickClientPid keeps a live session pid");
  if (pickClientPid(rows, { pid: 11, sessionId: "abc", n: 1 }) !== 21) fails.push("pickClientPid took the server");
  // A beep at 1.3 s over a bed of sound from 0 s: the onset is the beep, not the start of the bed.
  const bed = new Float32Array(RATE * 3).fill(0.15);
  for (let i = Math.round(RATE * 1.3); i < Math.round(RATE * 1.4); i++) bed[i] = 0.5;
  const so = slateOffsets(bed, [1.0], 0.25)[0];
  if (so.onset === null || Math.abs(so.onset - 1.25) > 0.06) fails.push(`slateOffsets looked back into a sound bed: ${JSON.stringify(so)}`);
  const jitter = chooseOffset([0.5, 0.6]);
  if (Math.abs(jitter.offsetSec - 0.55) > 1e-9 || jitter.masked) fails.push(`chooseOffset jitter: ${JSON.stringify(jitter)}`);
  const masked = chooseOffset([0.524, -1.072]);
  if (masked.offsetSec !== 0.524 || !masked.masked) fails.push(`chooseOffset masked end slate: ${JSON.stringify(masked)}`);
  const a = muxArgs({ video: "v.mp4", wav: "a.wav", out: "o.mp4", offsetSec: 0.25, videoSec: 5 }).join(" ");
  if (!a.includes("atrim=start=0.2500") || !a.includes("-c:v copy") || !a.includes("-t 5.000")) fails.push(`muxArgs trim: ${a}`);
  const b = muxArgs({ video: "v.mp4", wav: "a.wav", out: "o.mp4", offsetSec: -0.1, videoSec: 5 }).join(" ");
  if (!b.includes("adelay=100|100")) fails.push(`muxArgs pad: ${b}`);
  return fails;
}
