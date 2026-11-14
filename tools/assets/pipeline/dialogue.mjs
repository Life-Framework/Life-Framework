// tools\cli asset dialogue <npc>: one NPC's conversation from a data file to voiced lines in the addon.
// Playbook: docs/asset-pipeline/dialogue.md. Design and findings: docs/architecture/npc-dialogue.md.
//
//   asset dialogue <npc> [--dry-run] [--force <line>|all]     asset dialogue status
//
// The source is tools/assets/dialogue/<npc>/dialogue.json (tracked). For every line the stage
//   1. generates speech with ElevenLabs (paid per character, capped by the spec's budget_chars),
//   2. trims, normalises and converts it to the engine format with ffmpeg (mono 44.1 kHz 16-bit),
//   3. writes the .wav, its .meta, the NPC's audio project (.acp) and the mouth envelope,
//   4. writes the dialogue config (.conf) and the caption keys in the string table.
// A line is regenerated only when its text or voice changed: its hash is kept in the tracked
// tools/assets/dialogue/<npc>/elevenlabs.json, so a fresh worktree pays nothing for a line that is
// already in the addon. Raw downloads are kept in tmp/assets/dialogue/<npc>/ by hash, so a run
// that died after paying resumes from the download.
import { createHash } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, readdirSync, unlinkSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";
import * as eleven from "./elevenlabs.mjs";
import { freshGuid, readGuid } from "./meta.mjs";
import { addonFlag, addonOf, addonProblems, assertBoundary, guidsInUse, stringTable, withAddon } from "./addon.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
// The addon a dialogue writes into: the spec's `addon`, or `--addon` (#971); the core by default.
const ADDON = (spec) => `addons/${addonOf(spec)}`;
export const MAX_OPTIONS = 4; // the screen has four rows and the keys 1 to 4
export const MAX_BUDGET = 6000; // characters one NPC may ever cost
export const ENVELOPE_MS = 50;

export const specFile = (root, id) => join(root, "tools", "assets", "dialogue", id, "dialogue.json");
export const provenanceFile = (root, id) => join(root, "tools", "assets", "dialogue", id, "elevenlabs.json");
export const cacheDir = (root, id) => join(root, "tmp", "assets", "dialogue", id);

const pascal = (s) => s.split(/[^A-Za-z0-9]+/).filter(Boolean).map((p) => p[0].toUpperCase() + p.slice(1)).join("");
export const lineKey = (spec, id) => `LF-Dialogue_${spec.name}_${pascal(id)}`;
export const replyKey = (spec, id) => `LF-Dialogue_${spec.name}_Say${pascal(id)}`;
export const eventName = (spec, id) => `SOUND_DLG_${spec.id}_${id}`.toUpperCase().replace(/[^A-Z0-9_]/g, "_");
export const soundDir = (spec) => `Sounds/Dialogue/${spec.id}`;
export const wavRes = (spec, id) => `${soundDir(spec)}/${spec.id}_${id}.wav`;
export const acpRes = (spec) => `${soundDir(spec)}/Dialogue_${spec.id}.acp`;
export const confRes = (spec) => `Configs/Dialogue/LF_Dialogue_${spec.name}.conf`;

// What the voice is asked to say: `say` when the line has one (the caption `text` with eleven_v3
// audio tags such as [panicked] added), else the caption itself.
export const voicedText = (line) => (line.say || line.text || "").trim();
// `say` with its [audio tags] removed: what must read the same as the caption.
export const untagged = (say) => say.replace(/\[[a-z][a-z ,'-]*\]/g, " ").replace(/\s+/g, " ").trim();

export function lineHash(voice, text) {
  return createHash("sha1").update(JSON.stringify([voice.id, voice.model || eleven.DEFAULT_MODEL, voice.settings || null, text])).digest("hex").slice(0, 16);
}

// A stable 16-hex id for an object inside a generated file, so a rerun does not churn them.
export const stableGuid = (...parts) => createHash("sha1").update(parts.join("|")).digest("hex").slice(0, 16).toUpperCase();

export function loadSpec(root, id) {
  const file = specFile(root, id);
  if (!existsSync(file)) throw new Error(`no dialogue at ${file} (docs/asset-pipeline/dialogue.md "A new NPC")`);
  return expand(JSON.parse(readFileSync(file, "utf8")));
}

// "options_from": "<node>" on a node copies that node's options, so a hub is written once.
export function expand(spec) {
  for (const n of spec.nodes || []) {
    if (!n.options_from) continue;
    const from = spec.nodes.find((x) => x.id === n.options_from);
    if (!from) throw new Error(`node ${n.id}: options_from '${n.options_from}' does not exist`);
    n.options = (from.options || []).map((o) => ({ ...o }));
  }
  return spec;
}

export function loadProvenance(root, id) {
  const file = provenanceFile(root, id);
  return existsSync(file) ? JSON.parse(readFileSync(file, "utf8")) : { id, service: "ElevenLabs text-to-speech", lines: {} };
}

// Everything wrong with a dialogue file, as a list. `strings` maps existing string ids to text.
export function specProblems(spec, strings = new Map()) {
  const out = [];
  if (!/^[a-z][a-z0-9_]*$/.test(spec.id || "")) out.push("id must be lowercase letters, digits and underscores");
  if (!/^[A-Z][A-Za-z0-9]*$/.test(spec.name || "")) out.push("name must be PascalCase (it becomes file and key names)");
  if (!/^#/.test(spec.speaker_key || "")) out.push("speaker_key must be a localization key (#...)");
  if (!spec.voice?.id) out.push("voice.id is missing (docs/asset-pipeline/dialogue.md \"Voices\")");
  if (!(spec.budget_chars > 0 && spec.budget_chars <= MAX_BUDGET)) out.push(`budget_chars must be 1 to ${MAX_BUDGET}`);
  const lines = spec.lines || {};
  const replies = spec.replies || {};
  const nodes = spec.nodes || [];
  const nodeIds = new Set(nodes.map((n) => n.id));
  if (!nodeIds.has(spec.start)) out.push(`start node '${spec.start}' does not exist`);
  if (nodeIds.size !== nodes.length) out.push("two nodes share an id");
  const forKeys = new Set();
  for (const [id, line] of Object.entries(lines)) {
    if (!/^[a-z][a-z0-9_]*$/.test(id)) out.push(`line id '${id}' must be lowercase letters, digits and underscores`);
    const text = (line.text || "").trim();
    if (text.length < 2 || text.length > 320) out.push(`line ${id}: text must be 2 to 320 characters (a longer speech is two lines)`);
    if (/[<>{}\[\]%]|#[A-Z]/.test(text)) out.push(`line ${id}: text has markup or a placeholder; the voice would read it aloud`);
    if (line.say !== undefined) {
      const say = String(line.say).trim();
      if (/[<>{}%]|#[A-Z]/.test(say)) out.push(`line ${id}: say has markup other than [audio tags]`);
      if (untagged(say) !== text.replace(/\s+/g, " ")) out.push(`line ${id}: say without its [audio tags] must read exactly as text (the caption)`);
      if (!/\[[a-z]/.test(say)) out.push(`line ${id}: say adds no [audio tag]; drop it and keep text`);
      if (say.length > 400) out.push(`line ${id}: say must be at most 400 characters`);
    }
    if (line.for_key) {
      if (!strings.has(line.for_key.replace(/^#/, ""))) out.push(`line ${id}: for_key ${line.for_key} is not in the string table`);
      if (forKeys.has(line.for_key)) out.push(`line ${id}: for_key ${line.for_key} is spoken by two lines`);
      forKeys.add(line.for_key);
    }
  }
  for (const node of nodes) {
    if (node.line && !lines[node.line]) out.push(`node ${node.id}: line '${node.line}' does not exist`);
    const options = node.options || [];
    if (options.length > MAX_OPTIONS) out.push(`node ${node.id}: ${options.length} options; the screen shows ${MAX_OPTIONS}`);
    if (!node.line && !options.length) out.push(`node ${node.id} says nothing and offers nothing`);
    for (const o of options) {
      if (!replies[o.text]) out.push(`node ${node.id}: reply '${o.text}' does not exist`);
      if (o.next && !nodeIds.has(o.next)) out.push(`node ${node.id}: option '${o.text}' leads to unknown node '${o.next}'`);
      for (const hook of [o.action, o.condition]) if (hook && !/^[A-Z][A-Za-z0-9]*$/.test(hook)) out.push(`node ${node.id}: hook '${hook}' must be PascalCase (class LF_DialogueHook_${hook})`);
    }
    // A node with options needs a way out, or the player is held in the scene by its own data.
    if (options.length && !options.some((o) => !o.condition)) out.push(`node ${node.id}: every option has a condition; add one that is always offered`);
  }
  // Every node must be reachable from the start, or it is dead text that still costs characters.
  const seen = new Set([spec.start]);
  for (const queue = [spec.start]; queue.length;) {
    const at = queue.pop();
    const node = nodes.find((n) => n.id === at);
    for (const o of node?.options || []) if (o.next && !seen.has(o.next)) { seen.add(o.next); queue.push(o.next); }
  }
  for (const n of nodes) if (!seen.has(n.id)) out.push(`node ${n.id} cannot be reached from '${spec.start}'`);
  const used = new Set([...nodes.map((n) => n.line).filter(Boolean), ...Object.entries(lines).filter(([, l]) => l.for_key).map(([id]) => id)]);
  for (const id of Object.keys(lines)) if (!used.has(id)) out.push(`line ${id} is used by no node and answers no key (for_key)`);
  return out;
}

// ---------------------------------------------------------------- string table
// The pack's string table beside a core one, when the pack has one (#325): its text overrides the core's.
export function packStringFile(root) {
  const file = join(root, "addons", "LF_Everon1989", "Language", "lf_everon1989_localization.st");
  return existsSync(file) ? file : null;
}

export function parseStrings(stText) {
  const map = new Map();
  for (const m of stText.matchAll(/Id "([^"]+)"\s*\n\s*Target_en_us "((?:[^"\\]|\\.)*)"/g)) map.set(m[1], m[2]);
  return map;
}

// Append the entries the table lacks and update the English text of the ones this tool owns.
// Never touches another feature's row. Returns the new text.
export function upsertStrings(stText, entries, guid = () => freshGuid(new Set([...stText.matchAll(/\{([0-9A-F]{16})\}/g)].map((m) => m[1])))) {
  let text = stText;
  const esc = (s) => s.replace(/\\/g, "\\\\").replace(/"/g, '\\"');
  const add = [];
  for (const e of entries) {
    const at = text.indexOf(`     Id "${e.id}"\n`);
    if (at < 0) {
      add.push(`    CustomStringTableItem "{${guid()}}" {\n     Id "${e.id}"\n     Target_en_us "${esc(e.text)}"\n     Comment "${esc(e.comment)}"\n    }\n`);
      continue;
    }
    const lineStart = at + `     Id "${e.id}"\n`.length;
    const lineEnd = text.indexOf("\n", lineStart);
    const want = `     Target_en_us "${esc(e.text)}"`;
    if (text.slice(lineStart, lineEnd) !== want) {
      if (!text.slice(lineStart, lineEnd).startsWith("     Target_en_us ")) throw new Error(`string ${e.id}: the row is not Id then Target_en_us; edit it by hand`);
      text = text.slice(0, lineStart) + want + text.slice(lineEnd);
    }
  }
  if (!add.length) return text;
  const tail = text.lastIndexOf("  }\n }\n}");
  if (tail < 0) throw new Error("string table does not end with the Items block");
  return text.slice(0, tail) + add.join("") + text.slice(tail);
}

export function stringEntries(spec) {
  const out = [];
  for (const [id, line] of Object.entries(spec.lines)) {
    out.push({ id: lineKey(spec, id), text: line.text.trim(), comment: `Dialogue caption, spoken by the ${spec.id} NPC (voiced; tools/assets/dialogue/${spec.id}). Changing it needs: tools\\cli asset dialogue ${spec.id}` });
  }
  for (const [id, text] of Object.entries(spec.replies)) {
    out.push({ id: replyKey(spec, id), text: text.trim(), comment: `Dialogue reply the player can pick when talking to the ${spec.id} NPC.` });
  }
  return out;
}

// ---------------------------------------------------------------- audio
// Mouth envelope of a mono 16-bit PCM wav: one digit 0 to 9 per ENVELOPE_MS, RMS loudness scaled
// to the line's own loud level, so a quiet voice still opens the mouth.
export function envelope(wav) {
  if (wav.toString("latin1", 0, 4) !== "RIFF" || wav.toString("latin1", 8, 12) !== "WAVE") throw new Error("not a RIFF wav");
  let o = 12, fmt = null, data = null;
  while (o + 8 <= wav.length) {
    const id = wav.toString("latin1", o, o + 4), size = wav.readUInt32LE(o + 4);
    if (id === "fmt ") fmt = { format: wav.readUInt16LE(o + 8), channels: wav.readUInt16LE(o + 10), rate: wav.readUInt32LE(o + 12), bits: wav.readUInt16LE(o + 22) };
    if (id === "data") { data = wav.subarray(o + 8, Math.min(wav.length, o + 8 + size)); break; }
    o += 8 + size + (size & 1);
  }
  if (!fmt || !data) throw new Error("wav has no fmt or data chunk");
  if (fmt.format !== 1 || fmt.channels !== 1 || fmt.bits !== 16) throw new Error(`wav must be mono 16-bit PCM (got format ${fmt.format}, ${fmt.channels} ch, ${fmt.bits} bit)`);
  const samples = data.length >> 1;
  const win = Math.round(fmt.rate * ENVELOPE_MS / 1000);
  const rms = [];
  for (let s = 0; s < samples; s += win) {
    let sum = 0;
    const end = Math.min(samples, s + win);
    for (let i = s; i < end; i++) { const v = data.readInt16LE(i * 2) / 32768; sum += v * v; }
    rms.push(Math.sqrt(sum / (end - s)));
  }
  const sorted = [...rms].sort((a, b) => a - b);
  const loud = sorted[Math.floor(sorted.length * 0.9)] || 1e-6;
  const mouth = rms.map((v) => Math.max(0, Math.min(9, Math.round(Math.pow(Math.min(1, v / loud), 0.7) * 9)))).join("");
  return { seconds: Math.round(samples / fmt.rate * 100) / 100, mouth, rate: fmt.rate };
}

async function ffmpegPath() {
  const film = await import("../../film/film.mjs");
  let ff = film.resolveFfmpeg();
  if (!ff) {
    // The pinned build other worktrees already fetched works as it is.
    const siblings = readdirSync(resolve(ROOT, "..")).filter((d) => d.startsWith("Life-Framework")).map((d) => join(ROOT, "..", d, "tools", "film", ".bin", "ffmpeg.exe"));
    const found = siblings.find((p) => existsSync(p));
    if (found) ff = film.resolveFfmpeg({ ...process.env, FFMPEG_PATH: found });
  }
  if (!ff) throw new Error("ffmpeg not found. Run: node tools/film/film.mjs fetch  (or set FFMPEG_PATH)");
  return ff.path;
}

// mp3 from the service -> trimmed, loudness-normalised mono 44.1 kHz 16-bit wav.
export function processAudio(ffmpeg, src, dst) {
  const trim = "silenceremove=start_periods=1:start_duration=0:start_threshold=-45dB:detection=peak";
  const af = `${trim},areverse,${trim},areverse,loudnorm=I=-20:TP=-2:LRA=11,afade=t=in:d=0.01,apad=pad_dur=0.12`;
  mkdirSync(dirname(dst), { recursive: true });
  const res = spawnSync(ffmpeg, ["-hide_banner", "-loglevel", "error", "-y", "-i", src, "-af", af, "-ar", "44100", "-ac", "1", "-c:a", "pcm_s16le", "-map_metadata", "-1", "-fflags", "+bitexact", "-flags:a", "+bitexact", dst],
    { encoding: "utf8", timeout: 60000, windowsHide: true });
  if (res.error || res.status !== 0) throw new Error(`ffmpeg failed on ${src}: ${res.error?.message || res.stderr.trim().split("\n").pop()}`);
}

// ---------------------------------------------------------------- generated files
const SOUND_CONF = { PC: "F72B05D02F3F135E", XBOX_ONE: "A0776163143143AC", XBOX_SERIES: "382981CA1FC1C8DB", PS4: "89EB939911B4C093", HEADLESS: "D63592CA950AF3A8" };

export function wavMeta(res, guid) {
  const rows = Object.entries(SOUND_CONF).map(([p, g]) => `  WAVResourceClass ${p} : "{${g}}Configs/System/ResourceTypes/${p}/Sound.conf" {\n  }\n`).join("");
  return `MetaFileClass {\n Name "{${guid}}${res}"\n Configurations {\n${rows}  WAVResourceClass PS5 : PC {\n  }\n }\n}\n`;
}

export function simpleMeta(res, guid, cls) {
  const rows = ["XBOX_ONE", "XBOX_SERIES", "PS4", "HEADLESS"].map((p) => `  ${cls} ${p} : PC {\n  }\n`).join("");
  return `MetaFileClass {\n Name "{${guid}}${res}"\n Configurations {\n  ${cls} PC {\n  }\n${rows} }\n}\n`;
}

// The audio project: one event per line, each a bank with one sample through its own shader, all
// sharing one distance curve and one spatialiser, into the SFX input of the game's final mix.
// The node layout (ids are index * 1024 + a type code; the connection ids and ports) was read
// from Sounds/Siren/Vehicles_Siren.acp, the working project in this addon.
export function acpText(spec, lines) {
  const id = (n, type) => n * 1024 + type;
  const MIXER = id(1, 11), AMP = id(2, 19), SPATIAL = id(3, 28), SILENCE = id(4, 5);
  const link = (target, port = 65) => `      ConnectionClass connection {\n       id ${target}\n       port ${port}\n      }\n`;
  const conns = (connId, links) => `    ConnectionsClass connections {\n     id ${connId}\n     links {\n${links}     }\n    }\n`;
  const iop = (port, connId, links) => `    IOPConnectionClass iopConnection {\n     port ${port}\n     conn {\n      ConnectionsClass connections {\n       id ${connId}\n       links {\n${links.replace(/^/gm, "  ")}       }\n      }\n     }\n    }\n`;
  let sounds = "", shaders = "", banks = "";
  lines.forEach((l, i) => {
    const n = 10 + i * 3;
    const sound = id(n, 9), shader = id(n + 1, 18), bank = id(n + 2, 4);
    l.soundId = sound;
    sounds += `  SoundClass {\n   id ${sound}\n   name "${l.event}"\n   tl 300 ${i * 120}\n   conns {\n${conns(14, link(shader))}   }\n   outState ${MIXER}\n   outStatePort 17415\n  }\n`;
    shaders += `  ShaderClass {\n   id ${shader}\n   name "Shader ${l.id}"\n   tl 0 ${i * 120}\n   conns {\n${conns(0, link(bank))}${conns(1, link(SILENCE))}   }\n   iops {\n${iop(1, 15, link(AMP))}${iop(0, 24, link(SPATIAL))}   }\n   ITD 0\n  }\n`;
    banks += `  BankLocalClass {\n   id ${bank}\n   name "${l.id}"\n   tl -300 ${i * 120}\n   Volume 0\n   Samples {\n    AudioBankSampleClass "Sample(1)" {\n     Filename "${l.wav}"\n    }\n   }\n  }\n`;
  });
  const mixerLinks = lines.map((l) => link(l.soundId).replace(/^/gm, "  ")).join("");
  const mix = "{B764D803219C775E}Sounds/FinalMix.afm";
  const amp = "{BA1F5E72DA797274}Sounds/_SharedData/Configs/Amplitude/_SharedData/Amplitude_-30LUFS_to_-25LUFS.conf";
  return `AudioClass {\n sounds {\n${sounds} }\n generators {\n  GeneratorClass {\n   id ${SILENCE}\n   name "Silence"\n   tl -300 -200\n   function Silence\n  }\n }\n mixers {\n  MixerClass {\n   id ${MIXER}\n   name "OutputState"\n   tl 700 0\n   res "${mix}"\n   iops {\n    IOPConnectionClass iopConnection {\n     port 17415\n     conn {\n      ConnectionsClass connections {\n       id 5\n       links {\n${mixerLinks}       }\n      }\n     }\n    }\n   }\n   path "${mix}"\n   inputs {\n    "ANM_Footsteps" "ANM_Voice" "WPN_Handling" "WPN_Shots" "WPN_Explosions" "WNP_BulletHits" "CHAR" "ENV_Ambients_2D" "VEH_Animations" "Impacts" "Dialogue" "Music" "ENV_Doors" "UI" "VEH_Engine" "VEH_Tires" "VON" "SFX" "SFX_Reverb" "VON_Reverb" "Dialogue_Reverb" "Impacts_EXT" "ENV_Ambients_3D" "WPN_SonicCracks" "CHAR_Gear" "PA"\n   }\n  }\n }\n shaders {\n${shaders} }\n amplitudes {\n  AmplitudeClass : "${amp}" {\n   id ${AMP}\n   tl -300 -400\n   parent "${amp}"\n   outerRange ${spec.audible_m || 25}\n  }\n }\n banks_local {\n${banks} }\n spatialities {\n  SpatialityClass {\n   id ${SPATIAL}\n   tl -300 -300\n   spatialFactor 1\n  }\n }\n version 1\n}\n`;
}

// The dialogue config the game reads (class LF_DialogueConfig, Feature/Dialogue/LF_DialogueConfig.c).
export function confText(spec, lines, acp) {
  const q = (s) => `"${String(s ?? "").replace(/"/g, '\\"')}"`;
  let nodes = "";
  for (const n of spec.nodes) {
    let options = "";
    (n.options || []).forEach((o, i) => {
      options += `    LF_DialogueOption "{${stableGuid(spec.id, "opt", n.id, i)}}" {\n     m_sTextKey ${q("#" + replyKey(spec, o.text))}\n`
        + (o.next ? `     m_sNext ${q(o.next)}\n` : "") + (o.condition ? `     m_sCondition ${q(o.condition)}\n` : "")
        + (o.action ? `     m_sAction ${q(o.action)}\n` : "") + (o.param ? `     m_sParam ${q(o.param)}\n` : "") + "    }\n";
    });
    nodes += `  LF_DialogueNode "{${stableGuid(spec.id, "node", n.id)}}" {\n   m_sId ${q(n.id)}\n` + (n.line ? `   m_sLine ${q(n.line)}\n` : "")
      + (options ? `   m_aOptions {\n${options}   }\n` : "") + "  }\n";
  }
  let rows = "";
  for (const l of lines) {
    rows += `  LF_DialogueLine "{${stableGuid(spec.id, "line", l.id)}}" {\n   m_sId ${q(l.id)}\n   m_sTextKey ${q("#" + lineKey(spec, l.id))}\n`
      + (l.for_key ? `   m_sForKey ${q(l.for_key)}\n` : "") + `   m_sEvent ${q(l.event)}\n   m_fSeconds ${l.seconds}\n   m_sMouth ${q(l.mouth)}\n  }\n`;
  }
  return `LF_DialogueConfig {\n m_sId ${q(spec.id)}\n m_sSpeakerKey ${q(spec.speaker_key)}\n m_sSoundProject ${q(acp)}\n m_fHearRange ${spec.hear_range_m ?? 12}\n m_sStart ${q(spec.start)}\n m_aNodes {\n${nodes} }\n m_aLines {\n${rows} }\n}\n`;
}

// The rows the generated files are built from: spec order, with what the provenance knows.
export function lineRows(spec, prov, addon) {
  return Object.entries(spec.lines).map(([id, line]) => {
    const p = prov.lines[id] || {};
    const res = wavRes(spec, id);
    const guid = addon ? readGuid(join(addon, res + ".meta")) : null;
    return { id, text: line.text.trim(), for_key: line.for_key || "", event: eventName(spec, id), res, wav: guid ? `{${guid}}${res}` : res, seconds: p.seconds ?? 0, mouth: p.mouth ?? "", hash: p.hash };
  });
}

// What the tracked files should hold for this spec, for the validator: { path: text }.
export function expectedFiles(root, spec, prov) {
  const addon = join(root, ADDON(spec));
  const rows = lineRows(spec, prov, addon);
  const acpGuid = readGuid(join(addon, acpRes(spec) + ".meta"));
  return { [acpRes(spec)]: acpText(spec, rows), [confRes(spec)]: confText(spec, rows, `{${acpGuid}}${acpRes(spec)}`) };
}

// Lines whose audio in the addon does not match the spec (new, reworded, or a new voice).
export function staleLines(root, spec, prov) {
  return Object.entries(spec.lines).filter(([id, line]) => prov.lines[id]?.hash !== lineHash(spec.voice, voicedText(line)) || !existsSync(join(root, ADDON(spec), wavRes(spec, id)))).map(([id]) => id);
}

// ---------------------------------------------------------------- the stage
export async function run(root, id, { dryRun = false, force = "", fetchImpl, ffmpeg, convert = processAudio, log = console.log, addon: addonName } = {}) {
  const spec = withAddon(loadSpec(root, id), addonName);
  const addon = join(root, ADDON(spec));
  const stFile = stringTable(root, spec);
  if (!existsSync(stFile)) throw new Error(`${ADDON(spec)} has no ${stFile}; a pack's string table does not exist yet (core-pack-inventory.md, #325)`);
  const stText = readFileSync(stFile, "utf8");
  const packFile = packStringFile(root);
  let packText = packFile ? readFileSync(packFile, "utf8") : "";
  const known = parseStrings(stText);
  for (const [k, v] of parseStrings(packText)) known.set(k, v);
  const problems = [...addonProblems(root, spec), ...specProblems(spec, known)];
  if (problems.length) throw new Error("dialogue problems:\n  " + problems.join("\n  "));
  const prov = loadProvenance(root, id);
  const dir = cacheDir(root, id);
  const ledger = join(dir, "chars.jsonl");
  const opts = fetchImpl ? { fetchImpl } : undefined;

  const stale = new Set(staleLines(root, spec, prov));
  if (force === "all") Object.keys(spec.lines).forEach((l) => stale.add(l));
  else if (force) { if (!spec.lines[force]) throw new Error(`--force ${force}: no such line`); stale.add(force); }
  const todo = [...stale].map((lineId) => {
    const text = voicedText(spec.lines[lineId]);
    const hash = lineHash(spec.voice, text);
    const mp3 = join(dir, `${lineId}.${hash}.mp3`);
    // A forced line is a new paid take even when the old download is still there.
    const paid = force && (force === "all" || force === lineId) ? true : !existsSync(mp3);
    return { lineId, text, hash, mp3, paid, chars: paid ? eleven.cost(text, spec.voice.model) : 0 };
  });
  const need = todo.reduce((n, t) => n + t.chars, 0);
  const everSpent = Object.values(prov.lines).reduce((n, l) => n + (l.chars_total || 0), 0);
  log(`dialogue ${id}: ${Object.keys(spec.lines).length} lines, ${todo.length} to (re)generate, ${need} characters to buy; this NPC has cost ${everSpent} of its ${spec.budget_chars} budget`);
  for (const t of todo) log(`  ${t.paid ? "generate" : "resume  "} ${t.lineId} (${t.text.length} chars)`);
  if (everSpent + need > spec.budget_chars) throw new Error(`that would take this NPC to ${everSpent + need} characters, over its budget of ${spec.budget_chars}. Shorten the lines or raise budget_chars on purpose.`);
  if (dryRun) return { generated: 0, chars: 0, need };

  let before = null;
  if (need > 0) {
    before = await eleven.balance(opts);
    log(`ElevenLabs (${before.tier}): ${before.used} of ${before.limit} characters used before this run`);
    if (need > before.left) throw new Error(`the account has ${before.left} characters left and this run needs ${need}`);
    const voiceState = await eleven.ensureVoice(spec.voice, opts);
    if (voiceState === "added") log(`added voice ${spec.voice.name} to the account from the voice library`);
  }
  const ff = todo.length && convert === processAudio ? ffmpeg || await ffmpegPath() : ffmpeg;
  const used = guidsInUse(root);
  let bought = 0;
  for (const t of todo) {
    let requestId = prov.lines[t.lineId]?.hash === t.hash ? prov.lines[t.lineId].request_id : "";
    let billed = 0;
    if (t.paid) {
      const take = await eleven.speak(spec.voice, t.text, opts);
      mkdirSync(dir, { recursive: true });
      writeFileSync(t.mp3, take.audio);
      eleven.logChars(ledger, { line: t.lineId, hash: t.hash, chars: take.billed, voice: spec.voice.id, request: take.requestId });
      requestId = take.requestId;
      billed = take.billed;
      bought += billed;
    }
    const res = wavRes(spec, t.lineId);
    const wav = join(addon, res);
    convert(ff, t.mp3, wav);
    if (!existsSync(wav + ".meta")) writeFileSync(wav + ".meta", wavMeta(res, freshGuid(used)));
    const env = envelope(readFileSync(wav));
    const old = prov.lines[t.lineId] || {};
    prov.lines[t.lineId] = {
      hash: t.hash, date: new Date().toISOString().slice(0, 10), voice_id: spec.voice.id, voice_name: spec.voice.name, model: spec.voice.model || eleven.DEFAULT_MODEL,
      text: t.text, chars: t.text.length, chars_total: (old.chars_total || 0) + billed, request_id: requestId || "", seconds: env.seconds, mouth: env.mouth,
    };
    // Saved after every line: a run that dies keeps what it paid for.
    writeFileSync(provenanceFile(root, id), JSON.stringify(prov, null, 2) + "\n");
    log(`  ${t.lineId}: ${env.seconds} s -> ${ADDON(spec)}/${res}`);
  }

  // Lines that left the spec leave the addon too.
  const sdir = join(addon, soundDir(spec));
  mkdirSync(sdir, { recursive: true });
  for (const f of readdirSync(sdir)) {
    const m = new RegExp(`^${spec.id}_(.+)\\.wav(\\.meta)?$`).exec(f);
    if (m && !spec.lines[m[1]]) { unlinkSync(join(sdir, f)); log(`  removed ${f} (line no longer in the dialogue)`); }
  }
  for (const lineId of Object.keys(prov.lines)) if (!spec.lines[lineId]) delete prov.lines[lineId];
  prov.voice = { id: spec.voice.id, name: spec.voice.name, source: spec.voice.public_owner_id ? "voice library" : "premade" };
  writeFileSync(provenanceFile(root, id), JSON.stringify(prov, null, 2) + "\n");

  const acp = join(addon, acpRes(spec));
  if (!existsSync(acp + ".meta")) writeFileSync(acp + ".meta", simpleMeta(acpRes(spec), freshGuid(used), "AudioProjectResourceClass"));
  const conf = join(addon, confRes(spec));
  mkdirSync(dirname(conf), { recursive: true });
  if (!existsSync(conf + ".meta")) writeFileSync(conf + ".meta", simpleMeta(confRes(spec), freshGuid(used), "CONFResourceClass"));
  const files = expectedFiles(root, spec, prov);
  for (const [rel, text] of Object.entries(files)) writeFileSync(join(addon, rel), text);
  // A line whose key lives in the pack's table (its flavour text) is written there; the core keeps its neutral row.
  const inPack = new Set(parseStrings(packText).keys());
  const entries = stringEntries(spec);
  const newSt = upsertStrings(stText, entries.filter((e) => !inPack.has(e.id)));
  const newPack = packFile ? upsertStrings(packText, entries.filter((e) => inPack.has(e.id))) : packText;
  const stChanged = newSt !== stText || newPack !== packText;
  if (newSt !== stText) writeFileSync(stFile, newSt);
  if (newPack !== packText) writeFileSync(packFile, newPack);

  let after = null;
  if (before) {
    after = await eleven.balance(opts);
    log(`ElevenLabs: ${before.used} -> ${after.used} characters used (${after.used - before.used} this run, ${bought} billed to these lines); ${after.left} left`);
  }
  assertBoundary(root, spec);
  log(`wrote ${ADDON(spec)}/${confRes(spec)} and ${ADDON(spec)}/${acpRes(spec)} (${Object.keys(spec.lines).length} events)`);
  if (stChanged) log("string table changed: run tools\\cli regen-localization");
  return { generated: todo.length, chars: bought, before, after, stChanged };
}

function status(root) {
  const base = join(root, "tools", "assets", "dialogue");
  if (!existsSync(base)) return 0;
  for (const id of readdirSync(base).filter((d) => existsSync(specFile(root, d)))) {
    const spec = loadSpec(root, id), prov = loadProvenance(root, id);
    const total = Object.values(prov.lines).reduce((n, l) => n + (l.chars_total || 0), 0);
    const stale = staleLines(root, spec, prov);
    console.log(`${id}: voice ${spec.voice.name}, ${Object.keys(spec.lines).length} lines, ${total} of ${spec.budget_chars} characters spent, ${stale.length ? "stale: " + stale.join(", ") : "up to date"}`);
  }
  return 0;
}

export async function main(flags) {
  const [id, ...rest] = flags;
  if (id === "status") return status(ROOT);
  if (!id || id.startsWith("--")) {
    console.log("usage: tools\\cli asset dialogue <npc> [--dry-run] [--force <line>|all] [--addon <name>]\n       tools\\cli asset dialogue status\nplaybook: docs/asset-pipeline/dialogue.md");
    return id ? 2 : 0;
  }
  const dryRun = rest.includes("--dry-run");
  if (!dryRun) {
    const wt = await import("../../wt.mjs");
    if (wt.isMainCheckout(ROOT)) { console.log("FAIL  asset dialogue runs only in a worktree"); return 1; }
  }
  const f = rest.indexOf("--force");
  await run(ROOT, id, { dryRun, force: f >= 0 ? rest[f + 1] || "" : "", addon: addonFlag(rest) });
  return 0;
}
