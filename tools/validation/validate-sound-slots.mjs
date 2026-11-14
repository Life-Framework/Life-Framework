// inputs: addons/**, tools/validation/validate-item-catalogue.mjs
// Sound slots (#323): every slot's sound project resolves, every slot id a prefab, layer or script
// asks for is a core slot, and no core prefab, layer, script or config names a pack-owned sound
// directly (it asks for a slot, so a pack can swap the sound).
//
// A slots file is any addon .conf whose root is LF_SoundSlotsConfig: the core file and every pack
// overlay. Test fixtures under Configs/Tests/ are skipped (LF_Test_SoundSlots points a slot at a
// missing file on purpose). A project resolves when it is an addon .acp whose .meta carries the same
// GUID, or a base game project in the game's resource database (skipped with a note when the game
// is not installed, as in CI). Slot ids asked for: `m_sSoundSlot "id"` in an .et/.layer, and
// `LF_SoundSlots.Get("id")` in script.
//
// Pack-owned sounds are the folders in PACK_SOUND_DIRS (docs/architecture/core-pack-inventory.md
// lists Sounds/Atmosphere/ as pack). The core slots file is the one place core may name them: it holds
// each slot's default until the sounds move into the pack (#323 batch 4); the check counts those as a
// note. Core tests may name them too.
//
// red-proof: sound-slots-selftest.mjs (a fixture with a wrong GUID, a missing addon project, a
// duplicate core id, a slot id the core file lacks and a prefab that names an atmosphere sound
// directly must fail each).
import { execFileSync } from "node:child_process";
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { ROOT } from "./validate-item-catalogue.mjs";

export const CORE_FILE = "addons/LifeFramework/Configs/SoundSlots/LF_SoundSlots.conf";
export const PACK_SOUND_DIRS = ["Sounds/Atmosphere/"];
const GAME = process.env.ENFUSION_GAME_PATH || "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger";

/** The slot records of one .conf ({ line, slot, project, op }), or null when the root is another class. */
export function parseSlots(text) {
  if (!/^\s*LF_SoundSlotsConfig\s*\{/.test(text)) return null;
  const slots = [];
  let cur = null;
  let depth = 0;
  let recDepth = -1;
  text.split("\n").forEach((line, i) => {
    if (!cur && /^\s*LF_SoundSlot\b.*\{\s*$/.test(line)) {
      cur = { line: i + 1, slot: "", project: "", op: "" };
      recDepth = depth;
    }
    if (cur) {
      const f = /^\s*(m_sSlot|m_Project|m_sOp)\s+"(.*)"\s*$/.exec(line);
      if (f) cur[f[1] === "m_sSlot" ? "slot" : f[1] === "m_Project" ? "project" : "op"] = f[2];
    }
    for (const c of line) {
      if (c === "{") depth++;
      else if (c === "}") depth--;
    }
    if (cur && depth === recDepth) {
      slots.push(cur);
      cur = null;
    }
  });
  return slots;
}

/** Slot ids a prefab, layer or script asks for. */
export function askedSlotIds(file, text) {
  const ids = [];
  if (/\.(et|layer|ent)$/.test(file)) for (const m of text.matchAll(/m_sSoundSlot\s+"([^"]+)"/g)) ids.push(m[1]);
  if (file.endsWith(".c")) for (const m of text.matchAll(/LF_SoundSlots\.Get\(\s*"([^"]+)"\s*\)/g)) ids.push(m[1]);
  return ids;
}

const splitRef = (ref) => {
  const m = /^\{([0-9A-F]{16})\}(.+\.acp)$/.exec(ref || "");
  return m ? { guid: m[1], path: m[2] } : null;
};

/**
 * @param confs [{ file, text }]  @param others [{ file, text }] prefabs, layers, scripts and configs
 * @param ctx { addonMeta(path), basePaths: Set|null }
 */
export function checkSoundSlots(confs, others, ctx) {
  const fails = [];
  const notes = [];
  let unchecked = 0;
  const parsed = confs
    .filter((f) => !/\/Configs\/Tests\//.test(f.file))
    .map((f) => ({ ...f, slots: parseSlots(f.text) }))
    .filter((f) => f.slots);
  const core = parsed.find((f) => f.file === CORE_FILE);
  const coreIds = new Set();
  let packOwnedDefaults = 0;
  if (!core) fails.push(`${CORE_FILE}: missing or not an LF_SoundSlotsConfig`);
  for (const { file, slots } of parsed) {
    for (const s of slots) {
      const at = `${file}:${s.line} slot ${s.slot || "(no id)"}`;
      if (!s.slot) { fails.push(`${at}: has no m_sSlot`); continue; }
      if (file === CORE_FILE) {
        if (coreIds.has(s.slot)) fails.push(`${at}: the core file lists this slot twice`);
        coreIds.add(s.slot);
        if (PACK_SOUND_DIRS.some((d) => s.project.includes(d))) packOwnedDefaults++;
      }
      if (s.op === "remove" || !s.project) continue;
      const ref = splitRef(s.project);
      if (!ref) { fails.push(`${at}: ${s.project} is not a {GUID}path to an .acp`); continue; }
      const guid = ctx.addonMeta(ref.path);
      if (guid === null) fails.push(`${at}: ${ref.path} is an addon path with no .meta`);
      else if (guid !== undefined && guid !== ref.guid) fails.push(`${at}: ${ref.path} has GUID ${guid} in its .meta, the slot says ${ref.guid}`);
      else if (guid === undefined) {
        if (!ctx.basePaths) unchecked++;
        else if (!ctx.basePaths.has(ref.path)) fails.push(`${at}: ${ref.path} is neither an addon resource nor a base game sound project`);
      }
    }
  }
  for (const { file, text } of others) {
    if (core && !/\/Scripts\/Game\/Tests\//.test(file)) {
      for (const id of askedSlotIds(file, text)) {
        if (!coreIds.has(id)) fails.push(`${file}: asks for sound slot '${id}', which ${CORE_FILE} does not list`);
      }
    }
    const inCore = file.startsWith("addons/LifeFramework/");
    const exempt = file === CORE_FILE || /\/Configs\/Tests\//.test(file) || /\/Scripts\/Game\/Tests\//.test(file) || /^addons\/LifeFramework\/Sounds\//.test(file);
    if (inCore && !exempt) {
      text.split("\n").forEach((line, i) => {
        const dir = PACK_SOUND_DIRS.find((d) => line.includes(d));
        if (dir) fails.push(`${file}:${i + 1}: names a pack-owned sound (${dir}) directly; ask for a sound slot (Configs/SoundSlots/LF_SoundSlots.conf)`);
      });
    }
  }
  if (packOwnedDefaults) notes.push(`${packOwnedDefaults} core slot default(s) still name a pack-owned sound; they move with the sounds into the pack (#323 batch 4)`);
  if (unchecked) notes.push(`${unchecked} base game sound project(s) not checked: no game install at ENFUSION_GAME_PATH`);
  return { fails, notes, files: parsed.length, slots: coreIds.size };
}

/** Addon .acp path -> GUID from its .meta; null when the .acp has no readable .meta; undefined when not an addon path. */
export function repoAcpMeta(root, files) {
  const metas = new Map();
  for (const f of files) {
    const m = /^addons\/[^/]+\/(.+\.acp)\.meta$/.exec(f);
    if (!m) continue;
    const g = /Name\s+"\{([0-9A-F]{16})\}/.exec(readFileSync(join(root, f), "utf8"));
    metas.set(m[1], g ? g[1] : null);
  }
  const acps = new Set(files.map((f) => /^addons\/[^/]+\/(.+\.acp)$/.exec(f)?.[1]).filter(Boolean));
  return (path) => (metas.has(path) ? metas.get(path) : acps.has(path) ? null : undefined);
}

function rdbAcp(file) {
  if (!existsSync(file)) return null;
  return new Set(readFileSync(file).toString("latin1").match(/[A-Za-z0-9_/]+\.acp/g) || []);
}

function main() {
  const files = execFileSync("git", ["ls-files", "-z", "--cached", "--others", "--exclude-standard", "addons"], { cwd: ROOT, encoding: "utf8", maxBuffer: 1 << 28 })
    .split("\0").filter((f) => f && existsSync(join(ROOT, f)));
  const read = (f) => ({ file: f, text: readFileSync(join(ROOT, f), "utf8") });
  const text = (f) => /\.(et|layer|ent|c|conf)$/.test(f);
  const res = checkSoundSlots(files.filter((f) => f.endsWith(".conf")).map(read), files.filter(text).map(read), {
    addonMeta: repoAcpMeta(ROOT, files),
    basePaths: rdbAcp(join(GAME, "addons", "data", "resourceDatabase.rdb")),
  });
  for (const n of res.notes) console.log(`NOTE ${n}`);
  if (res.fails.length) {
    for (const f of res.fails) console.log(`FAIL ${f}`);
    console.log(`validate-sound-slots: ${res.fails.length} finding(s)`);
    process.exit(1);
  }
  console.log(`validate-sound-slots: OK (${res.slots} core slot(s), ${res.files} slots file(s))`);
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) main();
