// Git merge driver for Language/everonlife_localization.st.
// Unions string ids. When both sides changed the same item differently, or a
// side does not parse whole, it writes git's line merge (conflict markers
// where they clash) and exits 1, so nothing from either side is lost.
//
//   node tools/merge/union-string-table.mjs %O %A %B %P

import { readFileSync, writeFileSync } from "node:fs";
import { mergeLines } from "./merge-markdown.mjs";

// An item ends at the first line that holds only `}`. Its text never decides
// that: a value may hold a term placeholder such as {police} (#958) or a
// quote the file does not escape (`Comment "Term "police" ..."`), and the
// pre-#958 `[^}]*` parser cut items at the first such brace.
const ITEM_RE = /CustomStringTableItem[ \t]+"\{[0-9A-Fa-f]+\}"[ \t]*\{[ \t]*\r?\n(?:[^\n]*\n)*?[ \t]*\}(?=[ \t]*(?:\r?\n|$))/g;

export function parseStringTable(text) {
  const items = [];
  ITEM_RE.lastIndex = 0;
  let match;
  let previousEnd = 0;
  while ((match = ITEM_RE.exec(text))) {
    const block = match[0];
    const idMatch = /Id\s+"([^"]+)"/.exec(block);
    if (!idMatch) continue;
    items.push({
      id: idMatch[1],
      block,
      gapBefore: text.slice(previousEnd, match.index),
      first: items.length === 0,
    });
    previousEnd = match.index + block.length;
  }
  return {
    text,
    items,
    header: items.length ? items[0].gapBefore : text,
    footer: text.slice(previousEnd),
  };
}

function blockKey(item) {
  return item ? item.block.replace(/\r\n/g, "\n") : null;
}

function separatorOf(parsed) {
  const clean = parsed.items.find((item) => !item.first && !/\S/.test(item.gapBefore));
  if (clean) return clean.gapBefore;
  return parsed.text.includes("\r\n") ? "\r\n    " : "\n    ";
}

const ITEM_START_RE = /CustomStringTableItem\s+"\{[0-9A-Fa-f]+\}"/g;

// A parse that leaves part of an item outside every matched block writes that
// part nowhere: the pre-#958 parser ended an item at the first `}` of a {term}
// placeholder and the merge lost the rest of the item. So every item start must
// be matched, and only whitespace may sit between two items.
export function coverageError(parsed, label) {
  const starts = (parsed.text.match(ITEM_START_RE) || []).length;
  if (starts !== parsed.items.length) {
    return `lf-string-table: ${label} has ${starts} items but ${parsed.items.length} parsed whole`;
  }
  // A lone `}` between items is the stray brace main carried until this fix;
  // quotes or words there are the tail of an item the parse cut short.
  const bad = parsed.items.find((item) => !item.first && /["\w]/.test(item.gapBefore));
  if (bad) return `lf-string-table: ${label} has text outside an item before ${bad.id}`;
  if (/^\s*(Id|Target_)/m.test(parsed.footer)) return `lf-string-table: ${label} has item text after the last item`;
  return null;
}

export function mergeStringTables(baseText, oursText, theirsText) {
  const base = parseStringTable(baseText ?? "");
  const ours = parseStringTable(oursText ?? "");
  const theirs = parseStringTable(theirsText ?? "");
  for (const [parsed, label] of [[base, "base"], [ours, "ours"], [theirs, "theirs"]]) {
    const error = coverageError(parsed, label);
    if (error) return { ok: false, error };
  }
  const baseMap = new Map(base.items.map((item) => [item.id, item]));
  const oursMap = new Map(ours.items.map((item) => [item.id, item]));
  const theirsMap = new Map(theirs.items.map((item) => [item.id, item]));
  const ids = [];
  const seen = new Set();
  for (const parsed of [base, ours, theirs]) {
    for (const item of parsed.items) {
      if (seen.has(item.id)) continue;
      seen.add(item.id);
      ids.push(item.id);
    }
  }
  const chosen = [];
  for (const id of ids) {
    // The whole block: a side that only edited a Comment keeps its edit.
    const baseItem = blockKey(baseMap.get(id));
    const oursItem = blockKey(oursMap.get(id));
    const theirsItem = blockKey(theirsMap.get(id));
    let pick = null;
    if (oursItem === theirsItem) {
      pick = oursMap.get(id) || theirsMap.get(id) || null;
    } else if (oursItem === baseItem) {
      pick = theirsMap.get(id) || null;
    } else if (theirsItem === baseItem) {
      pick = oursMap.get(id) || null;
    } else {
      return { ok: false, error: `lf-string-table: id ${id} has different text` };
    }
    if (pick) chosen.push(pick);
  }
  const skeleton = ours.items.length ? ours : theirs.items.length ? theirs : base;
  let text = "";
  chosen.forEach((item, index) => {
    if (index === 0) text += item.first ? item.gapBefore : skeleton.header;
    else if (item.first || /\S/.test(item.gapBefore)) text += separatorOf(skeleton);
    else text += item.gapBefore;
    text += item.block;
  });
  text += chosen.length ? skeleton.footer : skeleton.text;
  const check = parseStringTable(text);
  const written = check.items.map((item) => item.block);
  if (coverageError(check, "merged") || written.length !== chosen.length || written.some((block, i) => block !== chosen[i].block)) {
    return { ok: false, error: "lf-string-table: the merged table does not hold every chosen item whole" };
  }
  return { ok: true, text };
}

function runDriver() {
  const basePath = process.argv[2];
  const oursPath = process.argv[3];
  const theirsPath = process.argv[4];
  if (!basePath || !oursPath || !theirsPath) {
    console.error("usage: union-string-table.mjs <base> <ours> <theirs> [path]");
    process.exit(2);
  }
  const merged = mergeStringTables(
    readFileSync(basePath, "utf8"),
    readFileSync(oursPath, "utf8"),
    readFileSync(theirsPath, "utf8"),
  );
  if (!merged.ok) {
    const ours = readFileSync(oursPath, "utf8");
    writeFileSync(oursPath, mergeLines(readFileSync(basePath, "utf8"), ours, readFileSync(theirsPath, "utf8")).text);
    console.error(`${merged.error}; wrote git's line merge instead, check it before you add the file`);
    process.exit(1);
  }
  writeFileSync(oursPath, merged.text);
}

const invoked = process.argv[1] && process.argv[1].replace(/\\/g, "/").includes("union-string-table.mjs");
if (invoked) runDriver();
