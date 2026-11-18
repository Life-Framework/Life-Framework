// tools\cli look stats [--hours 24]: the median and p90 of each look's lockWait, slotWait and work, read from the
// `look-*.log` files every worktree's tmp/logs keeps (one SUMMARY line each, tools/lib/look-job.mjs). It answers
// one question: do looks still queue? The lock is per worktree (#3002), so the stats read every worktree's logs.
// The reader and table are pure enough for tools/validation/look-job-selftest.mjs to check on fixtures.

import { spawnSync } from "node:child_process";
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { parseLookSummary } from "../lib/look-job.mjs";

// Linear interpolation between the two closest ranks (the usual median and p90). null for no values.
export function quantile(values, q) {
  const v = values.filter(Number.isFinite).sort((a, b) => a - b);
  if (!v.length) return null;
  const pos = (v.length - 1) * q;
  const lo = Math.floor(pos);
  const hi = Math.ceil(pos);
  return v[lo] + (v[hi] - v[lo]) * (pos - lo);
}

// The looks whose SUMMARY line is in one of `dirs`' tmp/logs and at or after sinceMs. Each row keeps its dir and file.
export function readLookLogs(dirs, { sinceMs = -Infinity } = {}) {
  const rows = [];
  for (const dir of dirs) {
    const logs = join(dir, "tmp", "logs");
    if (!existsSync(logs)) continue;
    for (const file of readdirSync(logs)) {
      if (!/^look-.*\.log$/.test(file)) continue;
      // The first look SUMMARY line in the file: one look prints one, a later line is not counted again.
      const s = readFileSync(join(logs, file), "utf8").split(/\r?\n/).map(parseLookSummary).find(Boolean);
      if (!s) continue;
      const t = Date.parse(s.at);
      if (!Number.isFinite(t) || t < sinceMs) continue;
      rows.push({ ...s, dir, file });
    }
  }
  return rows;
}

// The table: one line per wait kind with its median and p90 in seconds.
export function lookStatsTable(rows, { hours = 24, worktrees = null } = {}) {
  if (!rows.length) return `look stats, last ${hours} h: no looks with timing yet (a look keeps its SUMMARY line in tmp/logs from #3188 on)`;
  const ok = rows.filter((r) => r.ok).length;
  const dirs = worktrees ?? new Set(rows.map((r) => r.dir)).size;
  const lines = [`look stats, last ${hours} h: ${rows.length} looks (${ok} ok) across ${dirs} worktree(s)`];
  for (const [label, key] of [["lockWait", "lockWait"], ["slotWait", "slotWait"], ["work", "work"]]) {
    const values = rows.map((r) => r[key]);
    const med = quantile(values, 0.5);
    const p90 = quantile(values, 0.9);
    lines.push(`  ${label.padEnd(10)}median ${med.toFixed(1)} s   p90 ${p90.toFixed(1)} s`);
  }
  return lines.join("\n");
}

// Every worktree of this repo (the main checkout included), from git.
export function worktreeDirs(root) {
  const r = spawnSync("git", ["-C", root, "worktree", "list", "--porcelain"], { encoding: "utf8", windowsHide: true });
  if (r.status !== 0) return [root];
  const dirs = r.stdout.split(/\r?\n/).filter((l) => l.startsWith("worktree ")).map((l) => l.slice("worktree ".length));
  return dirs.length ? dirs : [root];
}

export function main({ root, hours } = {}) {
  const h = typeof hours === "string" && Number.isFinite(Number(hours)) && Number(hours) > 0 ? Number(hours) : 24;
  const dirs = worktreeDirs(root);
  const rows = readLookLogs(dirs, { sinceMs: Date.now() - h * 3600000 });
  console.log(lookStatsTable(rows, { hours: h, worktrees: dirs.length }));
  return 0;
}
