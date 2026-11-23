// The three sides of the merge a driver is running in, and one file's text at each (#4668).
//
// A driver gets only the file it merges (%O %A %B), but the credits rows depend on docs/credits.md, which
// git merges AFTER the string table and LF_CreditsData.c (path order), so the work tree still holds the old
// copy. This reads the file at the base, ours and theirs commits instead. It uses plain bounded git calls and
// imports nothing outside tools/merge (run-driver.mjs extracts only that folder); regen-generated.mjs has
// its own copy of the side lookup for the language confs, with its retry and trace plumbing.
import { spawnSync } from "node:child_process";
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";

const GIT_TIMEOUT_MS = 20_000;

function git(args) {
  const limit = Number(process.env.LF_MERGE_DRIVER_TIMEOUT_MS) > 0 ? Number(process.env.LF_MERGE_DRIVER_TIMEOUT_MS) : GIT_TIMEOUT_MS;
  const result = spawnSync("git", args, {
    windowsHide: true,
    encoding: "utf8",
    maxBuffer: 64 * 1024 * 1024,
    stdio: ["ignore", "pipe", "pipe"],
    timeout: limit,
    killSignal: "SIGKILL",
  });
  if (result.error || result.status !== 0) return null;
  return result.stdout;
}

function gitPath(rel) {
  const out = git(["rev-parse", "--git-path", rel]);
  return out ? out.trim() : null;
}

// { ours, theirs, base } commit ids, or null outside a merge, rebase or cherry-pick.
export function resolveMergeSides() {
  const head = git(["rev-parse", "HEAD"]);
  if (!head) return null;
  const ours = head.trim();
  let theirs = null;
  let baseFromParent = false;
  const rebase = gitPath("rebase-merge");
  if (rebase && existsSync(join(rebase, "done"))) {
    const picks = [...readFileSync(join(rebase, "done"), "utf8").matchAll(/^pick\s+([0-9a-f]{7,40})/gim)];
    if (picks.length) {
      theirs = picks[picks.length - 1][1];
      baseFromParent = true;
    }
  }
  if (!theirs) {
    const apply = gitPath("rebase-apply");
    if (apply && existsSync(join(apply, "original-commit"))) {
      theirs = readFileSync(join(apply, "original-commit"), "utf8").trim();
      baseFromParent = true;
    }
  }
  if (!theirs) {
    for (const key of Object.keys(process.env)) {
      const match = /^GITHEAD_([0-9a-f]{40})$/i.exec(key);
      if (match) {
        theirs = match[1];
        break;
      }
    }
  }
  if (!theirs) {
    for (const name of ["MERGE_HEAD", "CHERRY_PICK_HEAD"]) {
      const path = gitPath(name);
      if (path && existsSync(path)) {
        const text = readFileSync(path, "utf8").trim();
        if (text) {
          theirs = text;
          if (name === "CHERRY_PICK_HEAD") baseFromParent = true;
          break;
        }
      }
    }
  }
  if (!theirs) return null;
  const base = baseFromParent ? git(["rev-parse", `${theirs}^`]) : git(["merge-base", ours, theirs]);
  if (!base) return null;
  return { ours, theirs, base: base.trim() };
}

// A file's text at a commit, or null when it is not there.
export function showAt(rev, path) {
  return git(["-c", "core.autocrlf=false", "show", `${rev}:${path}`]);
}
