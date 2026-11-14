// Canonical order for a Language/*.st string table: items sorted by Id (ordinal), each block kept
// byte for byte with its own indent.
//
// Why: GitHub cannot run our merge drivers. When every tool appended new keys before the closing
// braces, any two PRs that added a string inserted at the same line and conflicted, in the .st and
// in all seven generated .conf files (whose Ids/Texts follow .st order). Sorted by Id, a feature's
// new keys (LF-<Feature>_*) land in their own gap, so plain git merges two string PRs cleanly
// unless their keys sort into the same gap between two existing keys.
//
//   node tools/merge/sort-string-table.mjs --write <file.st>...   rewrite in canonical order
//   node tools/merge/sort-string-table.mjs --check <file.st>...   exit 1 when a file is not canonical
//
// tools\cli regen-localization runs --write before it derives the confs; validate runs --check.

import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { coverageError, parseStringTable } from "./union-string-table.mjs";

export function compareIds(a, b) {
  if (a.id !== b.id) return a.id < b.id ? -1 : 1;
  if (a.block !== b.block) return a.block < b.block ? -1 : 1;
  return 0;
}

// The whitespace on the item's own line before `CustomStringTableItem`.
function indentOf(gap) {
  const nl = gap.lastIndexOf("\n");
  const tail = nl < 0 ? gap : gap.slice(nl + 1);
  return /^[ \t]*$/.test(tail) ? tail : "";
}

// -> { ok, text } with the items in canonical order, or { ok: false, error } when the table does
// not parse whole (nothing is rewritten then).
export function canonicalStringTable(text) {
  const parsed = parseStringTable(text);
  const error = coverageError(parsed, "table");
  if (error) return { ok: false, error };
  if (!parsed.items.length) return { ok: true, text };
  const eol = text.includes("\r\n") ? "\r\n" : "\n";
  const first = parsed.items[0];
  const firstIndent = indentOf(first.gapBefore);
  const head = first.gapBefore.slice(0, first.gapBefore.length - firstIndent.length);
  const items = parsed.items.map((item) => ({ id: item.id, block: item.block, indent: indentOf(item.gapBefore) }));
  items.sort(compareIds);
  const body = items.map((item) => item.indent + item.block).join(eol);
  const out = head + body + parsed.footer;
  const check = parseStringTable(out);
  if (coverageError(check, "sorted") || check.items.length !== items.length) {
    return { ok: false, error: "sort-string-table: the sorted table does not hold every item whole" };
  }
  return { ok: true, text: out };
}

function main(argv) {
  const mode = argv[0];
  const files = argv.slice(1);
  if ((mode !== "--write" && mode !== "--check") || !files.length) {
    console.error("usage: sort-string-table.mjs --write|--check <file.st>...");
    return 2;
  }
  let bad = 0;
  for (const file of files) {
    if (!existsSync(file)) continue;
    const text = readFileSync(file, "utf8");
    const res = canonicalStringTable(text);
    if (!res.ok) {
      console.log(`sort-string-table: FAILED - ${file}: ${res.error}`);
      bad++;
      continue;
    }
    if (res.text === text) continue;
    if (mode === "--write") {
      writeFileSync(file, res.text, "utf8");
      console.log(`sort-string-table: sorted ${file} by Id`);
    } else {
      console.log(`sort-string-table: FAILED - ${file} is not sorted by Id (run 'tools\\cli regen-localization')`);
      bad++;
    }
  }
  return bad ? 1 : 0;
}

const invoked = process.argv[1] && process.argv[1].replace(/\\/g, "/").endsWith("sort-string-table.mjs");
if (invoked) process.exit(main(process.argv.slice(2)));
