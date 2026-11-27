// Git merge driver for tools/place/island-baseline.json.
// The baseline only shrinks: branches drop the rows they fix. Two branches that drop neighboring rows
// conflict in a line merge, although nothing contradicts. This merges the row sets three ways:
// a row stays when no side dropped it, and a row one side added stays (placement-gate-selftest rejects
// additions on its own). Output is what `baselineText` writes, so the result is deterministic.
// Unparseable input falls back to a line merge with markers and exit 1.
//
//   node tools/merge/merge-island-baseline.mjs %O %A %B %P

import { readFileSync, writeFileSync } from "node:fs";
import { mergeLines } from "./merge-markdown.mjs";
import { baselineText } from "../lib/placement-gate.mjs";

function rowsOf(text, side) {
  if (side === "base" && !text.trim()) return [];
  const j = JSON.parse(text);
  if (!Array.isArray(j.rows)) throw new Error(`${side} has no rows array`);
  return j.rows;
}

export function mergeIslandBaselines(baseText, oursText, theirsText) {
  let b, o, t;
  try {
    b = new Set(rowsOf(baseText, "base"));
    o = new Set(rowsOf(oursText, "ours"));
    t = new Set(rowsOf(theirsText, "theirs"));
  } catch (e) {
    return { ok: false, error: `lf-island-baseline: cannot parse a side (${e.message})` };
  }
  const out = [];
  for (const r of new Set([...b, ...o, ...t])) {
    const keep = b.has(r) ? o.has(r) && t.has(r) : o.has(r) || t.has(r);
    if (keep) out.push(r);
  }
  return { ok: true, text: baselineText(out) };
}

function runDriver() {
  const [basePath, oursPath, theirsPath] = process.argv.slice(2, 5);
  if (!basePath || !oursPath || !theirsPath) {
    console.error("usage: merge-island-baseline.mjs <base> <ours> <theirs> [path]");
    process.exit(2);
  }
  const base = readFileSync(basePath, "utf8");
  const ours = readFileSync(oursPath, "utf8");
  const theirs = readFileSync(theirsPath, "utf8");
  const merged = mergeIslandBaselines(base, ours, theirs);
  if (!merged.ok) {
    writeFileSync(oursPath, mergeLines(base, ours, theirs).text);
    console.error(merged.error);
    process.exit(1);
  }
  writeFileSync(oursPath, merged.text);
}

const invoked = process.argv[1] && process.argv[1].replace(/\\/g, "/").includes("merge-island-baseline.mjs");
if (invoked) runDriver();
