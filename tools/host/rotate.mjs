// Host backup rotation (#1260). `backupSaves` (tools/host/host.mjs) writes
// backups/<stamp>-<reason>[-N]. Only the plain ones (reason start, stop or restart) are
// rotated; a scheduled restart (06:00, 18:00) would otherwise grow without limit (#1560).
// A preflight, wipe, crash, replaced-by-restore or manual backup, and any folder this
// does not recognise, is never deleted.

import { existsSync, readdirSync, statSync } from "node:fs";
import { join } from "node:path";
import { safeRemoveTree } from "../link-safety.mjs";

export const DEFAULT_KEEP = 20;

// <yyyy-mm-dd_hh-mm-ss>-(start|stop|restart)[-<n>]; the stamp sorts by time.
const PLAIN = /^\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}-(start|stop|restart)(-\d+)?$/;

export function isPlainBackup(name) {
  return PLAIN.test(name);
}

// What a prune would delete, oldest plain backups beyond `keep`. Read-only.
export function planPrune(backups, keep = DEFAULT_KEEP) {
  if (!Number.isInteger(keep) || keep < 0) throw new Error(`--keep must be a whole number >= 0 (got ${keep})`);
  if (!existsSync(backups)) return { keep: [], delete: [] };
  const plain = readdirSync(backups)
    .filter((n) => isPlainBackup(n) && statSync(join(backups, n)).isDirectory())
    .sort();
  const cut = Math.max(0, plain.length - keep);
  return { keep: plain.slice(cut), delete: plain.slice(0, cut) };
}

// Deletes the planned folders through safeRemoveTree. Returns { deleted, kept }.
export function pruneBackups(backups, keep = DEFAULT_KEEP, { dryRun = false } = {}) {
  const plan = planPrune(backups, keep);
  if (!dryRun) for (const n of plan.delete) safeRemoveTree(join(backups, n));
  return { deleted: plan.delete, kept: plan.keep };
}
