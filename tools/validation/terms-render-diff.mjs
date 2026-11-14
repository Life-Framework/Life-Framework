// tools/validation/terms-render-diff.mjs: the proof that a terms migration changed no shipped text.
// Reads the core string table at a git ref (default origin/main) and now, expands every
// placeholder in the new table with the core terms' default words per language, and lists each
// key and language whose rendered text differs from the old text.
//   node tools/validation/terms-render-diff.mjs [ref]
// Exit 1 when any rendered string differs. Not part of `validate` (it needs a git ref); run it in a
// terms migration PR and paste the last line into the body.
import { execFileSync } from "node:child_process";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { CORE_TABLE, CORE_TERMS, PLACEHOLDER, parseTable, parseTermsConf, termId } from "../lib/terms.mjs";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const ref = process.argv[2] || "origin/main";
const oldItems = parseTable(execFileSync("git", ["show", `${ref}:${CORE_TABLE}`], { cwd: root, encoding: "utf8", maxBuffer: 1 << 28 }));
const newItems = parseTable(readFileSync(join(root, CORE_TABLE), "utf8"));
const forms = parseTermsConf(readFileSync(join(root, CORE_TERMS), "utf8"));
const byId = Object.fromEntries(newItems.map((i) => [i.id, i]));

function words(id, form, lang) {
  const value = forms[id]?.[form] ?? forms[id]?.singular;
  if (value === undefined) return null;
  if (!value.startsWith("#")) return value;
  const item = byId[value.slice(1)];
  return item?.texts[lang] ?? item?.texts.en_us ?? null;
}

export function render(text, lang) {
  return text.replace(PLACEHOLDER, (all, name, form) => {
    const id = termId(name);
    let w = words(id, form === "pl" ? "plural" : form || "singular", lang);
    if (w === null) return all;
    if (name.length > 1 && name === name.toUpperCase()) return w.toUpperCase();
    if (name[0] !== name[0].toLowerCase()) w = w[0].toUpperCase() + w.slice(1);
    return w;
  });
}

const oldById = Object.fromEntries(oldItems.map((i) => [i.id, i]));
let checked = 0, differing = 0, changedKeys = 0;
for (const item of newItems) {
  const before = oldById[item.id];
  if (!before) continue;
  let changed = false;
  for (const [lang, text] of Object.entries(item.texts)) {
    const rendered = render(text, lang);
    checked++;
    const was = before.texts[lang] === undefined ? undefined : render(before.texts[lang], lang);
    if (rendered !== was) {
      differing++;
      console.log(`DIFF ${item.id} (${lang})\n  was: ${was}\n  now: ${rendered}`);
    }
    if (text !== before.texts[lang]) changed = true;
  }
  if (changed) changedKeys++;
}
console.log(`terms-render-diff vs ${ref}: ${checked} strings rendered, ${changedKeys} keys rewritten to placeholders, ${differing} differ`);
process.exit(differing ? 1 : 0);
