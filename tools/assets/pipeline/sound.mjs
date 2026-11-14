// tools\cli asset sound <slug>: the sound stage of the asset pipeline (docs/asset-pipeline/sounds.md).
//
//   asset sound <slug> --init                 write tools/assets/<slug>/sounds.json from the kind's event list
//   asset sound <slug> [--only generate|process|wire] [--again <id>]
//   asset sound <slug> --status
//   asset sound <slug> --settle               read the account's credit counter into sounds.json
//
// generate  ElevenLabs text-to-sound-effects, `variations` takes per sound; every paid call is a row in
//           sounds.json (the committed credit record) with the account's real balance before and after.
// process   ffmpeg decodes to 48 kHz mono; the trim, peak normalise, fades and loop seam are done here
//           in JS so they are the same on every machine; writes the WAV and a review picture.
// wire      copies the picked takes into the addon with their .meta files and writes the sound
//           projects (.acp). Stops until every sound has a `pick` and a `reason`.
//
// What the engine wants was read from the game on 2026-11-13 (1.9): vanilla weapon samples are RIFF
// PCM, 16 bit, mono, 48 kHz, and ship as plain .wav beside a WAVResourceClass .meta; the project
// graph below is the vanilla M9 handling project's (Weapons_Handguns_M9_Handling.acp), one chain per event.
// The key is read at call time from a file outside the repo and never leaves this module.
import { copyFileSync, existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { homedir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import { freshGuid, readGuid } from "./meta.mjs";
import { addonFlag, addonOf, addonPath, addonProblems, assertBoundary, guidsInUse, slugTarget } from "./addon.mjs";

export const BASE = "https://api.elevenlabs.io";
export const KEY_FILE = () => process.env.LF_ELEVENLABS_KEY_FILE || join(homedir(), ".life-framework", "elevenlabs-key");
// Read 2026-11-13: the capabilities page says 40 credits per second when the duration is given
// (0.5 to 30 s), the pricing page says about 200 per generation. The taser's 19 short takes (0.5 to
// 2 s) moved the account's counter by about 210 each, so the floor is what short sounds pay.
// The account's own counter (--settle) wins over this estimate.
export const CREDITS_PER_SECOND = 40;
export const CREDITS_FLOOR = 200;
export const MAX_GENERATIONS = 30;
export const RATE = 48000;

// ---------------------------------------------------------------- the event list per item kind
// `event` is the name the engine or our script plays. A weapon's are the ones the vanilla pistol's
// animation and muzzle fire; `replaces` are the vanilla projects the weapon prefab stops listing.
// bus and range choose the mixer input and the distance curve (BUS, RANGE below).
export const SOUND_KINDS = {
  weapon: {
    projectRoot: "Sounds/Weapons/Handguns",
    replaces: ["Weapons_Handguns_M9_Shot.acp", "Weapons_Handguns_M9_Handling.acp"],
    events: [
      { id: "shot", event: "SOUND_SHOT", project: "Shot", bus: "shots", range: "voice70", volume_db: -6, duration_seconds: 1.5 },
      { id: "dry", event: "SOUND_DRY", project: "Handling", bus: "shots", range: "near22", volume_db: -20, duration_seconds: 0.5 },
      { id: "mag_out", event: "SOUND_RELOAD_MAG_REMOVE", project: "Handling", bus: "handling", range: "reload60", volume_db: -18, duration_seconds: 0.7 },
      { id: "mag_in", event: "SOUND_RELOAD_MAG_INSERT", project: "Handling", bus: "handling", range: "reload60", volume_db: -16, duration_seconds: 0.7 },
    ],
  },
  gadget: {
    projectRoot: "Sounds/Items",
    replaces: [],
    events: [{ id: "use", event: "SOUND_LF_USE", project: "Use", bus: "sfx", range: "reload60", volume_db: -14, duration_seconds: 1 }],
  },
  deployable: {
    projectRoot: "Sounds/Props",
    replaces: [],
    events: [
      { id: "deploy", event: "SOUND_LF_DEPLOY", project: "Use", bus: "sfx", range: "reload60", volume_db: -12, duration_seconds: 1.5 },
      { id: "pickup", event: "SOUND_LF_PICKUP", project: "Use", bus: "sfx", range: "reload60", volume_db: -14, duration_seconds: 1 },
    ],
  },
};

// Mixer inputs of Sounds/FinalMix.afm (node ids read from the file).
export const BUS = { handling: 6151, shots: 7175, sfx: 79879 };
// Distance curves: vanilla amplitude configs, named by their outer range in metres.
export const RANGE = {
  near22: "{D6921F7062BE7960}Sounds/_SharedData/Configs/Amplitude/_SharedData/Amplitude_-60LUFS_to_-55LUFS.conf",
  reload60: "{011A82046959B996}Sounds/_SharedData/Configs/Amplitude/_SharedData/Amplitude_-45LUFS_to_-30LUFS_Weapons_Reload.conf",
  voice70: "{2D5607FF1ED3F5D3}Sounds/_SharedData/Configs/Amplitude/_SharedData/Amplitude_-35LUFS_to_-30LUFS.conf",
  far170: "{BA1F5E72DA797274}Sounds/_SharedData/Configs/Amplitude/_SharedData/Amplitude_-30LUFS_to_-25LUFS.conf",
};

export const soundsFile = (root, slug) => join(root, "tools", "assets", slug, "sounds.json");
export const soundCache = (root, slug) => join(root, "tmp", "assets", slug, "sound");

// Everything wrong with a sounds.json, as a list.
export function soundProblems(rec) {
  const out = [];
  if (!/^[a-z][a-z0-9-]*$/.test(rec.slug || "")) out.push("slug must be lowercase letters, digits and dashes");
  if (!/^[A-Z][A-Za-z0-9]*$/.test(rec.name || "")) out.push("name must be PascalCase (it becomes file names)");
  if (!/^Sounds\/[A-Za-z0-9_/]+$/.test(rec.project_dir || "")) out.push("project_dir must start with Sounds/");
  if (!/^[A-Za-z0-9_]+$/.test(rec.project_prefix || "")) out.push("project_prefix must be a file name stem");
  const cap = rec.budget?.generations;
  if (!(cap > 0 && cap <= MAX_GENERATIONS)) out.push(`budget.generations must be 1 to ${MAX_GENERATIONS}`);
  const ids = new Set();
  const events = new Set();
  for (const s of rec.sounds || []) {
    const at = `sound ${s.id || "?"}`;
    if (!/^[a-z][a-z0-9_]*$/.test(s.id || "")) out.push(`${at}: id must be lowercase letters, digits and underscores`);
    if (ids.has(s.id)) out.push(`${at}: id is used twice`);
    ids.add(s.id);
    if (!s.layer_only && !/^SOUND_[A-Z0-9_]+$/.test(s.event || "")) out.push(`${at}: event must look like SOUND_NAME`);
    if (s.event && events.has(s.event)) out.push(`${at}: event ${s.event} is used twice`);
    if (s.event) events.add(s.event);
    if (!s.layer_only) {
      if (!/^[A-Z][A-Za-z0-9]*$/.test(s.project || "")) out.push(`${at}: project must be a PascalCase suffix such as Shot or Handling`);
      if (!BUS[s.bus]) out.push(`${at}: bus must be one of ${Object.keys(BUS).join(", ")}`);
      if (!RANGE[s.range]) out.push(`${at}: range must be one of ${Object.keys(RANGE).join(", ")}`);
      if (!(s.volume_db <= 0 && s.volume_db >= -40)) out.push(`${at}: volume_db must be -40 to 0`);
    }
    if (s.compose && !(Array.isArray(s.compose.steps) && s.compose.steps.length && s.compose.steps.every((t) => t.ratio > 0.2 && t.ratio < 4 && t.ms >= 100 && t.ms <= 3000))) out.push(`${at}: compose.steps must be a list of { ratio 0.2-4, ms 100-3000, fromMs }`);
    if (s.compose && !(Number.isInteger(s.compose.from_take) && Number.isInteger(s.compose.as_take) && s.compose.as_take > s.compose.from_take)) out.push(`${at}: compose needs from_take and an as_take above it`);
    if (s.derive && !(Number.isInteger(s.derive.from_take) && Number.isInteger(s.derive.as_take) && s.derive.as_take > s.derive.from_take)) out.push(`${at}: derive needs from_take and an as_take above it`);
    if (s.derive && s.derive.on_ms !== undefined && !(s.derive.on_ms >= 50 && s.derive.on_ms <= 2000 && s.derive.off_ms >= 50 && s.derive.off_ms <= 2000)) out.push(`${at}: derive on_ms and off_ms must be 50-2000`);
    if (s.layer && !(rec.sounds || []).some((o) => o.id === s.layer.id && o.layer_only)) out.push(`${at}: layer.id must name a sound marked layer_only`);
    if (s.layer && !(Number.isInteger(s.layer.take) && s.layer.take >= 1)) out.push(`${at}: layer.take must be a take number`);
    if (!s.prompt) out.push(`${at}: prompt is empty`);
    else if (s.prompt.length > 450) out.push(`${at}: prompt is ${s.prompt.length} characters; keep it under 450`);
    if (!(s.duration_seconds >= 0.5 && s.duration_seconds <= 30)) out.push(`${at}: duration_seconds must be 0.5 to 30 (the API's limits)`);
    if (!(s.prompt_influence >= 0 && s.prompt_influence <= 1)) out.push(`${at}: prompt_influence must be 0 to 1`);
    if (!(s.variations >= 1 && s.variations <= 6)) out.push(`${at}: variations must be 1 to 6`);
    if (s.trim_db !== undefined && !(s.trim_db >= -60 && s.trim_db <= -20)) out.push(`${at}: trim_db must be -60 to -20`);
    if (s.pick && (!Array.isArray(s.pick) || !s.pick.length || s.pick.some((n) => !Number.isInteger(n) || n < 1))) out.push(`${at}: pick must be a list of take numbers`);
    if (s.pick && !s.reason) out.push(`${at}: a pick needs a reason`);
  }
  if (!(rec.sounds || []).length) out.push("sounds is empty");
  return out;
}

// ---------------------------------------------------------------- ElevenLabs
function key() {
  const file = KEY_FILE();
  if (!existsSync(file)) throw new Error(`no ElevenLabs key file at ${file} (set LF_ELEVENLABS_KEY_FILE)`);
  const k = readFileSync(file, "utf8").trim();
  if (!k) throw new Error(`ElevenLabs key file ${file} is empty`);
  return k;
}

function scrub(text) {
  let k = "";
  try { k = key(); } catch { /* no key: nothing to scrub */ }
  const s = String(text ?? "");
  return k ? s.split(k).join("<elevenlabs-key>") : s;
}

async function http(method, path, body, { fetchImpl = fetch, timeoutMs = 120000 } = {}) {
  let res;
  try {
    res = await fetchImpl(BASE + path, {
      method,
      headers: { "xi-api-key": key(), ...(body ? { "Content-Type": "application/json" } : {}) },
      body: body ? JSON.stringify(body) : undefined,
      signal: AbortSignal.timeout(timeoutMs),
    });
  } catch (e) {
    throw new Error(`ElevenLabs ${method} ${path}: ${scrub(e.message)}`);
  }
  if (!res.ok) {
    const text = await res.text();
    const why = { 401: "bad key, or the key lacks this permission", 402: "out of credits", 422: "the request was refused", 429: "rate limit" }[res.status] || "";
    throw new Error(`ElevenLabs ${method} ${path}: HTTP ${res.status} ${why} ${scrub(text.slice(0, 300))}`.trim());
  }
  return res;
}

// The account's credit counter. Needs the key's user_read permission.
export async function subscription(o) {
  const j = await (await http("GET", "/v1/user/subscription", null, o)).json();
  return { tier: j.tier, used: j.character_count, limit: j.character_limit };
}

export function requestBody(s) {
  const body = { text: s.prompt, duration_seconds: s.duration_seconds, prompt_influence: s.prompt_influence };
  if (s.loop) body.loop = true;
  if (s.model_id) body.model_id = s.model_id;
  return body;
}

// One paid generation. Returns the audio bytes as the API encoded them.
export async function generate(s, { outputFormat = "mp3_44100_128", ...o } = {}) {
  const res = await http("POST", `/v1/sound-generation?output_format=${outputFormat}`, requestBody(s), o);
  return Buffer.from(await res.arrayBuffer());
}

export const estimateCredits = (s) => Math.max(CREDITS_FLOOR, Math.round(s.duration_seconds * CREDITS_PER_SECOND));

// ---------------------------------------------------------------- samples: pure functions
const db = (v) => (v > 0 ? 20 * Math.log10(v) : -120);

// Cut to the part that sounds: the first to the last 5 ms window whose RMS is over the threshold,
// widened by the pads. Generated takes often end on a stray tick with a stretch of silence before
// it; a per-sample test would keep both. So the level is read per window, and a last burst of
// 10 ms or less that stands 100 ms or more after the rest is dropped. Null when all is silence.
export function trimSilence(x, rate = RATE, { thresholdDb = -45, leadMs = 3, tailMs = 25, windowMs = 5 } = {}) {
  const th = 32768 * 10 ** (thresholdDb / 20);
  const w = Math.max(1, Math.round((windowMs * rate) / 1000));
  const loud = [];
  for (let at = 0; at < x.length; at += w) {
    const end = Math.min(x.length, at + w);
    let sum = 0;
    for (let i = at; i < end; i++) sum += x[i] * x[i];
    loud.push(Math.sqrt(sum / (end - at)) >= th);
  }
  const first = loud.indexOf(true);
  if (first < 0) return null;
  let last = loud.lastIndexOf(true);
  const gap = Math.round(100 / windowMs);
  for (;;) {
    let start = last;
    while (start > first && loud[start - 1]) start--;
    const before = loud.lastIndexOf(true, start - 1);
    if (last - start + 1 > 2 || before < 0 || start - before - 1 < gap) break;
    last = before;
  }
  const a = Math.max(0, first * w - Math.round((leadMs * rate) / 1000));
  const b = Math.min(x.length, (last + 1) * w + Math.round((tailMs * rate) / 1000));
  return x.slice(a, b);
}

export function peakOf(x) {
  let p = 0;
  for (let i = 0; i < x.length; i++) { const v = Math.abs(x[i]); if (v > p) p = v; }
  return p;
}

export function normalisePeak(x, targetDb = -3) {
  const p = peakOf(x);
  if (!p) return x.slice();
  const g = (32768 * 10 ** (targetDb / 20)) / p;
  const out = new Int16Array(x.length);
  for (let i = 0; i < x.length; i++) out[i] = Math.max(-32768, Math.min(32767, Math.round(x[i] * g)));
  return out;
}

export function fade(x, rate = RATE, { inMs = 2, outMs = 12 } = {}) {
  const out = x.slice();
  const a = Math.min(out.length, Math.round((inMs * rate) / 1000));
  const b = Math.min(out.length, Math.round((outMs * rate) / 1000));
  for (let i = 0; i < a; i++) out[i] = Math.round((out[i] * i) / a);
  for (let i = 0; i < b; i++) out[out.length - 1 - i] = Math.round((out[out.length - 1 - i] * i) / b);
  return out;
}

// A seamless loop: the last `ms` are folded into the first `ms` with an equal-power crossfade, so
// the sample after the end is the sample at the start. The result is `ms` shorter.
export function loopSeam(x, rate = RATE, ms = 40) {
  const n = Math.min(Math.round((ms * rate) / 1000), Math.floor(x.length / 4));
  const len = x.length - n;
  const out = new Int16Array(len);
  for (let i = 0; i < len; i++) out[i] = x[i];
  for (let i = 0; i < n; i++) {
    const t = i / n;
    const v = x[i] * Math.sin((t * Math.PI) / 2) + x[len + i] * Math.cos((t * Math.PI) / 2);
    out[i] = Math.max(-32768, Math.min(32767, Math.round(v)));
  }
  return out;
}

// Lay `over` on top of `base`: over starts at 0, base starts delayMs later. For a sound the generator
// will not make in one piece (a pop, then a crackle): generate the parts and join them here.
export function mixLayer(base, over, rate = RATE, { gainDb = 0, delayMs = 0 } = {}) {
  const d = Math.round((delayMs * rate) / 1000);
  const g = 10 ** (gainDb / 20);
  const out = new Int16Array(Math.max(base.length + d, over.length));
  for (let i = 0; i < out.length; i++) {
    const v = (i >= d && i - d < base.length ? base[i - d] : 0) + (i < over.length ? over[i] * g : 0);
    out[i] = Math.max(-32768, Math.min(32767, Math.round(v)));
  }
  return out;
}

// A sound the generator will not make in one piece, a two-note siren: it returns one steady note and
// never alternates. Re-pitch one steady take into a repeating pattern of notes. Each step reads
// `ms` of output from the take starting at `fromMs`, `ratio` times as fast (pitch x ratio, by linear
// resampling: fine for a steady tone), with a short fade at each end so the notes do not click.
export function alternateSteps(x, steps, repeat = 1, rate = RATE, fadeMs = 15) {
  const parts = [];
  for (const st of steps) {
    const n = Math.round((st.ms * rate) / 1000);
    const from = Math.round(((st.fromMs ?? 0) * rate) / 1000);
    const seg = new Int16Array(n);
    for (let i = 0; i < n; i++) {
      const pos = from + i * st.ratio;
      const a = Math.floor(pos);
      if (a + 1 >= x.length) throw new Error(`step at ${st.fromMs} ms reads past the end of the take`);
      const f = pos - a;
      seg[i] = Math.round(x[a] * (1 - f) + x[a + 1] * f);
    }
    parts.push(fade(seg, rate, { inMs: fadeMs, outMs: fadeMs }));
  }
  const cycle = parts.reduce((n, p) => n + p.length, 0);
  const out = new Int16Array(cycle * repeat);
  for (let r = 0; r < repeat; r++) {
    let at = r * cycle;
    for (const p of parts) { out.set(p, at); at += p.length; }
  }
  return out;
}

// A take cut from fromMs on and, when onMs and offMs are given, chopped into a steady on/off cadence with short
// ramps (a buzzer or horn the generator gave as one held tone). Whole cycles only.
export function deriveTake(x, { fromMs = 0, onMs = 0, offMs = 0 } = {}, rate = RATE, rampMs = 8) {
  const from = Math.round((fromMs * rate) / 1000);
  let y = x.subarray(from);
  if (!(onMs > 0 && offMs > 0)) return Int16Array.from(y);
  const on = Math.round((onMs * rate) / 1000), cycle = on + Math.round((offMs * rate) / 1000), ramp = Math.round((rampMs * rate) / 1000);
  const n = Math.floor(y.length / cycle) * cycle;
  const out = new Int16Array(n);
  for (let i = 0; i < n; i++) {
    const k = i % cycle;
    let g = 0;
    if (k < on) g = Math.min(1, k / ramp, (on - 1 - k) / ramp);
    out[i] = Math.round(y[i] * Math.max(0, g));
  }
  return out;
}

// The whole deterministic chain for one take. Normalise first, so the silence threshold means the
// same for a quiet take and a loud one.
// trimDb raises the threshold for a take whose tail is a faint tone the generator left behind.
export function processSamples(x, { loop = false, peakDb = -3, trimDb = -45 } = {}) {
  let y = trimSilence(normalisePeak(x, peakDb), RATE, loop ? { leadMs: 0, tailMs: 0, thresholdDb: -40 } : { thresholdDb: trimDb });
  if (!y) throw new Error("the take is silent");
  y = loop ? loopSeam(y) : fade(y);
  return normalisePeak(y, peakDb);
}

export function encodeWav(x, rate = RATE) {
  const head = Buffer.alloc(44);
  head.write("RIFF", 0, "latin1");
  head.writeUInt32LE(36 + x.length * 2, 4);
  head.write("WAVEfmt ", 8, "latin1");
  head.writeUInt32LE(16, 16);
  head.writeUInt16LE(1, 20); // PCM
  head.writeUInt16LE(1, 22); // mono
  head.writeUInt32LE(rate, 24);
  head.writeUInt32LE(rate * 2, 28);
  head.writeUInt16LE(2, 32);
  head.writeUInt16LE(16, 34);
  head.write("data", 36, "latin1");
  head.writeUInt32LE(x.length * 2, 40);
  return Buffer.concat([head, Buffer.from(x.buffer, x.byteOffset, x.length * 2)]);
}

// Read a WAV's header and measure its samples.
export function wavInfo(buf) {
  if (buf.length < 44 || buf.toString("latin1", 0, 4) !== "RIFF" || buf.toString("latin1", 8, 12) !== "WAVE") throw new Error("not a RIFF WAVE file");
  let at = 12;
  let fmt = null;
  let data = null;
  while (at + 8 <= buf.length) {
    const id = buf.toString("latin1", at, at + 4);
    const size = buf.readUInt32LE(at + 4);
    if (id === "fmt ") fmt = { tag: buf.readUInt16LE(at + 8), channels: buf.readUInt16LE(at + 10), rate: buf.readUInt32LE(at + 12), bits: buf.readUInt16LE(at + 22) };
    if (id === "data") { data = buf.subarray(at + 8, Math.min(buf.length, at + 8 + size)); break; }
    at += 8 + size + (size % 2);
  }
  if (!fmt || !data) throw new Error("the WAV has no fmt or no data chunk");
  const info = { ...fmt, bytes: buf.length, chunksBeforeData: at };
  if (fmt.tag !== 1 || fmt.bits !== 16) return info;
  const n = Math.floor(data.length / 2 / fmt.channels);
  const x = new Int16Array(n);
  for (let i = 0; i < n; i++) x[i] = data.readInt16LE(i * 2 * fmt.channels);
  const peak = peakOf(x);
  let sum = 0;
  let cross = 0;
  let peakAt = 0;
  let maxStep = 0;
  for (let i = 0; i < n; i++) {
    sum += x[i] * x[i];
    if (i && Math.abs(x[i] - x[i - 1]) > maxStep) maxStep = Math.abs(x[i] - x[i - 1]);
    if (i && (x[i] >= 0) !== (x[i - 1] >= 0)) cross++;
    if (Math.abs(x[i]) === peak && !peakAt) peakAt = i;
  }
  const th = 32768 * 10 ** (-45 / 20);
  let lead = 0;
  while (lead < n && Math.abs(x[lead]) < th) lead++;
  let tail = 0;
  while (tail < n && Math.abs(x[n - 1 - tail]) < th) tail++;
  // Share of the energy in the first 100 ms: a transient scores high, a steady buzz low.
  const first = Math.min(n, Math.round(fmt.rate / 10));
  let head = 0;
  for (let i = 0; i < first; i++) head += x[i] * x[i];
  const seconds = n / fmt.rate;
  return {
    ...info, samples: n, seconds,
    peakDb: db(peak / 32768), rmsDb: db(Math.sqrt(sum / Math.max(1, n)) / 32768),
    leadSilenceMs: (lead * 1000) / fmt.rate, tailSilenceMs: (tail * 1000) / fmt.rate,
    peakAtMs: (peakAt * 1000) / fmt.rate, attackShare: sum ? head / sum : 0,
    zeroCrossHz: seconds ? cross / 2 / seconds : 0,
    // A loop wraps from the last sample to the first; maxStep is the largest step inside the sound.
    seam: n ? Math.abs(x[0] - x[n - 1]) / 32768 : 0, maxStep: maxStep / 32768,
  };
}

// What the engine and the mix need of a processed take.
export function checkWav(info, { loop = false, maxSeconds = 6 } = {}) {
  const out = [];
  if (info.tag !== 1) out.push(`format tag ${info.tag}, the engine's samples are PCM (1)`);
  if (info.bits !== 16) out.push(`${info.bits} bit, expected 16`);
  if (info.channels !== 1) out.push(`${info.channels} channels; a positioned sound is mono`);
  if (info.rate !== RATE) out.push(`${info.rate} Hz, expected ${RATE}`);
  if (out.length) return out;
  if (info.seconds < 0.03) out.push(`only ${(info.seconds * 1000).toFixed(0)} ms long`);
  if (info.seconds > maxSeconds) out.push(`${info.seconds.toFixed(2)} s long, over ${maxSeconds} s`);
  if (info.peakDb > -0.5) out.push(`peak ${info.peakDb.toFixed(1)} dBFS leaves no headroom`);
  if (info.peakDb < -9) out.push(`peak ${info.peakDb.toFixed(1)} dBFS is too quiet; normalise to -3`);
  if (!loop && info.leadSilenceMs > 20) out.push(`${info.leadSilenceMs.toFixed(0)} ms of silence before the sound: it would play late`);
  if (!loop && info.tailSilenceMs > 120) out.push(`${info.tailSilenceMs.toFixed(0)} ms of silence after the sound`);
  if (loop && info.seam > Math.max(0.01, info.maxStep)) out.push(`the loop wraps with a step of ${(info.seam * 100).toFixed(0)}% of full scale, larger than any step inside the sound (${(info.maxStep * 100).toFixed(0)}%): it would click`);
  return out;
}

// ---------------------------------------------------------------- .meta and .acp text
const PLATFORMS = ["XBOX_ONE", "XBOX_SERIES", "PS4", "HEADLESS"];

export function wavMeta(resPath, guid) {
  const conf = { PC: "F72B05D02F3F135E", XBOX_ONE: "A0776163143143AC", XBOX_SERIES: "382981CA1FC1C8DB", PS4: "89EB939911B4C093", HEADLESS: "D63592CA950AF3A8" };
  const rows = ["PC", ...PLATFORMS].map((p) => `  WAVResourceClass ${p} : "{${conf[p]}}Configs/System/ResourceTypes/${p}/Sound.conf" {\n  }`);
  return `MetaFileClass {\n Name "{${guid}}${resPath}"\n Configurations {\n${rows.join("\n")}\n  WAVResourceClass PS5 : PC {\n  }\n }\n}\n`;
}

export function acpMeta(resPath, guid) {
  const rows = PLATFORMS.map((p) => `  AudioProjectResourceClass ${p} : PC {\n  }`);
  return `MetaFileClass {\n Name "{${guid}}${resPath}"\n Configurations {\n  AudioProjectResourceClass PC {\n  }\n${rows.join("\n")}\n }\n}\n`;
}

const link = (id, port, pad) => `${pad}ConnectionClass connection {\n${pad} id ${id}\n${pad} port ${port}\n${pad}}`;
const conns = (id, links, pad = "    ") => `${pad}ConnectionsClass connections {\n${pad} id ${id}${links.length ? `\n${pad} links {\n${links.map(([i, p]) => link(i, p, pad + "  ")).join("\n")}\n${pad} }` : ""}\n${pad}}`;

// Node ids are index * 1024 + a type code, as the Audio Editor writes them.
const ID = { bank: 4, sound: 9, mixer: 11, signal: 13, shader: 18, amplitude: 19, frequency: 21, spatiality: 28, auxOut: 29 };
const nid = (index, type) => index * 1024 + ID[type];
const SIG = { spatiality: nid(64, "signal"), reverb: nid(69, "signal"), occlusion: nid(71, "signal") };
const MIXER = nid(35, "mixer");
const FREQ = nid(4, "frequency");
const SPAT = nid(65, "spatiality");
const AUX = [
  { id: nid(67, "auxOut"), name: "AuxOut Rev Small", port: 300039, reverbPort: 107 },
  { id: nid(68, "auxOut"), name: "AuxOut Rev Medium", port: 301063, reverbPort: 106 },
  { id: nid(69, "auxOut"), name: "AuxOut Rev Large", port: 302087, reverbPort: 110 },
];

const SIGNALS = ` signals {
  SignalClass {
   id ${SIG.spatiality}
   name "WPN Handling Spatiality"
   tl 432 -1248
   res "{1387EB9E4DBEB1F2}Sounds/_SharedData/Signals/Weapons/Spatiality/Weapons_Handling_Spatiality.sig"
   inputsport {
    1 3
   }
   outputsport {
    9
   }
   inputvalues {
    0 1
   }
   inputvaluesmin {
    0 0
   }
   inputvaluesmax {
    1 1
   }
   path "{1387EB9E4DBEB1F2}Sounds/_SharedData/Signals/Weapons/Spatiality/Weapons_Handling_Spatiality.sig"
  }
  SignalClass {
   id ${SIG.reverb}
   name "Reverb Base"
   tl 1088 -224
   res "{76CBB40EF7F227B6}Sounds/_SharedData/Signals/Reverb/Reverb_Base.sig"
   inputsport {
    1 35 40 112 139
   }
   outputsport {
    34 39 105 107 106 110
   }
   inputvalues {
    100 0.5 0.5 16000 16000
   }
   inputvaluesmin {
    0 0 0 0 0
   }
   inputvaluesmax {
    100 1 1 16000 16000
   }
   path "{76CBB40EF7F227B6}Sounds/_SharedData/Signals/Reverb/Reverb_Base.sig"
  }
  SignalClass {
   id ${SIG.occlusion}
   name "Occlusion Props"
   tl 256 -1040
   res "{A45B14CDEA2D5DCD}Sounds/_SharedData/Signals/Props/Props_Occlusion.sig"
   inputsport {
    1 6 7 23 52 56
   }
   outputsport {
    5 37
   }
   inputvalues {
    0 0 0 0 1000 400
   }
   inputvaluesmin {
    0 0 0 0 0 0
   }
   inputvaluesmax {
    1 1 1 1 10000 10000
   }
   path "{A45B14CDEA2D5DCD}Sounds/_SharedData/Signals/Props/Props_Occlusion.sig"
  }
 }`;

const MIXER_INPUTS = ["WPN_Handling", "WPN_Shots", "WPN_Explosions", "WNP_BulletHits", "CHAR", "ENV_AMB_2D", "VEH_Animations", "Impacts", "Dialog", "Music", "ENV_Doors", "VEH_Engine", "VEH_Tires", "VON", "SFX", "SFX_Reverb", "VON_Reverb", "Dialog_Reverb", "Impacts_EXT", "ENV_AMB_3D", "WPN_SonicCracks", "CHAR_Gear", "PA", "SFX_Reverb_Exterior", "UI", "ENV_AMB_3D_Reverb_Exterior", "SFX_Direct", "SFX_Reverb_Small", "SFX_Reverb_Medium", "SFX_Reverb_Large", "WPN_Shots_Player", "Dialog_Reverb_Small", "Dialog_Reverb_Medium", "Dialog_Reverb_Large", "WPN_TravelingProjectile", "Dialog_Delay_Exterior"];

// One sound project. events: [{ event, bus, range, volume_db, pitch_variance, loop, samples: ["{GUID}path.wav"] }].
// Per event: Sound <- Shader <- Bank, the shader also fed by one amplitude per range, the shared
// frequency and spatiality nodes; every sound goes to its mixer input and to the three reverb sends.
export function acpText(events) {
  if (!events.length) throw new Error("a sound project needs at least one event");
  const ranges = [...new Set(events.map((e) => e.range))];
  const amp = (range) => nid(50 + ranges.indexOf(range), "amplitude");
  const ev = events.map((e, i) => ({ ...e, sound: nid(100 + i, "sound"), shader: nid(200 + i, "shader"), bank: nid(300 + i, "bank"), y: i * 280 }));
  for (const e of ev) {
    if (!BUS[e.bus]) throw new Error(`event ${e.event}: unknown bus ${e.bus}`);
    if (!RANGE[e.range]) throw new Error(`event ${e.event}: unknown range ${e.range}`);
    if (!e.samples?.length) throw new Error(`event ${e.event}: no samples`);
  }
  const sounds = ev.map((e) => `  SoundClass {
   id ${e.sound}
   name "${e.event}"
   version 5
   tl 680 ${e.y}
   ins {
${conns(64, [[e.shader, 65]])}
   }
   pi {
    127 0
   }
   outState ${MIXER}
   outStatePort ${BUS[e.bus]}
  }`);
  const buses = [...new Set(ev.map((e) => BUS[e.bus]))].sort((a, b) => a - b);
  const mixerIns = [
    ...buses.map((port) => conns(port, ev.filter((e) => BUS[e.bus] === port).map((e) => [e.sound, 65]))),
    ...AUX.map((a) => conns(a.port, [[a.id, 65]])),
  ];
  const shaders = ev.map((e) => `  ShaderClass {
   id ${e.shader}
   name "${e.event}"
   version 5
   tl 432 ${e.y}
   ins {
${conns(0, [[SPAT, 65]])}
${conns(1, [[amp(e.range), 65]])}
${conns(2, [[FREQ, 65]])}
${conns(64, [[e.bank, 65]])}
   }
   pi {
    23 0
   }
  }`);
  const amplitudes = ranges.map((r, i) => `  AmplitudeClass : "${RANGE[r]}" {
   id ${amp(r)}
   version 5
   tl 432 ${-400 - i * 80}
   parent "${RANGE[r]}"
  }`);
  const banks = ev.map((e) => {
    const props = [`   Volume ${e.volume_db}`];
    if (e.loop) props.push('   "Loop count" 255', '   "Infinite loop" 1', '   "Termination Fade Out" 150', '   "Fade in time" 40');
    else if (e.pitch_variance !== 0) props.push('   "Volume variance" 1', `   "Pitch variance" ${e.pitch_variance ?? 0.5}`);
    const samples = e.samples.map((s, i) => `    AudioBankSampleClass Sample {\n     Filename "${s}"${i ? `\n     Index ${i}` : ""}\n    }`);
    return `  BankLocalClass {
   id ${e.bank}
   name "${e.event.replace(/^SOUND_/, "")}"
   version 7
   tl 150 ${e.y}
${props.join("\n")}
   Samples {
${samples.join("\n")}
   }
  }`;
  });
  const aux = AUX.map((a, i) => `  AuxOutClass {
   id ${a.id}
   name "${a.name}"
   version 5
   tl 1088 ${-32 + i * 112}
   ins {
${conns(0, ev.map((e) => [e.sound, 5]))}
${conns(1, [[SIG.reverb, a.reverbPort]])}
${conns(2, [[SIG.reverb, 39]])}
   }
   pi {
    7 0
   }
   OSPort ${a.port}
  }`);
  return `AudioClass {
${SIGNALS}
 sounds {
${sounds.join("\n")}
 }
 mixers {
  MixerClass {
   id ${MIXER}
   name "OutputState"
   version 4
   tl 1488 -288
   res "{B764D803219C775E}Sounds/FinalMix.afm"
   ins {
${mixerIns.join("\n")}
   }
   pi {
    947945475 0
   }
   path "{B764D803219C775E}Sounds/FinalMix.afm"
   inputs {
    ${MIXER_INPUTS.map((n) => `"${n}"`).join(" ")}
   }
  }
 }
 shaders {
${shaders.join("\n")}
 }
 amplitudes {
${amplitudes.join("\n")}
 }
 frequency {
  FrequencyClass : "{65257CB7AD05C1FD}Sounds/_SharedData/Configs/Frequency/Weapons/Frequency_Weapons_Handling_Rifle.conf" {
   id ${FREQ}
   version 5
   tl 432 -1008
   parent "{65257CB7AD05C1FD}Sounds/_SharedData/Configs/Frequency/Weapons/Frequency_Weapons_Handling_Rifle.conf"
   ins {
${conns(4, [[SIG.occlusion, 37]])}
   }
   pi {
    16 0
   }
  }
 }
 banks_local {
${banks.join("\n")}
 }
 spatialities {
  SpatialityClass : "{68C6E66C2A370788}Sounds/_SharedData/Configs/Spatiality/Spatiality_Signal_Update.conf" {
   id ${SPAT}
   version 5
   tl 432 -1152
   parent "{68C6E66C2A370788}Sounds/_SharedData/Configs/Spatiality/Spatiality_Signal_Update.conf"
   ins {
${conns(0, [[SIG.spatiality, 9]])}
   }
   pi + {
    0
   }
   pu {
    1 0
   }
  }
 }
 auxOuts {
${aux.join("\n")}
 }
 version 1
}
`;
}

// Every node a link names must exist in the project, and every sample must be a {GUID}path.wav.
export function acpProblems(text) {
  const out = [];
  const ids = new Set([...text.matchAll(/^   id (\d+)$/gm)].map((m) => m[1]));
  for (const m of text.matchAll(/ConnectionClass connection \{\s+id (\d+)/g)) if (!ids.has(m[1])) out.push(`a link names node ${m[1]}, which the project does not have`);
  for (const m of text.matchAll(/Filename "([^"]*)"/g)) if (!/^\{[0-9A-F]{16}\}Sounds\/.+\.wav$/.test(m[1])) out.push(`sample ${m[1]} is not a {GUID}Sounds/...wav reference`);
  if ((text.match(/{/g) || []).length !== (text.match(/}/g) || []).length) out.push("the braces do not balance");
  if (!/name "SOUND_/.test(text)) out.push("no sound event");
  return [...new Set(out)];
}

// ---------------------------------------------------------------- stage steps
export function ffmpegExe(root) {
  const local = join(root, "tools", "film", ".bin", "ffmpeg.exe");
  return process.env.FFMPEG_PATH || (existsSync(local) ? local : "ffmpeg");
}

function ffmpeg(root, args, { binary = false } = {}) {
  const res = spawnSync(ffmpegExe(root), ["-hide_banner", "-loglevel", "error", "-y", ...args], { windowsHide: true, timeout: 120000, maxBuffer: 256 * 1024 * 1024, encoding: binary ? "buffer" : "utf8" });
  if (res.error) throw new Error(`ffmpeg: ${res.error.message} (run: node tools/film/film.mjs fetch)`);
  if (res.status !== 0) throw new Error(`ffmpeg exited ${res.status}: ${String(res.stderr).trim().split("\n").slice(-3).join(" ")}`);
  return res.stdout;
}

// Decode any take to 48 kHz mono 16-bit samples. A `.pcm<rate>` take is the API's raw pcm_<rate>
// output: headerless 16-bit samples, stereo for sound effects (seen 2026-11-13; the row keeps the count).
export function decode(root, file, channels = 2) {
  const raw = /\.pcm(\d+)$/.exec(file);
  const input = raw ? ["-f", "s16le", "-ar", raw[1], "-ac", String(channels), "-i", file] : ["-i", file];
  const out = ffmpeg(root, [...input, "-ac", "1", "-ar", String(RATE), "-f", "s16le", "-"], { binary: true });
  return new Int16Array(out.buffer, out.byteOffset, Math.floor(out.length / 2)).slice();
}

const readRec = (root, slug) => {
  const file = soundsFile(root, slug);
  if (!existsSync(file)) throw new Error(`no ${file}; start with: tools\\cli asset sound ${slug} --init`);
  return JSON.parse(readFileSync(file, "utf8"));
};
const writeRec = (root, slug, rec) => writeFileSync(soundsFile(root, slug), JSON.stringify(rec, null, 2) + "\n");
const takeName = (s, n) => `${s.id}_${String(n).padStart(2, "0")}`;
const rawExt = (format) => (format.startsWith("pcm_") ? `pcm${format.slice(4)}` : format.split("_")[0]);

// How many interleaved channels a raw PCM answer holds, from its size and the asked duration.
export const rawChannels = (format, bytes, seconds) => (format.startsWith("pcm_") ? Math.max(1, Math.round(bytes / (Number(format.slice(4)) * 2 * seconds))) : undefined);

export async function stepGenerate(root, slug, { again, now = new Date(), fetchImpl, settleMs = 10000 } = {}) {
  const rec = readRec(root, slug);
  const problems = soundProblems(rec);
  if (problems.length) throw new Error("sounds.json problems:\n  " + problems.join("\n  "));
  const gens = (rec.generations ||= []);
  const dir = join(soundCache(root, slug), "raw");
  const format = rec.output_format || "mp3_44100_128";
  const todo = [];
  for (const s of rec.sounds) {
    const have = gens.filter((g) => g.id === s.id).map((g) => g.take);
    const want = s.variations + (again === s.id ? 1 : 0);
    for (let n = 1; todo.filter((t) => t.s === s).length + have.length < want; n++) if (!have.includes(n)) todo.push({ s, n });
  }
  if (again && !rec.sounds.some((s) => s.id === again)) throw new Error(`--again ${again}: no such sound`);
  if (!todo.length) { console.log(`generate: nothing to do, ${gens.length} generation(s) on record`); return rec; }
  if (gens.length + todo.length > rec.budget.generations) {
    throw new Error(`${gens.length} generation(s) on record and ${todo.length} more asked for; the cap is ${rec.budget.generations}. Stop and report (docs/asset-pipeline/sounds.md "Credits").`);
  }
  const o = fetchImpl ? { fetchImpl } : {};
  const start = await subscription(o);
  console.log(`ElevenLabs ${start.tier}: ${start.used} of ${start.limit} credits used; ${todo.length} generation(s), about ${todo.reduce((n, t) => n + estimateCredits(t.s), 0)} credits`);
  for (const { s, n } of todo) {
    const audio = await generate(s, { outputFormat: format, ...o });
    const file = join(dir, `${takeName(s, n)}.${rawExt(format)}`);
    mkdirSync(dir, { recursive: true });
    writeFileSync(file, audio);
    gens.push({
      id: s.id, take: n, date: now.toISOString().slice(0, 10), tool: "ElevenLabs text-to-sound-effects", model: s.model_id || "default",
      prompt: s.prompt, duration_seconds: s.duration_seconds, prompt_influence: s.prompt_influence, loop: !!s.loop,
      output_format: format, channels: rawChannels(format, audio.length, s.duration_seconds), bytes: audio.length, credits_estimate: estimateCredits(s),
    });
    writeRec(root, slug, rec); // after every paid call: a run that dies loses nothing
    console.log(`  ${takeName(s, n)}: ${s.duration_seconds} s, ${audio.length} bytes`);
  }
  // The account's counter lags a generation by some seconds, so the cost is measured per run, not
  // per call: one read before the first call and one after the last has settled.
  await new Promise((r) => setTimeout(r, settleMs));
  const end = await subscription(o);
  (rec.runs ||= []).push({ date: now.toISOString().slice(0, 10), generations: todo.length, seconds: todo.reduce((n, t) => n + t.s.duration_seconds, 0), plan: start.tier, credits_before: start.used, credits_after: end.used, credits: end.used - start.used });
  writeRec(root, slug, rec);
  console.log(`generate: ${todo.length} new, ${gens.length} in total; this run cost ${end.used - start.used} credits (balance ${start.used} -> ${end.used} of ${end.limit})`);
  return rec;
}

export function stepProcess(root, slug) {
  const rec = readRec(root, slug);
  const cache = soundCache(root, slug);
  const out = join(cache, "wav");
  const review = join(cache, "review");
  mkdirSync(out, { recursive: true });
  mkdirSync(review, { recursive: true });
  let bad = 0;
  for (const s of rec.sounds) {
    for (const g of (rec.generations || []).filter((x) => x.id === s.id)) {
      const name = takeName(s, g.take);
      const raw = join(cache, "raw", `${name}.${rawExt(g.output_format)}`);
      if (!existsSync(raw)) { console.log(`  ${name}: raw take missing at ${raw} (the cache is per worktree)`); bad++; continue; }
      const wav = join(out, `${name}.wav`);
      let y = processSamples(decode(root, raw, g.channels), { loop: !!s.loop, trimDb: s.trim_db });
      if (s.layer) {
        const over = rec.sounds.find((o) => o.id === s.layer.id);
        const og = (rec.generations || []).find((x) => x.id === over.id && x.take === s.layer.take);
        const overRaw = og && join(cache, "raw", `${takeName(over, og.take)}.${rawExt(og.output_format)}`);
        if (!og || !existsSync(overRaw)) throw new Error(`${name}: layer take ${s.layer.id} ${s.layer.take} is missing`);
        const top = processSamples(decode(root, overRaw, og.channels), { trimDb: over.trim_db });
        // Halve both first so the sum cannot clip, then bring the peak back.
        y = normalisePeak(mixLayer(normalisePeak(y, -9), normalisePeak(top, -9), RATE, { gainDb: s.layer.gain_db ?? 0, delayMs: s.layer.delay_ms ?? 0 }), -3);
      }
      if (s.compose && g.take === s.compose.from_take) {
        // The composed take stands in the pick list under its own number, built from this one.
        const composed = normalisePeak(alternateSteps(y, s.compose.steps, s.compose.repeat ?? 1), -3);
        const cname = takeName(s, s.compose.as_take);
        const cwav = join(out, `${cname}.wav`);
        writeFileSync(cwav, encodeWav(composed));
        const cinfo = wavInfo(readFileSync(cwav));
        const cproblems = checkWav(cinfo, { loop: !!s.loop });
        bad += cproblems.length;
        ffmpeg(root, ["-i", cwav, "-filter_complex", "[0:a]asplit[a][b];[a]showwavespic=s=900x160:colors=white,format=rgb24[w];[b]showspectrumpic=s=900x340:legend=0:stop=16000,format=rgb24[s];[w][s]vstack", "-frames:v", "1", join(review, `${cname}.png`)]);
        console.log(`  ${cname}: composed from ${name}, ${cinfo.seconds.toFixed(2)} s, peak ${cinfo.peakDb.toFixed(1)} dB, rms ${cinfo.rmsDb.toFixed(1)} dB${s.loop ? `, seam ${(cinfo.seam * 100).toFixed(1)}% (largest inner step ${(cinfo.maxStep * 100).toFixed(0)}%)` : ""}`);
        for (const p of cproblems) console.log(`FAIL  ${cname}: ${p}`);
      }
      if (s.derive && g.take === s.derive.from_take) {
        // A derived take stands in the pick list under its own number: cut and gated from this one.
        let d = deriveTake(y, { fromMs: s.derive.from_ms ?? 0, onMs: s.derive.on_ms ?? 0, offMs: s.derive.off_ms ?? 0 });
        d = normalisePeak(s.loop ? loopSeam(d) : d, -3);
        const dname = takeName(s, s.derive.as_take);
        const dwav = join(out, `${dname}.wav`);
        writeFileSync(dwav, encodeWav(d));
        const dinfo = wavInfo(readFileSync(dwav));
        const dproblems = checkWav(dinfo, { loop: !!s.loop });
        bad += dproblems.length;
        ffmpeg(root, ["-i", dwav, "-filter_complex", "[0:a]asplit[a][b];[a]showwavespic=s=900x160:colors=white,format=rgb24[w];[b]showspectrumpic=s=900x340:legend=0:stop=16000,format=rgb24[s];[w][s]vstack", "-frames:v", "1", join(review, `${dname}.png`)]);
        console.log(`  ${dname}: derived from ${name}, ${dinfo.seconds.toFixed(2)} s, peak ${dinfo.peakDb.toFixed(1)} dB, rms ${dinfo.rmsDb.toFixed(1)} dB${s.loop ? `, seam ${(dinfo.seam * 100).toFixed(1)}% (largest inner step ${(dinfo.maxStep * 100).toFixed(0)}%)` : ""}`);
        for (const p of dproblems) console.log(`FAIL  ${dname}: ${p}`);
      }
      writeFileSync(wav, encodeWav(y));
      const info = wavInfo(readFileSync(wav));
      const problems = checkWav(info, { loop: !!s.loop });
      bad += problems.length;
      // The review picture: waveform above, spectrogram to 16 kHz below. An agent cannot listen; it reads this.
      ffmpeg(root, ["-i", wav, "-filter_complex", "[0:a]asplit[a][b];[a]showwavespic=s=900x160:colors=white,format=rgb24[w];[b]showspectrumpic=s=900x340:legend=0:stop=16000,format=rgb24[s];[w][s]vstack", "-frames:v", "1", join(review, `${name}.png`)]);
      console.log(`  ${name}: ${info.seconds.toFixed(2)} s, peak ${info.peakDb.toFixed(1)} dB, rms ${info.rmsDb.toFixed(1)} dB, peak at ${info.peakAtMs.toFixed(0)} ms, first-100ms energy ${(info.attackShare * 100).toFixed(0)}%, zero-cross ${info.zeroCrossHz.toFixed(0)} Hz${s.loop ? `, seam ${(info.seam * 100).toFixed(1)}% (largest inner step ${(info.maxStep * 100).toFixed(0)}%)` : ""}`);
      for (const p of problems) console.log(`FAIL  ${name}: ${p}`);
    }
  }
  console.log(`process: WAVs in ${out}, review pictures in ${review}`);
  if (bad) throw new Error(`${bad} problem(s) in the processed takes`);
  console.log('next: read the pictures (docs/asset-pipeline/sounds.md "Review"), set "pick" and "reason" per sound, then --only wire');
}

// `target` is the addon to wire into: the asset spec's `addon`, or `--addon` (#971).
export function stepWire(root, slug, target = slugTarget(root, slug)) {
  const rec = readRec(root, slug);
  const targetProblems = addonProblems(root, target);
  if (targetProblems.length) throw new Error("addon problems:\n  " + targetProblems.join("\n  "));
  const problems = soundProblems(rec);
  if (problems.length) throw new Error("sounds.json problems:\n  " + problems.join("\n  "));
  const unpicked = rec.sounds.filter((s) => !s.layer_only && !s.pick?.length).map((s) => s.id);
  if (unpicked.length) throw new Error(`no pick for: ${unpicked.join(", ")}. Review the takes, then set "pick" and "reason" in sounds.json.`);
  const addon = addonPath(root, target);
  const used = guidsInUse(root);
  const cache = join(soundCache(root, slug), "wav");
  const place = (rel, text) => {
    const file = join(addon, rel);
    const guid = readGuid(file + ".meta") || freshGuid(used);
    mkdirSync(dirname(file), { recursive: true });
    writeFileSync(file + ".meta", text(guid));
    return `{${guid}}${rel}`;
  };
  const projects = {};
  for (const s of rec.sounds.filter((x) => !x.layer_only)) {
    const samples = s.pick.map((n, i) => {
      const src = join(cache, `${takeName(s, n)}.wav`);
      if (!existsSync(src)) throw new Error(`${src} is missing; run --only process (take ${n} of ${s.id})`);
      const info = wavInfo(readFileSync(src));
      const bad = checkWav(info, { loop: !!s.loop });
      if (bad.length) throw new Error(`${takeName(s, n)}: ${bad[0]}`);
      const rel = `${rec.project_dir}/Samples/${rec.project_prefix}_${s.event.replace(/^SOUND_/, "")}_${String(i + 1).padStart(2, "0")}.wav`;
      const ref = place(rel, (guid) => wavMeta(rel, guid));
      copyFileSync(src, join(addon, rel));
      return ref;
    });
    (projects[s.project] ||= []).push({ ...s, samples });
  }
  rec.wired = {};
  for (const [project, events] of Object.entries(projects)) {
    const rel = `${rec.project_dir}/${rec.project_prefix}_${project}.acp`;
    const text = acpText(events);
    const bad = acpProblems(text);
    if (bad.length) throw new Error(`${rel}: ${bad[0]}`);
    const ref = place(rel, (guid) => acpMeta(rel, guid));
    writeFileSync(join(addon, rel), text);
    rec.wired[project] = { project: ref, events: events.map((e) => e.event), samples: events.flatMap((e) => e.samples) };
    console.log(`  ${ref}: ${events.map((e) => `${e.event} x${e.samples.length}`).join(", ")}`);
  }
  if (addonOf(target) !== "LifeFramework") rec.addon = addonOf(target);
  else delete rec.addon;
  writeRec(root, slug, rec);
  assertBoundary(root, target);
  const kind = SOUND_KINDS[rec.kind];
  console.log(`wire: ${Object.keys(projects).length} project(s) under addons/${addonOf(target)}/${rec.project_dir}`);
  if (kind?.replaces.length) console.log(`next: in the weapon prefab, override WeaponSoundComponent "Filenames": drop ${kind.replaces.join(" and ")}, list the projects above, keep the rest (docs/asset-pipeline/sounds.md "Wiring")`);
  return rec;
}

function cmdInit(root, slug) {
  const file = soundsFile(root, slug);
  if (existsSync(file)) throw new Error(`${file} exists; edit it`);
  const specFile = join(root, "tools", "assets", slug, "spec.json");
  if (!existsSync(specFile)) throw new Error(`no spec at ${specFile}; the asset comes first (tools\\cli asset new)`);
  const spec = JSON.parse(readFileSync(specFile, "utf8"));
  const kind = SOUND_KINDS[spec.kind];
  const rec = {
    slug, name: spec.name, kind: spec.kind,
    project_dir: `${kind.projectRoot}/${spec.name}`, project_prefix: `${kind.projectRoot.split("/").slice(1).join("_")}_${spec.name}`,
    output_format: "mp3_44100_128", budget: { generations: 20 },
    sounds: kind.events.map((e) => ({ ...e, prompt: "", prompt_influence: 0.6, variations: 3 })),
    generations: [],
  };
  writeFileSync(file, JSON.stringify(rec, null, 2) + "\n");
  console.log(`wrote ${file}; write the prompts (docs/asset-pipeline/sounds.md), then: tools\\cli asset sound ${slug}`);
  return 0;
}

function cmdStatus(root, slug) {
  const rec = readRec(root, slug);
  const gens = rec.generations || [];
  console.log(`${slug}: ${gens.length} of ${rec.budget.generations} generation(s), ${gens.reduce((n, g) => n + g.duration_seconds, 0).toFixed(1)} s of audio, ${rec.account ? `${rec.account.used - (rec.account.used_before_first_run ?? 0)} credits on the account's counter (${rec.account.date})` : "no settled reading yet (--settle)"}`);
  for (const s of rec.sounds) console.log(`  ${s.id.padEnd(10)} ${(s.event || "(layer)").padEnd(26)} takes ${gens.filter((g) => g.id === s.id).length}/${s.variations}  pick ${s.pick ? s.pick.join(",") : "-"}`);
  return 0;
}

// The account's counter runs minutes behind a generation (seen 2026-11-13: 142, then 162, then 192
// for the same takes), so a run's own "after" is too low. Settle reads it again later and keeps
// the reading beside the first run's "before".
async function cmdSettle(root, slug, now = new Date()) {
  const rec = readRec(root, slug);
  const sub = await subscription();
  const first = (rec.runs || [])[0];
  rec.account = { date: now.toISOString().slice(0, 10), plan: sub.tier, used: sub.used, limit: sub.limit, used_before_first_run: first?.credits_before, note: "used - used_before_first_run is this asset's cost only if nothing else spent credits on the account in between" };
  writeRec(root, slug, rec);
  console.log(`${slug}: account ${sub.tier}, ${sub.used} of ${sub.limit} credits used; ${first ? sub.used - first.credits_before : "?"} since this asset's first run`);
  return 0;
}

export async function main(root, flags) {
  const value = (name) => { const i = flags.indexOf(name); return i >= 0 ? flags[i + 1] : undefined; };
  const only = value("--only");
  const again = value("--again");
  const addon = addonFlag(flags);
  const slug = flags.find((f) => !f.startsWith("--") && f !== only && f !== again && f !== addon);
  const known = ["--init", "--status", "--settle", "--only", "--again", "--addon"];
  if (!slug || flags.some((f) => f.startsWith("--") && !known.includes(f)) || (only && !["generate", "process", "wire"].includes(only))) {
    console.log("usage: tools\\cli asset sound <slug> [--init | --status | --settle | --only generate|process|wire] [--again <sound id>] [--addon <name>]\nplaybook: docs/asset-pipeline/sounds.md");
    return 2;
  }
  if (flags.includes("--status")) return cmdStatus(root, slug);
  const wt = await import("../../wt.mjs");
  if (wt.isMainCheckout(root)) { console.log("FAIL  asset sound runs only in a worktree"); return 1; }
  if (flags.includes("--init")) return cmdInit(root, slug);
  if (flags.includes("--settle")) return cmdSettle(root, slug);
  if (!only || only === "generate") { console.log("--- sound: generate"); await stepGenerate(root, slug, { again }); }
  if (!only || only === "process") { console.log("--- sound: process"); stepProcess(root, slug); }
  if (only === "wire" || (!only && readRec(root, slug).sounds.every((s) => s.layer_only || s.pick?.length))) { console.log("--- sound: wire"); stepWire(root, slug, slugTarget(root, slug, addon)); }
  return 0;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  process.exit(await main(resolve(dirname(fileURLToPath(import.meta.url)), "..", "..", ".."), process.argv.slice(2)));
}
