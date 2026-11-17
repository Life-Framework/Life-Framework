// tools\cli world-sync [--wt <slug>] [--dry-run] [--force] [--watch-hint] [--unchecked]
// Pull origin/main into the maintainer's world-editing worktree without touching his edits.
// The engine is tools/lib/world-sync.mjs; docs/worktrees.md "Maintainer world edits".
// (When tools/commands/world.mjs lands, its `// sync:` slot can call syncWorktree directly and this entry can go.)
export const help = "world-sync [--wt <slug>] [--dry-run] [--force] [--watch-hint] [--unchecked]   bring the maintainer's world worktree level with the newest compiled-ok commit of origin/main, keeping his edits; .layer files both sides changed are held; one SUMMARY line (exit 0 ok, 2 blocked, 1 error with state restored)";

import { existsSync } from "node:fs";
import { DEFAULT_SLUG, syncWorktree, watchHint, worktreeDirFor } from "../lib/world-sync.mjs";
import { hubDirOf, mainRootOf } from "../wt.mjs";
import { spawnSync } from "node:child_process";
import { resolveChecked } from "../lib/guard-pin.mjs";

// The commit to sync to: the newest one the main guard marked compiled-ok or green (never an unchecked head, so
// the maintainer's Workbench never opens a broken compile). { sha, behind, head, status } or null.
export function pinFor(dir, { hub, resolve = resolveChecked } = {}) {
  spawnSync("git", ["-C", dir, "fetch", "--quiet", "origin", "main"], { windowsHide: true, timeout: 180000 });
  return resolve(dir, { hub });
}

export function parseArgs(args) {
  const o = { slug: DEFAULT_SLUG, dryRun: false, force: false, watchHint: false, unchecked: false };
  for (let i = 0; i < args.length; i++) {
    const a = args[i];
    if (a === "--wt") {
      o.slug = args[++i];
      if (!o.slug || o.slug.startsWith("-")) return { error: "--wt needs a worktree slug" };
    } else if (a === "--dir") {
      o.dir = args[++i];
      if (!o.dir) return { error: "--dir needs a path" };
    } else if (a === "--dry-run") o.dryRun = true;
    else if (a === "--force") o.force = true;
    else if (a === "--watch-hint") o.watchHint = true;
    else if (a === "--unchecked") o.unchecked = true;
    else return { error: `unknown argument '${a}'` };
  }
  return o;
}

export async function main(args, { root }) {
  const o = parseArgs(args);
  if (o.error) {
    console.log(`${o.error}\nusage: tools\\cli world-sync [--wt <slug>] [--dry-run] [--force] [--watch-hint] [--unchecked]`);
    return 1;
  }
  if (o.watchHint) {
    console.log(watchHint(o.slug));
    return 0;
  }
  const dir = o.dir ?? worktreeDirFor(mainRootOf(root), o.slug);
  if (!existsSync(dir)) {
    console.log(`world-sync: no worktree at ${dir} (this runs on the PC that has ws/${o.slug}).`);
    console.log("SUMMARY world-sync error state-restored=no worktree not found");
    return 1;
  }
  let target = null;
  if (!o.unchecked) {
    const p = pinFor(dir, { hub: hubDirOf(mainRootOf(root)) });
    if (!p) {
      console.log("world-sync: no commit on origin/main is marked compiled-ok by the main guard yet; nothing pulled (--unchecked pulls the raw head)");
      console.log("SUMMARY world-sync blocked pulled=0 files held=0 blocked=0 (no compiled-ok commit) reload-workbench=no");
      return 2;
    }
    target = p.sha;
    console.log(`world-sync: syncing to ${p.sha.slice(0, 8)} (${p.status}), ${p.behind} commit(s) behind the origin/main head ${String(p.head).slice(0, 8)}`);
  }
  const r = syncWorktree(dir, { dryRun: o.dryRun, force: o.force, target, log: (l) => console.log(`world-sync: ${l}`) });
  if (r.status === "error") console.log(`world-sync: ${r.message}`);
  console.log(r.summary);
  return r.code;
}
