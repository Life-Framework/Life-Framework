// Proves the credits merge handling (#4668) in a throwaway git repo: two branches that each add a credits
// row to docs/credits.md merge (and rebase) clean, and the string table and LF_CreditsData.c hold BOTH rows,
// regenerated from the merged source, with each branch's other string rows kept.
// inputs: tools/merge/**, tools/lib/selftest-git.mjs, tools/lib/scratch.mjs, .gitattributes
// Does not boot the game and does not merge this worktree.
// red-proof (2026-11-24): with the string-table driver as it was on main (git show HEAD:tools/merge/union-string-table.mjs
//   put back in place) the merge and the rebase both stop with "id LF-Credits_4 has different text" and the unit control
//   fails the same way; with this change both finish clean.
import { cpSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { dataFile, entries, patchSt } from "../merge/credits-rows.mjs";
import { mergeCreditsData } from "../merge/regen-credits-data.mjs";
import { mergeStringTables } from "../merge/union-string-table.mjs";
import { scratchDir } from "../lib/scratch.mjs";
import { gitMust, removeScratch, runGit, useHermeticGit } from "../lib/selftest-git.mjs";

useHermeticGit();

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const failures = [];
const fail = (m) => failures.push(m);

const ST = "addons/LifeFramework/Language/everonlife_localization.st";
const DATA = "addons/LifeFramework/Scripts/Game/Feature/Credits/LF_CreditsData.c";
const MD = "docs/credits.md";

// Beta has no author or licence, so its branch writes a different LF_CreditsData.c than Alpha's (the same shape would give
// two identical files, which git resolves without any driver; `wt ship` re-derives that case).
const md = (...rows) => `# Credits

## Projects

| Project | Author | Licence |
|---|---|---|
${rows.map((r) => (r === "Beta" ? `| ${r} | | |` : `| ${r} | Someone | MIT |`)).join("\n")}
`;

const guids = new Map();
function row(id, text) {
  if (!guids.has(id)) guids.set(id, (0xA0000 + guids.size).toString(16).toUpperCase().padStart(16, "0"));
  const guid = guids.get(id);
  return `    CustomStringTableItem "{${guid}}" {
     Id "${id}"
     Target_en_us "${text}"
    }`;
}
const skeleton = (...extra) => `StringTable {
 ItemClassName "CustomStringTableItem"
 Items {
${[row("LF-Zeta_Keep", "Keep"), ...extra.map((id) => row(id, id))].join("\n")}
 }
}
`;
const stFor = (extra, text) => patchSt(skeleton(...extra), entries(text));

// Unit control: without the credits source the two tables clash on the same Id, as the old driver did.
{
  const base = stFor([], md("Base"));
  const ours = stFor([], md("Base", "Alpha"));
  const theirs = stFor([], md("Base", "Beta"));
  if (mergeStringTables(base, ours, theirs).ok) fail("control: two different credits rows should clash in the plain union");
  const merged = mergeStringTables(base, ours, theirs, { creditsMd: () => ({ ok: true, text: md("Base", "Alpha", "Beta") }) });
  if (!merged.ok) fail(`credits-aware union failed: ${merged.error}`);
  else if (merged.text !== stFor([], md("Base", "Alpha", "Beta"))) fail("credits-aware union is not the table the generator writes for the merged source");
  const data = mergeCreditsData(dataFile(entries(md("Base"))), dataFile(entries(md("Base", "Alpha"))), dataFile(entries(md("Base", "Beta"))), () => ({ ok: true, text: md("Base", "Alpha", "Beta") }));
  if (!data.ok || data.text !== dataFile(entries(md("Base", "Alpha", "Beta")))) fail("data merge is not the file the generator writes for the merged source");
  if (mergeCreditsData("a", "b", "c", () => ({ ok: false, text: "" })).ok) fail("data merge with an unmergeable source should not claim success");
  if (!mergeCreditsData("a", "b", "a", () => null).ok) fail("a one-sided change to the data file should take that side");
}

function git(cwd, ...args) {
  return gitMust(cwd, args);
}

function write(dir, rel, text) {
  mkdirSync(dirname(join(dir, rel)), { recursive: true });
  writeFileSync(join(dir, rel), text);
}

function commitState(dir, extra, text, message) {
  write(dir, MD, text);
  write(dir, ST, stFor(extra, text));
  write(dir, DATA, dataFile(entries(text)));
  git(dir, "add", "-A");
  git(dir, "commit", "-q", "-m", message);
}

function makeRepo() {
  const dir = scratchDir("lf-credits-merge-");
  git(dir, "init", "-q", "-b", "main");
  cpSync(join(ROOT, "tools", "merge"), join(dir, "tools", "merge"), { recursive: true });
  write(dir, ".gitattributes", `${ST} merge=lf-string-table\n${DATA} merge=lf-credits-data\n${MD} merge=lf-markdown\n`);
  git(dir, "config", "--local", "--add", "include.path", join(ROOT, "tools", "merge", "drivers.gitconfig"));
  commitState(dir, [], md("Base"), "base");
  return dir;
}

function branch(dir, name, extra, text) {
  git(dir, "checkout", "-q", "-b", name, "main");
  commitState(dir, extra, text, `add ${name}`);
}

function check(label, dir, result) {
  const read = (rel) => readFileSync(join(dir, rel), "utf8");
  if (result.status !== 0) {
    fail(`${label}: should finish clean, got exit ${result.status}:\n${result.stdout}${result.stderr}`);
    return;
  }
  const merged = read(MD);
  for (const name of ["Base", "Alpha", "Beta"]) if (!merged.includes(`| ${name} |`)) fail(`${label}: merged docs/credits.md lost ${name}`);
  const list = entries(merged);
  if (list.length !== 4) fail(`${label}: expected a heading and three rows, got ${list.length} entries`);
  const st = read(ST);
  if (st !== stFor(["LF-Alpha_Row", "LF-Beta_Row"], merged)) fail(`${label}: string table is not the generator's output for the merged source, with both branches' other rows`);
  for (const text of ["Alpha", "Beta"]) if (!st.includes(`Target_en_us "${text}"`)) fail(`${label}: string table has no row for ${text}`);
  if (read(DATA) !== dataFile(list)) fail(`${label}: LF_CreditsData.c is not the generator output for the merged source:
${read(DATA)}
--- merged md:
${merged}`);
  if (/<<<<<<<|>>>>>>>/.test(merged + st + read(DATA))) fail(`${label}: conflict markers left in a merged file`);
}

function scenario(kind) {
  const dir = makeRepo();
  try {
    branch(dir, "alpha", ["LF-Alpha_Row"], md("Base", "Alpha"));
    branch(dir, "beta", ["LF-Beta_Row"], md("Base", "Beta"));
    git(dir, "checkout", "-q", "alpha");
    const result = kind === "merge" ? runGit(dir, ["merge", "--no-edit", "beta"]) : runGit(dir, ["rebase", "beta"]);
    check(kind, dir, result);
  } finally {
    removeScratch(dir);
  }
}

scenario("merge");
scenario("rebase");

// The shipped config routes the files to the drivers.
const attrs = readFileSync(join(ROOT, ".gitattributes"), "utf8");
for (const line of [`${DATA} merge=lf-credits-data`, `${MD} merge=lf-markdown`, `${ST} merge=lf-string-table`]) {
  if (!attrs.includes(line)) fail(`.gitattributes is missing: ${line}`);
}
if (!readFileSync(join(ROOT, "tools", "merge", "drivers.gitconfig"), "utf8").includes('[merge "lf-credits-data"]')) fail("drivers.gitconfig does not define lf-credits-data");

if (failures.length) {
  for (const f of failures) console.log(`FAIL ${f}`);
  process.exit(1);
}
console.log("credits-merge: ok");
