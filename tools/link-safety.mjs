// tools/link-safety.mjs — delete a directory tree without following links.
//
// Worktrees hold junctions into the Steam installs
// (server/profile/<scope>/game-addons/{core,data}). Git for Windows 2.47
// `git worktree remove --force` follows a junction and empties its target: on
// 2026-11-07 it emptied the Arma Reforger, Server, and Tools addons. Every
// worktree removal goes through removeWorktree and every recursive delete
// through safeRemoveTree; both unlink every link first and refuse to continue
// while one remains.

import { existsSync, lstatSync, mkdirSync, mkdtempSync, readdirSync, rmdirSync, rmSync, symlinkSync, unlinkSync, writeFileSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { tmpdir } from "node:os";
import { join } from "node:path";

function isLink(path) {
  try {
    return lstatSync(path).isSymbolicLink();
  } catch {
    return false;
  }
}

// Remove the link itself. rmdir on a junction or directory symlink removes the
// reparse point and never touches the target; unlink covers file symlinks.
function unlinkLink(path) {
  try {
    rmdirSync(path);
  } catch {
    unlinkSync(path);
  }
  if (existsSync(path) || isLink(path)) throw new Error(`could not unlink ${path}`);
}

// Unlink every symlink and junction under root without descending into any of
// them. Returns the unlinked paths.
export function unlinkReparsePoints(root) {
  const removed = [];
  if (!existsSync(root) && !isLink(root)) return removed;
  if (isLink(root)) {
    unlinkLink(root);
    removed.push(root);
    return removed;
  }
  const stack = [root];
  while (stack.length) {
    const dir = stack.pop();
    let entries;
    try {
      entries = readdirSync(dir, { withFileTypes: true });
    } catch {
      continue;
    }
    for (const entry of entries) {
      const path = join(dir, entry.name);
      if (entry.isSymbolicLink() || isLink(path)) {
        unlinkLink(path);
        removed.push(path);
      } else if (entry.isDirectory()) {
        stack.push(path);
      }
    }
  }
  return removed;
}

export function findReparsePoints(root) {
  const found = [];
  if (!existsSync(root)) return found;
  const stack = [root];
  while (stack.length) {
    const dir = stack.pop();
    let entries;
    try {
      entries = readdirSync(dir, { withFileTypes: true });
    } catch {
      continue;
    }
    for (const entry of entries) {
      const path = join(dir, entry.name);
      if (entry.isSymbolicLink() || isLink(path)) found.push(path);
      else if (entry.isDirectory()) stack.push(path);
    }
  }
  return found;
}

// The only recursive delete allowed on a worktree or a generated profile.
export function safeRemoveTree(root) {
  unlinkReparsePoints(root);
  assertNoLinks(root);
  rmSync(root, { recursive: true, force: true });
}

function assertNoLinks(root) {
  const left = findReparsePoints(root);
  if (left.length) throw new Error(`refusing to delete ${root}: links remain (${left.join(", ")})`);
}

// The only way to remove a linked git worktree. Returns the git result.
export function removeWorktree(mainRoot, path, { force = false } = {}) {
  unlinkReparsePoints(path);
  assertNoLinks(path);
  return spawnSync("git", ["-C", mainRoot, "worktree", "remove", path, ...(force ? ["--force"] : [])], { windowsHide: true, encoding: "utf8" });
}

// Deletes a worktree that holds nothing worth keeping (a warm-pool tree, a
// half-made one): removeWorktree with --force, then, when git no longer knows
// the path, the leftover tree through safeRemoveTree. The only recycle path for
// warm worktrees (tools/wt.mjs). Returns true when the directory is gone.
export function recycleWorktree(mainRoot, path) {
  if (!existsSync(path)) return true;
  const rm = removeWorktree(mainRoot, path, { force: true });
  if (rm.status !== 0 && existsSync(path)) {
    const list = spawnSync("git", ["-C", mainRoot, "worktree", "list", "--porcelain"], { windowsHide: true, encoding: "utf8" }).stdout ?? "";
    const norm = (p) => p.replace(/[\\/]+/g, "/").replace(/\/$/, "").toLowerCase();
    const known = list.split(/\r?\n/).some((l) => l.startsWith("worktree ") && norm(l.slice(9).trim()) === norm(path));
    if (known) return false;
    safeRemoveTree(path);
  }
  return !existsSync(path);
}

// Builds a fake Steam install with a sentinel, a git worktree that junctions
// to it the way game-addons does, removes the worktree with --force, and checks
// the sentinel survived. Without the unlink, git deletes the sentinel.
export function selftestLinkSafety() {
  const failures = [];
  const base = mkdtempSync(join(tmpdir(), "lf-link-safety-"));
  const git = (args) => spawnSync("git", args, { windowsHide: true, cwd: base, encoding: "utf8" });
  try {
    const install = join(base, "steam", "addons", "core");
    mkdirSync(install, { recursive: true });
    const sentinel = join(install, "core.gproj");
    writeFileSync(sentinel, "sentinel");
    const repo = join(base, "repo");
    const worktree = join(base, "worktree");
    git(["init", "-q", repo]);
    git(["-C", repo, "-c", "user.name=t", "-c", "user.email=t@t", "commit", "-q", "--allow-empty", "-m", "init"]);
    if (git(["-C", repo, "worktree", "add", "-q", worktree, "-b", "fixture"]).status !== 0) throw new Error("git worktree add failed");
    const addons = join(worktree, "server", "profile", "test", "game-addons");
    mkdirSync(addons, { recursive: true });
    symlinkSync(install, join(addons, "core"), "junction");
    if (!existsSync(join(addons, "core", "core.gproj"))) failures.push("fixture junction did not resolve");

    const rm = removeWorktree(repo, worktree, { force: true });
    if (rm.status !== 0) failures.push(`git worktree remove failed: ${rm.stderr}`);
    if (existsSync(worktree)) failures.push("worktree was not deleted");
    if (!existsSync(sentinel)) failures.push("removing the worktree followed the junction and deleted the install sentinel");

    const tree = join(base, "tree");
    mkdirSync(tree);
    symlinkSync(install, join(tree, "core"), "junction");
    safeRemoveTree(tree);
    if (existsSync(tree)) failures.push("safeRemoveTree did not delete the tree");
    if (!existsSync(sentinel)) failures.push("safeRemoveTree followed the junction and deleted the install sentinel");

    // Warm-pool recycle (tools/wt.mjs discardWarm): a detached worktree that
    // somehow holds a game-addons junction, and a leftover tree git no longer
    // knows, both go through recycleWorktree and keep the install.
    const warm = join(base, "warm");
    if (git(["-C", repo, "worktree", "add", "-q", "--detach", warm]).status !== 0) throw new Error("git worktree add --detach failed");
    const warmAddons = join(warm, "server", "profile", "test", "game-addons");
    mkdirSync(warmAddons, { recursive: true });
    symlinkSync(install, join(warmAddons, "core"), "junction");
    if (!recycleWorktree(repo, warm)) failures.push("recycleWorktree did not delete the warm worktree");
    if (existsSync(warm)) failures.push("warm worktree still on disk after recycle");
    if (!existsSync(sentinel)) failures.push("recycling a warm worktree followed the junction and deleted the install sentinel");
    const orphan = join(base, "orphan");
    mkdirSync(join(orphan, "server", "profile", "test", "game-addons"), { recursive: true });
    symlinkSync(install, join(orphan, "server", "profile", "test", "game-addons", "core"), "junction");
    if (!recycleWorktree(repo, orphan)) failures.push("recycleWorktree did not delete a tree git does not know");
    if (!existsSync(sentinel)) failures.push("recycling an unknown tree followed the junction and deleted the install sentinel");
  } catch (e) {
    failures.push(`threw: ${e.message}`);
  } finally {
    unlinkReparsePoints(base);
    rmSync(base, { recursive: true, force: true });
  }
  return failures;
}
