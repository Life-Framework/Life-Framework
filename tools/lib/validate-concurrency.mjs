// tools/lib/validate-concurrency.mjs - how many checks `tools\cli validate` runs at once.
//
// Each concurrent check is a node process (about 52 MB working set, measured 2026-11-17) plus what it starts
// (PowerShell, git), so a full fan-out of 12 holds 0.6-1 GB for its seconds, and several agents validate at once.
// This PC sits at 1-3 GB free (docs/architecture/ram-budget.md); a fan-out there pushed free RAM under the pool's
// 1.5 GB floor and once took validate itself down. With room, the fan-out is as before (8-12 by CPU count); short
// of room, it shrinks to what fits over the floor, never under MIN.

import { mkdirSync, readdirSync, unlinkSync, writeFileSync } from "node:fs";
import { join } from "node:path";

export const PER_CHECK_MB = 80;
export const FLOOR_MB = 1500;
export const MIN = 2;

export function areaConcurrency(cpus, freeMb, { perCheckMb = PER_CHECK_MB, floorMb = FLOOR_MB } = {}) {
  const base = Math.max(8, Math.min(12, Number(cpus) || 8));
  if (!Number.isFinite(freeMb)) return base;
  const fit = Math.floor((freeMb - floorMb) / perCheckMb);
  return Math.max(MIN, Math.min(base, fit));
}

// Several agents validate at once: on 2026-11-22 eight validates ran together on lf-testbox, each with 12 checks
// in flight, and each check starts git and PowerShell (a conhost apiece). Git Bash then failed with "fork: Resource
// temporarily unavailable" at about 520 processes. The fan-out is shared: each run takes base / (validates live
// on this PC), never under MIN.
export function shareConcurrency(base, running) {
  return Math.max(MIN, Math.floor(base / Math.max(1, Number(running) || 1)));
}

function pidAlive(pid) {
  try {
    process.kill(pid, 0);
    return true;
  } catch (e) {
    return e?.code === "EPERM";
  }
}

// Registers this process as a running validate under dir (one file per pid, removed on exit) and returns how
// many validates are live there, this one included. Any I/O failure counts as alone.
let registered = null;
export function registerValidate(dir, pid = process.pid) {
  try {
    mkdirSync(dir, { recursive: true });
    if (registered !== dir) {
      const own = join(dir, String(pid));
      writeFileSync(own, String(Date.now()));
      registered = dir;
      process.on("exit", () => {
        try {
          unlinkSync(own);
        } catch {}
      });
    }
    let live = 0;
    for (const f of readdirSync(dir)) {
      const p = Number(f);
      if (!(p > 0)) continue;
      if (p === pid || pidAlive(p)) live++;
      else {
        try {
          unlinkSync(join(dir, f));
        } catch {}
      }
    }
    return Math.max(1, live);
  } catch {
    return 1;
  }
}
