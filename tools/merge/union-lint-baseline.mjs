// Git merge driver for tools/validation/enforce-lint-baseline.json.
// The baseline is a ratchet: branches remove entries when they fix debt and
// rarely add one for an accepted false positive. Two branches that each fix
// different debt delete neighbouring entries, and a line merge conflicts.
//
// The merge is a three-way multiset merge keyed like the lint (rule|file|text):
// count = max(0, ours + theirs - base). A removal on either side stays removed,
// an addition on either side stays. Output is sorted and formatted exactly as
// `validate-enforce-lint.mjs --update-baseline` writes it, so the result is
// deterministic. Unparseable input falls back to a line merge with markers.
//
//   node tools/merge/union-lint-baseline.mjs %O %A %B %P

import { readFileSync, writeFileSync } from "node:fs";
import { mergeLines } from "./merge-markdown.mjs";

const keyOf = (e) => `${e.rule}|${e.file}|${e.text}`;

function counts(entries) {
  const map = new Map();
  for (const e of entries) {
    const k = keyOf(e);
    const slot = map.get(k);
    if (slot) slot.n++;
    else map.set(k, { entry: { rule: e.rule, file: e.file, text: e.text }, n: 1 });
  }
  return map;
}

export function mergeLintBaselines(baseText, oursText, theirsText) {
  let base, ours, theirs;
  try {
    base = baseText.trim() ? JSON.parse(baseText) : [];
    ours = JSON.parse(oursText);
    theirs = JSON.parse(theirsText);
  } catch (e) {
    return { ok: false, error: `lf-lint-baseline: cannot parse a side as JSON (${e.message})` };
  }
  if (![base, ours, theirs].every(Array.isArray)) return { ok: false, error: "lf-lint-baseline: a side is not a JSON array" };
  const b = counts(base);
  const o = counts(ours);
  const t = counts(theirs);
  const out = [];
  for (const k of new Set([...b.keys(), ...o.keys(), ...t.keys()])) {
    const n = (o.get(k)?.n ?? 0) + (t.get(k)?.n ?? 0) - (b.get(k)?.n ?? 0);
    const entry = (o.get(k) ?? t.get(k) ?? b.get(k)).entry;
    for (let i = 0; i < n; i++) out.push(entry);
  }
  out.sort((x, y) => keyOf(x).localeCompare(keyOf(y)));
  const eol = /\r\n/.test(oursText) ? "\r\n" : "\n";
  return { ok: true, text: (JSON.stringify(out, null, 1) + "\n").replace(/\n/g, eol) };
}

function runDriver() {
  const [basePath, oursPath, theirsPath] = process.argv.slice(2, 5);
  if (!basePath || !oursPath || !theirsPath) {
    console.error("usage: union-lint-baseline.mjs <base> <ours> <theirs> [path]");
    process.exit(2);
  }
  const base = readFileSync(basePath, "utf8");
  const ours = readFileSync(oursPath, "utf8");
  const theirs = readFileSync(theirsPath, "utf8");
  const merged = mergeLintBaselines(base, ours, theirs);
  if (!merged.ok) {
    writeFileSync(oursPath, mergeLines(base, ours, theirs).text);
    console.error(merged.error);
    process.exit(1);
  }
  writeFileSync(oursPath, merged.text);
}

const invoked = process.argv[1] && process.argv[1].replace(/\\/g, "/").includes("union-lint-baseline.mjs");
if (invoked) runDriver();
