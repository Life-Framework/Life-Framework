// tools/lib/rdb-cache.mjs - share a refreshed resourceDatabase.rdb between worktrees.
//
// A Workbench rdb refresh costs ~28 s of run but, with the hub-wide build lock and the Workbench slot,
// queued 192 s on average (max 758 s) on lf-testbox, and most queued refreshes were slice worktrees
// from the same main wanting the same resources. The rdb holds no absolute paths, so one produced for an
// identical resource set is valid for every worktree with that set.
// key = sha1(addon name, sorted `path\0guid` of every .meta, Workbench exe size:mtime). Entries live in
// <hub>/rdb-cache/<addon>/<key>.rdb, at most MAX_ENTRIES per addon. LF_RDB_CACHE=0 turns it off.
// A hit is re-checked with rdbFreshness; a stale entry is deleted. Selftest: validation/rdb-cache-selftest.mjs.

import { createHash } from "node:crypto";
import { copyFileSync, existsSync, mkdirSync, readdirSync, renameSync, rmSync, statSync } from "node:fs";
import { join } from "node:path";
import { addonResources, rdbFreshness, RDB_NAME } from "./rdb-fresh.mjs";

export const MAX_ENTRIES = 40;

export const enabled = () => process.env.LF_RDB_CACHE !== "0";

// exeStamp: "size:mtimeMs" of the Workbench exe (the game build), "none" when absent.
export function exeStamp(exe) {
  try {
    const s = statSync(exe);
    return `${s.size}:${Math.round(s.mtimeMs)}`;
  } catch {
    return "none";
  }
}

export function cacheKey(addonDir, addonName, stamp) {
  const lines = addonResources(addonDir)
    .map((r) => `${r.path}\0${r.guid ?? ""}`)
    .sort();
  return createHash("sha1").update([addonName, stamp, ...lines].join("\n")).digest("hex");
}

export function cacheDir(hub, addonName) {
  return join(hub, "rdb-cache", addonName);
}

// A handle for one (addon, build): the key is computed once, so polling it is one existsSync.
export function open({ hub, addonName, addonDir, exe }) {
  if (!enabled()) return null;
  const key = cacheKey(addonDir, addonName, exeStamp(exe));
  const dir = cacheDir(hub, addonName);
  const file = join(dir, `${key}.rdb`);
  const swap = (from, to) => {
    const tmp = `${to}.tmp-${process.pid}`;
    copyFileSync(from, tmp);
    renameSync(tmp, to);
  };
  return {
    key,
    file,
    has: () => existsSync(file),
    // Copy the entry over the worktree's rdb. true when the rdb is then fresh; a stale entry is deleted.
    take() {
      if (!existsSync(file)) return false;
      try {
        swap(file, join(addonDir, RDB_NAME));
      } catch {
        return false;
      }
      if (rdbFreshness(addonDir).missing.length === 0) return true;
      rmSync(file, { force: true });
      return false;
    },
    // Keep the worktree's rdb when it is fresh; prune the oldest beyond MAX_ENTRIES.
    store() {
      const src = join(addonDir, RDB_NAME);
      if (!existsSync(src) || rdbFreshness(addonDir).missing.length !== 0) return false;
      try {
        mkdirSync(dir, { recursive: true });
        swap(src, file);
        const all = readdirSync(dir)
          .filter((n) => n.endsWith(".rdb"))
          .map((n) => ({ n, t: statSync(join(dir, n)).mtimeMs }))
          .sort((a, b) => b.t - a.t);
        for (const o of all.slice(MAX_ENTRIES)) rmSync(join(dir, o.n), { force: true });
        return true;
      } catch {
        return false;
      }
    },
  };
}
