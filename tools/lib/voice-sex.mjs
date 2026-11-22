// tools/lib/voice-sex.mjs - does a voiced NPC's recorded voice match its body? (#3831)
//
// The maintainer heard "a woman's voice come from a man" in the cafe: Odile Marchand's lines were recorded in a
// female voice (ElevenLabs "Mariva") and her prefab is a Reforger character, and every Reforger character body is
// male (the base game ships male heads only: Prefabs/Characters/Heads/Head_*.et, no female head or body; checked
// 2026-11-19 and again 2026-11-22 with `tools\cli call asset_search`: the CIV factions are businessman, construction
// worker, dockworker, generic civilians, press and racers, all male). So until the game has a female body, every NPC
// that stands in the world speaks in a male voice. Each `tools/assets/dialogue/<npc>/dialogue.json` records
// `voice.sex` (the voice library's own `labels.gender`). The NPC prefabs that speak it are found by their
// `m_sDialogueId`, by the role that plays it (`tools/assets/outfits/roles.json` `dialogue`), and for a pack's voiced
// patch through the dialogue it patches.
// A female given name on a male body is the same bug seen from the caption (Mireille and Helene, 2026-11-22: male
// bodies, male voices, women's names): the speaker's name is checked too.
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { join, relative } from "node:path";

// The sex of every character body the game has. Change it, and add a per-prefab table, the day a female body ships.
export const BODY_SEX = "male";
export const SEXES = ["male", "female"];
// Given names (and titles) that read as a woman's: a speaker called this on a male body is a woman's name on a man.
export const FEMALE_NAMES = new Set([
  "agnes", "anne", "anna", "bernadette", "brigitte", "catherine", "celine", "chantal", "christine", "claire", "colette",
  "danielle", "denise", "elise", "elena", "emma", "francoise", "genevieve", "helene", "isabelle", "jeanne", "josiane",
  "julie", "juliette", "louise", "lucie", "madeleine", "margaux", "marie", "martine", "mireille", "monique", "nathalie",
  "odile", "olga", "paulette", "sarah", "simone", "sophie", "suzanne", "sylvie", "therese", "valerie", "veronique",
  "yvette", "mme", "madame", "mrs", "miss", "mademoiselle",
]);

function walk(dir, ext, out = []) {
  if (!existsSync(dir)) return out;
  for (const name of readdirSync(dir)) {
    const path = join(dir, name);
    if (statSync(path).isDirectory()) walk(path, ext, out);
    else if (name.endsWith(ext)) out.push(path);
  }
  return out;
}

// Caption text by string id: the core table, then the pack tables over it (a pack row overrides the core's).
function strings(root) {
  const map = new Map();
  const tables = [join(root, "addons", "LifeFramework", "Language"), ...readdirSync(join(root, "addons")).filter((a) => a !== "LifeFramework").map((a) => join(root, "addons", a, "Language"))];
  for (const dir of tables) {
    for (const f of existsSync(dir) ? readdirSync(dir).filter((n) => n.endsWith(".st")) : []) {
      for (const m of readFileSync(join(dir, f), "utf8").matchAll(/Id "([^"]+)"\s*\n\s*Target_en_us "((?:[^"\\]|\\.)*)"/g)) map.set(m[1], m[2]);
    }
  }
  return map;
}

// The first word of a speaker name that is a woman's given name or title, or "".
export function femaleName(name) {
  const plain = String(name || "").normalize("NFD").replace(/[̀-ͯ]/g, "").toLowerCase();
  return plain.split(/[^a-z]+/).filter(Boolean).find((w) => FEMALE_NAMES.has(w)) || "";
}

// Every voiced NPC: { id, voice, sex, speaker, prefabs[] } from tools/assets/dialogue/*/dialogue.json.
export function voicedNpcs(root) {
  const specs = join(root, "tools", "assets", "dialogue");
  const prefabs = [];
  for (const f of walk(join(root, "addons"), ".et")) {
    const text = readFileSync(f, "utf8");
    const file = relative(root, f).replace(/\\/g, "/");
    const id = /m_sDialogueId "([^"]+)"/.exec(text)?.[1];
    if (id) prefabs.push({ id, file });
    const role = /m_sRole "([^"]+)"/.exec(text)?.[1];
    if (role) prefabs.push({ role, file });
  }
  const rolesFile = join(root, "tools", "assets", "outfits", "roles.json");
  const roleOf = new Map();
  if (existsSync(rolesFile)) for (const r of JSON.parse(readFileSync(rolesFile, "utf8")).roles || []) if (r.dialogue) roleOf.set(r.dialogue, r.id);
  const all = new Map();
  for (const name of existsSync(specs) ? readdirSync(specs) : []) {
    const path = join(specs, name, "dialogue.json");
    if (existsSync(path)) all.set(name, JSON.parse(readFileSync(path, "utf8")));
  }
  const text = strings(root);
  const out = [];
  for (const [name, spec] of all) {
    if (!spec.voice) continue;
    // A pack's voiced patch speaks for the bodies of the dialogue it patches, under that dialogue's speaker.
    const target = spec.patches || spec.id || name;
    const speakerKey = (spec.speaker_key || all.get(target)?.speaker_key || "").replace(/^#/, "");
    const bodies = prefabs.filter((p) => p.id === target || (p.role && p.role === roleOf.get(target))).map((p) => p.file);
    out.push({ id: spec.id || name, voice: spec.voice.name, sex: spec.voice.sex || "", speaker: text.get(speakerKey) || "", prefabs: bodies });
  }
  return out;
}

// The faults in a list of voiced NPCs: a voice with no declared sex, an unknown sex, a voice that is not the body's,
// a woman's name on a body that is not a woman's.
export function faults(npcs, bodySex = BODY_SEX) {
  const bad = [];
  for (const n of npcs) {
    if (!n.sex) bad.push(`${n.id}: voice "${n.voice}" has no voice.sex (male or female, the voice library's labels.gender)`);
    else if (!SEXES.includes(n.sex)) bad.push(`${n.id}: voice.sex "${n.sex}" is neither male nor female`);
    else if (n.prefabs.length && n.sex !== bodySex) bad.push(`${n.id}: ${n.sex} voice "${n.voice}" on a ${bodySex} body (${n.prefabs.join(", ")}); the game has ${bodySex} bodies only, pick a ${bodySex} voice`);
    const woman = femaleName(n.speaker);
    if (n.prefabs.length && bodySex === "male" && woman) bad.push(`${n.id}: the speaker "${n.speaker}" has a woman's name (${woman}) on a male body (${n.prefabs.join(", ")}); the game has male bodies only, give the NPC a man's name`);
  }
  return bad;
}
