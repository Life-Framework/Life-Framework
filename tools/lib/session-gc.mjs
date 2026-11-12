// Pool hygiene: remove old session folders, and refuse to start work on a full disk
// (issue #675: C: hit 0 MB under 94 session folders and 17 GB). Pure planning plus a
// thin runner, so tools/validation/pool-selftest.mjs proves it without a game.
// Deletion goes through link-safety.safeRemoveTree only (tools/safe-rm.mjs wraps the
// same function): a session folder can hold links, and rm -rf / Remove-Item -Recurse follow them.

import { readdirSync, readFileSync, statSync, statfsSync } from "node:fs";
import { join, resolve } from "node:path";
import { safeRemoveTree } from "../link-safety.mjs";

// A folder whose name contains "film" holds takes someone may still encode; never removed.
export const FILM_NAME = /film/i;

function readJson(file) {
  try {
    return JSON.parse(readFileSync(file, "utf8"));
  } catch {
    return null;
  }
}

function mtimeMs(file) {
  try {
    return statSync(file).mtimeMs;
  } catch {
    return 0;
  }
}

// Last time anything touched the folder: the newest of the folder, session.json and lease.json.
export function lastTouchedMs(dir) {
  return Math.max(mtimeMs(dir), mtimeMs(join(dir, "session.json")), mtimeMs(join(dir, "lease.json")));
}

// A session record that is not stopped and has a live server or client pid.
export function sessionInUse(dir, alive) {
  const s = readJson(join(dir, "session.json"));
  if (!s || s.stoppedAt) return false;
  return !!(alive(s.serverPid) || (s.clients ?? []).some((c) => alive(c.pid)));
}

// [{ name, dir, ageH, bytes? }] to remove, and [{ name, why }] kept.
// opts: now, keepHours, alive(pid), skip (dirs to leave alone, e.g. this checkout's own).
export function planSessionGc(base, { now = Date.now(), keepHours = 24, alive = () => false, skip = [] } = {}) {
  const remove = [];
  const keep = [];
  let names = [];
  try {
    names = readdirSync(base, { withFileTypes: true }).filter((d) => d.isDirectory()).map((d) => d.name);
  } catch {
    return { remove, keep };
  }
  const skipSet = new Set(skip.map((s) => resolve(String(s)).toLowerCase()));
  for (const name of names) {
    const dir = join(base, name);
    const ageH = (now - lastTouchedMs(dir)) / 3600000;
    if (skipSet.has(resolve(dir).toLowerCase())) keep.push({ name, why: "this checkout's own" });
    else if (FILM_NAME.test(name)) keep.push({ name, why: "film" });
    else if (sessionInUse(dir, alive)) keep.push({ name, why: "in use" });
    else if (ageH < keepHours) keep.push({ name, why: "recent" });
    else remove.push({ name, dir, ageH });
  }
  return { remove, keep };
}

// Removes the planned folders with safeRemoveTree; one failure does not stop the rest.
export function runSessionGc(base, opts = {}) {
  const plan = planSessionGc(base, opts);
  const removed = [];
  const failed = [];
  for (const r of plan.remove) {
    try {
      (opts.remove ?? safeRemoveTree)(r.dir);
      removed.push(r.name);
    } catch (e) {
      failed.push({ name: r.name, error: String(e?.message ?? e).split("\n")[0] });
    }
  }
  return { ...plan, removed, failed };
}

// Free space in GB on the volume holding `path`; null when it cannot be read.
export function freeDiskGb(path) {
  try {
    const s = statfsSync(path);
    return (Number(s.bavail) * Number(s.bsize)) / 1073741824;
  } catch {
    return null;
  }
}

// null when there is room (or the disk cannot be read), else the refusal message.
export function diskVerdict(freeGb, minFreeGb) {
  if (!Number.isFinite(freeGb) || !(minFreeGb > 0) || freeGb >= minFreeGb) return null;
  return `free disk ${freeGb.toFixed(1)} GB is under the ${minFreeGb} GB floor (pool minFreeDiskGb); clean up with tools\\cli wt prune <slug> or lower the floor in tools/pool.json or lf.local.json`;
}
