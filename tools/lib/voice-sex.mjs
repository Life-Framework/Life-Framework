// tools/lib/voice-sex.mjs - does a voiced NPC's recorded voice match its body? (#3831)
//
// The maintainer heard "a woman's voice come from a man" in the cafe: Odile Marchand's lines were recorded in a
// female voice (ElevenLabs "Mariva") and her prefab is a Reforger character, and every Reforger character body is
// male (the base game ships male heads only: Prefabs/Characters/Heads/Head_*.et, no female head or body; checked
// 2026-11-19 with `tools\cli call asset_search`). So until the game has a female body, every NPC that stands in the
// world speaks in a male voice. Each `tools/assets/dialogue/<npc>/dialogue.json` records `voice.sex` (the voice
// library's own `labels.gender`) and the NPC prefabs that speak it are found by their `m_sDialogueId`.
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { join, relative } from "node:path";

// The sex of every character body the game has. Change it, and add a per-prefab table, the day a female body ships.
export const BODY_SEX = "male";
export const SEXES = ["male", "female"];

function walk(dir, ext, out = []) {
  if (!existsSync(dir)) return out;
  for (const name of readdirSync(dir)) {
    const path = join(dir, name);
    if (statSync(path).isDirectory()) walk(path, ext, out);
    else if (name.endsWith(ext)) out.push(path);
  }
  return out;
}

// Every voiced NPC: { id, voice, sex, prefabs[] } from tools/assets/dialogue/*/dialogue.json.
export function voicedNpcs(root) {
  const specs = join(root, "tools", "assets", "dialogue");
  const prefabs = [];
  for (const f of walk(join(root, "addons"), ".et")) {
    const text = readFileSync(f, "utf8");
    const id = /m_sDialogueId "([^"]+)"/.exec(text)?.[1];
    if (id) prefabs.push({ id, file: relative(root, f).replace(/\\/g, "/") });
  }
  const out = [];
  for (const name of existsSync(specs) ? readdirSync(specs) : []) {
    const path = join(specs, name, "dialogue.json");
    if (!existsSync(path)) continue;
    const spec = JSON.parse(readFileSync(path, "utf8"));
    if (!spec.voice) continue;
    out.push({ id: spec.id || name, voice: spec.voice.name, sex: spec.voice.sex || "", prefabs: prefabs.filter((p) => p.id === (spec.id || name)).map((p) => p.file) });
  }
  return out;
}

// The faults in a list of voiced NPCs: a voice with no declared sex, an unknown sex, a voice that is not the body's.
export function faults(npcs, bodySex = BODY_SEX) {
  const bad = [];
  for (const n of npcs) {
    if (!n.sex) bad.push(`${n.id}: voice "${n.voice}" has no voice.sex (male or female, the voice library's labels.gender)`);
    else if (!SEXES.includes(n.sex)) bad.push(`${n.id}: voice.sex "${n.sex}" is neither male nor female`);
    else if (n.prefabs.length && n.sex !== bodySex) bad.push(`${n.id}: ${n.sex} voice "${n.voice}" on a ${bodySex} body (${n.prefabs.join(", ")}); the game has ${bodySex} bodies only, pick a ${bodySex} voice`);
  }
  return bad;
}
