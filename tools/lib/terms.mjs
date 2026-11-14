// tools/lib/terms.mjs: what the terms layer (#958) looks like on disk, for the validator and
// `tools\cli terms`. The runtime is addons/LifeFramework/Scripts/Game/Feature/Terms/.
import { readFileSync, readdirSync, existsSync, statSync } from "node:fs";
import { join } from "node:path";

export const CORE_TERMS = "addons/LifeFramework/Configs/Terms/LF_Terms.conf";
export const CORE_TABLE = "addons/LifeFramework/Language/everonlife_localization.st";

// A placeholder is {id}, {Id}, {id:form}. {0123456789ABCDEF} (a GUID) never has a colon and is 16 hex.
export const PLACEHOLDER = /\{([A-Za-z][A-Za-z0-9_]*)(?::([A-Za-z0-9_]+))?\}/g;

// The words core English text must not hard-code once its term exists. A listed term is a
// sentence word a pack or an owner would rename; the check is for the core table only.
export const TERM_WORDS = {
  police: /\bpolice\b/i,
  medic: /\bmedics?\b/i,
  town: /\btown\b/i,
  currency: /\$(?=%\d|\d)|\bdollars?\b/i,
};

// Terms of one .conf: { id: Set(forms) }. Reads the text format (m_sId, then m_aForms with m_sForm).
export function parseTermsConf(text) {
  const terms = {};
  let current = null;
  for (const line of text.split("\n")) {
    const id = /^\s*m_sId\s+"([^"]+)"/.exec(line);
    if (id) { current = id[1]; terms[current] ||= {}; continue; }
    const form = /^\s*m_sForm\s+"([^"]+)"/.exec(line);
    if (form && current) { terms[current][form[1] === "pl" ? "plural" : form[1]] = ""; terms.__form = form[1]; continue; }
    const value = /^\s*m_sValue\s+"([^"]*)"/.exec(line);
    if (value && current && terms.__form) { terms[current][terms.__form === "pl" ? "plural" : terms.__form] = value[1]; }
  }
  delete terms.__form;
  return terms;
}

// [{ id, texts: { lang: text } }] by lines, so braces inside a text cannot end an item early.
export function parseTable(stText) {
  const items = [];
  let item = null;
  for (const line of stText.split("\n")) {
    const id = /^\s*Id\s+"([^"]+)"/.exec(line);
    if (id) { item = { id: id[1], texts: {} }; items.push(item); continue; }
    const target = /^\s*Target_([a-z_]+)\s+"((?:[^"\\]|\\.)*)"/.exec(line);
    if (target && item) { item.texts[target[1]] = target[2]; continue; }
    if (item && /^\s*Comment\s+".*\(voiced/.test(line)) item.voiced = true;
  }
  return items;
}

// Every .conf under addons/*/Configs that is a terms file (root class LF_TermsConfig), core first.
export function termFiles(root) {
  const files = [];
  const addons = join(root, "addons");
  for (const addon of readdirSync(addons).sort()) {
    const walk = (dir) => {
      if (!existsSync(dir)) return;
      for (const name of readdirSync(dir)) {
        const p = join(dir, name);
        if (statSync(p).isDirectory()) { if (name !== "Tests") walk(p); }
        else if (name.endsWith(".conf") && /^LF_TermsConfig\s*\{/.test(readFileSync(p, "utf8").slice(0, 80))) files.push(p);
      }
    };
    walk(join(addons, addon, "Configs"));
  }
  return files;
}

// { id: { forms: {form: value}, files: [paths] } } over the core file and every pack overlay.
export function loadTerms(root) {
  const merged = {};
  for (const file of termFiles(root)) {
    const parsed = parseTermsConf(readFileSync(file, "utf8"));
    for (const [id, forms] of Object.entries(parsed)) {
      merged[id] ||= { forms: {}, files: [] };
      Object.assign(merged[id].forms, forms);
      merged[id].files.push(file.slice(root.length + 1).split("\\").join("/"));
    }
  }
  return merged;
}

// Problems with the placeholders of one table: a term or a form that does not exist.
// terms: { id: { forms } }. Returns [{ key, lang, text, problem }].
export function placeholderProblems(items, terms) {
  const out = [];
  for (const item of items) {
    for (const [lang, text] of Object.entries(item.texts)) {
      for (const m of text.matchAll(PLACEHOLDER)) {
        const id = termId(m[1]);
        const form = m[2] === "pl" ? "plural" : m[2] || "singular";
        const term = terms[id];
        if (!term) out.push({ key: item.id, lang, text, problem: `names term "${id}", which no terms file defines` });
        else if (!(form in term.forms)) out.push({ key: item.id, lang, text, problem: `names form "${form}" of term "${id}", which has: ${Object.keys(term.forms).join(", ")}` });
      }
    }
  }
  return out;
}

// The id a placeholder name stands for: {Police} and {POLICE} (all caps, upper-cases the words) name "police".
export function termId(name) {
  if (name.length > 1 && name === name.toUpperCase() && /[A-Z]/.test(name)) return name.toLowerCase();
  return name[0].toLowerCase() + name.slice(1);
}

// { key: [term, ...] } for core keys whose English text hard-codes a listed term's word.
// Exempt: the term words themselves (LF-Term_*) and voiced lines, whose caption must match the recording.
export function hardCoded(items) {
  const hits = {};
  for (const item of items) {
    // The credits name real people, projects and assets as written in docs/credits.md; they are proper
    // names, not game terms (#481).
    if (item.id.startsWith("LF-Credits_")) continue;
    const en = item.texts.en_us;
    if (!en || item.voiced || item.id.startsWith("LF-Term_")) continue;
    const plain = en.replace(PLACEHOLDER, "");
    const found = Object.entries(TERM_WORDS).filter(([, re]) => re.test(plain)).map(([t]) => t);
    if (found.length) hits[item.id] = found;
  }
  return hits;
}
