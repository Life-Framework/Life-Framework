// Disk thresholds for worktrees (docs/worktrees.md "Disk guard"). Pure; the callers read the free space.
//
// 2026-11-17: C: fell to 195 MB free with 214 worktrees of about 1.5 GB each and `wt new` failed halfway
// through a checkout. `lead` now warns early, `wt new` refuses before it starts, `lead prune-safe --oldest N`
// frees the most space the safety rules allow.
//
//   LF_DISK_WARN_GB     `lead` warns under this much free space (default 30)
//   LF_WT_MIN_FREE_GB   `wt new` refuses under this much (default 10; 0 turns the refusal off)

export const DISK_WARN_GB = 30;
export const DISK_REFUSE_GB = 10;

function envGb(name, dflt) {
  const raw = process.env[name];
  if (raw === undefined || raw === "") return dflt;
  const n = Number(raw);
  return Number.isFinite(n) && n >= 0 ? n : dflt;
}

export const warnGb = () => envGb("LF_DISK_WARN_GB", DISK_WARN_GB);
export const refuseGb = () => envGb("LF_WT_MIN_FREE_GB", DISK_REFUSE_GB);

// level: "unknown" (not measured), "refuse" (under the `wt new` floor), "warn" (under the lead warning), "ok".
// Exactly at a threshold is not under it.
export function diskVerdict(freeGb, { warn = warnGb(), refuse = refuseGb() } = {}) {
  if (!Number.isFinite(freeGb)) return { level: "unknown" };
  if (refuse > 0 && freeGb < refuse) return { level: "refuse", freeGb, limitGb: refuse };
  if (warn > 0 && freeGb < warn) return { level: "warn", freeGb, limitGb: warn };
  return { level: "ok", freeGb };
}

// The message `wt new` stops with. null when it may go on.
export function refuseMessage(freeGb, where = "this drive", opts) {
  const v = diskVerdict(freeGb, opts);
  if (v.level !== "refuse") return null;
  return `wt new refused: ${freeGb.toFixed(1)} GB free on ${where}, under the ${v.limitGb} GB floor (LF_WT_MIN_FREE_GB). A checkout is about 1.5 GB and a build more; free space first: tools\\cli lead prune-safe --oldest 20 (or --dry-run to preview).`;
}

// The DISK block of `lead`: one row per machine (rows: [{ name, freeGb|null, note? }]), then a WARN line for each one
// under the warning threshold.
export function diskLines(rows, opts) {
  const lines = ["DISK"];
  const warns = [];
  for (const r of rows) {
    const v = diskVerdict(r.freeGb, opts);
    const free = v.level === "unknown" ? "?" : r.freeGb.toFixed(r.freeGb < 100 ? 1 : 0);
    lines.push(`  ${r.name.padEnd(11)} ${free} GB free${r.note ? ` (${r.note})` : ""}`);
    if (v.level === "warn" || v.level === "refuse") {
      const hint = r.name === "this PC" ? "  -> tools\\cli lead prune-safe --oldest 20" : "";
      warns.push(`  WARN DISK: ${r.name} has ${r.freeGb.toFixed(1)} GB free, under ${v.limitGb} GB${v.level === "refuse" ? " (wt new refuses)" : ""}${hint}`);
    }
  }
  return [...lines, ...warns];
}

// `--oldest N` of prune-safe: N is a positive integer. Returns null when the flag is absent, NaN when malformed.
export function parseOldest(args) {
  const i = args.indexOf("--oldest");
  if (i < 0) return null;
  const v = args[i + 1];
  return /^[1-9][0-9]*$/.test(v ?? "") ? Number(v) : NaN;
}

// rows are prunable rows ({ slug, lastMs }); the n least recently active, oldest first. n null keeps them all.
export function oldestFirst(rows, n) {
  const sorted = [...rows].sort((a, b) => (a.lastMs || 0) - (b.lastMs || 0) || String(a.slug).localeCompare(String(b.slug)));
  return n === null || n === undefined ? sorted : sorted.slice(0, n);
}
