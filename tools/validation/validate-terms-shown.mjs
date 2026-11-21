// tools/validation/validate-terms-shown.mjs
// inputs: addons/**/Language/*.st, addons/**/Scripts/**/*.c, addons/**/UI/**/*.layout, tools/validation/terms-shown-baseline.json
//
// No shown string keeps a {term} token. A core key whose text names a term ({police}) reads
// "wanted by the {police}" on screen unless it goes through LF_Terms (Text, Shown, Expand, or the
// toast and UI kit paths that call them). Engine translation alone never expands a token, so this
// fails a script that hands such a key straight to WidgetManager.Translate, and a layout that puts
// one in a widget's Text (the engine translates layout text with no term pass).
// A Translate argument that is a call (`Translate(LF_X.AnswerKey(o), arg)`) is followed into that
// function: a returned "#Key" literal or a `return Other(...)` one level further counts.
// Fix: LF_Terms.Text(key, p1..p4) in script; set layout text from script through LF_Terms.Text.
// red-proof: tools/validation/terms-shown-selftest.mjs
import { readFileSync, readdirSync, existsSync, statSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";
import { parseTable, PLACEHOLDER } from "../lib/terms.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const ROOT = join(here, "..", "..");

// Keys whose English (or any language) text holds a {term} placeholder.
export function termedKeys(items) {
  const out = new Set();
  for (const item of items) {
    for (const value of Object.values(item.texts || {})) {
      PLACEHOLDER.lastIndex = 0;
      if (PLACEHOLDER.test(value)) { out.add(item.id); break; }
    }
  }
  return out;
}

const KEY_LITERAL = /^\s*"#([A-Za-z0-9_\-.]+)"\s*$/;
const CALL = /([A-Za-z_][A-Za-z0-9_]*)\s*\(/;

// The text of the first argument of the call whose "(" is at open (balanced parens, strings skipped).
function firstArg(text, open) {
  let depth = 0;
  let inStr = false;
  for (let i = open; i < text.length; i++) {
    const c = text[i];
    if (inStr) { if (c === "\\") i++; else if (c === '"') inStr = false; continue; }
    if (c === '"') { inStr = true; continue; }
    if (c === "(") depth++;
    else if (c === ")") { depth--; if (depth === 0) return text.slice(open + 1, i); }
    else if (c === "," && depth === 1) return text.slice(open + 1, i);
  }
  return "";
}

// name -> { literals: Set<key>, calls: Set<name> } for every function body in the sources.
export function functionReturns(sources) {
  const out = new Map();
  const head = /\b(?:static\s+)?(?:protected\s+|private\s+)?(?:static\s+)?string\s+([A-Za-z_][A-Za-z0-9_]*)\s*\([^)]*\)\s*\{/g;
  for (const text of sources) {
    head.lastIndex = 0;
    let m;
    while ((m = head.exec(text))) {
      let depth = 0;
      let end = m.index + m[0].length - 1;
      for (let i = end; i < text.length; i++) {
        if (text[i] === "{") depth++;
        else if (text[i] === "}") { depth--; if (depth === 0) { end = i; break; } }
      }
      const body = text.slice(m.index + m[0].length, end);
      const entry = out.get(m[1]) || { literals: new Set(), calls: new Set() };
      for (const r of body.matchAll(/return\s+"#([A-Za-z0-9_\-.]+)"/g)) entry.literals.add(r[1]);
      for (const r of body.matchAll(/return\s+(?:[A-Za-z_][A-Za-z0-9_]*\.)?([A-Za-z_][A-Za-z0-9_]*)\s*\(/g)) entry.calls.add(r[1]);
      out.set(m[1], entry);
    }
  }
  return out;
}

function keysOf(name, fns, depth = 0) {
  const entry = fns.get(name);
  if (!entry || depth > 2) return [];
  const out = [...entry.literals];
  for (const c of entry.calls) if (c !== name) out.push(...keysOf(c, fns, depth + 1));
  return out;
}

// Pure: failures for script files [{path, text}] and layouts [{path, text}].
export function check(termed, scripts, layouts) {
  const failures = [];
  const fns = functionReturns(scripts.map((s) => s.text));
  for (const { path, text } of scripts) {
    for (const m of text.matchAll(/WidgetManager\.Translate\s*\(/g)) {
      const open = m.index + m[0].length - 1;
      const arg = firstArg(text, open);
      const line = text.slice(0, m.index).split("\n").length;
      const lit = arg.match(KEY_LITERAL);
      let hits = [];
      if (lit) hits = termed.has(lit[1]) ? [lit[1]] : [];
      else if (/^\s*[A-Za-z_][A-Za-z0-9_]*\s*$/.test(arg)) {
        // A variable: what the same file assigns to it (a literal key or a key-returning call).
        const name = arg.trim();
        const keys = [];
        const assign = new RegExp(`\\b${name}\\s*=\\s*(?:"#([A-Za-z0-9_\\-.]+)"|(?:[A-Za-z_][A-Za-z0-9_]*\\.)?([A-Za-z_][A-Za-z0-9_]*)\\s*\\()`, "g");
        for (const a of text.matchAll(assign)) {
          if (a[1]) keys.push(a[1]);
          else keys.push(...keysOf(a[2], fns));
        }
        hits = keys.filter((k) => termed.has(k));
      } else {
        const call = arg.match(CALL);
        if (call) hits = keysOf(call[1], fns).filter((k) => termed.has(k));
      }
      if (hits.length) failures.push(`${path}:${line} WidgetManager.Translate shows ${[...new Set(hits)].slice(0, 3).join(", ")} with its {term} token unexpanded; use LF_Terms.Text`);
    }
  }
  for (const { path, text } of scripts) {
    for (const m of text.matchAll(/\.SetText(?:Format)?\s*\(\s*"#([A-Za-z0-9_\-.]+)"/g)) {
      if (!termed.has(m[1])) continue;
      const line = text.slice(0, m.index).split("\n").length;
      failures.push(`${path}:${line} SetText shows ${m[1]} with its {term} token unexpanded; use LF_Terms.Text`);
    }
  }
  for (const { path, text } of layouts) {
    for (const m of text.matchAll(/\bText\s+"#([A-Za-z0-9_\-.]+)"/g)) {
      if (!termed.has(m[1])) continue;
      const line = text.slice(0, m.index).split("\n").length;
      failures.push(`${path}:${line} layout text ${m[1]} keeps its {term} token (the engine never expands it); set it from script through LF_Terms.Text`);
    }
  }
  return failures;
}

function walk(dir, ext, out = []) {
  if (!existsSync(dir)) return out;
  for (const name of readdirSync(dir)) {
    const p = join(dir, name);
    if (statSync(p).isDirectory()) walk(p, ext, out);
    else if (name.endsWith(ext)) out.push(p);
  }
  return out;
}

function main() {
  const items = [];
  const scripts = [];
  const layouts = [];
  for (const addon of readdirSync(join(ROOT, "addons"))) {
    const base = join(ROOT, "addons", addon);
    for (const st of walk(join(base, "Language"), ".st")) items.push(...parseTable(readFileSync(st, "utf8")));
    for (const c of walk(join(base, "Scripts"), ".c")) {
      if (/[\\/]Tests[\\/]/.test(c)) continue;
      scripts.push({ path: relative(ROOT, c).replace(/\\/g, "/"), text: readFileSync(c, "utf8") });
    }
    for (const l of walk(join(base, "UI"), ".layout")) layouts.push({ path: relative(ROOT, l).replace(/\\/g, "/"), text: readFileSync(l, "utf8") });
  }
  const failures = check(termedKeys(items), scripts, layouts);
  for (const f of failures) console.log(`FAIL ${f}`);
  console.log(`validate-terms-shown: ${failures.length ? "FAIL" : "OK"} (${failures.length} unexpanded {term} site(s))`);
  process.exit(failures.length ? 1 : 0);
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) main();
