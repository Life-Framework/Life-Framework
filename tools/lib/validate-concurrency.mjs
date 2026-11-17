// tools/lib/validate-concurrency.mjs - how many checks `tools\cli validate` runs at once.
//
// Each concurrent check is a node process (about 52 MB working set, measured 2026-11-17) plus what it starts
// (PowerShell, git), so a full fan-out of 12 holds 0.6-1 GB for its seconds, and several agents validate at once.
// This PC sits at 1-3 GB free (docs/architecture/ram-budget.md); a fan-out there pushed free RAM under the pool's
// 1.5 GB floor and once took validate itself down. With room, the fan-out is as before (8-12 by CPU count); short
// of room, it shrinks to what fits over the floor, never under MIN.

export const PER_CHECK_MB = 80;
export const FLOOR_MB = 1500;
export const MIN = 2;

export function areaConcurrency(cpus, freeMb, { perCheckMb = PER_CHECK_MB, floorMb = FLOOR_MB } = {}) {
  const base = Math.max(8, Math.min(12, Number(cpus) || 8));
  if (!Number.isFinite(freeMb)) return base;
  const fit = Math.floor((freeMb - floorMb) / perCheckMb);
  return Math.max(MIN, Math.min(base, fit));
}
