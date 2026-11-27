// inputs: addons/**, tools/validation/validate-dialogue-patch.mjs
// Missing dialogue and sign text, offline (the "so much missing dialogue" and "no titled entry" reports of
// 2026-11-19). Fails when a player would read a raw id or an empty sheet:
//   key     a `#LF-...` / `#EL-...` key written in a layout, conf, prefab, layer or script (comments skipped) has no
//           row in any addon string table. Test fixtures under Configs/Tests/ are skipped;
//   id      a prefab or layer names a dialogue id (`m_sDialogueId`) no index registers: the core
//           `Configs/Dialogue/LF_DialogueIndex.conf` plus every pack `LF_DialogueIndexConfig` overlay (add, remove);
//   node    a registered dialogue has no start node, a node says a line id the dialogue lacks, a reply leads to a node
//           it lacks, or its speaker name, a caption or a reply has no row;
//   talk    a prefab carries `LF_DialogueTalkAction` but no `LF_DialogueComponent` in its chain (the talk opens
//           nothing), a pack event or scene conf spawns a character prefab with no conversation, or a pack NPC talks
//           through a dialogue whose greeting offers one reply or none (the core's neutral "Resident" sheet);
//   sign    a sign def (core `Configs/Signs/LF_Signs.conf` and every pack overlay, folders included) has an empty or
//           placeholder title (TODO, TBD, "no titled entry", a key with no row), or a placed sign or prefab names
//           a sign id no def has (it would read "has no titled entry in LF_Signs.conf").
// The runtime twin is the `talkaudit` harness verb (Feature/Dialogue/LF_DialogueAudit.c).
// red-proof: tools/validation/dialogue-refs-selftest.mjs plants each violation.
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { join, relative, sep } from "node:path";
import { fileURLToPath } from "node:url";
import { parseConf } from "./validate-dialogue-patch.mjs";

const ROOT = join(fileURLToPath(import.meta.url), "..", "..", "..");
const KEY_RE = /#((?:LF|EL)-[A-Za-z0-9_\-]*[A-Za-z0-9_])/g;
const PLACEHOLDER = /^(todo|tbd|placeholder|xxx|\?+)\b|no titled entr/i;
const SCANNED = /\.(c|conf|et|layout|layer)$/;

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

const stripComments = (path, text) => (path.endsWith(".c") ? text.replace(/\/\*[\s\S]*?\*\//g, "").replace(/\/\/.*$/gm, "") : text);
const guidOf = (ref) => (/^\{([0-9A-Fa-f]{16})\}/.exec(ref || "") || [])[1]?.toUpperCase() || "";

// files: Map<repo path with forward slashes, text>. Returns { fails: [string], warns: [string], counts }.
export function checkDialogueRefs(files) {
  const fails = [];
  const warns = [];
  const counts = { key: 0, id: 0, node: 0, talk: 0, sign: 0 };
  const fail = (kind, msg) => { counts[kind]++; fails.push(`${kind}: ${msg}`); };

  const rows = new Set();
  const byGuid = new Map();
  for (const [p, t] of files) {
    if (p.endsWith(".st")) for (const m of t.matchAll(/^\s*Id "([^"]+)"/gm)) rows.add(m[1]);
    if (p.endsWith(".meta")) {
      const m = /Name "\{([0-9A-Fa-f]{16})\}/.exec(t);
      if (m) byGuid.set(m[1].toUpperCase(), p.slice(0, -5));
    }
  }
  const hasRow = (key) => rows.has(String(key).replace(/^#/, ""));
  const isTest = (p) => /\/Configs\/Tests\//.test(p) || /\/Scripts\/Game\/Tests\//.test(p);

  // key
  for (const [p, t] of files) {
    if (!SCANNED.test(p) || isTest(p) || /localization\.[a-z_]+\.conf$/.test(p)) continue;
    const seen = new Set();
    for (const m of stripComments(p, t).matchAll(KEY_RE)) {
      if (seen.has(m[1]) || hasRow(m[1])) continue;
      seen.add(m[1]);
      fail("key", `${p}: #${m[1]} has no row in any .st`);
    }
  }

  // id: the registered dialogues, core index first, then pack overlays in path order.
  const registered = new Map();
  const indexes = [...files].filter(([p, t]) => p.endsWith(".conf") && !isTest(p) && /^LF_DialogueIndexConfig\b/.test(t.trimStart()));
  indexes.sort(([a], [b]) => (a.includes("/LifeFramework/") ? -1 : b.includes("/LifeFramework/") ? 1 : a.localeCompare(b)));
  for (const [p, t] of indexes) {
    for (const e of parseConf(t).lists.m_aEntries || []) {
      const id = e.fields.m_sId;
      if (e.fields.m_sOp === "remove") { registered.delete(id); continue; }
      if (e.fields.m_Conf) registered.set(id, { conf: e.fields.m_Conf, index: p });
    }
  }
  const askers = new Map();
  for (const [p, t] of files) {
    if (!/\.(et|layer)$/.test(p) || isTest(p)) continue;
    for (const m of t.matchAll(/m_sDialogueId "([^"]*)"/g)) {
      if (!m[1] || registered.has(m[1])) continue;
      if (!askers.has(m[1])) askers.set(m[1], p);
    }
  }
  for (const [id, p] of askers) fail("id", `${p}: dialogue id '${id}' is registered by no dialogue index; the talk opens nothing`);

  // node
  const thin = new Map();
  for (const [id, { conf, index }] of registered) {
    const path = byGuid.get(guidOf(conf)) || `addons/LifeFramework/${conf.replace(/^\{[0-9A-Fa-f]+\}/, "")}`;
    const text = files.get(path) ?? [...files].find(([p]) => p.endsWith("/" + conf.replace(/^\{[0-9A-Fa-f]+\}/, "")))?.[1];
    if (text === undefined) { fail("node", `${index}: dialogue '${id}' names ${conf}, which is in no addon`); continue; }
    const c = parseConf(text);
    const lines = new Map((c.lists.m_aLines || []).map((l) => [l.fields.m_sId, l.fields]));
    const nodes = new Map((c.lists.m_aNodes || []).map((n) => [n.fields.m_sId, n]));
    const where = `dialogue '${id}' (${path})`;
    if (!c.fields.m_sSpeakerKey || !hasRow(c.fields.m_sSpeakerKey)) fail("node", `${where}: speaker name '${c.fields.m_sSpeakerKey || ""}' has no row`);
    if (!nodes.has(c.fields.m_sStart)) fail("node", `${where}: start node '${c.fields.m_sStart || ""}' does not exist`);
    for (const [lid, l] of lines) if (!l.m_sTextKey || !hasRow(l.m_sTextKey)) fail("node", `${where}: line '${lid}' caption '${l.m_sTextKey || ""}' has no row`);
    for (const [nid, n] of nodes) {
      const say = n.fields.m_sLine;
      if (say && say.startsWith("#") && !hasRow(say)) fail("node", `${where}: node '${nid}' says ${say}, which has no row`);
      else if (say && !say.startsWith("#") && !lines.has(say)) fail("node", `${where}: node '${nid}' says line '${say}', which the dialogue lacks`);
      for (const o of n.lists.m_aOptions || []) {
        if (!o.fields.m_sTextKey || !hasRow(o.fields.m_sTextKey)) fail("node", `${where}: node '${nid}' reply '${o.fields.m_sTextKey || ""}' has no row`);
        if (o.fields.m_sNext && !nodes.has(o.fields.m_sNext)) fail("node", `${where}: node '${nid}' reply leads to '${o.fields.m_sNext}', which does not exist`);
      }
    }
    const start = nodes.get(c.fields.m_sStart);
    if (start && (start.lists.m_aOptions || []).length <= 1) thin.set(id, (start.lists.m_aOptions || []).length);
  }
  // A thin sheet (a greeting, then only "Goodbye") is the core's neutral example; a pack NPC on one is a placeholder.
  // These two are the core's shipped neutral sheets: no place names or flavour (core-vs-pack.md), so the greeting has
  // nothing to branch on. A pack NPC that uses one still fails below; only the core-only warning is allowed.
  const THIN_CORE_EXAMPLES = new Map([
    ["resident", "neutral core Resident sheet: a greeting and Goodbye; the pack's residents carry their own sheets"],
    ["dispatcher", "neutral core Dispatcher sheet: a greeting with no menu behind it; no pack NPC talks through it"],
  ]);
  for (const [id, replies] of thin) {
    const packUsers = [...files].filter(([p, t]) => /\.(et|layer)$/.test(p) && !isTest(p) && !p.startsWith("addons/LifeFramework/") && t.includes(`m_sDialogueId "${id}"`)).map(([p]) => p);
    if (packUsers.length) fail("talk", `${packUsers[0]}: talks through dialogue '${id}', whose greeting offers ${replies} reply (a placeholder sheet; give the NPC its own dialogue)`);
    else if (!THIN_CORE_EXAMPLES.has(id)) warns.push(`dialogue '${id}': the greeting offers ${replies} reply (add a real sheet, or allow it in THIN_CORE_EXAMPLES with a reason)`);
  }

  // talk: a prefab's chain, by its parent GUID.
  const chainText = (path) => {
    let out = "";
    for (let p = path, i = 0; p && files.has(p) && i < 16; i++) {
      const t = files.get(p);
      out += t + "\n";
      p = byGuid.get(guidOf((/^\w+[^\n{]*:\s*"([^"]+)"/.exec(t) || [])[1]));
    }
    return out;
  };
  for (const [p, t] of files) {
    if (!p.endsWith(".et") || isTest(p) || !t.includes("LF_DialogueTalkAction")) continue;
    if (!/LF_DialogueComponent/.test(chainText(p))) fail("talk", `${p}: a talk action with no LF_DialogueComponent in its chain`);
  }
  for (const [p, t] of files) {
    if (!/\/LF_[^/]+\/Configs\/Events\/.*\.conf$/.test(p) || p.includes("/LifeFramework/")) continue;
    for (const m of t.matchAll(/"(\{[0-9A-Fa-f]{16}\}Prefabs\/Characters\/[^"]+\.et)"/g)) {
      const prefab = byGuid.get(guidOf(m[1]));
      if (!prefab) { fail("talk", `${p}: spawns ${m[1]}, a base game character nobody can talk to (give the scene a pack child with LF_DialogueComponent)`); continue; }
      if (!/LF_DialogueComponent/.test(chainText(prefab))) fail("talk", `${p}: spawns ${prefab}, which has no conversation`);
    }
  }

  // sign
  const signs = new Map();
  const signFiles = [...files].filter(([p, t]) => p.endsWith(".conf") && !isTest(p) && /^LF_SignConfig\b/.test(t.trimStart()));
  signFiles.sort(([a], [b]) => (a.includes("/LifeFramework/") ? -1 : b.includes("/LifeFramework/") ? 1 : a.localeCompare(b)));
  for (const [p, t] of signFiles) {
    for (const s of parseConf(t).lists.m_aSigns || []) {
      const id = s.fields.m_sId;
      if (s.fields.m_sOp === "remove") { signs.delete(id); continue; }
      const prev = signs.get(id);
      const title = s.fields.m_sTitle ?? (s.fields.m_sOp === "patch" && prev ? prev.title : "");
      signs.set(id, { title, file: p });
    }
  }
  for (const [id, s] of signs) {
    const title = String(s.title || "").trim();
    if (!title) fail("sign", `${s.file}: sign '${id}' has no title`);
    else if (PLACEHOLDER.test(title.replace(/^#/, ""))) fail("sign", `${s.file}: sign '${id}' title '${title}' is a placeholder`);
    else if (title.startsWith("#") && !hasRow(title)) fail("sign", `${s.file}: sign '${id}' title ${title} has no row`);
    else if (title.startsWith("#") && rows.has(title.slice(1)) && PLACEHOLDER.test(enText(files, title.slice(1)))) fail("sign", `${s.file}: sign '${id}' title ${title} reads as a placeholder`);
  }
  const placed = new Map();
  for (const [p, t] of files) {
    if (!/\.(et|layer)$/.test(p) || isTest(p)) continue;
    for (const m of t.matchAll(/m_sSignId "([^"]*)"/g)) {
      if (!m[1] || m[1].startsWith("grid:") || signs.has(m[1]) || placed.has(m[1])) continue;
      placed.set(m[1], p);
    }
  }
  for (const [id, p] of placed) fail("sign", `${p}: sign id '${id}' has no titled entry in LF_Signs.conf or a pack overlay`);

  return { fails, warns, counts, dialogues: registered.size, signs: signs.size };
}

// The English text of a row, for the placeholder check.
function enText(files, id) {
  for (const [p, t] of files) {
    if (!p.endsWith(".st")) continue;
    const at = t.indexOf(`Id "${id}"`);
    if (at < 0) continue;
    const m = /Target_en_us "([^"]*)"/.exec(t.slice(at, at + 2000));
    if (m) return m[1];
  }
  return "";
}

export function readAddons(root = ROOT) {
  const files = new Map();
  for (const f of walk(join(root, "addons"))) {
    const p = relative(root, f).split(sep).join("/");
    if (/\.(c|conf|et|layout|layer|st|meta)$/.test(p)) files.set(p, readFileSync(f, "utf8"));
  }
  return files;
}

function main() {
  const r = checkDialogueRefs(readAddons());
  for (const w of r.warns) console.log(`WARN ${w}`);
  if (r.fails.length) {
    for (const f of r.fails) console.log(`FAIL ${f}`);
    console.log(`validate-dialogue-refs: FAIL ${r.fails.length} (key ${r.counts.key}, id ${r.counts.id}, node ${r.counts.node}, talk ${r.counts.talk}, sign ${r.counts.sign})`);
    process.exit(1);
  }
  console.log(`validate-dialogue-refs: OK (${r.dialogues} dialogues, ${r.signs} signs, every key, id, node, talk and sign title resolves)`);
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) main();
