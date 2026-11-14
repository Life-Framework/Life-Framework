// Rejects a new path-string compare of a prefab name in core script (#323).
// inputs: addons/LifeFramework/Scripts/Game/**
// A resource keeps its GUID when it moves to another folder or addon, but GetPrefabName()
// then reports the new path, so `==`, `!=`, `.Contains(` or `.StartsWith(` between a live
// prefab name and a saved or configured string silently stops matching. Compare with
// LF_Utils.SamePrefab (GUID first). Heuristic: a line that uses GetPrefabName / GetPrefab()
// or a *Prefab* identifier with ==, != or a path-string method, and no SamePrefab.
//
// A line passes when it is marked `// lf-path-ok: <why>` (on it or the line above), is a
// comparison against null/empty/-1/0, or is in prefab-compare-baseline.json (existing debt;
// the baseline may only shrink).
//
//   node tools/validation/validate-prefab-compare.mjs                    check
//   node tools/validation/validate-prefab-compare.mjs --update-baseline  rewrite the baseline
//
// red-proof: see prefab-compare-selftest.mjs.
import { readFileSync, writeFileSync, existsSync, readdirSync, statSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, "..", "..");
export const baselinePath = join(here, "prefab-compare-baseline.json");
const defaultDir = join(root, "addons", "LifeFramework", "Scripts", "Game");

const NAME = String.raw`[A-Za-z_][\w.:]*`;
const PREFAB_ID = String.raw`[\w.]*[Pp]refab\w*`;
// A live prefab name beside a comparison, or a prefab-named variable compared with another variable.
const PATTERNS = [
  /GetPrefabName\s*\([^()]*\)\s*(==|!=)/,
  /(==|!=)\s*[\w.:]*GetPrefabName\s*\(/,
  /GetPrefabName\s*\([^()]*\)\s*(\.\w+\s*\([^()]*\))*\.(Contains|StartsWith|EndsWith|Find)\s*\(/,
  new RegExp(String.raw`\b${PREFAB_ID}\s*(==|!=)\s*${NAME}\b(?!\s*\()`),
  new RegExp(String.raw`\b${NAME}\s*(==|!=)\s*${PREFAB_ID}\b(?!\s*\()`),
];
const TRIVIAL = /(==|!=)\s*(null|NULL|-?\d+|"[^"]*"|ResourceName\.Empty|string\.Empty|true|false|LF_E\w+\.\w+)\b/g;

function walk(dir, out = []) {
  for (const n of readdirSync(dir)) {
    const p = join(dir, n);
    if (statSync(p).isDirectory()) { if (n !== "Tests") walk(p, out); }
    else if (n.endsWith(".c")) out.push(p);
  }
  return out;
}

export function scanPrefabCompares(dir = defaultDir) {
  const hits = [];
  for (const file of walk(dir)) {
    const lines = readFileSync(file, "utf8").split("\n");
    lines.forEach((raw, i) => {
      const code = raw.replace(/\/\/.*$/, "");
      if (/SamePrefab|PrefabGuid|GuidOf/.test(code)) return;
      const stripped = code.replace(TRIVIAL, "$1 0");
      if (!PATTERNS.some((re) => re.test(stripped))) return;
      if (/lf-path-ok:/.test(raw) || (i > 0 && /lf-path-ok:/.test(lines[i - 1]))) return;
      hits.push({ file: relative(dir, file).split("\\").join("/"), line: i + 1, text: raw.trim() });
    });
  }
  return hits;
}

export const hitKey = (h) => `${h.file}: ${h.text}`;
export const toBaseline = (hits) => [...new Set(hits.map(hitKey))].sort();

export function compare(hits, baseline) {
  const known = new Set(baseline);
  const present = new Set(hits.map(hitKey));
  return { fresh: hits.filter((h) => !known.has(hitKey(h))), stale: baseline.filter((k) => !present.has(k)) };
}

if (process.argv[1] && process.argv[1].endsWith("validate-prefab-compare.mjs")) {
  const hits = scanPrefabCompares();
  if (process.argv.includes("--update-baseline")) {
    const b = toBaseline(hits);
    writeFileSync(baselinePath, JSON.stringify(b, null, 1) + "\n");
    console.log(`prefab-compare: baseline rewritten with ${b.length} line(s)`);
    process.exit(0);
  }
  const baseline = existsSync(baselinePath) ? JSON.parse(readFileSync(baselinePath, "utf8")) : [];
  const { fresh, stale } = compare(hits, baseline);
  for (const h of fresh) console.log(`FAIL ${h.file}:${h.line} ${h.text}`);
  console.log(`prefab-compare: ${hits.length} baselined path compare(s), ${fresh.length} new, ${stale.length} gone since baseline`);
  if (stale.length > 0) console.log("prefab-compare: baseline lists lines that are gone; shrink it with --update-baseline");
  if (fresh.length > 0) {
    console.log("prefab-compare: compare prefab names with LF_Utils.SamePrefab (GUID first); a path compare breaks when the resource moves (pack-move-spike section 3).");
    console.log("  If a path compare is meant, mark the line `// lf-path-ok: <why>`.");
    process.exit(1);
  }
}
