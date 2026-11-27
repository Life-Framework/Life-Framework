// tools/lib/git-common-dir.mjs - `git rev-parse --git-common-dir` without starting git.
//
// Every tools\cli start and every selftest scratch repo asked git for the common dir once: 148 spawns in one
// `validate --all` (116 s of wall time under load, 0.6 s each; a git spawn costs 0.1 s alone and 5 times that on a
// busy box). Git finds it from files: a `.git` folder is the common dir; a `.git` file holds `gitdir: <path>`,
// and that folder's `commondir` file names the common dir relative to itself (a linked worktree). This reads those
// files, walking up from the folder the way git's own discovery does.
//
// It answers only when the answer is certain and returns null otherwise: git's own environment overrides
// (GIT_DIR, GIT_COMMON_DIR, GIT_WORK_TREE), a bare repository or a folder with no `.git` above it, an
// unreadable or odd `.git` file. The caller then asks git, so nothing changes for those.
import { readFileSync, statSync } from "node:fs";
import { dirname, join, resolve } from "node:path";

function commonFrom(gitDir) {
  try {
    const rel = readFileSync(join(gitDir, "commondir"), "utf8").trim();
    return rel ? resolve(gitDir, rel) : null;
  } catch (e) {
    if (e?.code === "ENOENT") return gitDir; // a plain repository: no commondir file
    return null;
  }
}

export function commonDirFromFiles(root) {
  if (process.env.GIT_DIR || process.env.GIT_COMMON_DIR || process.env.GIT_WORK_TREE) return null;
  let dir = resolve(root);
  for (;;) {
    const dotgit = join(dir, ".git");
    let st = null;
    try {
      st = statSync(dotgit);
    } catch {
      st = null;
    }
    if (st?.isDirectory()) return commonFrom(dotgit);
    if (st?.isFile()) {
      let text = "";
      try {
        text = readFileSync(dotgit, "utf8");
      } catch {
        return null;
      }
      const m = /^gitdir:\s*(.+?)\s*$/m.exec(text);
      return m ? commonFrom(resolve(dir, m[1])) : null;
    }
    const up = dirname(dir);
    if (up === dir) return null;
    dir = up;
  }
}
