// Launcher for the merge drivers in tools/merge/.
//
// Git runs a driver command in the worktree being merged into, so a plain
// `node tools/merge/<driver>.mjs` runs the copy on the branch's own HEAD. A
// branch forked before a driver fix then merges main with the broken driver:
// on 2026-11-14 two branches forked before #958 merged main with the old
// string-table parser and lost text cut at a `{term}` placeholder.
//
// This launcher runs the newest copy instead. Candidates are HEAD (the
// working tree, so local edits to tools/merge count), the commit being merged
// in (GITHEAD_<sha>, or REBASE_HEAD/CHERRY_PICK_HEAD) and origin/main. For each it
// takes the last commit that touched tools/merge; the candidate whose commit
// has every other one as an ancestor wins. When the histories diverge it uses
// the working tree.
//
//   node tools/merge/run-driver.mjs <driver.mjs> %O %A %B %P

import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
const MERGE_DIR = "tools/merge";

function git(args) {
  const result = spawnSync("git", args, { windowsHide: true, encoding: "utf8", maxBuffer: 64 * 1024 * 1024 });
  if (result.error || result.status !== 0) return null;
  return result.stdout;
}

function lastDriverCommit(ref) {
  const out = git(["log", "-1", "--format=%H", ref, "--", MERGE_DIR]);
  return out ? out.trim() || null : null;
}

function isAncestor(older, newer) {
  if (older === newer) return true;
  const result = spawnSync("git", ["merge-base", "--is-ancestor", older, newer], { windowsHide: true });
  return result.status === 0;
}

// Returns the commit whose tools/merge copy should run, or null for the working tree.
export function pickDriverCommit(candidates, ancestor = isAncestor) {
  const head = candidates.find((candidate) => candidate.ref === "HEAD");
  const known = candidates.filter((candidate) => candidate.commit);
  if (!known.length) return null;
  const newest = known.find((candidate) => known.every((other) => ancestor(other.commit, candidate.commit)));
  if (!newest) return null;
  if (head && head.commit && newest.commit === head.commit) return null;
  return newest;
}

function extractDriverDir(ref) {
  const listing = git(["ls-tree", "--name-only", `${ref}:${MERGE_DIR}`]);
  if (!listing) return null;
  const root = mkdtempSync(join(tmpdir(), "lf-merge-driver-"));
  const dir = join(root, ...MERGE_DIR.split("/"));
  mkdirSync(dir, { recursive: true });
  for (const name of listing.split("\n").filter((line) => line.endsWith(".mjs"))) {
    const body = git(["show", `${ref}:${MERGE_DIR}/${name}`]);
    if (body === null) {
      rmSync(root, { recursive: true, force: true });
      return null;
    }
    writeFileSync(join(dir, name), body);
  }
  return { root, dir };
}

function main() {
  const [driver, ...args] = process.argv.slice(2);
  if (!driver || !/^[a-z0-9-]+\.mjs$/.test(driver) || args.length < 3) {
    console.error("usage: run-driver.mjs <driver.mjs> <base> <ours> <theirs> [path]");
    process.exit(2);
  }
  // `git merge` writes MERGE_HEAD only after the drivers ran; it names each
  // merged head in a GITHEAD_<sha> variable instead.
  const merged = Object.keys(process.env)
    .map((key) => /^GITHEAD_([0-9a-f]{40,64})$/i.exec(key))
    .filter(Boolean)
    .map((match) => match[1]);
  const pending = ["MERGE_HEAD", "REBASE_HEAD", "CHERRY_PICK_HEAD"].filter(
    (ref) => git(["rev-parse", "-q", "--verify", ref]) !== null,
  );
  const refs = ["HEAD", ...merged, ...pending, "origin/main"];
  const candidates = refs.map((ref) => ({ ref, commit: lastDriverCommit(ref) }));
  const pick = process.env.LF_MERGE_DRIVER_LOCAL === "1" ? null : pickDriverCommit(candidates);

  let script = join(HERE, driver);
  let extracted = null;
  if (pick) {
    extracted = extractDriverDir(pick.commit);
    if (extracted && existsSync(join(extracted.dir, driver))) {
      script = join(extracted.dir, driver);
      console.error(`lf-merge: ${driver} from ${pick.ref} (${pick.commit.slice(0, 8)}), newer than this branch's copy`);
    }
  }
  const result = spawnSync(process.execPath, [script, ...args], { stdio: "inherit", windowsHide: true });
  if (extracted) rmSync(extracted.root, { recursive: true, force: true });
  process.exit(result.status === null ? 1 : result.status);
}

const invoked = process.argv[1] && process.argv[1].replace(/\\/g, "/").endsWith("tools/merge/run-driver.mjs");
if (invoked) main();
