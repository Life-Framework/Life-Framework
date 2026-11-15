// Host log archive for `tools\cli host logs archive|list` (#1456).
//
// The engine writes each session's logs to <host>/profile/logs/logs_<stamp>/ (console.log and
// friends). A finished session's folder moves to <host>/logs/<stamp>/ so the profile stays small;
// the newest 30 archived sessions are kept. The session still being written is never touched:
// the newest folder is live while the host process runs or while any file in it changed within
// the last LIVE_QUIET_MS (a host started by hand, a crash that left no pid). Pure functions with
// the paths passed in; the command (commands/logs.mjs) supplies the host dir.

import { cpSync, existsSync, mkdirSync, readdirSync, readFileSync, renameSync, statSync } from "node:fs";
import { join } from "node:path";
import { safeRemoveTree } from "../link-safety.mjs";
import { loadTriageRows, triageLog } from "../lib/log-triage.mjs";
import { freshDir } from "./host.mjs";

export const ARCHIVE_KEEP = 30;
// A folder with a file written this recently counts as still open when no host pid vouches for it.
export const LIVE_QUIET_MS = 120000;

const FOLDER = /^logs_(\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2})$/;
const ARCHIVED = /^\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}(-\d+)?$/;

/** The session stamp of an engine log folder name (logs_<stamp>), or null for any other name. */
export function sessionStamp(folderName) {
  return FOLDER.exec(folderName)?.[1] ?? null;
}

// The newest modification time of a folder and the files directly in it.
function newestMtime(dir) {
  let t = statSync(dir).mtimeMs;
  for (const e of readdirSync(dir, { withFileTypes: true })) {
    if (e.isFile()) t = Math.max(t, statSync(join(dir, e.name)).mtimeMs);
  }
  return t;
}

/**
 * Which engine log folders are finished. { live, finished }: `live` is the newest folder when the
 * host runs or it changed within quietMs (else null); `finished` is every other logs_<stamp> folder
 * in name order. Read-only.
 */
export function planArchive(profile, { hostRunning = false, now = Date.now(), quietMs = LIVE_QUIET_MS } = {}) {
  const logsDir = join(profile, "logs");
  if (!existsSync(logsDir)) return { live: null, finished: [] };
  const dirs = readdirSync(logsDir, { withFileTypes: true })
    .filter((e) => e.isDirectory() && !e.isSymbolicLink() && FOLDER.test(e.name))
    .map((e) => e.name)
    .sort();
  if (!dirs.length) return { live: null, finished: [] };
  const newest = dirs[dirs.length - 1];
  const open = hostRunning || now - newestMtime(join(logsDir, newest)) < quietMs;
  return open ? { live: newest, finished: dirs.slice(0, -1) } : { live: null, finished: dirs };
}

// Rename into place; across volumes (EXDEV) copy then remove the source.
function moveDir(from, to) {
  try {
    renameSync(from, to);
  } catch (e) {
    if (e.code !== "EXDEV") throw e;
    cpSync(from, to, { recursive: true });
    safeRemoveTree(from);
  }
}

/**
 * Archive finished sessions: { archived: [<dest name>], live, pruned: [<name>], failed: [{ name, error }] }.
 * A folder that cannot be moved (a file still open on Windows) stays in the profile and is
 * reported in `failed`; the rest still move. Retention (`keep`) runs after the move.
 */
export function archiveLogs(profile, archiveDir, { keep = ARCHIVE_KEEP, hostRunning = false, now = Date.now(), quietMs = LIVE_QUIET_MS, dryRun = false } = {}) {
  checkKeep(keep);
  const plan = planArchive(profile, { hostRunning, now, quietMs });
  const archived = [];
  const failed = [];
  for (const name of plan.finished) {
    const stamp = sessionStamp(name);
    if (dryRun) {
      archived.push(stamp);
      continue;
    }
    try {
      mkdirSync(archiveDir, { recursive: true });
      const dest = freshDir(join(archiveDir, stamp));
      moveDir(join(profile, "logs", name), dest);
      archived.push(dest.slice(archiveDir.length + 1));
    } catch (e) {
      failed.push({ name, error: e.message });
    }
  }
  const pruned = pruneArchive(archiveDir, keep, { dryRun }).deleted;
  return { archived, live: plan.live, pruned, failed };
}

function checkKeep(keep) {
  if (!Number.isInteger(keep) || keep < 1) throw new Error(`keep must be a whole number of 1 or more, got ${keep}`);
}

/** The archived session folders (<stamp> or <stamp>-N), oldest first. Other names are never listed. */
export function archivedSessions(archiveDir) {
  if (!existsSync(archiveDir)) return [];
  return readdirSync(archiveDir, { withFileTypes: true })
    .filter((e) => e.isDirectory() && !e.isSymbolicLink() && ARCHIVED.test(e.name))
    .map((e) => e.name)
    .sort();
}

/** Delete the oldest archived sessions beyond `keep`. { deleted, kept }. */
export function pruneArchive(archiveDir, keep = ARCHIVE_KEEP, { dryRun = false } = {}) {
  checkKeep(keep);
  const all = archivedSessions(archiveDir);
  const deleted = all.slice(0, Math.max(0, all.length - keep));
  if (!dryRun) for (const n of deleted) safeRemoveTree(join(archiveDir, n));
  return { deleted, kept: all.slice(deleted.length) };
}

function folderBytes(dir) {
  let sum = 0;
  for (const e of readdirSync(dir, { withFileTypes: true })) {
    const p = join(dir, e.name);
    if (e.isSymbolicLink()) continue;
    sum += e.isDirectory() ? folderBytes(p) : statSync(p).size;
  }
  return sum;
}

/**
 * Archived sessions newest first: [{ name, bytes, errors, groups, newGroups }]. `errors` is the
 * number of lines `host triage` counts in the session's console.log (errors, VMEs, [LFDebug]
 * warnings); `newGroups` those with no docs/debug-log-triage.md row. A session without a
 * console.log reports errors null.
 */
export function listArchive(archiveDir, triageFile) {
  const rows = loadTriageRows(triageFile);
  return archivedSessions(archiveDir)
    .reverse()
    .map((name) => {
      const dir = join(archiveDir, name);
      const log = join(dir, "console.log");
      const out = { name, bytes: folderBytes(dir), errors: null, groups: null, newGroups: null };
      if (existsSync(log)) {
        const { total, groups } = triageLog(readFileSync(log, "utf8"), rows);
        out.errors = total;
        out.groups = groups.length;
        out.newGroups = groups.filter((g) => !g.known).length;
      }
      return out;
    });
}
