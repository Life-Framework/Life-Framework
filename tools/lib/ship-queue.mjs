// tools/lib/ship-queue.mjs - the ship queue (merge train): `wt ship` puts a PR here instead of gating
// on this PC; one worker (tools/ship-queue.mjs) takes entries in FIFO order, merges origin/<base> into
// each once, runs `ci --remote` on a pool machine with the workbench cap, pushes and queues auto-merge.
// docs/worktrees.md "Ship queue". The rules below are pure; tools/validation/ship-queue-selftest.mjs
// proves ordering, the parallel/overlap rule and the re-gate decision.

import { existsSync, mkdirSync, readFileSync, renameSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { conflictKind } from "../prs.mjs";

export const RUNNING = ["merging", "gating", "pushing"];
export const DONE = ["passed", "failed", "cancelled"];
export const KEEP_DONE = 30; // finished entries kept for `wt ship --wait`, `ship-queue status` and `pool status`

// Files the merge drivers and `prs heal` resolve on every merge (docs, the string table, the lint
// baseline, the generated registry and language confs). Nearly every PR touches docs/features.md, so
// they never make two PRs "overlap"; otherwise every feature PR would gate in sequence.
export function driverMerged(path) {
  return conflictKind(path) !== null;
}

// Files a merge regenerates or derives from (the test registry, the language confs and the string table
// they come from). A main commit touching one of them changes what the PR compiles, so it re-gates.
export function generated(path) {
  const k = conflictKind(path);
  return k === "regen-tests" || k === "regen-localization" || k === "string-table";
}

const norm = (p) => String(p).replace(/\\/g, "/");

// The files two PRs both change, driver-merged files left out.
export function overlap(a = [], b = []) {
  const set = new Set(a.map(norm).filter((p) => !driverMerged(p)));
  return [...new Set(b.map(norm))].filter((p) => set.has(p));
}

// Queued entries in arrival order (seq, then enqueue time).
export function ordered(entries) {
  return entries.filter((e) => e.state === "queued").sort((x, y) => (x.seq - y.seq) || String(x.enqueuedAt).localeCompare(String(y.enqueuedAt)));
}

// Which queued entries start now. `slots` gates run at once (the pool's Workbench room decides the
// machine). An entry starts when no running gate and no earlier waiting entry changes a file it
// changes; so PRs on disjoint files gate side by side and overlapping ones keep their FIFO order.
// Returns [{ entry, why }] for starts and [{ entry, why }] for the ones that wait.
export function pickRunnable(entries, { slots = 1 } = {}) {
  const running = entries.filter((e) => RUNNING.includes(e.state));
  let free = Math.max(0, slots - running.length);
  const start = [];
  const wait = [];
  const ahead = [...running];
  for (const e of ordered(entries)) {
    const clash = ahead.map((o) => ({ o, files: overlap(o.files, e.files) })).find((c) => c.files.length);
    if (clash) wait.push({ entry: e, why: `waits for ${clash.o.slug} (both change ${clash.files.slice(0, 3).join(", ")}${clash.files.length > 3 ? ", ..." : ""})` });
    else if (free <= 0) wait.push({ entry: e, why: `waits for a gate slot (${slots} running)` });
    else {
      free--;
      start.push({ entry: e, why: running.length + start.length > 0 ? "starts beside the running gate(s): no shared files" : "head of the queue" });
    }
    ahead.push(e);
  }
  return { start, wait };
}

// Main moved while the gate ran (mainFiles: what the new main commits changed). Re-gate only when they
// change a file the PR changes, or a generated file; otherwise the gated head goes up as is and GitHub's
// required validate check covers the combination.
export function regateDecision({ prFiles = [], mainFiles = [], attempts = 1, maxAttempts = 3 }) {
  if (!mainFiles.length) return { regate: false, why: "main did not move during the gate" };
  const shared = overlap(prFiles, mainFiles); // driver-merged docs both sides append to do not count
  const gen = [...new Set(mainFiles.map(norm))].filter(generated);
  const why = shared.length ? `main changed ${shared.length} file(s) this PR changes (${shared.slice(0, 3).join(", ")})` : gen.length ? `main changed generated file(s) (${gen.slice(0, 2).join(", ")})` : null;
  if (!why) return { regate: false, why: `main moved (${mainFiles.length} file(s)), none shared with this PR or generated; pushing the gated head` };
  if (attempts >= maxAttempts) return { regate: false, why: `${why}, but ${attempts} gates ran already; pushing, GitHub's validate check covers it`, capped: true };
  return { regate: true, why };
}

// ---------------------------------------------------------------- state (tmp/wt/ship-queue.json in the main checkout)

export function statePath(hubDir) {
  return join(hubDir, "ship-queue.json");
}

export function readQueue(hubDir) {
  const p = statePath(hubDir);
  if (!existsSync(p)) return { version: 1, seq: 0, entries: [] };
  try {
    const s = JSON.parse(readFileSync(p, "utf8"));
    return { version: 1, seq: s.seq ?? 0, entries: Array.isArray(s.entries) ? s.entries : [] };
  } catch {
    return { version: 1, seq: 0, entries: [] };
  }
}

export function writeQueue(hubDir, q) {
  mkdirSync(hubDir, { recursive: true });
  const done = q.entries.filter((e) => DONE.includes(e.state)).sort((a, b) => String(b.endedAt).localeCompare(String(a.endedAt))).slice(0, KEEP_DONE);
  const live = q.entries.filter((e) => !DONE.includes(e.state));
  const p = statePath(hubDir);
  writeFileSync(`${p}.tmp`, JSON.stringify({ ...q, entries: [...live, ...done] }, null, 2) + "\n", "utf8");
  renameSync(`${p}.tmp`, p);
}

// Add or replace the entry for a slug. A slug still queued is replaced (same seq, so it keeps its place);
// one already running is refused, the caller waits for it.
export function enqueue(q, entry, now = new Date().toISOString()) {
  const cur = q.entries.find((e) => e.slug === entry.slug && !DONE.includes(e.state));
  if (cur && RUNNING.includes(cur.state)) throw new Error(`${entry.slug} is already in the ship queue and ${cur.state} (log ${cur.log}); wait for it: tools\\cli ship-queue wait ${entry.slug}`);
  const seq = cur ? cur.seq : ++q.seq;
  q.entries = q.entries.filter((e) => e !== cur);
  const e = { ...entry, id: `${entry.slug}-${seq}`, seq, state: "queued", enqueuedAt: cur?.enqueuedAt ?? now, attempts: 0, history: [] };
  q.entries.push(e);
  return e;
}

// The newest entry for a slug (live first).
export function entryFor(q, slug) {
  return q.entries.find((e) => e.slug === slug && !DONE.includes(e.state)) ?? q.entries.filter((e) => e.slug === slug).sort((a, b) => b.seq - a.seq)[0] ?? null;
}

// Status lines for `pool status` and `ship-queue status`.
export function statusLines(q, { now = Date.now(), waits = null } = {}) {
  const mins = (t) => (t ? `${Math.max(0, Math.round((now - Date.parse(t)) / 60000))} min` : "");
  const live = q.entries.filter((e) => !DONE.includes(e.state)).sort((a, b) => a.seq - b.seq);
  const done = q.entries.filter((e) => DONE.includes(e.state)).sort((a, b) => String(b.endedAt).localeCompare(String(a.endedAt))).slice(0, 5);
  const out = [`ship queue: ${live.filter((e) => RUNNING.includes(e.state)).length} gating, ${live.filter((e) => e.state === "queued").length} queued`];
  const why = new Map((waits ?? []).map((w) => [w.entry.id, w.why]));
  for (const e of live) {
    const pr = e.pr ? `#${e.pr}` : "";
    const where = e.machine ? ` on ${e.machine}` : "";
    const since = RUNNING.includes(e.state) ? `for ${mins(e.startedAt)}` : `queued ${mins(e.enqueuedAt)}`;
    out.push(`  ${String(e.seq).padStart(3)} ${e.slug.padEnd(24).slice(0, 24)} ${pr.padEnd(6)} ${e.state.padEnd(8)}${where} ${since}${why.has(e.id) ? `; ${why.get(e.id)}` : ""}`);
  }
  for (const e of done) {
    const took = e.startedAt && e.endedAt ? `${Math.round((Date.parse(e.endedAt) - Date.parse(e.startedAt)) / 60000)} min gate` : "";
    out.push(`  done ${e.slug.padEnd(24).slice(0, 24)} ${(e.pr ? `#${e.pr}` : "").padEnd(6)} ${e.state.padEnd(9)} ${took}, ${mins(e.endedAt)} ago${e.state === "failed" ? `: ${e.why ?? ""}` : ""}`);
  }
  return out;
}
