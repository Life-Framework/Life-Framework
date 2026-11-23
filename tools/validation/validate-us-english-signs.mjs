// Sign rows, livery plates and texture-text specs are US English: no French, Czech or British words, no other alphabet.
// inputs: addons/**/Language/*.st, addons/**/Configs/Signs/**, tools/livery/table.mjs, tools/assets/**/texture-text.json, tools/lib/us-english.mjs, tools/validation/validate-us-english-signs.mjs, tools/validation/fixtures/us-english/**
//
// Maintainer rule (2026-11-22): every readable word is US English; place names (Saint-Philippe, Meaux) and people's
// names stay. validate-us-english.mjs already reads every string row, sign config and texture script for French and
// British words. This check adds what it misses (WO-W1.5, #4234):
//   - Czech words, the shop sign "DOMACI POTREBY" and the wall ad "NEPTUN" that the vanilla Saint-Philippe market
//     props carry (matched with accents folded, so the unaccented capitals on a sign still hit);
//   - any letter of another alphabet (Cyrillic, Greek, CJK, Arabic, Hebrew, Hangul) and the letters only Czech uses
//     (ě ř ů ď ť ň): the vanilla carton and vodka labels print Cyrillic (#4383);
//   - the livery table's door plate text (tools/livery/table.mjs), which no other check reads;
//   - the texture-text.json spec of every painted texture, so a reviewed spec cannot hold a foreign word.
// French and British words (tools/lib/us-english.mjs) are checked on the same sources, on sign-like string rows
// (an Id naming a sign, shop, plate, livery, poster, label ...) and on every literal sign config field.
// Every other string row is checked for another alphabet and Czech only: a dialogue line may keep a French name.
//
// Allow-list: NAMES below (places and people) are cut from the text before any word is looked up, so a street named
// for a person or a town with a French word in it passes.
//
// The default run scans the repo and then runs every case in tools/validation/fixtures/us-english/<case>/ against its
// own planted tree (`tree/`), reading `expect.json` ({ "exit": 0|1, "match": ["text that must appear"] }). A fixture
// that no longer fails (or a clean one that fails) fails this check, so the check proves itself on every run.
// LF_USENGLISH_SIGNS_ROOT points the scan at another tree; `--no-fixtures` skips the fixtures.
//
// red-proof: 2026-11-23, with the Czech, script and French lookups returning nothing the seven failing fixtures
// printed "FAIL  fixture <case>: expected exit 1, got 0" (the output is in the PR body).
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { findNonUs, signConfTexts, stRows } from "../lib/us-english.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const repoRoot = join(here, "..", "..");

// Places and people that keep their own spelling. Cut from the text before the word lists run.
export const NAMES = [
  "Saint-Philippe", "Saint-Pierre", "Meaux", "Montignac", "Levie", "Lévie", "Morton", "Lamentin", "Regina", "Provins",
  "Figari", "Gravette", "Le Moule", "La Guillotière", "Chotain", "Entre-Deux", "Everon", "Malden", "Arland",
  "Marchand", "Boulanger", "Duval", "Rémi",
];

// Czech words (accents folded) seen on vanilla signs and ads, plus common shop-sign words.
export const CZECH = [
  "domaci", "potreby", "domaci potreby", "neptun", "otevreno", "zavreno", "obchod", "prodejna", "potraviny", "vstup",
  "vychod", "pozor", "zakaz", "lekarna", "pekarna", "hospoda", "masna", "nadrazi", "slevy", "vyprodej", "cukrarna",
  "trafika", "zelenina", "ovoce", "sklenarstvi", "zahradnictvi", "tiskarna", "kadernictvi", "holic", "dobrou",
];

const OTHER_SCRIPT = /[\p{Script=Cyrillic}\p{Script=Greek}\p{Script=Han}\p{Script=Arabic}\p{Script=Hebrew}\p{Script=Hangul}\p{Script=Hiragana}\p{Script=Katakana}]/u;
const CZECH_LETTER = /[ěřůďťňĚŘŮĎŤŇ]/u;

const esc = (s) => s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
const fold = (s) => s.normalize("NFD").replace(/\p{M}/gu, "").toLowerCase();
const NAME_RE = new RegExp(`(?<![\\p{L}\\p{N}_])(${NAMES.map(esc).join("|")})(?![\\p{L}\\p{N}_])`, "giu");
const CZECH_RE = new RegExp(`(?<![a-z0-9_])(${CZECH.map(esc).join("|")})(?![a-z0-9_])`, "g");
const SIGN_ID = /(^|[-_])(sign|signs|shop|livery|plate|poster|billboard|banner|label|notice|door|wall|ad|ads|board|sticker|window|storefront|carton|bottle)([-_]|$)/i;

// [{ word, kind, hint }] for one piece of text; wide: also French and British words.
export function findForeign(text, wide) {
  const t = text.replace(NAME_RE, " ");
  const out = [];
  const m = OTHER_SCRIPT.exec(t);
  if (m) out.push({ word: m[0], kind: "script", hint: "another alphabet: write it in English letters" });
  const z = CZECH_LETTER.exec(t);
  if (z) out.push({ word: z[0], kind: "czech", hint: "a letter only Czech uses" });
  for (const c of fold(t).matchAll(CZECH_RE)) out.push({ word: c[1], kind: "czech", hint: "write it in English" });
  if (wide) out.push(...findNonUs(t));
  return out;
}

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

const lineOf = (text, needle) => {
  const i = text.indexOf(needle);
  return i < 0 ? 1 : text.slice(0, i).split("\n").length;
};

// Scan one tree: { fails: [string], scanned: number }.
export function scan(root) {
  const fails = [];
  let scanned = 0;
  // A word validate-us-english.mjs baselines for a texture script (another branch re-authors it) passes here too.
  const bf = join(root, "tools", "validation", "us-english-baseline.json");
  const baselined = existsSync(bf) ? JSON.parse(readFileSync(bf, "utf8")).entries || [] : [];
  const isBaselined = (file, where, word) => {
    const m = /^tools\/assets\/([^/]+)\/texture-text\.json$/.exec(file);
    return baselined.some((e) => e.where === where && e.word.toLowerCase() === word.toLowerCase() && (e.file === file || (m && e.file.startsWith(`tools/assets/${m[1]}/`))));
  };
  const hit = (file, line, where, text, wide) => {
    scanned++;
    for (const h of findForeign(text, wide)) {
      if (isBaselined(file, where, h.word)) continue;
      fails.push(`FAIL  ${file}:${line} ${where}: "${h.word}" (${h.kind}; ${h.hint}) in "${text.slice(0, 80)}"`);
    }
  };

  const files = walk(join(root, "addons"), "addons", []);
  for (const f of files.filter((x) => /\/Language\/[^/]+\.st$/.test(x))) {
    for (const r of stRows(readFileSync(join(root, f), "utf8"))) hit(f, r.line, r.id, r.text, SIGN_ID.test(r.id));
  }
  for (const f of files.filter((x) => /\/Configs\/Signs\/.*\.conf$/.test(x))) {
    for (const r of signConfTexts(readFileSync(join(root, f), "utf8"))) hit(f, r.line, r.field, r.text, true);
  }

  const table = join(root, "tools", "livery", "table.mjs");
  if (existsSync(table)) {
    const src = readFileSync(table, "utf8");
    for (const m of src.matchAll(/\bplate:\s*\{\s*text:\s*"([^"]*)"/g)) hit("tools/livery/table.mjs", lineOf(src, m[0]), "livery plate", m[1], true);
  }

  const assets = join(root, "tools", "assets");
  for (const slug of existsSync(assets) ? readdirSync(assets) : []) {
    const spec = join(assets, slug, "texture-text.json");
    if (!existsSync(spec)) continue;
    const raw = readFileSync(spec, "utf8");
    let json;
    try {
      json = JSON.parse(raw);
    } catch (e) {
      fails.push(`FAIL  tools/assets/${slug}/texture-text.json:1 not valid JSON: ${e.message}`);
      continue;
    }
    for (const list of Object.values(json.scripts || {})) {
      for (const s of list) hit(`tools/assets/${slug}/texture-text.json`, lineOf(raw, JSON.stringify(s)), "texture text", s, true);
    }
  }
  return { fails, scanned };
}

// Each fixtures/us-english/<case>/ holds tree/ (planted files) and expect.json.
function runFixtures() {
  const dir = join(here, "fixtures", "us-english");
  const bad = [];
  let n = 0;
  for (const name of existsSync(dir) ? readdirSync(dir).sort() : []) {
    const cdir = join(dir, name);
    if (!statSync(cdir).isDirectory()) continue;
    n++;
    const want = JSON.parse(readFileSync(join(cdir, "expect.json"), "utf8"));
    const { fails } = scan(join(cdir, "tree"));
    const code = fails.length ? 1 : 0;
    if (code !== want.exit) {
      bad.push(`FAIL  fixture ${name}: expected exit ${want.exit}, got ${code}${fails.length ? ` (${fails[0]})` : ""}`);
      continue;
    }
    for (const m of want.match || []) {
      if (!fails.some((f) => f.includes(m))) bad.push(`FAIL  fixture ${name}: no finding names "${m}"`);
    }
  }
  if (!n) bad.push("FAIL  tools/validation/fixtures/us-english/ has no cases");
  return { bad, n };
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  const root = process.env.LF_USENGLISH_SIGNS_ROOT || repoRoot;
  const { fails, scanned } = scan(root);
  const fx = process.argv.includes("--no-fixtures") ? { bad: [], n: 0 } : runFixtures();
  for (const f of [...fails, ...fx.bad]) console.log(f);
  if (fails.length || fx.bad.length) {
    console.log(`SUMMARY validate-us-english-signs fail ${fails.length + fx.bad.length} (place and people's names stay; anything else on a sign, plate or texture is US English: tools/validation/validate-us-english-signs.mjs)`);
    process.exit(1);
  }
  console.log(`PASS  validate-us-english-signs: ${scanned} sign, plate and texture texts are US English, ${fx.n} fixtures hold`);
}
