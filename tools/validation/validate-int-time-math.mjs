// No new Math.Max/Min/Clamp on an int time value in production script (issue #2787).
// inputs: addons/**/*.c, tools/lib/int-time-math.mjs, tools/validation/int-time-math-baseline.json
//
// Enforce `Math.Max`, `Math.Min` and `Math.Clamp` work in floats, so an int above 2^24 loses its low bits: unix
// 1800000480 came back as 1800000512 (#2677 found it). A time (seconds, milliseconds, a unix stamp, a deadline, a
// cooldown) or other large int goes through LF_Utils.MaxInt / MinInt / ClampInt. tools/lib/int-time-math.mjs flags a
// call whose argument is time-named and not a float (`m_f` prefix, or `float <name>` declared above).
// Scope: addons/*/Scripts/** except Tests/.
//
// A flagged site passes when it is
//   - marked `// lf-float: <why this is float time math>` on its line or the line above, or
//   - in int-time-math-baseline.json: { "<file>::<method>": { "count": N, "reason": "..." } }.
// The baseline can only shrink: a new site fails, a fixed site is "stale" and fails until its entry is removed
// (`--update-baseline` lowers counts and drops entries; it never adds one or writes a reason).
//
//   node tools/validation/validate-int-time-math.mjs                    check
//   node tools/validation/validate-int-time-math.mjs --update-baseline  shrink the baseline to what is still found
//
// LF_INTTIME_ROOT and LF_INTTIME_BASELINE point the scan at another tree and baseline (the selftest uses them).
//
// red-proof: see int-time-math-selftest.mjs.
import { existsSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";
import { compare, keyHits, scanSource } from "../lib/int-time-math.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const root = process.env.LF_INTTIME_ROOT || join(here, "..", "..");
const baselinePath = process.env.LF_INTTIME_BASELINE || join(here, "int-time-math-baseline.json");

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

if (process.argv[1] && process.argv[1].endsWith("validate-int-time-math.mjs")) {
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
    console.log(`int-time-math: baseline shrunk by ${dropped} site(s), ${Object.keys(next).length} entr(ies) left`);
    process.exit(0);
  }

  const { fresh, stale, noReason } = compare(found, baseline);
  for (const f of fresh) {
    for (const s of sites.filter((x) => `${x.file}::${x.method}` === f.key)) {
      console.log(`FAIL addons/${s.file}:${s.line} ${s.method} Math.${s.call}(... ${s.ident} ...) ${s.text.slice(0, 100)}`);
    }
  }
  for (const s of stale) console.log(`FAIL stale baseline ${s.key}: allows ${s.allowed}, found ${s.found}; run --update-baseline`);
  for (const k of noReason) console.log(`FAIL baseline entry has no reason: ${k}`);
  const total = [...found.values()].reduce((a, b) => a + b, 0);
  console.log(`int-time-math: ${total} flagged site(s), ${fresh.length} new, ${stale.length} stale, ${noReason.length} without a reason`);
  if (fresh.length > 0) {
    console.log("int-time-math: Math.Max/Min/Clamp work in floats, so an int above 2^24 loses its low bits (unix 1800000480 -> 1800000512).");
    console.log("  Use LF_Utils.MaxInt / MinInt / ClampInt for an int time. For real float time math, mark the line `// lf-float: <why>`.");
  }
  if (fresh.length > 0 || stale.length > 0 || noReason.length > 0) process.exit(1);
}
