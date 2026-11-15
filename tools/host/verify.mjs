// Host backup verification (#1454): the cheap check behind `tools\cli host verify`.
// Opens each backups/<name> folder (written by `backupSaves`, tools/host/host.mjs) and
// checks it is not empty, holds a save point a restore can use (a folder with the engine's
// meta-info.json plus at least one other file), and that the build stamp (m_sGameVersion,
// #980) reads. A good backup gets a checksum recorded beside it, in
// backups/<name>.sha256.json (never inside the folder: a restore copies the folder, and
// rotation only looks at folders). A later run recomputes it and flags a backup that changed.
// Read only: it never deletes, edits or moves a backup. `host drill` (#1261) is the full
// boot test; this is the one that runs in a second.

import { createHash } from "node:crypto";
import { existsSync, readdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { findSavePoints, readSaveMeta } from "./host.mjs";

export const CHECKSUM_SUFFIX = ".sha256.json";

// Every file under a folder as { rel, abs, size }, sorted by rel. Symlinks are skipped.
function listFiles(root) {
  const out = [];
  const walk = (dir, rel) => {
    for (const e of readdirSync(dir, { withFileTypes: true })) {
      if (e.isSymbolicLink()) continue;
      const abs = join(dir, e.name);
      const r = rel ? `${rel}/${e.name}` : e.name;
      if (e.isDirectory()) walk(abs, r);
      else if (e.isFile()) out.push({ rel: r, abs, size: statSync(abs).size });
    }
  };
  walk(root, "");
  return out.sort((a, b) => (a.rel < b.rel ? -1 : a.rel > b.rel ? 1 : 0));
}

// A save point holds data when something besides meta-info.json beside it has a byte in it.
// The engine's WorldState entry can be a folder, so look inside folders too (#1636).
function hasSaveData(savePointDir) {
  return listFiles(savePointDir).some((f) => f.rel !== "meta-info.json" && f.size > 0);
}

// sha256 over every file's relative path, size and bytes, in path order.
export function checksumFiles(files) {
  const h = createHash("sha256");
  for (const f of files) {
    h.update(`${f.rel}\0${f.size}\0`);
    h.update(readFileSync(f.abs));
  }
  return h.digest("hex");
}

// One backup: { name, status, detail, ... }. status is ok, empty, nosave, nostamp, changed
// or error. A checksum is recorded only for a backup that passed every other check.
export function verifyBackup(backups, name) {
  const dir = join(backups, name);
  const sumFile = join(backups, name + CHECKSUM_SUFFIX);
  try {
    const files = listFiles(dir);
    if (!files.some((f) => f.size > 0)) return { name, status: "empty", detail: files.length ? "every file is zero bytes" : "no files" };
    const points = findSavePoints(dir).filter((p) => hasSaveData(p.dir));
    if (!points.length) return { name, status: "nosave", detail: "no save point (meta-info.json with save data beside it): a restore has nothing to load" };
    const meta = readSaveMeta(points[0].dir);
    const build = meta && typeof meta.m_sGameVersion === "string" ? meta.m_sGameVersion : "";
    if (!build) return { name, status: "nostamp", detail: "the newest save point's meta-info.json has no readable m_sGameVersion" };
    const sha = checksumFiles(files);
    if (existsSync(sumFile)) {
      let recorded = null;
      try {
        recorded = JSON.parse(readFileSync(sumFile, "utf8"));
      } catch {
        return { name, status: "error", detail: `${name}${CHECKSUM_SUFFIX} is unreadable; delete it to record a fresh checksum` };
      }
      if (recorded.sha256 !== sha) return { name, status: "changed", build, detail: `checksum ${sha.slice(0, 12)} differs from the recorded ${String(recorded.sha256).slice(0, 12)} (${recorded.recorded ?? "?"})` };
      return { name, status: "ok", build, detail: `build ${build}, ${files.length} files, checksum matches` };
    }
    writeFileSync(sumFile, JSON.stringify({ sha256: sha, files: files.length, bytes: files.reduce((n, f) => n + f.size, 0), build, recorded: new Date().toISOString() }, null, 2));
    return { name, status: "ok", build, detail: `build ${build}, ${files.length} files, checksum recorded` };
  } catch (e) {
    return { name, status: "error", detail: e.message };
  }
}

// Verifies the newest `newest` backup folders (all when unset). { ok, results }; a missing
// backups folder is no backups and no failure.
export function verifyBackups(backups, { newest } = {}) {
  if (newest !== undefined && (!Number.isInteger(newest) || newest < 1)) throw new Error(`--newest must be a whole number >= 1 (got ${newest})`);
  if (!existsSync(backups)) return { ok: true, results: [] };
  let names = readdirSync(backups)
    .filter((n) => statSync(join(backups, n)).isDirectory())
    .sort();
  if (newest !== undefined) names = names.slice(-newest);
  const results = names.map((n) => verifyBackup(backups, n));
  return { ok: results.every((r) => r.status === "ok"), results };
}
