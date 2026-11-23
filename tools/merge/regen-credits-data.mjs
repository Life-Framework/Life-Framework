// Git merge driver for Feature/Credits/LF_CreditsData.c (#4668).
//
// The file is generated from docs/credits.md by tools\cli credits: one `rows.Insert(...)` line per credits entry,
// keyed by the LF-Credits_<n> string ids, which are numbered in source order. Two branches that each add an
// entry rewrite the same lines, so a line merge conflicts. When only one side changed the file this takes that
// side; when both did it regenerates the file from the merged docs/credits.md (read at the base, ours and
// theirs commits, because git merges docs/credits.md after this file). If that source cannot be merged, or the
// sides are unknown, the file gets git's line merge with conflict markers and the driver exits 1: resolve
// docs/credits.md, then run tools\cli credits.
//
//   node tools/merge/regen-credits-data.mjs %O %A %B %P

import { readFileSync, writeFileSync } from "node:fs";
import { dataFile, entries, mergedCreditsFromGit } from "./credits-rows.mjs";
import { mergeLines } from "./merge-markdown.mjs";

const norm = (text) => text.replace(/\r\n/g, "\n");

// { ok, text } or { ok: false, error }; `creditsMd` is injectable for the selftest.
export function mergeCreditsData(base, ours, theirs, creditsMd = mergedCreditsFromGit) {
  if (norm(ours) === norm(base)) return { ok: true, text: theirs };
  if (norm(theirs) === norm(base)) return { ok: true, text: ours };
  // Both changed it. The file holds keys, not text, so two branches that each add one entry write the SAME
  // file and git sees no conflict, yet the merged source has two entries: always regenerate.
  const merged = creditsMd();
  if (norm(ours) === norm(theirs) && (!merged || !merged.ok)) return { ok: true, text: ours };
  if (!merged || !merged.ok) return { ok: false, error: "lf-credits-data: docs/credits.md did not merge cleanly (or the merge sides are unknown)" };
  const list = entries(merged.text);
  if (!list.length) return { ok: false, error: "lf-credits-data: the merged docs/credits.md gave no rows" };
  return { ok: true, text: dataFile(list) };
}

function runDriver() {
  const [basePath, oursPath, theirsPath] = process.argv.slice(2, 5);
  if (!basePath || !oursPath || !theirsPath) {
    console.error("usage: regen-credits-data.mjs <base> <ours> <theirs> [path]");
    process.exit(2);
  }
  const base = readFileSync(basePath, "utf8");
  const ours = readFileSync(oursPath, "utf8");
  const theirs = readFileSync(theirsPath, "utf8");
  const merged = mergeCreditsData(base, ours, theirs);
  if (!merged.ok) {
    writeFileSync(oursPath, mergeLines(base, ours, theirs).text);
    console.error(`${merged.error}; wrote git's line merge instead, resolve docs/credits.md and run tools\\cli credits`);
    process.exit(1);
  }
  writeFileSync(oursPath, merged.text);
}

const invoked = process.argv[1] && process.argv[1].replace(/\\/g, "/").includes("regen-credits-data.mjs");
if (invoked) runDriver();
