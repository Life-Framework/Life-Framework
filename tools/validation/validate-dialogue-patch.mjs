// tools/validation/validate-dialogue-patch.mjs
//
// A pack's dialogue patch read against its core dialogue, offline (#957). The runtime applies a pack
// `.conf` of class LF_DialogueConfig over the core file and drops a broken result whole
// (LF_DialogueFlow.Choose); this catches the same breakage before a play session. A pack `.conf`
// (anything under addons/<pack>/ except the core) whose root class is LF_DialogueConfig is a patch of
// the core dialogue with its `m_sId`. Fails when:
//   - the patch names a dialogue id the core does not ship, or its verbs (add, replace, patch, remove)
//     name ids the way the override model refuses (docs/config/override-model.md);
//   - the result has no start node, a reply to an unknown node, a node saying an unknown line, more than
//     four replies, a reply hook with no class LF_DialogueHook_<Name>, or a line with a half-made recording
//     (an event without a length or the reverse; a text-only line has neither);
//   - a text key the patch writes is in no .st.
// A pack LF_DialogueIndexConfig overlay is checked the same way: its verbs against the core index and its
// files against the addons.
// red-proof: tools/validation/dialogue-patch-selftest.mjs plants each violation.

import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { dirname, join, relative, sep } from "node:path";
import { fileURLToPath } from "node:url";

const CORE = "LifeFramework";
const MAX_OPTIONS = 4;
const VERBS = new Set(["", "add", "replace", "patch", "remove"]);

function walk(dir, out = []) {
  if (!existsSync(dir)) return out;
  for (const name of readdirSync(dir)) {
    if (name === "log" || name === ".git" || name === "node_modules") continue;
    const p = join(dir, name);
    if (statSync(p).isDirectory()) walk(p, out);
    else out.push(p);
  }
  return out;
}

// One-field-per-line `.conf` text to a tree: { cls, guid, fields, lists: { m_aNodes: [node, ...] } }.
export function parseConf(text) {
  const root = { cls: "", guid: "", fields: {}, lists: {} };
  // Each frame is the object whose fields/lists the next lines fill, or a list that collects children.
  const stack = [];
  for (const raw of text.split(/\r?\n/)) {
    const line = raw.trim();
    if (!line) continue;
    if (line === "}") { stack.pop(); continue; }
    const open = /^([A-Za-z_]\w*)(?:\s+"([^"]*)")?\s*\{$/.exec(line);
    if (open) {
      const top = stack[stack.length - 1];
      if (open[1].startsWith("m_")) {
        const list = [];
        top.obj.lists[open[1]] = list;
        stack.push({ list });
      } else if (!top) {
        root.cls = open[1];
        root.guid = open[2] || "";
        stack.push({ obj: root });
      } else {
        const child = { cls: open[1], guid: open[2] || "", fields: {}, lists: {} };
        top.list.push(child);
        stack.push({ obj: child });
      }
      continue;
    }
    const f = /^(m_\w+)\s+(.+)$/.exec(line);
    const top = stack[stack.length - 1];
    if (f && top && top.obj) top.obj.fields[f[1]] = f[2].replace(/^"(.*)"$/, "$1");
  }
  return root;
}

const num = (s) => Number(s || 0);
const guidPath = (ref) => (ref || "").replace(/^\{[0-9A-Fa-f]+\}/, "");

function stringKeys(root) {
  const keys = new Set();
  for (const file of walk(join(root, "addons"))) {
    if (!file.endsWith(".st")) continue;
    for (const m of readFileSync(file, "utf8").matchAll(/^\s*Id "([^"]+)"/gm)) keys.add(m[1]);
  }
  return keys;
}

function hookClasses(root) {
  const names = new Set();
  for (const file of walk(join(root, "addons"))) {
    if (!file.endsWith(".c")) continue;
    for (const m of readFileSync(file, "utf8").matchAll(/class\s+LF_DialogueHook_(\w+)/g)) names.add(m[1]);
  }
  return names;
}

// A dialogue as lines and nodes keyed by id.
function toModel(conf) {
  const lines = new Map(), nodes = new Map();
  for (const l of conf.lists.m_aLines || []) lines.set(l.fields.m_sId, { ...l.fields });
  for (const n of conf.lists.m_aNodes || []) nodes.set(n.fields.m_sId, { ...n.fields, options: (n.lists.m_aOptions || []).map((o) => ({ ...o.fields })) });
  return { id: conf.fields.m_sId, start: conf.fields.m_sStart, lines, nodes };
}

// Applies the patch's entries with the override model's refusals (a refused entry skips only itself).
function applyList(where, kind, model, entries, errors) {
  const map = kind === "line" ? model.lines : model.nodes;
  for (const e of entries) {
    const id = e.fields.m_sId, op = e.fields.m_sOp || "";
    if (!VERBS.has(op)) { errors.push(`${where}: ${kind} ${id}: op "${op}" is not add, replace, patch or remove`); continue; }
    const has = map.has(id);
    if (op === "add" && has) { errors.push(`${where}: ${kind} ${id}: add names an id the core dialogue already has`); continue; }
    if (op && op !== "add" && !has) { errors.push(`${where}: ${kind} ${id}: ${op} names an id the core dialogue lacks`); continue; }
    if (op === "remove") { map.delete(id); continue; }
    const fresh = kind === "line" ? { ...e.fields } : { ...e.fields, options: (e.lists.m_aOptions || []).map((o) => ({ ...o.fields })) };
    if (op === "replace" || op === "add" || !has) { map.set(id, fresh); continue; }
    const cur = map.get(id);
    for (const [k, v] of Object.entries(e.fields)) if (k !== "m_sOp") cur[k] = v;
    if (kind === "node" && e.lists.m_aOptions) cur.options = fresh.options;
  }
}

function problems(where, model, keys, hooks, patchKeys) {
  const out = [];
  if (!model.nodes.has(model.start)) out.push(`${where}: start node '${model.start}' does not exist after the patch`);
  for (const [id, l] of model.lines) {
    if (!!l.m_sEvent !== (num(l.m_fSeconds) > 0)) out.push(`${where}: line ${id} is a half-made recording (an event needs a length and a length an event; a text-only line has neither)`);
    if (!l.m_sTextKey) out.push(`${where}: line ${id} has no caption key`);
  }
  for (const [id, n] of model.nodes) {
    if (n.m_sLine && !n.m_sLine.startsWith("#") && !model.lines.has(n.m_sLine)) out.push(`${where}: node ${id} says unknown line '${n.m_sLine}'`);
    if (n.options.length > MAX_OPTIONS) out.push(`${where}: node ${id} offers ${n.options.length} replies; the screen shows ${MAX_OPTIONS}`);
    for (const o of n.options) {
      if (!o.m_sTextKey) out.push(`${where}: node ${id} has a reply with no text`);
      if (o.m_sNext && !model.nodes.has(o.m_sNext)) out.push(`${where}: node ${id} leads to unknown node '${o.m_sNext}'`);
      for (const h of [o.m_sCondition, o.m_sAction]) if (h && !hooks.has(h)) out.push(`${where}: node ${id} names hook '${h}', which has no class LF_DialogueHook_${h}`);
    }
  }
  for (const k of patchKeys) if (!keys.has(k.replace(/^#/, ""))) out.push(`${where}: text key ${k} is in no .st`);
  return out;
}

// Violation strings; empty means every pack dialogue patch holds against its core.
export function checkDialoguePatches(root) {
  const errors = [];
  const addonsDir = join(root, "addons");
  if (!existsSync(addonsDir)) return errors;
  const core = new Map();
  for (const f of walk(join(addonsDir, CORE, "Configs", "Dialogue"))) {
    if (!f.endsWith(".conf")) continue;
    const conf = parseConf(readFileSync(f, "utf8"));
    if (conf.cls === "LF_DialogueConfig" && conf.fields.m_sId) core.set(conf.fields.m_sId, conf);
  }
  const indexFile = join(addonsDir, CORE, "Configs", "Dialogue", "LF_DialogueIndex.conf");
  const coreIndex = new Set(existsSync(indexFile) ? (parseConf(readFileSync(indexFile, "utf8")).lists.m_aEntries || []).map((e) => e.fields.m_sId) : []);
  const keys = stringKeys(root), hooks = hookClasses(root);
  const addons = readdirSync(addonsDir).filter((d) => statSync(join(addonsDir, d)).isDirectory());
  for (const pack of addons.filter((d) => d !== CORE)) {
    for (const file of walk(join(addonsDir, pack))) {
      if (!file.endsWith(".conf")) continue;
      const text = readFileSync(file, "utf8");
      if (!/^\s*LF_Dialogue(Config|IndexConfig)\s*\{/m.test(text)) continue;
      const where = relative(root, file).split(sep).join("/");
      const conf = parseConf(text);
      if (conf.cls === "LF_DialogueIndexConfig") {
        for (const e of conf.lists.m_aEntries || []) {
          const id = e.fields.m_sId, op = e.fields.m_sOp || "";
          if (!VERBS.has(op)) errors.push(`${where}: index ${id}: op "${op}" is not add, replace, patch or remove`);
          else if (op === "add" && coreIndex.has(id)) errors.push(`${where}: index ${id}: add names an id the core index already has`);
          else if (op && op !== "add" && !coreIndex.has(id)) errors.push(`${where}: index ${id}: ${op} names an id the core index lacks`);
          const p = guidPath(e.fields.m_Conf);
          if (op !== "remove" && p && !addons.some((a) => existsSync(join(addonsDir, a, p)))) errors.push(`${where}: index ${id}: file ${p} is in no addon`);
        }
        continue;
      }
      const id = conf.fields.m_sId;
      const base = core.get(id);
      if (!base) { errors.push(`${where}: patches dialogue '${id}', which the core does not ship`); continue; }
      const model = toModel(base);
      applyList(where, "line", model, conf.lists.m_aLines || [], errors);
      applyList(where, "node", model, conf.lists.m_aNodes || [], errors);
      if (conf.fields.m_sStart !== undefined) model.start = conf.fields.m_sStart;
      const patchKeys = [
        ...(conf.lists.m_aLines || []).map((l) => l.fields.m_sTextKey).filter(Boolean),
        ...(conf.lists.m_aNodes || []).flatMap((n) => (n.lists.m_aOptions || []).map((o) => o.fields.m_sTextKey).filter(Boolean)),
      ];
      errors.push(...problems(where, model, keys, hooks, patchKeys));
    }
  }
  return errors;
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) {
  const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const errors = checkDialoguePatches(root);
  if (errors.length) {
    for (const e of errors) console.log(`FAIL ${e}`);
    console.log(`validate-dialogue-patch: ${errors.length} violation(s) (docs/config/override-model.md "Dialogue patches")`);
    process.exit(1);
  }
  console.log("validate-dialogue-patch: OK (every pack dialogue patch holds against its core dialogue)");
}
