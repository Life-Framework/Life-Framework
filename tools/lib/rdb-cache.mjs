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
import { copyFileSync, existsSync, mkdirSync, readdirSync, readFileSync, renameSync, rmSync, statSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
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

// ---- A bad entry must not poison every worktree (#3586). 2026-11-19 two entries written by another worktree were
// corrupt: the server died right after "ResourceDB: loading cache" with no SUMMARY, and the main guard's recheck and
// bisect restored the same entries, so the red looked deterministic. take() leaves a marker in the worktree; a boot
// that ends with a SUMMARY confirms the rdb (confirmRestored), one that ends with none evicts the entry and the
// worktree's copy (evictRestored), so the next run refreshes for real.
const markerFile = (root) => join(root, "tmp", "rdb-from-cache.json");

function readMarker(root) {
  try {
    const m = JSON.parse(readFileSync(markerFile(root), "utf8"));
    return m && typeof m === "object" ? m : {};
  } catch {
    return {};
  }
}

function markRestored(root, addonName, entry) {
  try {
    const m = readMarker(root);
    m[addonName] = { ...entry, at: new Date().toISOString() };
    mkdirSync(dirname(markerFile(root)), { recursive: true });
    writeFileSync(markerFile(root), JSON.stringify(m));
  } catch {}
}

// A boot that printed a SUMMARY proves the restored rdb loads: forget the marker.
export function confirmRestored(root) {
  rmSync(markerFile(root), { force: true });
}

// A boot that ended with no SUMMARY after an rdb came from the shared cache: delete each such entry and the
// worktree's copy of it. -> the evicted entries ([{ addon, key }], empty when nothing was restored).
export function evictRestored(root, { log = console.log, why = "the boot ended with no SUMMARY" } = {}) {
  const m = readMarker(root);
  const out = [];
  for (const [addon, e] of Object.entries(m)) {
    if (e?.file) rmSync(e.file, { force: true });
    if (e?.addonDir) rmSync(join(e.addonDir, RDB_NAME), { force: true });
    out.push({ addon, key: e?.key ?? "" });
    log(`rdb cache: evicted ${String(e?.key ?? "").slice(0, 8)} [${addon}] (${why}); the next run refreshes the rdb`);
  }
  confirmRestored(root);
  return out;
}

// Should this finished boot evict? Only a run with no SUMMARY and no compile finding: a compile error is the
// branch's own fault and says nothing about the rdb.
export function bootDiedWithoutVerdict({ hasSummary, text = "" }) {
  if (hasSummary) return false;
  return !/Can't compile|does not compile|SCRIPT\s*\(E\)/i.test(String(text));
}

// The main guard's rechecks and bisect probes start from no rdb at all (the entry that made the first run red may
// be what sits in the checkout). Removes every addon's rdb under root.
export function dropAllRdbs(root) {
  const dropped = [];
  try {
    for (const d of readdirSync(join(root, "addons"))) {
      const f = join(root, "addons", d, RDB_NAME);
      if (existsSync(f)) {
        rmSync(f, { force: true });
        dropped.push(f);
      }
    }
  } catch {}
  confirmRestored(root);
  return dropped;
}

// A handle for one (addon, build): the key is computed once, so polling it is one existsSync.
export function open({ hub, addonName, addonDir, exe }) {
  if (!enabled()) return null;
  const key = cacheKey(addonDir, addonName, exeStamp(exe));
  const dir = cacheDir(hub, addonName);
  const file = join(dir, `${key}.rdb`);
  const root = dirname(dirname(addonDir)); // <root>/addons/<addon>

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
    // A restored entry is remembered in tmp/rdb-from-cache.json until a boot gives a verdict (see evictRestored).
    take() {
      if (!existsSync(file)) return false;
      try {
        swap(file, join(addonDir, RDB_NAME));
      } catch {
        return false;
      }
      if (rdbFreshness(addonDir).missing.length === 0) {
        markRestored(root, addonName, { file, key, addonDir });
        return true;
      }
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
        // A copy cut short (disk full, a second writer) is a corrupt entry for every worktree: drop it (#3586).
        if (statSync(file).size !== statSync(src).size) {
          rmSync(file, { force: true });
          return false;
        }
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
