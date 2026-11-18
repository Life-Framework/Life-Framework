// tools/validation/town-docs-no-draft-selftest.mjs
// inputs: docs/starter-packs/town.md, docs/worktrees.md, docs/architecture/everon-world.md,
//         docs/everon/world-bible.md
//
// Town cards ship with `wt ship` and auto-merge: there are no maintainer approvals since 2026-11-18.
// Proves the town starter pack never tells a town agent to open a draft PR, that the worktrees ship
// docs say town cards auto-merge, that the town pack asks for a contact sheet with the before and
// after pictures, and that the architecture and bible docs about town work say the same (no draft,
// names auto-merge). The `town-draft-<place>-<slot>` worktree name stays (the zone guard reads it), so
// it is removed before the draft word is searched for. No game, no network.
// red-proof: before this change docs/starter-packs/town.md:7, :38 and docs/worktrees.md:160 said the
// town PR is a draft, so this printed "FAIL ... says draft" lines and exited 1 (2026-11-18, #3199).
// red-proof: before the #3258 follow-up, docs/architecture/everon-world.md:161, :249, :250 and :283 and
// docs/everon/world-bible.md:199 said "draft", so section 3 printed "FAIL ... says draft" and exited 1
// (2026-11-18, #3199).

import { dirname, join, resolve } from "node:path";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const read = (rel) => readFileSync(join(ROOT, rel), "utf8");

let failed = 0;
function fail(msg) {
  console.log(`FAIL  ${msg}`);
  failed++;
}

// The worktree name is a zone-guard key, not a draft PR: hide it before searching for the word.
const worktreeName = /town-draft/gi;
const hideName = (line) => line.replace(worktreeName, "town-card");

// 1. The town starter pack: no line says draft (the worktree name aside), and it asks for the
//    contact sheet and the auto-merge ship.
const townRel = "docs/starter-packs/town.md";
const town = read(townRel).split("\n");
town.forEach((line, i) => {
  if (/draft/i.test(hideName(line))) fail(`${townRel}:${i + 1} says draft: ${line.trim()}`);
});
const townText = town.join("\n");
if (!/auto-merge/i.test(townText)) fail(`${townRel} never says a town card auto-merges`);
if (!/wt ship/.test(townText)) fail(`${townRel} never names \`wt ship\` as the ship step`);
if (!/contact sheet/i.test(townText)) fail(`${townRel} never asks for a contact sheet with the pictures`);
if (/maintainer reviews/i.test(townText)) fail(`${townRel} still says the maintainer reviews before merge`);

// 2. The ship docs: the --draft paragraph names no town card, and a Ship line says town cards
//    auto-merge.
const wtRel = "docs/worktrees.md";
const wt = read(wtRel).split("\n");
const start = wt.findIndex((l) => l.includes("`--draft` runs"));
const end = wt.findIndex((l, i) => i > start && l.includes("(#2940)"));
if (start < 0 || end < 0) {
  fail(`${wtRel}: the \`--draft\` paragraph is missing its start or its (#2940) end`);
} else {
  wt.slice(start, end + 1).forEach((line, i) => {
    if (/town/i.test(line)) fail(`${wtRel}:${start + i + 1} names town in the --draft paragraph: ${line.trim()}`);
  });
}
const townAutoMerge = wt.some((l) => /town/i.test(l) && /auto-merge/i.test(l) && !/--draft/.test(l));
if (!townAutoMerge) fail(`${wtRel} has no line saying town cards ship with auto-merge`);

// 3. The architecture and bible docs about town work: the same rule, no draft line and an
//    auto-merge ship.
for (const rel of ["docs/architecture/everon-world.md", "docs/everon/world-bible.md"]) {
  const lines = read(rel).split("\n");
  lines.forEach((line, i) => {
    if (/draft/i.test(hideName(line))) fail(`${rel}:${i + 1} says draft: ${line.trim()}`);
  });
  if (!/auto-merge/i.test(lines.join("\n"))) fail(`${rel} never says a town card auto-merges`);
}

if (failed) {
  console.log(`town-docs-no-draft-selftest: ${failed} failure(s)`);
  process.exit(1);
}
console.log("town-docs-no-draft-selftest: ok (town pack, ship docs, everon-world and world-bible say auto-merge, no draft)");
