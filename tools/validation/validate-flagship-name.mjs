// Every page that names the flagship's browser listing writes it exactly as server/flagship/server.json has it (#1679).
// inputs: docs/**, website/src/**, website/content/*.json, server/flagship/**, tools/validation/fixtures/flagship-name/**
//
// The listing is `game.name` in server/flagship/server.json, "Life Framework | Everon 1989 (Alpha)". A player
// searches the server browser for that string, so a colon for the bar, a missing "(Alpha)" or other spacing sends
// them to a server that is not there. A string counts as naming the listing when it ends "Alpha" or follows
// "server browser" on its line; the Workshop and pack title "Life Framework: Everon 1989" is a different name and
// is left alone. A name wrapped across two lines is read as one with a single space.
// Fixtures: fixtures/flagship-name/near-miss.md must fail and exact.md must pass, every run.
// red-proof: `node tools/validation/validate-flagship-name.mjs --surface tools/validation/fixtures/flagship-name/near-miss.md`
// exits 1 with "FAIL ... near-miss.md:3" (2026-11-15, #1679).

import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FIXTURES = join("tools", "validation", "fixtures", "flagship-name");
const TEXT = /\.(md|mdx|tsx?|jsx?|json|bbcode|txt|html|st)$/i;
const SKIP = new Set(["node_modules", "docs-images", "dist"]);
const SKIP_FILES = new Set(["website/content/github-snapshot.json"]);

export function flagshipName() {
  return JSON.parse(readFileSync(join(ROOT, "server", "flagship", "server.json"), "utf8")).game.name;
}

// Each place in `text` that names the listing: { line, found }. Whitespace around a line break inside a name is
// folded to one space first (offsets are kept by working on the original and counting newlines to the match).
const NAME = /Life Framework[ \t]*[|:\-–—\/]?[ \t]*Everon[ \t]*1989(?:(?:[ \t]*\r?\n[ \t>]*|[ \t]*)[(\[]?[ \t]*alpha[ \t]*[)\]]?)?/gi;
export function findNames(text) {
  const out = [];
  for (const m of text.matchAll(NAME)) {
    const lineStart = text.lastIndexOf("\n", m.index - 1) + 1;
    const before = text.slice(lineStart, m.index);
    const endsAlpha = /alpha[ \t]*[)\]]?$/i.test(m[0]);
    const afterBrowser = /server browser[^.;]{0,60}$/i.test(before);
    if (!endsAlpha && !afterBrowser) continue;
    out.push({ line: text.slice(0, m.index).split("\n").length, found: m[0].replace(/[ \t]*\r?\n[ \t>]*/g, " ").trim() });
  }
  return out;
}

export function problems(text, name) {
  return findNames(text).filter((n) => n.found !== name);
}

function walk(dir, out) {
  if (!existsSync(dir)) return;
  for (const e of readdirSync(dir)) {
    if (SKIP.has(e)) continue;
    const p = join(dir, e);
    if (statSync(p).isDirectory()) walk(p, out);
    else if (TEXT.test(e)) out.push(p);
  }
}

function pagesToCheck() {
  const files = [];
  for (const d of ["docs", join("website", "src"), "server/flagship"]) walk(join(ROOT, d), files);
  const content = join(ROOT, "website", "content");
  if (existsSync(content)) for (const e of readdirSync(content)) if (/\.json$/.test(e)) files.push(join(content, e));
  return files.filter((f) => !SKIP_FILES.has(relative(ROOT, f).replaceAll("\\", "/")));
}

const rel = (f) => relative(ROOT, f).replaceAll("\\", "/");
const name = flagshipName();
const surfaceArg = process.argv.indexOf("--surface");

if (surfaceArg > -1) {
  const file = process.argv[surfaceArg + 1];
  if (!file || !existsSync(file)) { console.error(`FAIL no such file: ${file}`); process.exit(1); }
  const bad = problems(readFileSync(file, "utf8"), name);
  for (const b of bad) console.error(`FAIL ${file}:${b.line}: "${b.found}" is not the listing name "${name}"`);
  if (bad.length) process.exit(1);
  console.log(`validate-flagship-name: OK ${file}`);
  process.exit(0);
}

const errors = [];
for (const [fixture, expectOk] of [["near-miss.md", false], ["exact.md", true]]) {
  const f = join(ROOT, FIXTURES, fixture);
  if (!existsSync(f)) { errors.push(`fixture ${join(FIXTURES, fixture)} is missing`); continue; }
  const ok = problems(readFileSync(f, "utf8"), name).length === 0;
  if (ok !== expectOk) errors.push(`fixture ${join(FIXTURES, fixture).replaceAll("\\", "/")} should ${expectOk ? "pass" : "fail"} the name rule`);
}

let checked = 0;
let named = 0;
for (const f of pagesToCheck()) {
  const text = readFileSync(f, "utf8");
  checked++;
  named += findNames(text).length;
  for (const b of problems(text, name)) errors.push(`${rel(f)}:${b.line}: "${b.found}" is not the listing name "${name}"`);
}

if (errors.length) {
  for (const e of errors) console.error("FAIL " + e);
  console.error(`validate-flagship-name: ${errors.length} problem(s)`);
  process.exit(1);
}
console.log(`validate-flagship-name: OK (${named} mention(s) of "${name}" in ${checked} file(s))`);
