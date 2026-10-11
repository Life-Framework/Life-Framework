// Git merge driver for Language/everonlife_localization.st.
// Unions string ids. Fails only when the same id has different target text.
//
//   node tools/merge/union-string-table.mjs %O %A %B %P

import { readFileSync, writeFileSync } from "node:fs";

const ITEM_RE = /CustomStringTableItem\s+"\{[0-9A-Fa-f]+\}"\s*\{[^}]*\}/g;

export function parseStringTable(text) {
  const items = [];
  ITEM_RE.lastIndex = 0;
  let match;
  let previousEnd = 0;
  while ((match = ITEM_RE.exec(text))) {
    const block = match[0];
    const idMatch = /Id\s+"([^"]+)"/.exec(block);
    if (!idMatch) continue;
    const targets = {};
    const targetRe = /Target_([A-Za-z0-9_]+)\s+"([^"]*)"/g;
    let target;
    while ((target = targetRe.exec(block))) targets[target[1]] = target[2];
    items.push({
      id: idMatch[1],
      block,
      targets,
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

function textKey(item) {
  if (!item) return null;
  const keys = Object.keys(item.targets).sort();
  return JSON.stringify(keys.map((key) => [key, item.targets[key]]));
}

function separatorOf(parsed) {
  if (parsed.items.length >= 2) return parsed.items[1].gapBefore;
  return parsed.text.includes("\r\n") ? "\r\n    " : "\n    ";
}

export function mergeStringTables(baseText, oursText, theirsText) {
  const base = parseStringTable(baseText ?? "");
  const ours = parseStringTable(oursText ?? "");
  const theirs = parseStringTable(theirsText ?? "");
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
    const baseItem = textKey(baseMap.get(id));
    const oursItem = textKey(oursMap.get(id));
    const theirsItem = textKey(theirsMap.get(id));
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
    else if (item.first) text += separatorOf(skeleton);
    else text += item.gapBefore;
    text += item.block;
  });
  text += chosen.length ? skeleton.footer : skeleton.text;
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
    console.error(merged.error);
    process.exit(1);
  }
  writeFileSync(oursPath, merged.text);
}

const invoked = process.argv[1] && process.argv[1].replace(/\\/g, "/").includes("union-string-table.mjs");
if (invoked) runDriver();
