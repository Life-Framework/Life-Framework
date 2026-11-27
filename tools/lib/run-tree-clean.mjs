// The main guard's own run worktree (Life-Framework-ws-main-guard-run) is moved by detached checkouts, and the pre-move
// e2e checks out an older ref inside it. When the worker dies mid-e2e (2026-11-27 13:11 UTC) that old checkout leaves
// untracked files under addons/ (Assets/Buildings/, Props/Roleplay/BankVault/); the next pre-move e2e then refuses with
// "needs a clean tree" and the guard called it red twice (#5117). This drops them before a checkout and before the
// pre-move step. Never `git clean -fdx`: the worktree's server/profile/*/game-addons are junctions into the Steam
// installs, so the paths git lists go through safeRemoveTree, which unlinks a junction instead of following it
// (cleanRunFolder, tools/link-safety.mjs). Scope is addons/ of this one directory; the rdb files are kept.
import { spawnSync } from "node:child_process";
import { join } from "node:path";
import { cleanRunFolder, safeRemoveTree } from "../link-safety.mjs";

// Returns the removed paths (relative to dir), or [] when `dir` is not a git checkout. Throws when git cannot list or a
// path survives; the caller logs it and goes on, the e2e's own clean-tree check then speaks.
export function cleanRunTree(dir) {
  return cleanRunFolder(dir, { spawnSync, join, remove: safeRemoveTree });
}
