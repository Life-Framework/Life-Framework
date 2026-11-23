// tools/validation/validate-term-sinks.mjs
// inputs: addons/LifeFramework/Language/*.st, addons/**/Scripts/**/*.c
//
// Core text names a term in a placeholder ("No {police}"); LF_Terms expands it (#958). A screen,
// toast, action reason or label that hands such a key straight to the engine
// (WidgetManager.Translate, SetCannotPerformReason, SetText, an action's outName) shows the braces
// as typed: the 0.2.1 map's lawless zone read "No {police}". This fails a line that names a
// placeholder key as a literal beside one of those raw sinks with no LF_Terms call. Keys that
// reach a sink through a variable are covered by the sink's own helper (LF_Terms.Text / Shown).
// red-proof: tools/validation/term-sinks-selftest.mjs plants a raw sink and a routed one.
import { readFileSync, readdirSync, statSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";

// A quoted text may hold braces, so the body skips quoted strings whole.
const ITEM = /CustomStringTableItem\s+"\{[0-9A-Fa-f]+\}"\s*\{((?:[^}"]|"(?:[^"\\]|\\.)*")*)\}/g;
const PLACEHOLDER = /\{[A-Za-z]+(?::[a-z]+)?\}/;
const RAW_SINK = /(WidgetManager\.Translate\(|SetCannotPerformReason\(|\.SetText\(|\boutName\s*=)/;

// The string ids whose English text holds a {term} placeholder.
export function placeholderKeys(stText) {
  const keys = new Set();
  for (const m of stText.matchAll(ITEM)) {
    const id = /Id\s+"([^"]+)"/.exec(m[1])?.[1];
    const en = /Target_en_us\s+"((?:[^"\\]|\\.)*)"/.exec(m[1])?.[1];
    if (id && en && PLACEHOLDER.test(en)) keys.add(id);
  }
  return keys;
}

// [{ line, key }] for each source line that gives a placeholder key to a raw sink without LF_Terms.
export function rawSinkHits(source, keys) {
  const hits = [];
  const lines = source.split("\n");
  lines.forEach((text, i) => {
    if (text.includes("LF_Terms.") || !RAW_SINK.test(text)) return;
    for (const m of text.matchAll(/"#([A-Za-z0-9_-]+)"/g)) {
      if (keys.has(m[1])) hits.push({ line: i + 1, key: m[1] });
    }
  });
  return hits;
}

// [{ line, key }] for each prefab line whose Name or Description is a placeholder key: the engine renders
// a prefab's UIInfo text itself, with no chance to expand it.
export function prefabHits(source, keys) {
  const hits = [];
  source.split("\n").forEach((text, i) => {
    const m = /^\s*(?:Name|Description)\s+"#([A-Za-z0-9_-]+)"/.exec(text);
    if (m && keys.has(m[1])) hits.push({ line: i + 1, key: m[1] });
  });
  return hits;
}

function sources(dir, ext, out) {
  for (const name of readdirSync(dir)) {
    const p = join(dir, name);
    if (statSync(p).isDirectory()) sources(p, ext, out);
    else if (name.endsWith(ext) && !p.includes("Tests")) out.push(p);
  }
  return out;
}

if (process.argv[1] && process.argv[1].split("\\").join("/").endsWith("validate-term-sinks.mjs")) {
  const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const st = readFileSync(join(root, "addons", "LifeFramework", "Language", "everonlife_localization.st"), "utf8");
  const keys = placeholderKeys(st);
  let bad = 0;
  for (const addon of ["LifeFramework", "LF_Everon1989"]) {
    for (const [folder, ext] of [["Scripts", ".c"], ["Prefabs", ".et"]]) {
      let files = [];
      try {
        files = sources(join(root, "addons", addon, folder), ext, []);
      } catch {
        continue;
      }
      for (const f of files) {
        const text = readFileSync(f, "utf8");
        const found = ext === ".c" ? rawSinkHits(text, keys) : prefabHits(text, keys);
        for (const h of found) {
          bad++;
          const rel = relative(root, f).split("\\").join("/");
          if (ext === ".c") console.log(`FAIL ${rel}:${h.line}: "#${h.key}" has a {term} placeholder; show it through LF_Terms.Text(key) or LF_Terms.Shown(key)`);
          else console.log(`FAIL ${rel}:${h.line}: prefab text "#${h.key}" has a {term} placeholder, but the engine renders prefab Name and Description itself; word it without the term`);
        }
      }
    }
  }
  if (bad) process.exit(1);
  console.log("validate-term-sinks: OK");
}
