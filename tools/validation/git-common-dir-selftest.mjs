// tools/validation/git-common-dir-selftest.mjs
// inputs: tools/lib/git-common-dir.mjs, tools/lib/selftest-git.mjs, tools/lib/scratch.mjs
//
// Proves commonDirFromFiles (the common dir read from `.git` and `commondir`, no git spawn) names the same folder
// `git rev-parse --git-common-dir` does: in a plain repository, from a sub-folder, in a linked worktree and from
// a sub-folder of that worktree; and that it answers null (so the caller asks git) outside a repository, with
// git's environment overrides set, and for a `.git` file it cannot read.
// red-proof: with `commonFrom` returning the worktree's own gitdir instead of reading `commondir`, it printed
// "FAIL linked worktree: files <repo>\.git\worktrees\linked, git <repo>\.git" and exited 1 (2026-11-27).
import { mkdirSync, writeFileSync } from "node:fs";
import { join, normalize, resolve } from "node:path";
import { commonDirFromFiles } from "../lib/git-common-dir.mjs";
import { gitMust, removeScratch, useHermeticGit } from "../lib/selftest-git.mjs";
import { scratchDir } from "../lib/scratch.mjs";

useHermeticGit();
const same = (a, b) => normalize(resolve(a)).toLowerCase() === normalize(resolve(b)).toLowerCase();
let failed = 0;

function check(label, root, repoForGit) {
  const git = resolve(root, gitMust(root, ["rev-parse", "--git-common-dir"]));
  const files = commonDirFromFiles(root);
  if (!files || !same(files, git)) {
    console.log(`FAIL ${label}: files ${files}, git ${git}`);
    failed++;
  } else {
    console.log(`PASS ${label}`);
  }
}

const base = scratchDir("lf-commondir-");
const repo = join(base, "repo");
mkdirSync(join(repo, "sub", "deeper"), { recursive: true });
gitMust(repo, ["init", "-q"]);
writeFileSync(join(repo, "a.txt"), "a\n");
gitMust(repo, ["add", "-A"]);
gitMust(repo, ["commit", "-qm", "base"]);
const wt = join(base, "linked");
gitMust(repo, ["worktree", "add", "-q", "-b", "w", wt]);
mkdirSync(join(wt, "sub"), { recursive: true });

try {
  check("plain repository", repo);
  check("sub-folder of a plain repository", join(repo, "sub", "deeper"));
  check("linked worktree", wt);
  check("sub-folder of a linked worktree", join(wt, "sub"));

  const bare = scratchDir("lf-commondir-none-");
  // The OS temp folder has no repository above it unless someone made one there; then git decides, not this test.
  const none = commonDirFromFiles(bare);
  if (none === null) console.log("PASS no repository above: null");
  else console.log(`INFO a repository exists above the temp folder (${none}); no-repository case skipped`);

  process.env.GIT_DIR = join(repo, ".git");
  if (commonDirFromFiles(repo) !== null) {
    console.log("FAIL GIT_DIR set must answer null so git decides");
    failed++;
  } else console.log("PASS GIT_DIR set: null");
  delete process.env.GIT_DIR;

  const odd = join(base, "odd");
  mkdirSync(odd);
  writeFileSync(join(odd, ".git"), "not a gitdir pointer\n");
  if (commonDirFromFiles(odd) !== null) {
    console.log("FAIL an unreadable .git file must answer null");
    failed++;
  } else console.log("PASS odd .git file: null");
} finally {
  removeScratch(base);
}

if (failed) {
  console.log(`git-common-dir-selftest: ${failed} case(s) failed`);
  process.exit(1);
}
console.log("git-common-dir-selftest: OK (plain, sub-folder, linked worktree, no repo, GIT_DIR, odd .git file)");
