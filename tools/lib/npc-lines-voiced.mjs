// tools/lib/npc-lines-voiced.mjs - is every NPC line a player can hear voiced, from the string it speaks? (2026-11-27)
//
// validate-dialogue-voices judges the lines the dialogues list (a recording, the dialogue's voice, the current text).
// This is the other half: every SOURCE of NPC speech reaches a recorded line.
//   hook    a `LF_DialogueHook_<Name>` that puts keys in `spoken` declares them in a `// lf-spoken: #LF-A #LF-B` comment
//           (a `*` in a key matches a run of key characters; `{param}` is the reply's param). Every dialogue that names the
//           hook as an `action` needs a recorded `for_key` line for each key. A key literal in the class that is not
//           declared (nor marked `// lf-not-spoken: #LF-K`) fails: a new line of speech cannot slip in silent.
//   quest   a quest giver's description, hand-in and idle strings are spoken by its conversation (QuestConverse), so
//           each needs a `for_key` line in the dialogue of every NPC prefab that carries that giver id.
//   rumor   a rumor row's `m_sLineKey` names a line of a recorded conversation.
//   announce `LF_Dialogue.Announce(npc, key)` needs a literal key with a recorded line, or a `// lf-spoken:` comment.
//   pin     a `for_key` line is a spoken paraphrase of its string, so its `key_pin` (a hash of the string's English text
//           when the line was last checked against it) must still match: the string changed, so re-read the line,
//           re-record it if it no longer says the same, then `validate-npc-lines-voiced --update`.
import { existsSync, readFileSync, readdirSync, statSync, writeFileSync } from "node:fs";
import { createHash } from "node:crypto";
import { join, relative } from "node:path";
import { loadSpecs, readPrefabs, resolvePrefab, PACK } from "./dialogue-voices.mjs";
import { lineKey, parseStrings, spokenLines } from "../assets/pipeline/dialogue.mjs";

export const KEY_RE = /#LF-[A-Za-z0-9_]+/g;

function walk(dir, out = []) {
  if (!existsSync(dir)) return out;
  for (const name of readdirSync(dir)) {
    if (name === "node_modules" || name === "log" || name === ".git") continue;
    const p = join(dir, name);
    if (statSync(p).isDirectory()) walk(p, out);
    else out.push(p);
  }
  return out;
}

// The English text of every string, the pack's row over the core's.
export function loadStrings(root) {
  const strings = new Map();
  for (const f of ["addons/LifeFramework/Language/everonlife_localization.st", `addons/${PACK}/Language/lf_everon1989_localization.st`]) {
    const file = join(root, f);
    if (existsSync(file)) for (const [k, v] of parseStrings(readFileSync(file, "utf8"))) strings.set(k, v);
  }
  return strings;
}

export const pinOf = (text) => createHash("sha1").update(String(text ?? "")).digest("hex").slice(0, 8);

// Code under Scripts/, minus the tests: { file, text }.
export function scriptFiles(root) {
  return walk(join(root, "addons")).filter((f) => f.endsWith(".c") && /[\\/]Scripts[\\/]/.test(f) && !/[\\/]Tests[\\/]/.test(f)).map((file) => ({ file: relative(root, file).replace(/\\/g, "/"), text: readFileSync(file, "utf8") }));
}

// The text of the braces that open at or after `from`, and the line number of `at`.
function braceBody(text, from) {
  const open = text.indexOf("{", from);
  if (open < 0) return "";
  let depth = 0;
  for (let i = open; i < text.length; i++) {
    if (text[i] === "{") depth++;
    else if (text[i] === "}" && --depth === 0) return text.slice(open, i + 1);
  }
  return text.slice(open);
}
const lineOf = (text, at) => text.slice(0, at).split("\n").length;

// Keys a `// lf-spoken:` comment names, and the ones `// lf-not-spoken:` clears.
const tagged = (text, tag) => [...text.matchAll(new RegExp(`//\\s*${tag}:([^\\n]*)`, "g"))].flatMap((m) => (m[1].match(/#?LF-[A-Za-z0-9_*{}-]+/g) || []).map((k) => (k.startsWith("#") ? k : "#" + k)));

// { name -> { file, line, declared: [key|glob], literals: Set, inserts: number, declaredAny: bool } } for each hook class.
export function hookClasses(files) {
  const out = new Map();
  for (const { file, text } of files) {
    for (const m of text.matchAll(/^class\s+LF_DialogueHook_(\w+)\s*:\s*LF_DialogueHook\b/gm)) {
      const body = braceBody(text, m.index);
      // The comment block right above the class belongs to it.
      const above = text.slice(Math.max(0, text.lastIndexOf("\n\n", m.index) + 1), m.index);
      const inserts = (body.match(/\bspoken\.Insert\s*\(/g) || []).length;
      const notSpoken = new Set(tagged(`${above}\n${body}`, "lf-not-spoken"));
      const declared = tagged(`${above}\n${body}`, "lf-spoken");
      const cleared = [...notSpoken].map((d) => globToRe(d));
      const literals = new Set((body.match(KEY_RE) || []).filter((k) => !cleared.some((re) => re.test(k))));
      out.set(m[1], { file, line: lineOf(text, m.index), declared, literals, inserts, notSpoken });
    }
  }
  return out;
}

// `LF_Dialogue.Announce(npc, <key>)` sites: { file, line, key|null, declared: [keys] }.
export function announceSites(files) {
  const out = [];
  for (const { file, text } of files) {
    for (const m of text.matchAll(/\bLF_Dialogue\.Announce\s*\(\s*[A-Za-z_][\w.]*\s*,\s*([^;]*?)\)\s*;/g)) {
      const at = lineOf(text, m.index);
      const lines = text.split("\n");
      const near = lines.slice(Math.max(0, at - 4), at).join("\n");
      out.push({ file, line: at, key: /^"(#LF-[A-Za-z0-9_]+)"$/.exec(m[1].trim())?.[1] || null, declared: tagged(near, "lf-spoken") });
    }
  }
  return out;
}

// Every `for_key` line a dialogue speaks, merged over a patch's core: { key -> { id, owner: spec of the json that declares it, voiced } }.
export function dialogueModel(root) {
  const specs = loadSpecs(root);
  const byKey = new Map();
  const patchOf = new Map();
  for (const s of specs.values()) if (s.patches && s.addon === PACK) patchOf.set(s.patches, s);
  for (const spec of specs.values()) {
    if (spec.patches) continue; // judged with its core below
    const patch = patchOf.get(spec.id) || null;
    const lines = new Map();
    const actions = new Set();
    const addNodes = (nodes, removed) => {
      for (const n of nodes || []) {
        if (n.op === "remove" || removed?.has(n.id)) continue;
        for (const o of n.options || []) if (o.action) actions.add(`${o.action}|${o.param || ""}`);
      }
    };
    for (const [id, l] of Object.entries(spokenLines(spec))) lines.set(id, { ...l, owner: spec, own: true });
    if (patch) for (const [id, l] of Object.entries(spokenLines(patch))) {
      const was = lines.get(id);
      lines.set(id, { ...was, ...l, for_key: l.for_key || was?.for_key, owner: l.for_key || !was ? patch : was.owner, pinOwner: l.for_key ? patch : was?.owner });
    }
    for (const [id, l] of lines) if (l.op === "remove") lines.delete(id);
    const removed = new Set((patch?.nodes || []).filter((n) => n.op === "remove").map((n) => n.id));
    addNodes(spec.nodes, removed);
    if (patch) addNodes(patch.nodes, null);
    const voiced = Boolean(patch?.voice || (spec.voice && !spec.text_only));
    byKey.set(spec.id, { spec, patch, lines, actions, voiced, forKeys: new Map([...lines].filter(([, l]) => l.for_key).map(([id, l]) => [l.for_key, { id, line: l, owner: l.pinOwner || l.owner }])) });
  }
  return byKey;
}

// Quest keys spoken by a giver: { giver -> Set(key) } from the quest confs, plus the giver's idle key from its prefab.
export function questKeys(root) {
  const out = new Map();
  const add = (giver, key) => { if (giver && key) (out.get(giver) || out.set(giver, new Set()).get(giver)).add(key); };
  for (const f of walk(join(root, "addons")).filter((p) => p.endsWith(".conf") && /[\\/]Configs[\\/]/.test(p) && /Quest/.test(p) && !/[\\/]Tests[\\/]/.test(p))) {
    const text = readFileSync(f, "utf8");
    if (!text.includes("LF_QuestGiverRow")) continue;
    let giver = "";
    for (const line of text.split("\n")) {
      const g = /m_sGiverId "([^"]*)"/.exec(line);
      if (g) giver = g[1];
      const k = /(?:m_sDescriptionKey|m_sTurnInKey) "(#[^"]*)"/.exec(line);
      if (k) add(giver, k[1]);
    }
  }
  for (const f of walk(join(root, "addons")).filter((p) => p.endsWith(".et"))) {
    const text = readFileSync(f, "utf8");
    const g = /m_sGiverId "([^"]*)"/.exec(text)?.[1];
    const idle = /m_sIdleKey "(#[^"]*)"/.exec(text)?.[1];
    if (g && idle) add(g, idle);
  }
  return out;
}

// { dialogue id -> Set(giver id) }: the NPC prefabs the pack owns or places, by the giver id they carry.
export function giversByDialogue(root) {
  const { info, placedFiles, packDir } = readPrefabs(root);
  const out = new Map();
  const giverOf = (file) => {
    for (let p = file, i = 0; p && i < 24; i++) {
      const g = /m_sGiverId "([^"]*)"/.exec(readFileSync(p, "utf8"))?.[1];
      if (g) return g;
      p = info.get(p)?.parent;
    }
    return null;
  };
  for (const f of [...info.keys()].filter((p) => p.startsWith(packDir) || placedFiles.has(p))) {
    const r = resolvePrefab(info, f);
    if (!r.dlg) continue;
    const g = giverOf(f);
    if (g) (out.get(r.dlg) || out.set(r.dlg, new Set()).get(r.dlg)).add(g);
  }
  return out;
}

// Rumor rows: [{ file, line, id, key }].
export function rumorRows(root) {
  const rows = [];
  for (const f of walk(join(root, "addons", PACK, "Configs")).filter((p) => p.endsWith(".conf") && p.includes("Rumor"))) {
    const lines = readFileSync(f, "utf8").split("\n");
    let id = "";
    lines.forEach((ln, i) => {
      const a = /m_sId "([^"]*)"/.exec(ln);
      if (a) id = a[1];
      const k = /m_sLineKey "(#[^"]*)"/.exec(ln);
      if (k) rows.push({ file: relative(root, f).replace(/\\/g, "/"), line: i + 1, id, key: k[1] });
    });
  }
  return rows;
}

// Levels of a trade that open something (a row that is not a milestone), from the pack's unlock tables.
export function tradeUnlockLevels(root) {
  const out = new Map();
  for (const f of walk(join(root, "addons")).filter((p) => p.endsWith(".conf") && /TradeUnlocks/.test(p))) {
    for (const block of readFileSync(f, "utf8").split("LF_TradeUnlockRow").slice(1)) {
      const job = /m_sJob "(\w+)"/.exec(block)?.[1];
      const level = Number(/m_iLevel (\d+)/.exec(block)?.[1]);
      const kind = /m_eKind (\w+)/.exec(block)?.[1] || "MILESTONE";
      if (job && level && kind !== "MILESTONE") (out.get(job) || out.set(job, []).get(job)).push(level);
    }
  }
  return out;
}

// The keys one `lf-spoken` token stands for: itself; a `*` matched against the string table; `{param}` the reply's
// param; `{unlock-level}` the two-digit levels of the param's trade that open something.
export function expandDeclared(token, param, keyNames, unlockLevels) {
  const withParam = token.replace(/\{param\}/g, param || "*");
  if (withParam.includes("{unlock-level}")) return (unlockLevels.get(param) || []).map((l) => withParam.replace("{unlock-level}", String(l).padStart(2, "0")));
  if (withParam.includes("*")) return keyNames.filter((k) => globToRe(withParam).test(k));
  return [withParam];
}

const globToRe = (g) => new RegExp(`^${g.replace(/[.+?^$()|[\]\\]/g, "\\$&").replace(/\*/g, "[A-Za-z0-9_]*")}$`);

// The whole check. Returns { fails: [text], total, covered } (a "line" is one required speech source).
export function check(root) {
  const fails = [];
  const strings = loadStrings(root);
  const files = scriptFiles(root);
  const hooks = hookClasses(files);
  const model = dialogueModel(root);
  const keyNames = [...strings.keys()].map((k) => `#${k}`);
  let total = 0;
  let covered = 0;
  const need = (dlg, key, why) => {
    total++;
    if (dlg.forKeys.has(key)) { covered++; return; }
    fails.push(`${dlg.spec.id}: nothing records ${key} (${why}); add a line with "for_key": "${key}" to tools/assets/dialogue/${dlg.patch ? dlg.patch.id : dlg.spec.id}/dialogue.json and run tools\\cli asset dialogue ${dlg.patch ? dlg.patch.id : dlg.spec.id}`);
  };

  // 1. hooks
  const unlockLevels = tradeUnlockLevels(root);
  for (const [name, h] of hooks) {
    if (h.inserts && !h.declared.length && !h.notSpoken.size) fails.push(`${h.file}:${h.line}: LF_DialogueHook_${name} puts keys in 'spoken' with no "// lf-spoken: #LF-Key ..." comment naming them (or "// lf-spoken: #LF-None-<reason>" ... use // lf-not-spoken: for keys that are not speech)`);
    for (const lit of h.literals) {
      if (h.declared.some((d) => globToRe(d.replace(/\{[a-z-]+\}/g, "*")).test(lit))) continue;
      if (h.inserts) fails.push(`${h.file}:${h.line}: LF_DialogueHook_${name} names ${lit} but does not declare it: "// lf-spoken: ${lit}" if the NPC says it, "// lf-not-spoken: ${lit}" if not`);
    }
  }
  for (const dlg of model.values()) {
    if (!dlg.voiced) continue;
    for (const act of dlg.actions) {
      const [name, param] = act.split("|");
      const h = hooks.get(name);
      if (!h) continue;
      for (const d of h.declared) {
        for (const k of expandDeclared(d, param, keyNames, unlockLevels)) need(dlg, k, `hook ${name}`);
      }
    }
  }

  // 2. quests
  const quest = questKeys(root);
  const givers = giversByDialogue(root);
  for (const [dlgId, ids] of givers) {
    const dlg = model.get(dlgId);
    if (!dlg || !dlg.voiced || ![...dlg.actions].some((a) => a.startsWith("QuestConverse"))) continue;
    for (const g of ids) for (const k of quest.get(g) || []) need(dlg, k, `quest giver '${g}'`);
  }

  // 3. rumor rows
  const lineKeys = new Map();
  for (const dlg of model.values()) for (const id of dlg.lines.keys()) lineKeys.set(`#${lineKey(dlg.spec, id)}`, dlg);
  for (const r of rumorRows(root)) {
    total++;
    const dlg = lineKeys.get(r.key);
    if (dlg?.voiced) covered++;
    else fails.push(`${r.file}:${r.line}: rumor '${r.id}' says ${r.key}, which is ${dlg ? "a text-only line" : "no line of any recorded conversation"}`);
  }

  // 4. announce sites
  const allForKeys = new Set([...model.values()].flatMap((d) => (d.voiced ? [...d.forKeys.keys()] : [])));
  for (const a of announceSites(files)) {
    const keys = a.key ? [a.key] : a.declared;
    if (!keys.length) { fails.push(`${a.file}:${a.line}: LF_Dialogue.Announce with a computed key needs a "// lf-spoken: #LF-Key ..." comment above it`); continue; }
    for (const k of keys) {
      total++;
      if (allForKeys.has(k)) covered++;
      else fails.push(`${a.file}:${a.line}: the NPC announces ${k} and no recorded line has "for_key": "${k}"`);
    }
  }

  // 5. pins
  for (const dlg of model.values()) {
    for (const [key, { id, owner }] of dlg.forKeys) {
      const text = strings.get((owner.lines?.[id]?.pin_key || key).replace(/^#/, ""));
      if (text === undefined) { fails.push(`${owner.id}/${id}: for_key ${key} is not in the string table`); continue; }
      const pin = owner.lines?.[id]?.key_pin;
      if (pin === undefined) fails.push(`${owner.id}/${id}: no key_pin for ${key}; check the line against "${text.slice(0, 60)}" then run node tools/validation/validate-npc-lines-voiced.mjs --update`);
      else if (pin !== pinOf(text)) fails.push(`${owner.id}/${id}: ${key} changed since the line was recorded (now "${text.slice(0, 80)}"); re-read what the NPC says, re-record it if it differs, then run node tools/validation/validate-npc-lines-voiced.mjs --update`);
    }
  }
  return { fails, total, covered, model, strings };
}

// Write the current pin into every for_key line (as text, so the file keeps its layout).
export function writePins(root) {
  const { model, strings } = check(root);
  let written = 0;
  const seen = new Set();
  for (const dlg of model.values()) {
    for (const [key, { id, owner }] of dlg.forKeys) {
      const text = strings.get((owner.lines?.[id]?.pin_key || key).replace(/^#/, ""));
      if (text === undefined) continue;
      const tag = `${owner.id}|${id}`;
      if (seen.has(tag)) continue;
      seen.add(tag);
      const file = join(root, "tools", "assets", "dialogue", owner.id, "dialogue.json");
      let json = readFileSync(file, "utf8");
      const pin = pinOf(text);
      const at = json.indexOf(`"for_key": "${key}"`);
      if (at < 0) continue;
      const stale = /^"for_key": "[^"]*",\s*"key_pin": "[0-9a-f]{8}"/.exec(json.slice(at));
      const next = `"for_key": "${key}", "key_pin": "${pin}"`;
      if (stale) json = json.slice(0, at) + next + json.slice(at + stale[0].length);
      else json = json.slice(0, at) + next + json.slice(at + `"for_key": "${key}"`.length);
      writeFileSync(file, json);
      written++;
    }
  }
  return written;
}
