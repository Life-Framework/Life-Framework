// Proves #5117: the main guard's run worktree is cleaned of untracked files under addons/ before the pre-move e2e, and
// an `e2e --pre-move needs a clean tree` refusal is infra, never red (no recheck, no bisect, no fix card, no revert).
// 2026-11-27 the worker died mid pre-move e2e at 13:11 UTC; the old-ref checkout left Assets/Buildings/ and
// Props/Roleplay/BankVault/ untracked, the next pre-move e2e refused, and the guard filed a "missing
// LF_E2E.RequiresPayload guard" card for 03731faa and 921ff355. The clean goes through safeRemoveTree: a junction
// inside addons/ (the game-addons shape) is unlinked, its target kept.
// inputs: tools/lib/run-tree-clean.mjs, tools/lib/main-guard.mjs, tools/main-guard.mjs, tools/link-safety.mjs
// red-proof 2026-11-27: with cleanRunTree returning [] at once this printed "FAIL  untracked Assets/Buildings file is
//   gone"; with the clean-tree pattern removed from INFRA_REFUSALS it printed "FAIL  pre-move e2e clean-tree refusal:
//   infra, got {\"verdict\":\"red\"...".
import { existsSync, mkdirSync, readFileSync, symlinkSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { scratchDir } from "../lib/scratch.mjs";
import { gitMust, useHermeticGit } from "../lib/selftest-git.mjs";
import { cleanRunTree } from "../lib/run-tree-clean.mjs";
import * as g from "../lib/main-guard.mjs";

useHermeticGit();
let failed = 0;
const check = (c, m) => { if (!c) { console.log(`FAIL  ${m}`); failed++; } };
const put = (p, text = "x\n") => { mkdirSync(join(p, ".."), { recursive: true }); writeFileSync(p, text); };

// 1. A scratch repo shaped like the run worktree: one tracked addon file, the kept rdb ignored.
const repo = scratchDir("lf-guard-run-tree-");
gitMust(repo, ["init", "-q"]);
put(join(repo, ".gitignore"), "resourceDatabase.rdb\n");
put(join(repo, "addons", "LifeFramework", "Scripts", "Keep.c"), "// tracked\n");
put(join(repo, "tools", "tracked.txt"), "tracked\n");
gitMust(repo, ["add", "-A"]);
gitMust(repo, ["-c", "user.name=t", "-c", "user.email=t@t", "commit", "-q", "-m", "base"]);

// Leftovers of the old-ref checkout, the rdb the pool keeps, an untracked file outside addons/, and a junction into a
// sentinel tree (the Steam install shape) that the clean must unlink, not follow.
put(join(repo, "addons", "LifeFramework", "Assets", "Buildings", "b.xob"));
put(join(repo, "addons", "LifeFramework", "Assets", "Props", "Roleplay", "BankVault", "v.xob"));
put(join(repo, "addons", "LifeFramework", "resourceDatabase.rdb"), "rdb");
put(join(repo, "tmp", "note.txt"), "outside addons\n");
const install = scratchDir("lf-guard-install-");
put(join(install, "sentinel.txt"), "steam\n");
symlinkSync(install, join(repo, "addons", "LifeFramework", "game-link"), "junction");

const removed = cleanRunTree(repo);
check(!existsSync(join(repo, "addons", "LifeFramework", "Assets", "Buildings", "b.xob")), "untracked Assets/Buildings file is gone");
check(!existsSync(join(repo, "addons", "LifeFramework", "Assets", "Props", "Roleplay", "BankVault", "v.xob")), "untracked BankVault file is gone");
check(removed.length >= 2, `the removed paths are reported, got ${JSON.stringify(removed)}`);
check(existsSync(join(repo, "addons", "LifeFramework", "Scripts", "Keep.c")), "a tracked addons file stays");
check(existsSync(join(repo, "addons", "LifeFramework", "resourceDatabase.rdb")), "the rdb stays");
check(existsSync(join(repo, "tmp", "note.txt")), "a file outside addons/ stays");
check(existsSync(join(install, "sentinel.txt")), "the junction target (the Steam install) keeps its files");
check(!existsSync(join(repo, "addons", "LifeFramework", "game-link")), "the junction itself is unlinked");
// What the e2e then sees: nothing under addons/ is dirty (the e2e's own check is `git status --porcelain`).
const dirty = gitMust(repo, ["status", "--porcelain", "--untracked-files=normal", "--", "addons"]);
check(dirty === "", `git status under addons/ is clean after the clean, got ${JSON.stringify(dirty)}`);
check(cleanRunTree(repo).length === 0, "a second clean finds nothing");

// 2. The refusal the e2e printed, verbatim from the 2026-11-27 run, is infra; a real e2e failure stays red.
const REFUSAL = "FAIL  e2e --pre-move needs a clean tree (commit first): ?? addons/LifeFramework/Assets/Buildings/; ?? addons/LifeFramework/Assets/Props/Roleplay/BankVault/";
const v = g.classifyStep("pre-move e2e", 1, REFUSAL);
check(v.verdict === "infra" && /not clean/.test(v.why), `pre-move e2e clean-tree refusal: infra, got ${JSON.stringify(v)}`);
check(g.classifyStep("e2e", 1, REFUSAL).verdict === "infra", "the plain e2e step reads it as infra too");
check(g.classifyStep("pre-move e2e", 1, `${REFUSAL}\nFAIL  e2e: pass 2 [LFTEST] FAIL LF_Test_Save`).verdict === "red", "a refusal beside a real test failure: red");
check(g.classifyStep("pre-move e2e", 1, "FAIL  e2e: FAILED in pass 2 (reload boot)").verdict === "red", "an unexplained pre-move failure: red");

// 3. The guard calls the clean before every pre-move e2e run and after each checkout, and keeps the run non-fatal.
const src = readFileSync(new URL("../main-guard.mjs", import.meta.url), "utf8");
const runCli = src.slice(src.indexOf("function runCli("), src.indexOf("// A flake leaves its evidence"));
check(/args\.includes\("--pre-move"\)\)\s*dropStrays\(dir/.test(runCli), "runCli cleans the run tree before a --pre-move command");
const checkout = src.slice(src.indexOf("function checkout("), src.indexOf("// The step logs of the children"));
check(/status === 0\) \{ dropStrays\(dir\)/.test(checkout), "checkout cleans after moving to the head");
check(/function dropStrays[\s\S]{0,400}catch \(e\)/.test(src), "a clean that fails is logged, never thrown");
check(/classifyStep\(step\.name, r\.code, r\.text, failing\)[\s\S]{0,300}verdict !== "red"/.test(src), "the first run is classified before any recheck");

if (failed) process.exit(1);
console.log("PASS  main-guard-clean-run-tree-selftest");
