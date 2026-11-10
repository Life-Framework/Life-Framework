// Git merge driver for markdown files whose main content is a row-per-thing
// table (docs/asset-inventory.md, keyed by the Prefab column, or the Item
// column when Prefab is empty).
//
// It is merge-markdown.mjs (git 3-way line merge, both-appended rows kept) plus
// a keyed pass: two rows with the same key inside one table mean both sides
// added that thing. Identical rows collapse to one; differing rows become a
// conflict block and the driver exits 1. Both sides editing the same existing
// row is already a conflict in the line merge.
//
//   node tools/merge/merge-table-rows.mjs %O %A %B %P

import { readFileSync, writeFileSync } from "node:fs";
import { mergeLines } from "./merge-markdown.mjs";

const splitKeep = (text) => text.match(/[^\n]*\n|[^\n]+$/g) || [];
const bare = (line) => line.replace(/\r?\n$/, "");
const isRow = (line) => /^\s*\|/.test(line);
const isSeparator = (line) => /^\s*\|[\s:|-]+\|\s*$/.test(line) && line.includes("-");

function rowKey(line) {
  const cells = bare(line).trim().replace(/^\||\|$/g, "").split("|").map((c) => c.trim().replace(/`/g, ""));
  const key = cells[1] || cells[0] || "";
  return key.toLowerCase();
}

// Collapses duplicate-key rows in clean-merged text. Returns { text, conflicts }.
export function dedupeRows(text) {
  const lines = splitKeep(text);
  const out = [];
  let conflicts = 0;
  let seen = null;
  let position = 0;
  for (const line of lines) {
    if (!isRow(line)) {
      seen = null;
      position = 0;
      out.push(line);
      continue;
    }
    if (seen === null) seen = new Map();
    position++;
    if (position <= 2 || isSeparator(line)) {
      out.push(line);
      continue;
    }
    const key = rowKey(line);
    if (!key || !seen.has(key)) {
      if (key) seen.set(key, out.length);
      out.push(line);
      continue;
    }
    const at = seen.get(key);
    const first = out[at];
    if (bare(first) === bare(line)) continue;
    const eol = first.endsWith("\r\n") ? "\r\n" : "\n";
    out[at] = `<<<<<<< ours${eol}${bare(first)}${eol}=======${eol}${bare(line)}${eol}>>>>>>> theirs${eol}`;
    conflicts++;
  }
  return { text: out.join(""), conflicts };
}

export function mergeTableRows(baseText, oursText, theirsText) {
  const merged = mergeLines(baseText, oursText, theirsText);
  if (!merged.ok) return merged;
  const deduped = dedupeRows(merged.text);
  return { ok: deduped.conflicts === 0, conflicts: deduped.conflicts, text: deduped.text };
}

function runDriver() {
  const [basePath, oursPath, theirsPath, path] = process.argv.slice(2);
  if (!basePath || !oursPath || !theirsPath) {
    console.error("usage: merge-table-rows.mjs <base> <ours> <theirs> [path]");
    process.exit(2);
  }
  const merged = mergeTableRows(
    readFileSync(basePath, "utf8"),
    readFileSync(oursPath, "utf8"),
    readFileSync(theirsPath, "utf8"),
  );
  writeFileSync(oursPath, merged.text);
  if (!merged.ok) {
    console.error(`lf-table-rows: ${merged.conflicts} conflict(s) in ${path || oursPath}; both sides changed the same row`);
    process.exit(1);
  }
}

const invoked = process.argv[1] && process.argv[1].replace(/\\/g, "/").endsWith("tools/merge/merge-table-rows.mjs");
if (invoked) runDriver();
