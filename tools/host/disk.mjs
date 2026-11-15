// Host disk report (#1407). Read only: sizes the save and the backups
// (tools/host/host.mjs `backupSaves`, tools/host/rotate.mjs) and says how many
// more sessions fit before the drive drops under the 5 GB floor. Never deletes.

import { existsSync, readdirSync, statfsSync, statSync } from "node:fs";
import { join } from "node:path";
import { hostPaths, saveRoot } from "./host.mjs";

export const FLOOR_BYTES = 5 * 1024 ** 3;
export const GROWTH_WINDOW = 5;

// Total bytes under a path (a file or a folder tree). Symlinks are not followed.
export function treeBytes(path) {
  let st;
  try {
    st = statSync(path);
  } catch {
    return 0;
  }
  if (!st.isDirectory()) return st.size;
  let sum = 0;
  let entries = [];
  try {
    entries = readdirSync(path, { withFileTypes: true });
  } catch {
    return 0;
  }
  for (const e of entries) {
    if (e.isSymbolicLink()) continue;
    sum += treeBytes(join(path, e.name));
  }
  return sum;
}

// Free bytes on the drive holding `path` (its nearest existing parent).
export function freeBytesOf(path) {
  let p = path;
  while (!existsSync(p)) {
    const up = join(p, "..");
    if (up === p) break;
    p = up;
  }
  const s = statfsSync(p);
  return Number(s.bavail) * Number(s.bsize);
}

// { saveBytes, backupsBytes, backupCount, growthBytes, growthPerBackup,
//   newestBackupBytes, freeBytes, sessionsFit, low }
// Backup folders sort by name (the stamp sorts by time). Growth is the size of
// the newest backup minus the oldest of the last five, and per backup over the
// steps between them. A session writes a start and a stop backup, each about
// the size of the newest one.
export function diskReport(dir, { freeBytes } = {}) {
  const { profile, backups } = hostPaths(dir);
  const root = saveRoot(profile);
  const saveBytes = root ? treeBytes(root) : 0;

  const names = existsSync(backups)
    ? readdirSync(backups).filter((n) => statSync(join(backups, n)).isDirectory()).sort()
    : [];
  const sizes = names.map((n) => treeBytes(join(backups, n)));
  const backupsBytes = sizes.reduce((a, b) => a + b, 0);
  const last = sizes.slice(-GROWTH_WINDOW);
  const growthBytes = last.length > 1 ? last[last.length - 1] - last[0] : 0;
  const growthPerBackup = last.length > 1 ? growthBytes / (last.length - 1) : 0;
  const newestBackupBytes = sizes.length ? sizes[sizes.length - 1] : saveBytes;

  const free = freeBytes ?? freeBytesOf(dir);
  const perSession = 2 * newestBackupBytes;
  const headroom = free - FLOOR_BYTES;
  const sessionsFit = headroom <= 0 ? 0 : perSession > 0 ? Math.floor(headroom / perSession) : Infinity;
  return {
    saveBytes,
    backupsBytes,
    backupCount: names.length,
    growthBytes,
    growthPerBackup,
    newestBackupBytes,
    freeBytes: free,
    sessionsFit,
    low: free < FLOOR_BYTES,
  };
}

export function fmtBytes(n) {
  const u = ["B", "KB", "MB", "GB", "TB"];
  let i = 0;
  let v = Math.abs(n);
  while (v >= 1024 && i < u.length - 1) {
    v /= 1024;
    i++;
  }
  return `${n < 0 ? "-" : ""}${v.toFixed(i ? 1 : 0)} ${u[i]}`;
}
