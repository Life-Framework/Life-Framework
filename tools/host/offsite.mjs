// Offsite copy of the host backups (#1455). `host offsite <dir>` copies every
// backups/<stamp>-<reason> folder that is not already in <dir> (another drive or a NAS
// share), checks the copy by file list, size and sha256, and replaces a copy that does
// not match. The source is never written to. The target follows the host rotation
// (rotate.mjs): only the newest `keep` plain start/stop backups are copied and kept
// there; a tagged backup (preflight, wipe, crash, ...) is always copied and kept.

import { closeSync, existsSync, lstatSync, mkdirSync, openSync, readSync, readdirSync, readlinkSync, renameSync, cpSync } from "node:fs";
import { createHash } from "node:crypto";
import { join, relative, resolve, sep } from "node:path";
import { safeRemoveTree } from "../link-safety.mjs";
import { DEFAULT_KEEP, isPlainBackup, pruneBackups } from "./rotate.mjs";

// Any folder host.mjs `backupSaves` can have written: <yyyy-mm-dd_hh-mm-ss>-<reason>[-N].
const STAMPED = /^\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}-.+$/;
const PARTIAL = ".partial";

function isBackupDir(root, name) {
  if (!STAMPED.test(name) || name.endsWith(PARTIAL)) return false;
  try {
    return lstatSync(join(root, name)).isDirectory();
  } catch {
    return false;
  }
}

function sha256(file) {
  const h = createHash("sha256");
  const buf = Buffer.allocUnsafe(1 << 20);
  const fd = openSync(file, "r");
  try {
    for (;;) {
      const n = readSync(fd, buf, 0, buf.length, null);
      if (n <= 0) break;
      h.update(buf.subarray(0, n));
    }
  } finally {
    closeSync(fd);
  }
  return h.digest("hex");
}

// Every entry under `dir` as "<relative path>": "<size>:<sha256>" (a link as its target).
function manifest(dir) {
  const out = new Map();
  const walk = (d) => {
    for (const e of readdirSync(d, { withFileTypes: true })) {
      const p = join(d, e.name);
      const rel = relative(dir, p).split(sep).join("/");
      const st = lstatSync(p);
      if (st.isSymbolicLink()) out.set(rel, `link:${readlinkSync(p)}`);
      else if (st.isDirectory()) {
        out.set(rel + "/", "dir");
        walk(p);
      } else out.set(rel, `${st.size}:${sha256(p)}`);
    }
  };
  walk(dir);
  return out;
}

// null when `copy` matches `orig`, else the first difference.
function diff(orig, copy) {
  let a, b;
  try {
    a = manifest(orig);
    b = manifest(copy);
  } catch (e) {
    return `unreadable (${e.message})`;
  }
  for (const [k, v] of a) {
    if (!b.has(k)) return `missing ${k}`;
    if (b.get(k) !== v) return `differs ${k}`;
  }
  for (const k of b.keys()) if (!a.has(k)) return `extra ${k}`;
  return null;
}

function inside(parent, child) {
  const p = resolve(parent).toLowerCase();
  const c = resolve(child).toLowerCase();
  return c === p || c.startsWith(p.endsWith(sep) ? p : p + sep);
}

// Copies what the target lacks or holds damaged. Returns
// { copied, replaced, skipped, pruned, failed: [{ name, why }] } (names). `dryRun` writes nothing.
export function offsiteBackups(backups, target, { keep = DEFAULT_KEEP, dryRun = false } = {}) {
  if (!Number.isInteger(keep) || keep < 0) throw new Error(`--keep must be a whole number >= 0 (got ${keep})`);
  if (inside(backups, target)) throw new Error(`target ${target} is the backups folder or inside it`);
  const res = { copied: [], replaced: [], skipped: [], pruned: [], failed: [] };
  if (!existsSync(backups)) return res;

  const names = readdirSync(backups).filter((n) => isBackupDir(backups, n)).sort();
  const plain = names.filter(isPlainBackup);
  const wanted = new Set(names.filter((n) => !isPlainBackup(n)));
  for (const n of plain.slice(Math.max(0, plain.length - keep))) wanted.add(n);

  if (!dryRun) mkdirSync(target, { recursive: true });
  for (const name of [...wanted].sort()) {
    const from = join(backups, name);
    const to = join(target, name);
    const had = existsSync(to);
    if (had && diff(from, to) === null) {
      res.skipped.push(name);
      continue;
    }
    if (dryRun) {
      (had ? res.replaced : res.copied).push(name);
      continue;
    }
    const tmp = to + PARTIAL;
    try {
      if (existsSync(tmp)) safeRemoveTree(tmp);
      cpSync(from, tmp, { recursive: true, verbatimSymlinks: true });
      const why = diff(from, tmp);
      if (why) throw new Error(`verify failed: ${why}`);
      if (had) safeRemoveTree(to);
      renameSync(tmp, to);
      (had ? res.replaced : res.copied).push(name);
    } catch (e) {
      res.failed.push({ name, why: e.message });
      try {
        if (existsSync(tmp)) safeRemoveTree(tmp);
      } catch { /* the next run removes it */ }
    }
  }

  // The target's rotation: its plain backups beyond `keep`, never a tagged one.
  if (dryRun) {
    const there = existsSync(target) ? readdirSync(target).filter((n) => isBackupDir(target, n) && isPlainBackup(n)) : [];
    const all = [...new Set([...there, ...res.copied.filter(isPlainBackup)])].sort();
    res.pruned = all.slice(0, Math.max(0, all.length - keep));
  } else if (res.failed.length === 0) {
    res.pruned = pruneBackups(target, keep).deleted;
  }
  return res;
}
