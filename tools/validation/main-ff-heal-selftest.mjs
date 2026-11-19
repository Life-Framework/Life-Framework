// Proves #3700: a stray tracked edit in the main checkout no longer blocks its fast-forward, on throwaway git repos.
//   1. a clean checkout behind origin fast-forwards and nothing is stashed;
//   2. a tracked file an agent edited (the file origin changed too) is stashed under a label that names it, the
//      checkout fast-forwards, the edit is still in `git stash list` (never discarded) and one alert line says so;
//   3. a Worlds/ layer edit (the maintainer's hand work) is never stashed: the checkout stays blocked, the layer
//      keeps its edit, and the alert names it; a mixed set stashes only the non-maintainer files;
//   4. an untracked file that blocks is not stashed (the lead's stray mover owns that); the alert says the cause is not a tracked edit;
//   5. the alert is written to the hub file the lead view reads, and the lead reads it back as one MAINFF line, only while fresh.
// inputs: tools/lib/main-ff-heal.mjs
// red-proof: 2026-11-19, with the module absent the selftest failed on import; with `maintainerPath` always false check 3 failed
// (the layer was stashed); with the retry ff removed check 2 failed (still blocked); restored.
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { gitMust, removeScratch, runGit, useHermeticGit } from "../lib/selftest-git.mjs";
import { scratchDir } from "../lib/scratch.mjs";
import { alertFresh, healBlockedFastForward, maintainerPath, parseTracked, readAlert, stashLabel, writeAlert } from "../lib/main-ff-heal.mjs";

useHermeticGit();
const failures = [];
const check = (ok, msg) => { if (!ok) failures.push(msg); };

// pure pieces
check(maintainerPath("addons/LifeFramework/Worlds/DebugWorld/DebugWorld_Layers/town.layer") && maintainerPath("addons/LF_Everon1989/Worlds/Everon/x.layer"), "a Worlds/ path is the maintainer's");
check(!maintainerPath("docs/features/sites.md") && !maintainerPath("tools/town/drops.mjs") && !maintainerPath("addons/LifeFramework/Scripts/Game/Worlds.c"), "docs, tools and a file merely named Worlds are not");
const tr = parseTracked([" M docs/a.md", "M  tools/b.mjs", "R  new.md", "old.md", "?? x.txt", "AM added.c", ""].join("\0"));
check(tr.join("|") === "docs/a.md|tools/b.mjs|new.md|added.c", `parseTracked keeps tracked paths, skips untracked, reads a rename's two records (got ${tr.join("|")})`);
const label = stashLabel(["a.md", "b.md", "c.md", "d.md", "e.md"], new Date("2026-11-19T10:00:00Z"));
check(label.includes("a.md") && label.includes("c.md") && label.includes("+2 more") && label.startsWith("lf-main-ff-heal"), `the stash label names the files (got ${label})`);

const base = scratchDir("lf-ffheal-");
const sh = (cwd, args) => gitMust(cwd, args);
const origin = join(base, "origin.git");
const main = join(base, "main");
mkdirSync(origin, { recursive: true });
sh(origin, ["init", "--bare", "-b", "main"]);
const seed = join(base, "seed");
mkdirSync(seed, { recursive: true });
sh(seed, ["init", "-b", "main"]);
const put = (dir, rel, text) => { mkdirSync(join(dir, rel, ".."), { recursive: true }); writeFileSync(join(dir, rel), text); };
for (const [rel, text] of [["docs/a.md", "a1\n"], ["tools/b.mjs", "b1\n"], ["addons/LifeFramework/Worlds/W/town.layer", "layer1\n"], ["other.txt", "o1\n"]]) put(seed, rel, text);
sh(seed, ["add", "-A"]);
sh(seed, ["commit", "-m", "one"]);
sh(seed, ["remote", "add", "origin", origin]);
sh(seed, ["push", "origin", "main"]);
sh(base, ["clone", origin, "main"]);

const advance = (files, msg) => {
  for (const [rel, text] of files) put(seed, rel, text);
  sh(seed, ["add", "-A"]);
  sh(seed, ["commit", "-m", msg]);
  sh(seed, ["push", "origin", "main"]);
  sh(main, ["fetch", "-q", "origin"]);
};
const runner = (args) => runGit(main, args);
const head = () => sh(main, ["rev-parse", "HEAD"]);
const originHead = () => sh(main, ["rev-parse", "origin/main"]);
const stashes = () => sh(main, ["stash", "list"]).split("\n").filter(Boolean);

// 1. clean and behind: plain fast-forward
advance([["other.txt", "o2\n"]], "two");
let r = healBlockedFastForward(main, runner);
check(r.action === "ff" && head() === originHead() && stashes().length === 0, `a clean checkout fast-forwards and stashes nothing (${JSON.stringify(r)})`);

// 2. an agent's stray edit to a file origin also changed
advance([["docs/a.md", "a2 from origin\n"]], "three");
writeFileSync(join(main, "docs/a.md"), "a1 stray agent edit\n");
r = healBlockedFastForward(main, runner, { now: new Date("2026-11-19T10:00:00Z") });
check(r.action === "stashed" && head() === originHead(), `a stray tracked edit is stashed and the checkout fast-forwards (${JSON.stringify(r)})`);
check(stashes().length === 1 && stashes()[0].includes("docs/a.md") && stashes()[0].includes("lf-main-ff-heal"), `the stash is labelled with the file (${stashes().join(" / ")})`);
check(readFileSync(join(main, "docs/a.md"), "utf8") === "a2 from origin\n", "the checkout holds origin's file after the heal");
check(sh(main, ["stash", "show", "-p", "stash@{0}"]).includes("a1 stray agent edit"), "the stray edit is kept in the stash, not discarded");
check(/stashed 1 file/.test(r.line ?? "") && r.line.includes("docs/a.md") && r.line.includes("git stash pop"), `one alert line names the file and the restore command (${r.line})`);
sh(main, ["stash", "drop"]);

// 3. the maintainer's layer is never touched; a mixed set stashes only the other file
advance([["addons/LifeFramework/Worlds/W/town.layer", "layer2 from origin\n"]], "four");
writeFileSync(join(main, "addons/LifeFramework/Worlds/W/town.layer"), "layer1 maintainer hand edit\n");
const before = head();
r = healBlockedFastForward(main, runner);
check(r.action === "maintainer" && head() === before && stashes().length === 0, `a maintainer layer edit blocks nothing silently: no stash, no move (${JSON.stringify(r)})`);
check(readFileSync(join(main, "addons/LifeFramework/Worlds/W/town.layer"), "utf8") === "layer1 maintainer hand edit\n", "the layer keeps the maintainer's edit");
check(/town\.layer/.test(r.line ?? "") && /maintainer/i.test(r.line), `the alert names the layer and says it is the maintainer's (${r.line})`);
advance([["tools/b.mjs", "b2 from origin\n"]], "five");
writeFileSync(join(main, "tools/b.mjs"), "b1 stray\n");
r = healBlockedFastForward(main, runner);
check(r.action === "maintainer" && stashes().length === 1 && stashes()[0].includes("tools/b.mjs") && !stashes()[0].includes("town.layer"), `a mixed set stashes only the non-maintainer file, and the checkout stays blocked by the layer (${JSON.stringify(r)})`);
check(readFileSync(join(main, "addons/LifeFramework/Worlds/W/town.layer"), "utf8") === "layer1 maintainer hand edit\n", "the layer is still the maintainer's after a mixed heal");
sh(main, ["stash", "drop"]);
sh(main, ["checkout", "--", "addons/LifeFramework/Worlds/W/town.layer"]); // the fixture maintainer is done
sh(main, ["merge", "--ff-only", "-q", "origin/main"]);

// 4. an untracked file in the way is the stray mover's job
advance([["new/file.txt", "from origin\n"]], "six");
// Make the untracked file collide with origin's new path, then re-run: git refuses ("would be overwritten").
sh(main, ["reset", "--hard", "-q", "HEAD~1"]);
put(main, "new/file.txt", "local untracked\n");
const behind = head();
r = healBlockedFastForward(main, runner);
check(r.action === "blocked" && head() === behind && stashes().length === 0, `an untracked collision is not stashed (${JSON.stringify(r)})`);
check(/not a tracked edit/.test(r.line ?? ""), `the alert says the block is not a tracked edit (${r.line})`);

// 5. the alert file the lead reads
const hub = join(base, "hub");
mkdirSync(hub, { recursive: true });
const t0 = Date.parse("2026-11-19T10:00:00Z");
writeAlert(hub, "stashed 1 file(s)", t0);
check(readAlert(hub)?.line === "stashed 1 file(s)", "writeAlert and readAlert round-trip");
check(alertFresh(readAlert(hub), t0 + 3600 * 1000) && !alertFresh(readAlert(hub), t0 + 13 * 3600 * 1000), "an alert shows for 12 h, then goes quiet");
check(readAlert(join(base, "nowhere")) === null, "no alert file reads as null");
check(!existsSync(join(hub, "locks")), "writeAlert makes only its own file");

removeScratch(base);
if (failures.length) {
  console.log(`FAIL  main-ff-heal-selftest: ${failures.length} check(s) failed`);
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}
console.log("ok  main-ff-heal-selftest: a stray tracked edit is stashed and the fast-forward goes through; maintainer layers and untracked files are left alone");
