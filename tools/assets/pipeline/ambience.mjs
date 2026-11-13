// tools\cli asset ambience: music and looped background sound for LF_AmbientEmitterComponent
// (docs/architecture/atmosphere.md). The spec and the credit record are tools/assets/atmosphere/ambience.json.
//
//   asset ambience [--only generate|process|wire] [--again <id>]
//   asset ambience --status
//
// generate  ElevenLabs: `kind: "music"` goes to the music API (instrumental only), `kind: "loop"` to
//           text-to-sound-effects with loop on. The account's balance is read before and after every
//           call, so the record holds the real cost of each take.
// process   ffmpeg decodes to mono at the sound's own rate (a radio is band-limited like the vanilla
//           broadcast samples); the loop seam is an equal-power crossfade, then peak normalise.
// wire      copies the picked take into the addon with its .meta and writes one sound project per
//           sound: a positioned loop with an inline distance curve (inner_m, outer_m).
//
// Reused, not copied: the HTTP client is the dialogue stage's (elevenlabs.mjs), the sample maths,
// WAV writer and .meta text are the sound stage's (sound.mjs). The project graph is the vanilla prop
// loop's (Structures_DieselGenerator_01.acp, Props_TapeRecorder_01.acp, read from the game 2026-11-13)
// without its trigger signal: LF_Atmosphere starts and stops the voice.
import { copyFileSync, existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import * as eleven from "./elevenlabs.mjs";
import { acpMeta, acpProblems, encodeWav, ffmpegExe, loopSeam, normalisePeak, trimSilence, wavInfo, wavMeta } from "./sound.mjs";
import { freshGuid, readGuid, usedGuids } from "./meta.mjs";

export const EVENT = "SOUND_LF_AMBIENT_LP";
export const PROJECT_DIR = "Sounds/Atmosphere";
export const MAX_CREDITS = 12000;
// Measured on the Starter plan on 2026-11-13: ten generations (140 s of music, 158 s of loops) moved
// the balance by 3330, which is exactly 12.5 a second for music and 10 for a loop. The price list
// says 15 and 40. The account counter lags a call by up to a minute, so the cost of a run is one
// read before and one after (`runs`), and each take carries its share at these rates.
export const CREDITS_PER_SECOND = { music: 12.5, loop: 10 };
export const RATES = [16000, 24000, 32000, 44100, 48000];

export const specFile = (root) => join(root, "tools", "assets", "atmosphere", "ambience.json");
export const cacheDir = (root) => join(root, "tmp", "assets", "atmosphere");
export const projectPath = (s) => `${PROJECT_DIR}/Atmosphere_${s.name}.acp`;
export const samplePath = (s) => `${PROJECT_DIR}/Samples/Atmosphere_${s.name}_LP.wav`;
export const estimate = (s) => Math.round(s.seconds * CREDITS_PER_SECOND[s.kind]);
export const slopeOf = (s) => s.slope ?? Math.max(2, Math.round((s.outer_m / 13) * 10) / 10);

// Everything wrong with the spec, as a list.
export function specProblems(rec) {
  const out = [];
  if (!(rec.budget?.credits > 0 && rec.budget.credits <= MAX_CREDITS)) out.push(`budget.credits must be 1 to ${MAX_CREDITS}`);
  const ids = new Set();
  const names = new Set();
  for (const s of rec.sounds || []) {
    const at = `sound ${s.id || "?"}`;
    if (!/^[a-z][a-z0-9_]*$/.test(s.id || "")) out.push(`${at}: id must be lowercase letters, digits and underscores`);
    if (ids.has(s.id)) out.push(`${at}: id is used twice`);
    ids.add(s.id);
    if (!/^[A-Z][A-Za-z0-9]*$/.test(s.name || "")) out.push(`${at}: name must be PascalCase (it becomes file names)`);
    if (names.has(s.name)) out.push(`${at}: name ${s.name} is used twice`);
    names.add(s.name);
    if (!CREDITS_PER_SECOND[s.kind]) out.push(`${at}: kind must be music or loop`);
    if (!s.prompt) out.push(`${at}: prompt is empty`);
    else if (s.prompt.length > 450) out.push(`${at}: prompt is ${s.prompt.length} characters; keep it under 450`);
    if (s.kind === "loop" && !(s.seconds >= 2 && s.seconds <= 30)) out.push(`${at}: a loop is 2 to 30 seconds (the API's limit)`);
    if (s.kind === "music" && !(s.seconds >= 20 && s.seconds <= 120)) out.push(`${at}: a music track is 20 to 120 seconds`);
    if (!RATES.includes(s.rate)) out.push(`${at}: rate must be one of ${RATES.join(", ")}`);
    if (!(s.volume_db <= 0 && s.volume_db >= -40)) out.push(`${at}: volume_db must be -40 to 0`);
    if (!(s.inner_m > 0 && s.outer_m > s.inner_m && s.outer_m <= 120)) out.push(`${at}: need 0 < inner_m < outer_m <= 120`);
    if (!(s.seam_ms >= 20 && s.seam_ms <= 5000)) out.push(`${at}: seam_ms must be 20 to 5000`);
    if (!(s.takes >= 1 && s.takes <= 3)) out.push(`${at}: takes must be 1 to 3`);
    if (s.pick !== undefined && !(Number.isInteger(s.pick) && s.pick >= 1)) out.push(`${at}: pick must be a take number`);
    if (s.pick && !s.reason) out.push(`${at}: a pick needs a reason`);
  }
  if (!(rec.sounds || []).length) out.push("sounds is empty");
  return out;
}

// What a processed loop must be before it goes in the addon.
export function loopProblems(info, s) {
  const out = [];
  if (info.tag !== 1 || info.bits !== 16) out.push("not 16-bit PCM");
  if (info.channels !== 1) out.push(`${info.channels} channels; a positioned sound is mono`);
  if (info.rate !== s.rate) out.push(`${info.rate} Hz, the spec says ${s.rate}`);
  if (out.length) return out;
  if (info.seconds < Math.min(8, s.seconds * 0.6)) out.push(`only ${info.seconds.toFixed(1)} s long`);
  if (info.peakDb > -0.5) out.push(`peak ${info.peakDb.toFixed(1)} dBFS leaves no headroom`);
  if (info.peakDb < -9) out.push(`peak ${info.peakDb.toFixed(1)} dBFS is too quiet`);
  if (info.wrap > 1.5) out.push(`the step from the last sample to the first is ${info.wrap.toFixed(1)} times the largest step inside the take: it would click`);
  if (info.rmsDb < -45) out.push(`rms ${info.rmsDb.toFixed(1)} dBFS: the take is almost silent`);
  return out;
}

// ---------------------------------------------------------------- the sound project
const ID = { bank: 4, sound: 9, mixer: 11, signal: 13, shader: 18, amplitude: 19, frequency: 21, spatiality: 28, auxOut: 29 };
const nid = (index, type) => index * 1024 + ID[type];
const N = {
  sound: nid(1, "sound"), mixer: nid(27, "mixer"), shader: nid(3, "shader"), amplitude: nid(4, "amplitude"), frequency: nid(21, "frequency"),
  bank: nid(17, "bank"), spatiality: nid(29, "spatiality"), occlusion: nid(20, "signal"), reverb: nid(25, "signal"),
};
const AUX = [
  { id: nid(28, "auxOut"), name: "AuxOut Reverb INT Small", port: 300039, reverbPort: 107 },
  { id: nid(24, "auxOut"), name: "AuxOut Reverb INT Medium", port: 301063, reverbPort: 106 },
  { id: nid(26, "auxOut"), name: "AuxOut Reverb INT Large", port: 302087, reverbPort: 110 },
];
const SFX_PORT = 79879;
const MIXER_INPUTS = ["WPN_Handling", "WPN_Shots", "WPN_Explosions", "WNP_BulletHits", "CHAR", "ENV_AMB_2D", "VEH_Animations", "Impacts", "Dialog", "ENV_Doors", "VEH_Engine", "VEH_Tires", "VON", "SFX", "SFX_Reverb_OBSOLETE", "VON_Reverb_Small", "Dialog_Reverb", "Impacts_EXT", "ENV_AMB_3D", "WPN_SonicCracks", "CHAR_Gear", "PA", "SFX_Reverb_Exterior", "ENV_AMB_3D_Reverb_Exterior", "SFX_Direct", "SFX_Reverb_Small", "SFX_Reverb_Medium", "SFX_Reverb_Large", "WPN_Shots_Player", "Dialog_Reverb_Small", "Dialog_Reverb_Medium", "Dialog_Reverb_Large", "WPN_TravelingProjectile", "Dialog_Delay_Exterior", "SFX_Reverb_InCabin", "VON_Reverb_Medium", "VON_Reverb_Large"];

const con = (port, links) => `    ConnectionsClass connections {
     id ${port}
     links {
${links.map(([id, p]) => `      ConnectionClass connection {\n       id ${id}\n       port ${p}\n      }`).join("\n")}
     }
    }`;

// One positioned loop. The occlusion signal reads the source's Interior and RoomSize (set by
// SCR_SoundManagerModule when the voice starts) against the listener's GInterior and GRoomSize, so a
// source inside a building is muffled for a listener outside and the other way round.
export function ambientAcpText(s, sampleRef) {
  if (!/^\{[0-9A-F]{16}\}Sounds\/.+\.wav$/.test(sampleRef)) throw new Error(`sample ${sampleRef} is not a {GUID}Sounds/...wav reference`);
  return `AudioClass {
 signals {
  SignalClass {
   id ${N.occlusion}
   name "Signal Occlusion"
   tl -256 -176
   res "{A45B14CDEA2D5DCD}Sounds/_SharedData/Signals/Props/Props_Occlusion.sig"
   inputsport {
    23 52
   }
   outputsport {
    37
   }
   inputvalues {
    0 1000
   }
   inputvaluesmin {
    0 0
   }
   inputvaluesmax {
    1 10000
   }
   path "{A45B14CDEA2D5DCD}Sounds/_SharedData/Signals/Props/Props_Occlusion.sig"
  }
  SignalClass {
   id ${N.reverb}
   name "Signal Reverb"
   tl 256 112
   res "{76CBB40EF7F227B6}Sounds/_SharedData/Signals/Reverb/Reverb_Base.sig"
   inputsport {
    1 35 112
   }
   outputsport {
    39 105 107 106 110
   }
   inputvalues {
    40 0 0
   }
   inputvaluesmin {
    0 0 0
   }
   inputvaluesmax {
    100 1 16000
   }
   path "{76CBB40EF7F227B6}Sounds/_SharedData/Signals/Reverb/Reverb_Base.sig"
  }
 }
 sounds {
  SoundClass {
   id ${N.sound}
   name "${EVENT}"
   version 5
   ins {
${con(64, [[N.shader, 65]])}
   }
   varName "Props"
   varResource "{A60F08955792B575}Sounds/_SharedData/Variables/GlobalVariables.conf"
   outState ${N.mixer}
   outStatePort ${SFX_PORT}
  }
 }
 mixers {
  MixerClass {
   id ${N.mixer}
   name "OutputState"
   version 4
   tl 512 0
   res "{B764D803219C775E}Sounds/FinalMix.afm"
   ins {
${con(SFX_PORT, [[N.sound, 65]])}
${AUX.map((a) => con(a.port, [[a.id, 65]])).join("\n")}
   }
   pi {
    234889216 0
   }
   path "{B764D803219C775E}Sounds/FinalMix.afm"
   inputs {
    ${MIXER_INPUTS.map((n) => `"${n}"`).join(" ")}
   }
  }
 }
 shaders {
  ShaderClass {
   id ${N.shader}
   name "Shader"
   version 5
   tl -256 0
   ins {
${con(0, [[N.spatiality, 65]])}
${con(1, [[N.amplitude, 65]])}
${con(2, [[N.frequency, 65]])}
${con(64, [[N.bank, 65]])}
   }
   pi {
    7 0
   }
  }
 }
 amplitudes {
  AmplitudeClass {
   id ${N.amplitude}
   name "Amplitude ${s.inner_m} to ${s.outer_m} m"
   version 5
   tl -256 -256
   curve "1/r"
   innerRange ${s.inner_m}
   outerRange ${s.outer_m}
   slopeFactor ${slopeOf(s)}
  }
 }
 frequency {
  FrequencyClass : "{0005FE42A6D80399}Sounds/_SharedData/Configs/Frequency/_SharedData/Frequency_DynamicUpdate.conf" {
   id ${N.frequency}
   version 5
   tl -256 -96
   parent "{0005FE42A6D80399}Sounds/_SharedData/Configs/Frequency/_SharedData/Frequency_DynamicUpdate.conf"
   ins {
${con(4, [[N.occlusion, 37]])}
   }
   pi {
    16 0
   }
   pu {
    16 0
   }
  }
 }
 banks_local {
  BankLocalClass {
   id ${N.bank}
   name "${s.name}"
   version 7
   tl -512 0
   Volume ${s.volume_db}
   "Loop count" 255
   "Infinite loop" 1
   "Termination Fade Out" ${s.fade_out_ms ?? 900}
   "Random start offset" 1
   "Fade in time" ${s.fade_in_ms ?? 700}
   Samples {
    AudioBankSampleClass Sample {
     Filename "${sampleRef}"
    }
   }
  }
 }
 spatialities {
  SpatialityClass {
   id ${N.spatiality}
   name "Spatiality"
   version 5
   tl -256 -336
   spatialFactor 1
  }
 }
 auxOuts {
${AUX.map((a, i) => `  AuxOutClass {
   id ${a.id}
   name "${a.name}"
   version 5
   tl 256 ${288 + i * 128}
   ins {
${con(0, [[N.sound, 5]])}
${con(1, [[N.reverb, a.reverbPort]])}
${con(2, [[N.reverb, 39]])}
   }
   pi {
    7 0
   }
   pu {
    6 0
   }
   OSPort ${a.port}
  }`).join("\n")}
 }
 version 1
}
`;
}

// ---------------------------------------------------------------- steps
const readRec = (root) => {
  const file = specFile(root);
  if (!existsSync(file)) throw new Error(`no ${file}`);
  return JSON.parse(readFileSync(file, "utf8"));
};
const writeRec = (root, rec) => writeFileSync(specFile(root), JSON.stringify(rec, null, 2) + "\n");
const takeName = (s, n) => `${s.id}_${String(n).padStart(2, "0")}`;
export const spentCredits = (rec) => (rec.generations || []).reduce((n, g) => n + (g.credits ?? 0), 0);

export function requestFor(s) {
  if (s.kind === "music") return { path: "/v1/music?output_format=mp3_44100_128", body: { prompt: s.prompt, music_length_ms: Math.round(s.seconds * 1000), model_id: s.model_id || "music_v1", force_instrumental: true }, timeoutMs: 600000 };
  const body = { text: s.prompt, duration_seconds: s.seconds, prompt_influence: s.prompt_influence ?? 0.5, loop: true };
  if (s.model_id) body.model_id = s.model_id;
  return { path: "/v1/sound-generation?output_format=mp3_44100_128", body, timeoutMs: 180000 };
}

export async function stepGenerate(root, { again, now = new Date(), fetchImpl, settleMs = 60000 } = {}) {
  const rec = readRec(root);
  const problems = specProblems(rec);
  if (problems.length) throw new Error("ambience.json problems:\n  " + problems.join("\n  "));
  if (again && !rec.sounds.some((s) => s.id === again)) throw new Error(`--again ${again}: no such sound`);
  const gens = (rec.generations ||= []);
  const dir = join(cacheDir(root), "raw");
  mkdirSync(dir, { recursive: true });
  const todo = [];
  for (const s of rec.sounds) {
    const have = gens.filter((g) => g.id === s.id).map((g) => g.take);
    const want = s.takes + (again === s.id ? have.length - s.takes + 1 : 0);
    for (let n = 1; have.length + todo.filter((t) => t.s === s).length < want; n++) if (!have.includes(n)) todo.push({ s, n });
  }
  if (!todo.length) { console.log(`generate: nothing to do, ${gens.length} generation(s) on record, ${spentCredits(rec)} credits`); return rec; }
  const o = fetchImpl ? { fetchImpl } : {};
  const first = await eleven.balance(o);
  console.log(`ElevenLabs ${first.tier}: ${first.used} of ${first.limit} credits used; ${todo.length} generation(s), about ${todo.reduce((n, t) => n + estimate(t.s), 0)} credits by the price list`);
  const done = [];
  for (const { s, n } of todo) {
    if (spentCredits(rec) + estimate(s) > rec.budget.credits) throw new Error(`${takeName(s, n)} would pass the budget: ${spentCredits(rec)} spent, about ${estimate(s)} more, cap ${rec.budget.credits}. Stop and report.`);
    const req = requestFor(s);
    const res = await eleven.call("POST", req.path, req.body, { ...o, timeoutMs: req.timeoutMs });
    const audio = Buffer.from(await res.arrayBuffer());
    if (audio.length < 4000) throw new Error(`ElevenLabs returned ${audio.length} bytes for ${takeName(s, n)}`);
    writeFileSync(join(dir, `${takeName(s, n)}.mp3`), audio);
    gens.push({
      id: s.id, take: n, date: now.toISOString().slice(0, 10), tool: s.kind === "music" ? "ElevenLabs Music" : "ElevenLabs text-to-sound-effects", model: s.model_id || (s.kind === "music" ? "music_v1" : "default"),
      prompt: s.prompt, seconds: s.seconds, bytes: audio.length, credits: estimate(s),
    });
    writeRec(root, rec); // after every paid call: a run that dies loses nothing
    console.log(`  ${takeName(s, n)}: ${s.seconds} s, ${audio.length} bytes, about ${estimate(s)} credits`);
    done.push(s);
  }
  await new Promise((r) => setTimeout(r, settleMs));
  const last = await eleven.balance(o);
  (rec.runs ||= []).push({ date: now.toISOString().slice(0, 10), generations: done.length, plan: first.tier, credits_before: first.used, credits_after: last.used, credits: last.used - first.used, by_rate: done.reduce((n, x) => n + estimate(x), 0) });
  writeRec(root, rec);
  console.log(`generate: ${done.length} new; the balance moved ${last.used - first.used} (${first.used} -> ${last.used}; other agents share the account); ${spentCredits(rec)} credits on record of a ${rec.budget.credits} budget`);
  return rec;
}

function ffmpeg(root, args, binary) {
  const res = spawnSync(ffmpegExe(root), ["-hide_banner", "-loglevel", "error", "-y", ...args], { windowsHide: true, timeout: 180000, maxBuffer: 512 * 1024 * 1024, encoding: binary ? "buffer" : "utf8" });
  if (res.error) throw new Error(`ffmpeg: ${res.error.message} (run: node tools/film/film.mjs fetch)`);
  if (res.status !== 0) throw new Error(`ffmpeg exited ${res.status}: ${String(res.stderr).trim().split("\n").slice(-3).join(" ")}`);
  return res.stdout;
}

// A small 1989 radio speaker: no bass, no air, squeezed dynamics. Everything else keeps its band.
// `even` is for a sparse bed (hens, a quarry, a square): one loud call would set the peak and leave the
// rest of the take inaudible, so the level is evened out over about a second before the peak is set.
export const filterFor = (s) => (s.radio ? "highpass=f=180,lowpass=f=5200,acompressor=threshold=-20dB:ratio=3:attack=15:release=250" : s.even ? "highpass=f=35,dynaudnorm=f=300:g=21:m=6:p=0.9" : "highpass=f=35");

// The step across the loop point over the 99.9th percentile of the steps inside the take. Dense noise
// (a saw, a compressor) has large steps everywhere, so the ends are judged against the take itself.
export function wrapRatio(x) {
  if (x.length < 100) return 0;
  const steps = new Float64Array(x.length - 1);
  for (let i = 1; i < x.length; i++) steps[i - 1] = Math.abs(x[i] - x[i - 1]);
  steps.sort();
  const ref = Math.max(8, steps[Math.floor(steps.length * 0.999)]);
  return Math.abs(x[0] - x[x.length - 1]) / ref;
}

// Decode, cut the silent ends, close the loop, set the level.
export function processTake(root, s, rawFile) {
  const out = ffmpeg(root, ["-i", rawFile, "-af", filterFor(s), "-ac", "1", "-ar", String(s.rate), "-f", "s16le", "-"], true);
  const x = new Int16Array(out.buffer, out.byteOffset, Math.floor(out.length / 2)).slice();
  const cut = trimSilence(x, s.rate, { leadMs: 0, tailMs: 0, thresholdDb: -40 });
  if (!cut) throw new Error("the take is silent");
  return normalisePeak(loopSeam(cut, s.rate, s.seam_ms), -3);
}

export function stepProcess(root) {
  const rec = readRec(root);
  const cache = cacheDir(root);
  mkdirSync(join(cache, "wav"), { recursive: true });
  mkdirSync(join(cache, "review"), { recursive: true });
  let bad = 0;
  for (const s of rec.sounds) {
    for (const g of (rec.generations || []).filter((x) => x.id === s.id)) {
      const name = takeName(s, g.take);
      const raw = join(cache, "raw", `${name}.mp3`);
      // A sound with a pick is not re-judged take by take: its other takes are history in the record.
      if (!existsSync(raw) && (g.processed || (s.pick && g.take !== s.pick))) continue;
      if (!existsSync(raw)) { console.log(`  ${name}: raw take missing at ${raw} (the cache is per worktree)`); bad++; continue; }
      const wav = join(cache, "wav", `${name}.wav`);
      let samples;
      try { samples = processTake(root, s, raw); } catch (e) { const history = s.pick && g.take !== s.pick; console.log(`${history ? "note  " : "FAIL  "}${name}: ${e.message}${history ? " (not the pick)" : ""}`); if (!history) bad++; continue; }
      writeFileSync(wav, encodeWav(samples, s.rate));
      const info = { ...wavInfo(readFileSync(wav)), wrap: wrapRatio(samples) };
      const problems = loopProblems(info, s);
      if (!(s.pick && g.take !== s.pick)) bad += problems.length;
      // An agent cannot listen: waveform above, spectrogram below.
      ffmpeg(root, ["-i", wav, "-filter_complex", `[0:a]asplit[a][b];[a]showwavespic=s=1200x160:colors=white,format=rgb24[w];[b]showspectrumpic=s=1200x340:legend=0:stop=${Math.min(16000, s.rate / 2)},format=rgb24[s];[w][s]vstack`, "-frames:v", "1", join(cache, "review", `${name}.png`)]);
      g.processed = { seconds: Math.round(info.seconds * 100) / 100, peak_db: Math.round(info.peakDb * 10) / 10, rms_db: Math.round(info.rmsDb * 10) / 10, wrap: Math.round(info.wrap * 100) / 100 };
      console.log(`  ${name}: ${info.seconds.toFixed(2)} s at ${s.rate} Hz, peak ${info.peakDb.toFixed(1)} dB, rms ${info.rmsDb.toFixed(1)} dB, loop step ${info.wrap.toFixed(2)}x`);
      for (const p of problems) console.log(`FAIL  ${name}: ${p}`);
    }
  }
  writeRec(root, rec);
  console.log(`process: WAVs in ${join(cache, "wav")}, review pictures in ${join(cache, "review")}`);
  if (bad) throw new Error(`${bad} problem(s) in the processed takes`);
}

export function stepWire(root) {
  const rec = readRec(root);
  const problems = specProblems(rec);
  if (problems.length) throw new Error("ambience.json problems:\n  " + problems.join("\n  "));
  const unpicked = rec.sounds.filter((s) => !s.pick).map((s) => s.id);
  if (unpicked.length) throw new Error(`no pick for: ${unpicked.join(", ")}. Read the review pictures, then set "pick" and "reason".`);
  const addon = join(root, "addons", "LifeFramework");
  const used = usedGuids(addon);
  const place = (rel, text) => {
    const file = join(addon, rel);
    const guid = readGuid(file + ".meta") || freshGuid(used);
    mkdirSync(dirname(file), { recursive: true });
    writeFileSync(file + ".meta", text(guid));
    return `{${guid}}${rel}`;
  };
  rec.wired = {};
  for (const s of rec.sounds) {
    const src = join(cacheDir(root), "wav", `${takeName(s, s.pick)}.wav`);
    const sample = samplePath(s);
    if (existsSync(src)) {
      const bytes = readFileSync(src);
      const bad = loopProblems({ ...wavInfo(bytes), wrap: wrapRatio(new Int16Array(bytes.buffer, bytes.byteOffset + 44, (bytes.length - 44) >> 1)) }, s);
      if (bad.length) throw new Error(`${takeName(s, s.pick)}: ${bad[0]}`);
      const ref = place(sample, (guid) => wavMeta(sample, guid));
      copyFileSync(src, join(addon, sample));
      void ref;
    } else if (!existsSync(join(addon, sample))) {
      throw new Error(`${src} is missing and the addon has no ${sample}; run --only process`);
    }
    const sampleRef = `{${readGuid(join(addon, sample) + ".meta")}}${sample}`;
    const rel = projectPath(s);
    const text = ambientAcpText(s, sampleRef);
    const bad = acpProblems(text);
    if (bad.length) throw new Error(`${rel}: ${bad[0]}`);
    const ref = place(rel, (guid) => acpMeta(rel, guid));
    writeFileSync(join(addon, rel), text);
    rec.wired[s.id] = { project: ref, sample: sampleRef, event: EVENT };
    console.log(`  ${s.id.padEnd(12)} ${ref}  ${s.inner_m}-${s.outer_m} m  ${s.volume_db} dB`);
  }
  writeRec(root, rec);
  console.log(`wire: ${rec.sounds.length} project(s) under addons/LifeFramework/${PROJECT_DIR}`);
  return rec;
}

function cmdStatus(root) {
  const rec = readRec(root);
  const gens = rec.generations || [];
  console.log(`atmosphere: ${gens.length} generation(s), ${gens.reduce((n, g) => n + g.seconds, 0)} s of audio, ${spentCredits(rec)} of ${rec.budget.credits} credits`);
  for (const s of rec.sounds) {
    const mine = gens.filter((g) => g.id === s.id);
    console.log(`  ${s.id.padEnd(12)} ${s.kind.padEnd(5)} takes ${mine.length}/${s.takes}  pick ${s.pick ?? "-"}  ${mine.reduce((n, g) => n + (g.credits ?? 0), 0)} credits  ${rec.wired?.[s.id] ? "wired" : "not wired"}`);
  }
  return 0;
}

export async function main(root, flags) {
  const value = (name) => { const i = flags.indexOf(name); return i >= 0 ? flags[i + 1] : undefined; };
  const only = value("--only");
  if (flags.includes("--help") || (only && !["generate", "process", "wire"].includes(only))) {
    console.log("usage: tools\\cli asset ambience [--status | --only generate|process|wire] [--again <sound id>]\nspec: tools/assets/atmosphere/ambience.json   doc: docs/architecture/atmosphere.md");
    return 2;
  }
  if (flags.includes("--status")) return cmdStatus(root);
  const wt = await import("../../wt.mjs");
  if (wt.isMainCheckout(root)) { console.log("FAIL  asset ambience runs only in a worktree"); return 1; }
  if (!only || only === "generate") { console.log("--- ambience: generate"); await stepGenerate(root, { again: value("--again") }); }
  if (!only || only === "process") { console.log("--- ambience: process"); stepProcess(root); }
  if (only === "wire" || (!only && readRec(root).sounds.every((s) => s.pick))) { console.log("--- ambience: wire"); stepWire(root); }
  return 0;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  process.exit(await main(resolve(dirname(fileURLToPath(import.meta.url)), "..", "..", ".."), process.argv.slice(2)));
}
