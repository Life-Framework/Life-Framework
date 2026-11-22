// Player-readable text is US English: no French sign words, no British spellings.
// inputs: addons/**/Language/*.st, addons/**/Configs/Signs/**, tools/assets/**/*.py, tools/assets/**/texture-text.json, tools/lib/us-english.mjs, tools/validation/us-english-baseline.json
//
// Maintainer rule (2026-11-22): the entire mod is US English, signs and in-game assets included; it is still set in
// the region, but players must be able to read the signs. Place names (Saint-Philippe, Everon) and people's names
// stay. The words checked are in tools/lib/us-english.mjs (FRENCH, BRITISH).
//
// Fails, with file:line, on a hit in:
//   - a Target_en_us value of a string table row (core and pack);
//   - a literal text field of a sign config (a `#Key` is checked through its row);
//   - a string literal of a texture build script that paints text (tools/assets/<slug>/*.py calling `.text(`).
// A hit listed in tools/validation/us-english-baseline.json passes (voiced lines that need a new recording, work
// another branch owns); the baseline only shrinks, and a listed hit that is gone prints a STALE note.
//
// `--update` rewrites tools/assets/<slug>/texture-text.json, the reviewable list of every piece of text each
// texture script paints. A manifest that differs from its script prints a note (never a failure).
//
// LF_USENGLISH_ROOT points the scan at another tree (the selftest).
//
// red-proof: 2026-11-22, before the US English pass the real tree failed with 61 hits (Boulangerie, Pharmacie,
// Tabac-presse, Fermé, À vendre, licence, harbour, ...); tools/validation/us-english-selftest.mjs plants each kind.
import { existsSync, readdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { findNonUs, paintsText, signConfTexts, stRows, textureTexts } from "../lib/us-english.mjs";

const root = process.env.LF_USENGLISH_ROOT || join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const update = process.argv.includes("--update");

function walk(dir, rel, out) {
  if (!existsSync(dir)) return out;
  for (const name of readdirSync(dir)) {
    if (name === ".git" || name === "node_modules" || name === "log") continue;
    const p = join(dir, name);
    const r = `${rel}/${name}`;
    if (statSync(p).isDirectory()) walk(p, r, out);
    else out.push(r);
  }
  return out;
}

const baselineFile = join(root, "tools", "validation", "us-english-baseline.json");
const baseline = existsSync(baselineFile) ? JSON.parse(readFileSync(baselineFile, "utf8")).entries || [] : [];
const allowKey = (file, where, word) => `${file}|${where}|${word.toLowerCase()}`;
const allowed = new Set(baseline.map((e) => allowKey(e.file, e.where, e.word)));
const seen = new Set();

const fails = [];
const hit = (file, line, where, text, h) => {
  const k = allowKey(file, where, h.word);
  if (allowed.has(k)) return seen.add(k);
  fails.push(`FAIL  ${file}:${line} ${where}: "${h.word}" (${h.kind}; ${h.hint}) in "${text.slice(0, 80)}"`);
};

let scanned = 0;
const addons = walk(join(root, "addons"), "addons", []);
for (const f of addons.filter((x) => /\/Language\/[^/]+\.st$/.test(x))) {
  for (const r of stRows(readFileSync(join(root, f), "utf8"))) {
    scanned++;
    for (const h of findNonUs(r.text)) hit(f, r.line, r.id, r.text, h);
  }
}
for (const f of addons.filter((x) => /\/Configs\/Signs\/.*\.conf$/.test(x))) {
  for (const r of signConfTexts(readFileSync(join(root, f), "utf8"))) {
    scanned++;
    for (const h of findNonUs(r.text)) hit(f, r.line, r.field, r.text, h);
  }
}
const notes = [];
const assets = join(root, "tools", "assets");
for (const slug of existsSync(assets) ? readdirSync(assets) : []) {
  const dir = join(assets, slug);
  if (!statSync(dir).isDirectory()) continue;
  const manifest = {};
  for (const name of readdirSync(dir).filter((n) => n.endsWith(".py")).sort()) {
    const src = readFileSync(join(dir, name), "utf8");
    if (!paintsText(src)) continue;
    const texts = textureTexts(src);
    manifest[name] = [...new Set(texts.map((t) => t.text))];
    for (const t of texts) {
      scanned++;
      for (const h of findNonUs(t.text)) hit(`tools/assets/${slug}/${name}`, t.line, "texture text", t.text, h);
    }
  }
  if (!Object.keys(manifest).length) continue;
  const mf = join(dir, "texture-text.json");
  const want = JSON.stringify({ about: "Every piece of text this slug's build script paints onto its texture; checked by tools/validation/validate-us-english.mjs. Regenerate: node tools/validation/validate-us-english.mjs --update", scripts: manifest }, null, 2) + "\n";
  if (update) writeFileSync(mf, want);
  else if (!existsSync(mf) || readFileSync(mf, "utf8") !== want) notes.push(`NOTE  tools/assets/${slug}/texture-text.json is out of date: node tools/validation/validate-us-english.mjs --update`);
}

for (const n of notes) console.log(n);
for (const e of baseline) if (!seen.has(allowKey(e.file, e.where, e.word))) console.log(`STALE ${e.file} ${e.where} "${e.word}": gone, drop it from us-english-baseline.json`);
if (fails.length) {
  for (const f of fails) console.log(f);
  console.log(`SUMMARY validate-us-english fail ${fails.length} (place and people's names stay; anything else is US English: tools/lib/us-english.mjs)`);
  process.exit(1);
}
console.log(`PASS  validate-us-english: ${scanned} texts are US English (${seen.size} baselined)`);
