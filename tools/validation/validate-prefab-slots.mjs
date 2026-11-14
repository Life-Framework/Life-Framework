// inputs: addons/**, tools/validation/validate-item-catalogue.mjs
// Prefab slots (#964): every slot prefab resolves, and every slot id script asks for is a core slot.
//
// A slots file is any addon .conf whose root is LF_PrefabSlotsConfig: the core file and every pack
// overlay. Test fixtures under Configs/Tests/ are skipped (LF_Test_PrefabSlotsOverlay points a slot
// at a missing file on purpose). A prefab resolves when it is an addon .et whose .meta carries the
// same GUID, or a base game prefab in the game's resource database (skipped with a note when the
// game is not installed, as in CI). Script slot ids are the `static const string SLOT... = "id"`
// constants that feed LF_PrefabSlots.Get.
//
// red-proof: prefab-slots-selftest.mjs (a fixture with a wrong GUID, a missing addon prefab, a
// duplicate core id, a slot with no prefab and a script id the core file lacks must fail each).
// Observed 2026-11-14 on the real file: the cash slot GUID changed to 5439738849229353 printed "FAIL
// ... LF_PrefabSlots.conf:3 slot cash: ... has GUID 5439738849229352 in its .meta, the slot says
// 5439738849229353" and exited 1.
import { execFileSync } from "node:child_process";
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { ROOT, rdbPaths, repoMeta, splitRef } from "./validate-item-catalogue.mjs";

export const CORE_FILE = "addons/LifeFramework/Configs/PrefabSlots/LF_PrefabSlots.conf";
const GAME = process.env.ENFUSION_GAME_PATH || "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger";

/** The slot records of one .conf ({ line, slot, prefab, op }), or null when the root is another class. */
export function parseSlots(text) {
  if (!/^\s*LF_PrefabSlotsConfig\s*\{/.test(text)) return null;
  const slots = [];
  let cur = null;
  let depth = 0;
  let recDepth = -1;
  text.split("\n").forEach((line, i) => {
    if (!cur && /^\s*LF_PrefabSlot\b.*\{\s*$/.test(line)) {
      cur = { line: i + 1, slot: "", prefab: "", op: "" };
      recDepth = depth;
    }
    if (cur) {
      const f = /^\s*(m_sSlot|m_Prefab|m_sOp)\s+"(.*)"\s*$/.exec(line);
      if (f) cur[f[1] === "m_sSlot" ? "slot" : f[1] === "m_Prefab" ? "prefab" : "op"] = f[2];
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

/** Slot ids script names: `static const string SLOT[_X] = "id";` in a file that calls LF_PrefabSlots.Get. */
export function scriptSlotIds(text) {
  if (!/LF_PrefabSlots\.Get\(/.test(text)) return [];
  return [...text.matchAll(/static\s+const\s+string\s+SLOT\w*\s*=\s*"([a-z0-9_]+)"/g)].map((m) => m[1]);
}

/**
 * @param confs [{ file, text }]   @param scripts [{ file, text }]
 * @param ctx { addonMeta(path), basePaths: Set|null }
 */
export function checkSlots(confs, scripts, ctx) {
  const fails = [];
  const notes = [];
  let unchecked = 0;
  const parsed = confs
    .filter((f) => !/\/Configs\/Tests\//.test(f.file))
    .map((f) => ({ ...f, slots: parseSlots(f.text) }))
    .filter((f) => f.slots);
  const core = parsed.find((f) => f.file === CORE_FILE);
  const coreIds = new Set();
  if (!core) fails.push(`${CORE_FILE}: missing or not an LF_PrefabSlotsConfig`);
  for (const { file, slots } of parsed) {
    for (const s of slots) {
      const at = `${file}:${s.line} slot ${s.slot || "(no id)"}`;
      if (!s.slot) { fails.push(`${at}: has no m_sSlot`); continue; }
      if (file === CORE_FILE) {
        if (coreIds.has(s.slot)) fails.push(`${at}: the core file lists this slot twice`);
        coreIds.add(s.slot);
        if (!s.prefab) fails.push(`${at}: a core slot needs m_Prefab`);
      }
      if (s.op === "remove" || !s.prefab) continue;
      const ref = splitRef(s.prefab);
      if (!ref) { fails.push(`${at}: ${s.prefab} is not a {GUID}path to an .et`); continue; }
      const guid = ctx.addonMeta(ref.path);
      if (guid === null) fails.push(`${at}: ${ref.path} is an addon path with no .meta`);
      else if (guid !== undefined && guid !== ref.guid) fails.push(`${at}: ${ref.path} has GUID ${guid} in its .meta, the slot says ${ref.guid}`);
      else if (guid === undefined) {
        if (!ctx.basePaths) unchecked++;
        else if (!ctx.basePaths.has(ref.path)) fails.push(`${at}: ${ref.path} is neither an addon resource nor a base game prefab`);
      }
    }
  }
  if (core) {
    for (const { file, text } of scripts) {
      for (const id of scriptSlotIds(text)) {
        if (!coreIds.has(id)) fails.push(`${file}: asks for slot '${id}', which ${CORE_FILE} does not list`);
      }
    }
  }
  if (unchecked) notes.push(`${unchecked} base game prefab(s) not checked: no game install at ENFUSION_GAME_PATH`);
  return { fails, notes, files: parsed.length, slots: coreIds.size };
}

function main() {
  const files = execFileSync("git", ["ls-files", "-z", "--cached", "--others", "--exclude-standard", "addons"], { cwd: ROOT, encoding: "utf8", maxBuffer: 1 << 28 })
    .split("\0").filter((f) => f && existsSync(join(ROOT, f)));
  const read = (f) => ({ file: f, text: readFileSync(join(ROOT, f), "utf8") });
  const res = checkSlots(files.filter((f) => f.endsWith(".conf")).map(read), files.filter((f) => f.endsWith(".c")).map(read), {
    addonMeta: repoMeta(ROOT, files),
    basePaths: rdbPaths(join(GAME, "addons", "data", "resourceDatabase.rdb")),
  });
  for (const n of res.notes) console.log(`NOTE ${n}`);
  if (res.fails.length) {
    for (const f of res.fails) console.log(`FAIL ${f}`);
    console.log(`validate-prefab-slots: ${res.fails.length} finding(s)`);
    process.exit(1);
  }
  console.log(`validate-prefab-slots: OK (${res.slots} core slot(s), ${res.files} slots file(s))`);
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) main();
