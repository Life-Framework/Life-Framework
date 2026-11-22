// tools/lib/pool-commit.mjs - the pool's COMMIT admission, beside its RAM admission (docs/worktrees.md "RAM admission").
//
// Windows refuses an allocation when committed memory (every private page promised to a process, resident or
// not) reaches the commit limit (physical RAM plus the pagefile), whatever physical RAM is free. On 2026-11-19
// lf-testbox sat at 70.2 of 71.2 GB committed with about 9 GB of physical RAM free: the pool admits by free
// physical RAM only, so it kept starting game clients (3.8-5.3 GB of commit each), Workbenches (2.7-5.7 GB) and
// servers (1-2.6 GB), and git (0xc000012d, STATUS_COMMITMENT_LIMIT), gh ("out of memory allocating heap arena
// map"), bash fork and the Claude agents themselves (0xC0000409) started failing. The pagefile is fixed, so the
// gate has to keep commit headroom: a floor (default 6 GB) stays free for agents, git and node, and a job
// that would cross it queues. A working-set trim frees physical RAM but no commit; what frees commit is a
// session closing (the idle-session lease already closes one while a job waits).
//
// Same shape as pool-ram.mjs: a per-kind peak (seeded, then the p90 of the private bytes seen on real runs,
// recorded by pool-ram trackPeaks under `commit:<kind>`), what running processes still grow into, what the pool's
// own fresh locks will take, one verdict. Pure functions; the PowerShell read is the only I/O.

import { spawnSync } from "node:child_process";
import { MIN_SAMPLES, percentile } from "./pool-ram.mjs";

// The commit kept free for agents, git and node (MB). pool.json / lf.local.json "minFreeCommitMb", env LF_POOL_MIN_FREE_COMMIT_MB.
export const DEFAULT_MIN_FREE_COMMIT_MB = 6144;

// Seeds (MB), measured 2026-11-19 on the laptop: client 3.8-5.3 GB, Workbench 2.7-5.7 GB, server 1-2.6 GB.
// Used until a kind has MIN_SAMPLES observed peaks.
export const COMMIT_SEED_MB = { client: 5500, workbench: 6000, server: 2600, test: 1500 };

export const commitKey = (kind) => `commit:${kind}`;

// The commit one process of this kind takes at its peak (MB): the p90 of observed private-byte peaks once
// MIN_SAMPLES exist, else the seed.
export function commitPeakMb(peaks, kind) {
  const samples = peaks?.[commitKey(kind)];
  const measured = Array.isArray(samples) && samples.length >= MIN_SAMPLES ? percentile(samples) : null;
  return Math.round(measured ?? COMMIT_SEED_MB[kind] ?? 0);
}

// { client, workbench, server, test } peaks (MB), for the PowerShell holder's config and `pool status`.
export function commitPeakTable(peaks) {
  const out = {};
  for (const kind of Object.keys(COMMIT_SEED_MB)) out[kind] = commitPeakMb(peaks, kind);
  return out;
}

// What a job takes at its peak (MB). kinds: session (server + clients), test, workbench.
export function jobCommitMb(peaks, { kind, clients = 1 } = {}) {
  if (kind === "workbench") return commitPeakMb(peaks, "workbench");
  if (kind === "test") return commitPeakMb(peaks, "test");
  return commitPeakMb(peaks, "server") + commitPeakMb(peaks, "client") * Math.max(0, Number(clients) || 0);
}

// Commit the running processes will still take: for each, its kind's peak less the private bytes it holds
// (the larger of now and the highest seen, maxPrivMb). A scan without private bytes counts nothing for it.
// A game process past the growth window (pool-ram GROWTH_WINDOW_S, its scan age ageS) has loaded and counts nothing.
export function unrealisedCommitMb(procs, peaks, { windowS = 300 } = {}) {
  let mb = 0;
  for (const p of procs ?? []) {
    if (!Number.isFinite(Number(p.privMb))) continue;
    if (p.kind !== "workbench" && Number.isFinite(p.ageS) && p.ageS > windowS) continue;
    mb += Math.max(0, commitPeakMb(peaks, p.kind) - Math.max(Number(p.privMb) || 0, Number(p.maxPrivMb) || 0));
  }
  return Math.round(mb);
}

// Commit of the pool's own lane-lock jobs in their first gapS seconds, before their processes exist to be
// scanned (the pool-ram lockGapMb rule). infos: [{ purpose, at, clients? }].
export function lockGapCommitMb(infos, peaks, now = Date.now(), gapS = 45) {
  let mb = 0;
  for (const i of infos ?? []) {
    if (!i || i.alive === false || !i.purpose) continue;
    const age = now - Date.parse(i.at);
    if (!Number.isFinite(age) || age > gapS * 1000) continue;
    mb += jobCommitMb(peaks, { kind: i.purpose === "session" ? "session" : "test", clients: i.clients ?? 1 });
  }
  return mb;
}

// One verdict. usedMb / limitMb: the machine's committed memory and commit limit now (null when unread: ok).
// The job queues when limit - used - pending - growth - reserve would be under floorMb.
// Returns { ok, needMb?, headroomMb?, reason? }; needMb is the headroom (limit - used) that would admit it.
export function commitAdmit({ usedMb, limitMb, floorMb = DEFAULT_MIN_FREE_COMMIT_MB, reserveMb = 0, pendingMb = 0, growthMb = 0, what = "the job" }) {
  const gb = (x) => (x / 1024).toFixed(1);
  if (!Number.isFinite(usedMb) || !Number.isFinite(limitMb) || limitMb <= 0) return { ok: true };
  const headroomMb = limitMb - usedMb;
  const committed = (Number(pendingMb) || 0) + (Number(growthMb) || 0);
  const needMb = floorMb + reserveMb + committed;
  if (headroomMb >= needMb) return { ok: true, needMb, headroomMb };
  const why = [`commit ${gb(usedMb)}/${gb(limitMb)} GB used (${gb(Math.max(0, headroomMb))} GB headroom)`, `${gb(reserveMb)} GB commit peak for ${what}`];
  if (committed > 0) why.push(`${gb(committed)} GB the running jobs still grow into`);
  return { ok: false, needMb, headroomMb, reason: `${why.join(" minus ")} would be under ${gb(floorMb)} GB` };
}

// "commit 70.2/71.2 GB used (floor 6.0 GB free)" for `pool status` and the queue lines; "" when unread.
export function commitText(usedMb, limitMb, floorMb) {
  if (!Number.isFinite(usedMb) || !Number.isFinite(limitMb)) return "";
  const gb = (x) => (x / 1024).toFixed(1);
  return `commit ${gb(usedMb)}/${gb(limitMb)} GB used${Number.isFinite(floorMb) ? ` (floor ${gb(floorMb)} free)` : ""}`;
}

// ---------------------------------------------------------------- read (here and over ssh: same output)

// Prints "lf-pool: commit <usedMb> <limitMb>": committed = TotalVirtualMemorySize - FreeVirtualMemory (KB).
export const PS_COMMIT_READ = String.raw`
try { $cos = Get-CimInstance Win32_OperatingSystem -ErrorAction Stop; Write-Host ('lf-pool: commit ' + [int](($cos.TotalVirtualMemorySize - $cos.FreeVirtualMemory) / 1024) + ' ' + [int]($cos.TotalVirtualMemorySize / 1024)) } catch {}
`;

// { usedMb, limitMb } from output with a commit line, else null.
export function parseCommit(text) {
  const m = /^lf-pool: commit (\d+) (\d+)\s*$/m.exec(String(text ?? ""));
  if (!m) return null;
  const v = { usedMb: Number(m[1]), limitMb: Number(m[2]) };
  return v.limitMb > 0 ? v : null;
}

// This PC's commit, one PowerShell call kept for 3 s. LF_POOL_TEST_COMMIT_MB="<used>/<limit>" is the selftest
// seam; with LF_POOL_TEST_FREE_MB set and no commit seam the gate reads nothing (null), so a selftest never
// depends on what the PC it runs on is doing. opts.run stands in for the process call.
let readCache = null;
export function readLocalCommit({ now = Date.now(), run, env = process.env } = {}) {
  const seam = /^(\d+)\/(\d+)$/.exec(String(env.LF_POOL_TEST_COMMIT_MB ?? ""));
  if (seam) return { usedMb: Number(seam[1]), limitMb: Number(seam[2]) };
  if (Number(env.LF_POOL_TEST_FREE_MB) > 0 && !run) return null;
  if (!run && readCache && now - readCache.at < 3000) return readCache.v;
  let v = null;
  try {
    const r = run ? run() : spawnSync("powershell", ["-NoProfile", "-NonInteractive", "-Command", PS_COMMIT_READ], { windowsHide: true, encoding: "utf8", timeout: 15000 });
    if (r.status === 0) v = parseCommit(r.stdout);
  } catch {}
  if (!run) readCache = { at: now, v };
  return v;
}
