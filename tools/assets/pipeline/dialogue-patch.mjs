// A pack's patch of a core dialogue, written by `tools\cli asset dialogue <npc> --addon <pack>` (#957).
// A dialogue.json with "patches": "<core npc>" does not make a new conversation: it writes one `.conf` of
// LF_DialogueConfig carrying the override verbs (add, patch, remove by id) and the caption strings into the
// pack's table. Its lines are text only: no ElevenLabs call, no budget, no wav; the voice stays the core's.
// A pack that wants its own voice writes a full dialogue under its own id and replaces the id in the
// dialogue registry. Playbook: docs/asset-pipeline/dialogue.md "Patching a core dialogue from a pack".
//
//   { "id": "clerk_everon", "name": "ClerkEveron", "patches": "clerk", "addon": "LF_Everon1989",
//     "lines":   { "greet": { "text": "..." }, "lost_note": { "text": "..." } },   // patch if the core has it, else add
//     "nodes":   [ { "id": "work", "op": "remove" }, { "id": "menu", "options": [ { "text": "ask", "next": "bye" } ] } ],
//     "replies": { "ask": "..." } }
import { existsSync, mkdirSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { freshGuid } from "./meta.mjs";
import { addonProblems, assertBoundary, guidsInUse, stringTable } from "./addon.mjs";
import { MAX_OPTIONS, confRes, expand, lineKey, parseStrings, replyKey, simpleMeta, specFile, stableGuid, upsertStrings } from "./dialogue.mjs";

const CORE_NAME = "LifeFramework";
const OPS = new Set(["add", "patch", "remove"]);
const addonDirOf = (spec) => `addons/${spec.addon}`;

export const isPatch = (spec) => Boolean(spec.patches);

// The pack's own string table: the first Language/*.st of its addon.
export function patchStringFile(root, spec) {
  const dir = join(root, addonDirOf(spec), "Language");
  const st = existsSync(dir) ? readdirSync(dir).find((f) => f.endsWith(".st")) : null;
  return st ? join(dir, st) : null;
}

// The op a patch entry carries: the one written, else patch when the core has the id and add when it does not.
export const patchOp = (entry, coreHas) => entry.op || (coreHas ? "patch" : "add");

// The core dialogue spec a patch is read against: tools/assets/dialogue/<patches>/dialogue.json.
export function loadCoreSpec(root, spec) {
  const file = specFile(root, spec.patches);
  if (!existsSync(file)) throw new Error(`patches '${spec.patches}': no core dialogue at ${file}`);
  return expand(JSON.parse(readFileSync(file, "utf8")));
}

// Everything wrong with a patch spec against its core, as a list. `strings` maps existing string ids to text.
export function patchProblems(spec, core, strings = new Map()) {
  const out = [];
  if (!/^[a-z][a-z0-9_]*$/.test(spec.id || "")) out.push("id must be lowercase letters, digits and underscores");
  if (!/^[A-Z][A-Za-z0-9]*$/.test(spec.name || "")) out.push("name must be PascalCase (it becomes file and key names)");
  if (!spec.addon || spec.addon === CORE_NAME) out.push("a patch lives in a pack: set addon (or pass --addon) to the pack");
  if (spec.voice || spec.budget_chars) out.push("a patch is text only: drop voice and budget_chars (a pack with its own voice writes a full dialogue and replaces the id in the registry)");
  const coreLines = core.lines || {};
  const coreNodes = new Map((core.nodes || []).map((n) => [n.id, n]));
  const nodes = spec.nodes || [];
  const replies = spec.replies || {};
  for (const [id, line] of Object.entries(spec.lines || {})) {
    const op = patchOp(line, Boolean(coreLines[id]));
    if (!OPS.has(op)) out.push(`line ${id}: op '${op}' is not add, patch or remove`);
    else if (op === "add" && coreLines[id]) out.push(`line ${id}: add names a line the core dialogue already has; use patch`);
    else if (op !== "add" && !coreLines[id]) out.push(`line ${id}: ${op} names a line the core dialogue lacks`);
    if (op !== "remove") {
      const text = (line.text || "").trim();
      if (text.length < 2 || text.length > 320) out.push(`line ${id}: text must be 2 to 320 characters`);
      if (/[<>{}\[\]%]|#[A-Z]/.test(text)) out.push(`line ${id}: text has markup or a placeholder`);
    }
    if (line.for_key && !strings.has(line.for_key.replace(/^#/, ""))) out.push(`line ${id}: for_key ${line.for_key} is not in the string table`);
  }
  const removed = new Set(nodes.filter((n) => n.op === "remove").map((n) => n.id));
  const nodeIds = new Set([...coreNodes.keys(), ...nodes.filter((n) => n.op !== "remove").map((n) => n.id)]);
  for (const id of removed) nodeIds.delete(id);
  const lineIds = new Set([...Object.keys(coreLines), ...Object.entries(spec.lines || {}).filter(([, l]) => l.op !== "remove").map(([id]) => id)]);
  for (const n of nodes) {
    const op = patchOp(n, coreNodes.has(n.id));
    if (!OPS.has(op)) out.push(`node ${n.id}: op '${op}' is not add, patch or remove`);
    else if (op === "add" && coreNodes.has(n.id)) out.push(`node ${n.id}: add names a node the core dialogue already has; use patch`);
    else if (op !== "add" && !coreNodes.has(n.id)) out.push(`node ${n.id}: ${op} names a node the core dialogue lacks`);
    if (n.line && !lineIds.has(n.line)) out.push(`node ${n.id}: line '${n.line}' does not exist`);
    if ((n.options || []).length > MAX_OPTIONS) out.push(`node ${n.id}: ${n.options.length} options; the screen shows ${MAX_OPTIONS}`);
    for (const o of n.options || []) {
      if (!replies[o.text]) out.push(`node ${n.id}: reply '${o.text}' is not in this patch's replies`);
      if (o.next && !nodeIds.has(o.next)) out.push(`node ${n.id}: option '${o.text}' leads to unknown node '${o.next}'`);
      for (const hook of [o.action, o.condition]) if (hook && !/^[A-Z][A-Za-z0-9]*$/.test(hook)) out.push(`node ${n.id}: hook '${hook}' must be PascalCase`);
    }
  }
  // A core node the patch keeps that led to a removed node would break the dialogue at runtime.
  for (const [id, n] of coreNodes) {
    if (removed.has(id) || nodes.some((x) => x.id === id && x.options)) continue;
    for (const o of n.options || []) if (o.next && !nodeIds.has(o.next)) out.push(`node ${id} (core) still leads to '${o.next}', which this patch removes; patch ${id} too`);
  }
  if (spec.start && !nodeIds.has(spec.start)) out.push(`start node '${spec.start}' does not exist`);
  return out;
}

export function patchStringEntries(spec) {
  const out = [];
  for (const [id, line] of Object.entries(spec.lines || {})) {
    if (line.op === "remove") continue;
    out.push({ id: lineKey(spec, id), text: line.text.trim(), comment: `Pack patch of the ${spec.patches} dialogue's line ${id} (text only; tools/assets/dialogue/${spec.id}). Changing it needs: tools\\cli asset dialogue ${spec.id}` });
  }
  for (const [id, text] of Object.entries(replies(spec))) {
    out.push({ id: replyKey(spec, id), text: text.trim(), comment: `Pack patch reply for the ${spec.patches} dialogue (tools/assets/dialogue/${spec.id}).` });
  }
  return out;
}

const replies = (spec) => spec.replies || {};

// The patch .conf: LF_DialogueConfig of the core dialogue's id, with the override verbs.
export function patchConfText(spec, core) {
  const q = (s) => `"${String(s ?? "").replace(/"/g, '\\"')}"`;
  const coreLines = core.lines || {}, coreNodes = new Set((core.nodes || []).map((n) => n.id));
  let rows = "";
  for (const [id, l] of Object.entries(spec.lines || {})) {
    const op = patchOp(l, Boolean(coreLines[id]));
    rows += `  LF_DialogueLine "{${stableGuid(spec.id, "line", id)}}" {\n   m_sOp ${q(op)}\n   m_sId ${q(id)}\n`
      + (op === "remove" ? "" : `   m_sTextKey ${q("#" + lineKey(spec, id))}\n`) + (l.for_key && op !== "remove" ? `   m_sForKey ${q(l.for_key)}\n` : "") + "  }\n";
  }
  let nodes = "";
  for (const n of spec.nodes || []) {
    const op = patchOp(n, coreNodes.has(n.id));
    let options = "";
    (n.options || []).forEach((o, i) => {
      options += `    LF_DialogueOption "{${stableGuid(spec.id, "opt", n.id, i)}}" {\n     m_sTextKey ${q("#" + replyKey(spec, o.text))}\n`
        + (o.next ? `     m_sNext ${q(o.next)}\n` : "") + (o.condition ? `     m_sCondition ${q(o.condition)}\n` : "")
        + (o.action ? `     m_sAction ${q(o.action)}\n` : "") + (o.param ? `     m_sParam ${q(o.param)}\n` : "") + "    }\n";
    });
    nodes += `  LF_DialogueNode "{${stableGuid(spec.id, "node", n.id)}}" {\n   m_sOp ${q(op)}\n   m_sId ${q(n.id)}\n` + (n.line ? `   m_sLine ${q(n.line)}\n` : "")
      + (n.options ? `   m_aOptions {\n${options}   }\n` : "") + "  }\n";
  }
  return `LF_DialogueConfig {\n m_sId ${q(spec.patches)}\n` + (spec.start ? ` m_sStart ${q(spec.start)}\n` : "")
    + (rows ? ` m_aLines {\n${rows} }\n` : "") + (nodes ? ` m_aNodes {\n${nodes} }\n` : "") + "}\n";
}

// What the tracked files should hold for a patch spec, for the validator: { path: text } (relative to the addon).
export function patchExpectedFiles(root, spec) {
  return { [confRes(spec)]: patchConfText(spec, loadCoreSpec(root, spec)) };
}

export function runPatch(root, spec, { dryRun = false, log = console.log } = {}) {
  const addon = join(root, addonDirOf(spec));
  const stFile = patchStringFile(root, spec);
  if (!stFile) throw new Error(`${addonDirOf(spec)} has no Language/*.st; the pack's string table does not exist yet (core-pack-inventory.md, #325)`);
  const stText = readFileSync(stFile, "utf8");
  const known = parseStrings(readFileSync(stringTable(root, { addon: CORE_NAME }), "utf8"));
  for (const [k, v] of parseStrings(stText)) known.set(k, v);
  const core = loadCoreSpec(root, spec);
  const problems = [...addonProblems(root, spec), ...patchProblems(spec, core, known)];
  if (problems.length) throw new Error("dialogue patch problems:\n  " + problems.join("\n  "));
  log(`dialogue patch ${spec.id}: ${Object.keys(spec.lines || {}).length} line(s) and ${(spec.nodes || []).length} node(s) of '${spec.patches}', text only: 0 characters to buy`);
  if (dryRun) return { generated: 0, chars: 0, need: 0 };
  const conf = join(addon, confRes(spec));
  mkdirSync(dirname(conf), { recursive: true });
  if (!existsSync(conf + ".meta")) writeFileSync(conf + ".meta", simpleMeta(confRes(spec), freshGuid(guidsInUse(root)), "CONFResourceClass"));
  writeFileSync(conf, patchConfText(spec, core));
  const newSt = upsertStrings(stText, patchStringEntries(spec));
  const stChanged = newSt !== stText;
  if (stChanged) writeFileSync(stFile, newSt);
  assertBoundary(root, spec);
  log(`wrote ${addonDirOf(spec)}/${confRes(spec)}; list it in the pack's mission header m_aLF_ConfigOverlays`);
  if (stChanged) log("string table changed: run tools\\cli regen-localization");
  return { generated: 0, chars: 0, need: 0, stChanged };
}
