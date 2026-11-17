// No new Substring splice or long-argument string.Format in production script (issue #2673).
// inputs: addons/**/*.c, tools/lib/string-splice.mjs
//
// Enforce `Substring` and `string.Format` silently cut a string at 8191 chars (docs/architecture/enforce-string-limits.md;
// PR #2520 lost the tail of a 9388-char save that way). tools/lib/string-splice.mjs finds the shapes:
//   splice  `a.Substring(i, <not a number or CONSTANT>) + ...` (a rebuilt string),
//   tail    `json.Substring(i, json.Length() - i)` of a payload-named string,
//   format  `string.Format(..., <payload-named arg>)` outside a log line.
// Scope: addons/*/Scripts/** except Tests/ (a test builds long strings on purpose).
//
// A flagged site passes when it is
//   - marked `// lf-bounded: <why the input stays under 8191>` on its line or the line above, or
//   - in string-splice-baseline.json: { "<file>::<method>::<kind>": { "count": N, "reason": "..." } }.
// The baseline can only shrink: a new site fails, a fixed site is "stale" and fails until its entry is removed
// (`--update-baseline` lowers counts and drops entries; it never adds one or writes a reason, so an addition is
// a hand edit with a reason a reviewer reads).
//
//   node tools/validation/validate-string-splice.mjs                    check
//   node tools/validation/validate-string-splice.mjs --update-baseline  shrink the baseline to what is still found
//
// LF_SPLICE_ROOT and LF_SPLICE_BASELINE point the scan at another tree and baseline (the selftest uses fixtures).
//
// red-proof: see string-splice-selftest.mjs.
import { existsSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";
import { compare, keyHits, scanSource } from "../lib/string-splice.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const root = process.env.LF_SPLICE_ROOT || join(here, "..", "..");
const baselinePath = process.env.LF_SPLICE_BASELINE || join(here, "string-splice-baseline.json");

function walk(dir, out) {
  for (const e of readdirSync(dir, { withFileTypes: true })) {
    const p = join(dir, e.name);
    if (e.isDirectory()) {
      if (e.name !== "Tests") walk(p, out);
    } else if (e.name.endsWith(".c")) out.push(p);
  }
  return out;
}

/** Production script files, repo-relative with forward slashes. */
export function productionScripts(base) {
  const addons = join(base, "addons");
  const files = [];
  for (const a of existsSync(addons) ? readdirSync(addons) : []) {
    const scripts = join(addons, a, "Scripts");
    if (existsSync(scripts)) walk(scripts, files);
  }
  return files.map((f) => relative(base, f).split("\\").join("/")).sort();
}

/** { found: Map key->count, sites: [{file,line,...}] } for the whole tree. */
export function scanTree(base) {
  const found = new Map();
  const sites = [];
  for (const f of productionScripts(base)) {
    const hits = scanSource(readFileSync(join(base, f), "utf8"));
    keyHits(f.replace(/^addons\//, ""), hits, found);
    for (const h of hits) sites.push({ file: f.replace(/^addons\//, ""), ...h });
  }
  return { found, sites };
}

if (process.argv[1] && process.argv[1].endsWith("validate-string-splice.mjs")) {
  const { found, sites } = scanTree(root);
  const baseline = existsSync(baselinePath) ? JSON.parse(readFileSync(baselinePath, "utf8")) : {};

  if (process.argv.includes("--update-baseline")) {
    const next = {};
    let dropped = 0;
    for (const [k, b] of Object.entries(baseline)) {
      const n = Math.min(found.get(k) || 0, b.count);
      if (n > 0) next[k] = { ...b, count: n };
      if (n < b.count) dropped += b.count - n;
    }
    writeFileSync(baselinePath, JSON.stringify(next, null, 1) + "\n");
    console.log(`string-splice: baseline shrunk by ${dropped} site(s), ${Object.keys(next).length} entr(ies) left`);
    process.exit(0);
  }

  const { fresh, stale, noReason } = compare(found, baseline);
  for (const f of fresh) {
    for (const s of sites.filter((x) => `${x.file}::${x.method}::${x.kind}` === f.key)) {
      console.log(`FAIL addons/${s.file}:${s.line} ${s.method} [${s.kind}] ${s.text.slice(0, 100)}`);
    }
  }
  for (const s of stale) console.log(`FAIL stale baseline ${s.key}: allows ${s.allowed}, found ${s.found}; run --update-baseline`);
  for (const k of noReason) console.log(`FAIL baseline entry has no reason: ${k}`);
  const total = [...found.values()].reduce((a, b) => a + b, 0);
  console.log(`string-splice: ${total} flagged site(s), ${fresh.length} new, ${stale.length} stale, ${noReason.length} without a reason`);
  if (fresh.length > 0) {
    console.log("string-splice: Enforce Substring and string.Format cut a string at 8191 chars with no error (docs/architecture/enforce-string-limits.md).");
    console.log("  Join pieces of under 8 KB with `+`, or rebuild through JsonSaveContext. If the input is bounded, mark the line `// lf-bounded: <why>`.");
  }
  if (fresh.length > 0 || stale.length > 0 || noReason.length > 0) process.exit(1);
}
