// tools/lib/dialogue-voices.mjs - is every line an island NPC speaks voiced, with the voice that NPC owns? (2026-11-22)
//
// "Can we ensure all dialogs are voiced." Every character prefab the Everon pack owns or places that a player can
// talk to is resolved through its prefab chain to the conversation it plays:
//   dialogue  m_sDialogueId: the authored dialogue (tools/assets/dialogue/<id>/dialogue.json) plus the pack's voiced
//             patch of it (`patches`); each line needs a recording, made with the spec's voice, from the current text;
//   role      no dialogue but an LF_NpcRoleComponent role: the runtime-built role conversation (LF_Roles.BuildDialogue;
//             five text-only lines per role: Greet What Where Rumour Bye);
//   silent    a shop, ATM, sale or quest action and neither of the above: the NPC has nothing to say.
// Voiced means: spec not text_only (or a voiced patch covering the line), a wav in the pack, a provenance row recorded
// with the spec's own voice id and the current text hash. `voice.sex` against the body is validate-voice-sex.
import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { join, relative } from "node:path";
import { expand, lineHash, lineVoice, loadProvenance, spokenLines, voicedText, wavRes } from "../assets/pipeline/dialogue.mjs";

export const PACK = "LF_Everon1989";
export const ROLE_LINES = ["Greet", "What", "Where", "Rumour", "Bye"];
// Actions that make a character a conversation partner or a counter (the witness question is not one).
export const TALK_ACTIONS = new Set(["LF_DialogueTalkAction", "LF_QuestAction", "LF_ShopAction", "LF_ATMAction", "LF_BuyerSaleAction"]);

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

// { prefab file -> { cls, parent, dlg, role, acts } } and the set of prefab GUIDs the pack places.
export function readPrefabs(root) {
  const files = walk(join(root, "addons"));
  const byGuid = new Map();
  for (const f of files.filter((p) => p.endsWith(".et.meta"))) {
    const m = /Name "\{([0-9A-F]{16})\}/.exec(readFileSync(f, "utf8"));
    if (m) byGuid.set(m[1], f.slice(0, -5));
  }
  const info = new Map();
  for (const f of files.filter((p) => p.endsWith(".et"))) {
    const t = readFileSync(f, "utf8");
    const head = /^(\w+) : "\{([0-9A-F]{16})\}/.exec(t);
    info.set(f, {
      cls: head?.[1] || "",
      parent: head ? byGuid.get(head[2]) || null : null,
      dlg: /m_sDialogueId "([^"]*)"/.exec(t)?.[1] || null,
      role: /m_sRole "([^"]*)"/.exec(t)?.[1] || null,
      acts: [...t.matchAll(/^\s+(\w+)\s+"\{[0-9A-F]{16}\}"\s*\{/gm)].map((m) => m[1]).filter((a) => TALK_ACTIONS.has(a)),
    });
  }
  // GUIDs the pack's worlds and configs name: the prefabs it places.
  const refs = new Set();
  const packDir = join(root, "addons", PACK);
  for (const f of files.filter((p) => p.startsWith(packDir) && /\.(layer|conf)$/.test(p))) {
    for (const m of readFileSync(f, "utf8").matchAll(/\{([0-9A-F]{16})\}(?:Prefabs|Configs)\/[^"\s]*\.et\b/g)) refs.add(m[1]);
  }
  const placedFiles = new Set([...refs].map((g) => byGuid.get(g)).filter(Boolean));
  return { info, placedFiles, packDir };
}

// What one prefab resolves to through its chain: nearest dialogue id and role, every talk action on the way.
export function resolvePrefab(info, file) {
  const out = { dlg: null, role: null, acts: new Set(), npc: false };
  for (let p = file, i = 0; p && i < 24; i++) {
    const x = info.get(p);
    if (!x) break;
    if (x.cls === "LF_NpcBaseEntity") out.npc = true;
    if (x.dlg && out.dlg === null) out.dlg = x.dlg;
    if (x.role && out.role === null) out.role = x.role;
    x.acts.forEach((a) => out.acts.add(a));
    p = x.parent;
  }
  return out;
}

export function loadSpecs(root) {
  const base = join(root, "tools", "assets", "dialogue");
  const specs = new Map();
  for (const id of existsSync(base) ? readdirSync(base) : []) {
    const file = join(base, id, "dialogue.json");
    if (!existsSync(file)) continue;
    const raw = JSON.parse(readFileSync(file, "utf8"));
    specs.set(id, raw.patches ? raw : expand(raw));
  }
  return specs;
}

// Is one recorded line sound? A list of what is wrong with it ([] when voiced and right).
function lineIssues(root, spec, id, line, prov) {
  const bad = [];
  const p = prov.lines?.[id];
  const voice = lineVoice(spec, line);
  const addon = join(root, "addons", spec.addon || "LifeFramework");
  if (!p) return [`line ${id}: no recording`];
  if (!existsSync(join(addon, wavRes(spec, id)))) bad.push(`line ${id}: clip ${wavRes(spec, id)} is missing`);
  if (p.voice_id !== voice.id) bad.push(`line ${id}: recorded with voice ${p.voice_name || p.voice_id}, the dialogue's voice is ${voice.name || voice.id}`);
  else if (p.hash !== lineHash(voice, voicedText(line))) bad.push(`line ${id}: the clip is stale (text or voice changed since it was recorded)`);
  return bad;
}

// The scan: one row per conversation an island NPC offers.
// { key, kind: dialogue|role|silent, who: [prefab names], lines, voiced, missing: [text], issues: [text], voice }
export function scan(root) {
  const { info, placedFiles, packDir } = readPrefabs(root);
  const specs = loadSpecs(root);
  const patches = new Map();
  for (const s of specs.values()) if (s.patches && s.addon === PACK) patches.set(s.patches, s);
  const rows = new Map();
  const name = (f) => relative(root, f).replace(/\\/g, "/").replace(/^addons\//, "");
  const files = [...info.keys()].filter((f) => f.startsWith(packDir) || placedFiles.has(f));
  for (const f of files) {
    const r = resolvePrefab(info, f);
    if (!r.npc && !r.dlg) continue;
    let key = null;
    let kind = null;
    if (r.dlg) { key = r.dlg; kind = "dialogue"; }
    else if (r.role) { key = `role:${r.role}`; kind = "role"; }
    else if (r.acts.size) { key = `silent:${/([^/\\]+)\.et$/.exec(f)[1]}`; kind = "silent"; }
    else continue;
    if (!rows.has(key)) rows.set(key, { key, kind, who: [], lines: 0, voiced: 0, missing: [], issues: [], voice: "", voiceId: "" });
    rows.get(key).who.push(name(f));
  }
  for (const row of rows.values()) {
    if (row.kind === "role") {
      row.lines = ROLE_LINES.length;
      row.missing = ROLE_LINES.map((l) => `${l.toLowerCase()} (role conversation, built at runtime)`);
      continue;
    }
    if (row.kind === "silent") { row.lines = 1; row.missing = ["an interaction with no conversation at all"]; continue; }
    const spec = specs.get(row.key);
    if (!spec) { row.issues.push(`no tools/assets/dialogue/${row.key}/dialogue.json`); continue; }
    const patch = patches.get(row.key);
    const own = !spec.patches && !spec.text_only ? spec : null;
    const lines = new Map();
    for (const [id, line] of Object.entries(spokenLines(spec))) lines.set(id, { text: line.text, spec: own ? spec : null, line });
    if (patch) for (const [id, line] of Object.entries(spokenLines(patch))) lines.set(id, { text: line.text, spec: patch, line });
    row.lines = lines.size;
    for (const [id, l] of lines) {
      if (!l.spec) { row.missing.push(`${id}: "${(l.text || "").trim().slice(0, 48)}"`); continue; }
      const issues = lineIssues(root, l.spec, id, l.line, loadProvenance(root, l.spec.id));
      if (issues.length) issues.forEach((i) => row.issues.push(i));
      else row.voiced++;
      row.voice = l.spec.voice?.name || "";
      row.voiceId = l.spec.voice?.id || "";
    }
  }
  // Hand-written pack overlays of a dialogue (rumours and the like) add lines of their own: each needs an event too.
  const generated = new Set([...specs.values()].map((s) => `LF_Dialogue_${s.name}.conf`));
  for (const f of walk(join(packDir, "Configs", "Dialogue")).filter((p) => p.endsWith(".conf") && !generated.has(p.split(/[\\/]/).pop()))) {
    const text = readFileSync(f, "utf8");
    if (!text.startsWith("LF_DialogueConfig {")) continue;
    const row = rows.get(/^ m_sId "([^"]+)"/m.exec(text)?.[1]);
    if (!row) continue;
    const at = text.indexOf(" m_aLines {");
    for (const block of at < 0 ? [] : text.slice(at).split('LF_DialogueLine "{').slice(1)) {
      const id = /m_sId "([^"]+)"/.exec(block)?.[1];
      if (!id || /m_sOp "remove"/.test(block) || !/m_sTextKey/.test(block)) continue;
      row.lines++;
      if (/m_sEvent "[^"]+"/.test(block)) row.voiced++;
      else row.missing.push(`${id}: overlay line with no sound event (${f.split(/[\\/]/).pop()})`);
    }
  }
  // One voice per NPC: two different conversations never share a voice id.
  const owner = new Map();
  for (const row of rows.values()) {
    if (!row.voiceId) continue;
    if (owner.has(row.voiceId)) row.issues.push(`voice ${row.voice} is also ${owner.get(row.voiceId)}'s; each NPC keeps its own voice`);
    else owner.set(row.voiceId, row.key);
  }
  return [...rows.values()].sort((a, b) => a.key.localeCompare(b.key));
}

// The fails of a scan against the baseline (a shrink-only list of what is allowed to stay unvoiced).
export function judge(rows, baseline) {
  const fails = [];
  const allowed = new Map(Object.entries(baseline || {}));
  for (const row of rows) {
    const gaps = row.missing.length;
    if (row.issues.length) for (const i of row.issues) fails.push(`${row.key}: ${i} (${row.who[0]})`);
    if (gaps && !allowed.has(row.key)) fails.push(`${row.key}: ${row.missing.length} line(s) with no voice clip (${row.who.join(", ")})${row.missing[0] ? `, first: ${row.missing[0]}` : ""}`);
  }
  const byKey = new Map(rows.map((r) => [r.key, r]));
  for (const key of allowed.keys()) {
    const row = byKey.get(key);
    if (!row || !row.missing.length) fails.push(`baseline: ${key} is voiced or gone; delete it from tools/validation/dialogue-voices-baseline.json (the list only shrinks)`);
  }
  return fails;
}

export function totals(rows) {
  return { lines: rows.reduce((n, r) => n + r.lines, 0), voiced: rows.reduce((n, r) => n + r.voiced, 0) };
}
