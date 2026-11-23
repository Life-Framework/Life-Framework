// Work-site sounds (WK-W23, #4183): generate, process and wire the shift whistle and the mine and sawmill tool
// beds into addons/LF_Everon1989/Sounds/Work/, with a manifest.json and the slot overlay conf.
//
//   node tools/assets/site-ambience/build.mjs [--only generate|process|wire]
//
// The sound project, loop maths and ffmpeg steps are tools/assets/pipeline/ambience.mjs's (the same positioned
// loop LF_AmbientEmitterComponent starts); only the spec, the cache and the target folder are this card's.
import { copyFileSync, existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { createHash } from "node:crypto";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import * as eleven from "../pipeline/elevenlabs.mjs";
import { EVENT, ambientAcpText, callWithRetry, loopProblems, processTake, specProblems, wrapRatio } from "../pipeline/ambience.mjs";
import { acpMeta, acpProblems, encodeWav, wavInfo, wavMeta } from "../pipeline/sound.mjs";
import { freshGuid, readGuid } from "../pipeline/meta.mjs";
import { addonPath, guidsInUse, withAddon } from "../pipeline/addon.mjs";

export const PROJECT_DIR = "Sounds/Work";
export const CONF = "Configs/SoundSlots/LF_SoundSlots_Work_Everon1989.conf";
const CREDITS_PER_SECOND = 10;
const here = dirname(fileURLToPath(import.meta.url));
const root = resolve(here, "..", "..", "..");
const specFile = join(here, "site-ambience.json");
const cache = join(root, "tmp", "assets", "site-ambience");
const readRec = () => JSON.parse(readFileSync(specFile, "utf8"));
const writeRec = (rec) => writeFileSync(specFile, JSON.stringify(rec, null, 2) + "\n");
const takeName = (s, n) => `${s.id}_${String(n).padStart(2, "0")}`;
const projectPath = (s) => `${PROJECT_DIR}/Work_${s.name}.acp`;
const samplePath = (s) => `${PROJECT_DIR}/Samples/Work_${s.name}_LP.wav`;
const estimate = (s) => Math.round(s.seconds * CREDITS_PER_SECOND);
const spent = (rec) => (rec.generations || []).reduce((n, g) => n + (g.credits ?? 0), 0);

export function problems(rec) {
  const out = specProblems({ budget: rec.budget, sounds: rec.sounds.map((s) => ({ ...s, pick: 1, reason: "single take" })) });
  for (const s of rec.sounds) if (!/^atmo_[a-z0-9_]+$/.test(s.slot || "")) out.push(`sound ${s.id}: slot must be atmo_...`);
  return out;
}

async function generate() {
  const rec = readRec();
  const bad = problems(rec);
  if (bad.length) throw new Error("site-ambience.json problems:\n  " + bad.join("\n  "));
  const gens = (rec.generations ||= []);
  mkdirSync(join(cache, "raw"), { recursive: true });
  const todo = rec.sounds.filter((s) => !gens.some((g) => g.id === s.id));
  if (!todo.length) { console.log(`generate: nothing to do, ${gens.length} generation(s), ${spent(rec)} credits`); return; }
  const first = await eleven.balance();
  console.log(`ElevenLabs ${first.tier}: ${first.used} of ${first.limit} credits used; ${todo.length} generation(s), about ${todo.reduce((n, s) => n + estimate(s), 0)} credits`);
  for (const s of todo) {
    if (spent(rec) + estimate(s) > rec.budget.credits) throw new Error(`${s.id} would pass the ${rec.budget.credits} credit budget`);
    const body = { text: s.prompt, duration_seconds: s.seconds, prompt_influence: s.prompt_influence ?? 0.5, loop: s.loop !== false };
    const res = await callWithRetry({ path: "/v1/sound-generation?output_format=mp3_44100_128", body }, { timeoutMs: 180000 });
    const audio = Buffer.from(await res.arrayBuffer());
    if (audio.length < 4000) throw new Error(`ElevenLabs returned ${audio.length} bytes for ${s.id}`);
    writeFileSync(join(cache, "raw", `${takeName(s, 1)}.mp3`), audio);
    gens.push({ id: s.id, take: 1, date: new Date().toISOString().slice(0, 10), tool: "ElevenLabs text-to-sound-effects", prompt: s.prompt, seconds: s.seconds, bytes: audio.length, credits: estimate(s) });
    writeRec(rec); // after every paid call: a run that dies loses nothing
    console.log(`  ${s.id}: ${s.seconds} s, ${audio.length} bytes, about ${estimate(s)} credits`);
  }
  console.log(`generate: ${spent(rec)} of ${rec.budget.credits} credits on record`);
}

function process_() {
  const rec = readRec();
  mkdirSync(join(cache, "wav"), { recursive: true });
  let failed = 0;
  for (const s of rec.sounds) {
    const raw = join(cache, "raw", `${takeName(s, 1)}.mp3`);
    if (!existsSync(raw)) { console.log(`FAIL  ${s.id}: raw take missing at ${raw}`); failed++; continue; }
    const samples = processTake(root, s, raw);
    const wav = join(cache, "wav", `${takeName(s, 1)}.wav`);
    writeFileSync(wav, encodeWav(samples, s.rate));
    const info = { ...wavInfo(readFileSync(wav)), wrap: wrapRatio(samples) };
    console.log(`  ${s.id}: ${info.seconds.toFixed(2)} s, peak ${info.peakDb.toFixed(1)} dB, rms ${info.rmsDb.toFixed(1)} dB, loop step ${info.wrap.toFixed(2)}x`);
    for (const p of loopProblems(info, s)) { console.log(`FAIL  ${s.id}: ${p}`); failed++; }
  }
  if (failed) throw new Error(`${failed} problem(s) in the processed takes`);
}

// A record id is a stable function of the slot, so a rewire leaves the conf unchanged.
const recordId = (slot) => createHash("sha1").update("site-ambience:" + slot).digest("hex").slice(0, 16).toUpperCase();

function wire() {
  const rec = readRec();
  const target = withAddon(rec);
  const addon = addonPath(root, target);
  const used = guidsInUse(root);
  const place = (rel, text) => {
    const file = join(addon, rel);
    const guid = readGuid(file + ".meta") || freshGuid(used);
    mkdirSync(dirname(file), { recursive: true });
    writeFileSync(file + ".meta", text(guid));
    return `{${guid}}${rel}`;
  };
  const sounds = [];
  for (const s of rec.sounds) {
    const src = join(cache, "wav", `${takeName(s, 1)}.wav`);
    const sample = samplePath(s);
    if (existsSync(src)) {
      place(sample, (guid) => wavMeta(sample, guid));
      copyFileSync(src, join(addon, sample));
    } else if (!existsSync(join(addon, sample))) throw new Error(`${src} is missing; run --only process`);
    const sampleRef = `{${readGuid(join(addon, sample) + ".meta")}}${sample}`;
    const rel = projectPath(s);
    const text = ambientAcpText(s, sampleRef);
    const bad = acpProblems(text);
    if (bad.length) throw new Error(`${rel}: ${bad[0]}`);
    const ref = place(rel, (guid) => acpMeta(rel, guid));
    writeFileSync(join(addon, rel), text);
    sounds.push({ id: s.id, slot: s.slot, project: ref, sample: sampleRef, event: EVENT, source: "elevenlabs", tool: "ElevenLabs text-to-sound-effects", prompt: s.prompt, licence: s.licence, inner_m: s.inner_m, outer_m: s.outer_m, volume_db: s.volume_db, seconds: s.seconds });
    console.log(`  ${s.id.padEnd(14)} ${ref}  ${s.inner_m}-${s.outer_m} m`);
  }
  const confRows = sounds.map((x) => `  LF_SoundSlot "{${recordId(x.slot)}}" {\n   m_sOp "patch"\n   m_sSlot "${x.slot}"\n   m_Project "${x.project}"\n  }`).join("\n");
  const confFile = join(addon, CONF);
  const confGuid = readGuid(confFile + ".meta") || freshGuid(used);
  writeFileSync(confFile, `LF_SoundSlotsConfig {\n m_aSlots {\n${confRows}\n }\n}\n`);
  writeFileSync(confFile + ".meta", `MetaFileClass {\n Name "{${confGuid}}${CONF}"\n Configurations {\n  CONFResourceClass PC {\n  }\n  CONFResourceClass XBOX_ONE : PC {\n  }\n  CONFResourceClass XBOX_SERIES : PC {\n  }\n  CONFResourceClass PS4 : PC {\n  }\n  CONFResourceClass HEADLESS : PC {\n  }\n }\n}\n`);
  const manifest = {
    schema: 1,
    note: "The work-site sounds of WK-W23 (#4183). Source order: vanilla first, then ElevenLabs sound effects (rec.vanilla_checked in tools/assets/site-ambience/site-ambience.json says what vanilla offered). Built by tools/assets/site-ambience/build.mjs.",
    addon: rec.addon,
    vanilla_checked: rec.vanilla_checked,
    whistle: rec.whistle,
    sounds,
  };
  writeFileSync(join(addon, PROJECT_DIR, "manifest.json"), JSON.stringify(manifest, null, 2) + "\n");
  console.log(`wire: ${sounds.length} sound(s), manifest and ${CONF}`);
}

const only = process.argv.includes("--only") ? process.argv[process.argv.indexOf("--only") + 1] : undefined;
if (!only || only === "generate") { console.log("--- site ambience: generate"); await generate(); }
if (!only || only === "process") { console.log("--- site ambience: process"); process_(); }
if (!only || only === "wire") { console.log("--- site ambience: wire"); wire(); }
