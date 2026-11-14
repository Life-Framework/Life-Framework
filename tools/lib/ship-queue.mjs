// tools/lib/ship-queue.mjs - the ship queue (merge train): `wt ship` puts a PR here instead of gating
// on this PC; one worker (tools/ship-queue.mjs) takes entries in FIFO order, merges origin/<base> into
// each once, runs `ci --remote` on a pool machine with the workbench cap, pushes and queues auto-merge.
// docs/worktrees.md "Ship queue". The rules below are pure; tools/validation/ship-queue-selftest.mjs
// proves ordering, the parallel/overlap rule and the re-gate decision.

import { existsSync, mkdirSync } from "node:fs";
import { join } from "node:path";
import { TRANSIENT, readFileRetry, writeFileAtomic } from "./atomic-file.mjs";
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

// Prose under docs/ (docs/worktrees.md, docs/architecture/*.md ...). Two PRs that both edit one, or only
// append to it, are not made to wait for each other: a text conflict shows at merge time (the worker's
// merge of origin/<base>, then GitHub's mergeable state, then `prs heal`), and no gate result depends on it.
export function docsText(path) {
  return /^docs\/.+\.md$/i.test(norm(path));
}

// The files two PRs both change, driver-merged files and docs/**/*.md left out.
export function overlap(a = [], b = []) {
  const skip = (p) => driverMerged(p) || docsText(p);
  const set = new Set(a.map(norm).filter((p) => !skip(p)));
  return [...new Set(b.map(norm))].filter((p) => set.has(p));
}

// ---------------------------------------------------------------- gate slots

// How many Workbench gates run at once. `cfg` is poolSettings(root).shipQueue ({ parallel } from pool.json
// or the "pool" key of lf.local.json) and wins when set; else the sum of the Workbench limits of the ssh
// machines with the workbench cap (a machine's own "workbenchLimit", 1 when unset), at least 1. This PC
// ("local") is not counted: its RAM is short and the gate's Workbench steps run on a pool machine.
export function slotCount({ cfg = {}, machines = [] } = {}) {
  const par = Math.floor(Number(cfg?.parallel));
  if (par > 0) return par;
  const n = machines
    .filter((m) => m.via === "ssh" && !m.disabled && (m.caps ?? []).includes("workbench"))
    .reduce((s, m) => s + Math.max(1, Math.floor(Number(m.workbenchLimit)) || 1), 0);
  return Math.max(1, n);
}

// Gates that need no Workbench (validate only) run beside the Workbench gates, up to this many at once
// ("shipQueue": { "light": N }); they are cheap but not free (validate --all spawns a node per check).
export const DEFAULT_LIGHT = 4;
export function lightCount({ cfg = {} } = {}) {
  const n = Math.floor(Number(cfg?.light));
  return n > 0 ? n : DEFAULT_LIGHT;
}

// Does this entry's gate take a Workbench slot (the ci compile and build on a pool machine)?
//   - no file under addons/: T0 of docs/worktrees.md "Gate tiers", `validate --all` only: no.
//   - e.tier 0: the same, no. e.tier 1 (data only, one LOGIC boot): only when e.builds is true.
//   - otherwise (T2, or a tier nobody recorded yet): yes.
// The tier of a PR that touches addons/ is known only once `ci --tiered` classifies the merged diff
// (PR #1252, tools/lib/gate-tier.mjs). The hook: the worker records it on the entry as `tier` (0|1|2)
// and `builds` (does the T1 gate compile?) and this rule picks it up from there.
export function needsWorkbench(e) {
  const files = e.files ?? [];
  if (files.length && !files.some((f) => norm(f).startsWith("addons/"))) return false;
  if (e.tier === 0) return false;
  if (e.tier === 1) return e.builds === true;
  return true;
}

// Queued entries in arrival order (seq, then enqueue time).
export function ordered(entries) {
  return entries.filter((e) => e.state === "queued").sort((x, y) => (x.seq - y.seq) || String(x.enqueuedAt).localeCompare(String(y.enqueuedAt)));
}

// Which queued entries start now. `slots` Workbench gates run at once (the pool's Workbench room decides
// the machine); an entry that needs no Workbench (needsWorkbench) takes none of them and counts against
// `lightSlots` instead. An entry starts when no running gate and no earlier waiting entry changes a file
// it changes; so PRs on disjoint files gate side by side and overlapping ones keep their FIFO order.
// Returns [{ entry, why }] for starts and [{ entry, why }] for the ones that wait.
export function pickRunnable(entries, { slots = 1, lightSlots = DEFAULT_LIGHT, now = Date.now() } = {}) {
  const running = entries.filter((e) => RUNNING.includes(e.state));
  let free = Math.max(0, slots - running.filter(needsWorkbench).length);
  let freeLight = Math.max(0, lightSlots - running.filter((e) => !needsWorkbench(e)).length);
  const start = [];
  const wait = [];
  const ahead = [...running];
  for (const e of ordered(entries)) {
    const clash = ahead.map((o) => ({ o, files: overlap(o.files, e.files) })).find((c) => c.files.length);
    if (e.notBefore && Date.parse(e.notBefore) > now) wait.push({ entry: e, why: `retries after ${e.notBefore.slice(11, 16)} UTC (the pool failed its last gate)` });
    else if (clash) wait.push({ entry: e, why: `waits for ${clash.o.slug} (both change ${clash.files.slice(0, 3).join(", ")}${clash.files.length > 3 ? ", ..." : ""})` });
    else if (!needsWorkbench(e)) {
      if (freeLight <= 0) wait.push({ entry: e, why: `waits for a gate slot (${lightSlots} validate-only gates running)` });
      else {
        freeLight--;
        start.push({ entry: e, why: "validate-only gate: takes no Workbench slot" });
      }
    } else if (free <= 0) wait.push({ entry: e, why: `waits for a gate slot (${slots} running)` });
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

// A gate that failed because the pool could not run it (no machine answered ssh, a test shard placed
// nowhere and ran 0 tests) says nothing about the PR: the worker requeues it instead of failing it.
// Returns the reason, or null for a real failure (a shard that ran tests and failed one is real).
export const MAX_ENV_RETRIES = 2;
export function environmentFailure(text) {
  const t = String(text);
  if (/FAIL {2}pool: no reachable machine/.test(t)) return "no pool machine answered";
  const shards = [...t.matchAll(/^shard \S+ \d+\/\d+: exit (\d+), [\d.]+ s, (\d+) test\(s\)/gm)];
  if (shards.some((m) => m[1] === "3" && m[2] === "0") && !shards.some((m) => m[1] !== "0" && m[2] !== "0")) return "a test shard could not be placed (exit 3, 0 tests)";
  return null;
}

// ---------------------------------------------------------------- state (tmp/wt/ship-queue.json in the main checkout)

export function statePath(hubDir) {
  return join(hubDir, "ship-queue.json");
}

// A status reader never sees a torn file (writeQueue renames whole). A read that keeps failing with a
// transient error (EPERM, EBUSY, EACCES: a writer mid-rename) throws instead of returning an empty queue:
// withQueue would write that empty queue back over the real one.
export function readQueue(hubDir) {
  const p = statePath(hubDir);
  if (!existsSync(p)) return { version: 1, seq: 0, entries: [] };
  let text;
  try {
    text = readFileRetry(p, "utf8");
  } catch (e) {
    if (TRANSIENT.includes(e?.code)) throw e;
    return { version: 1, seq: 0, entries: [] };
  }
  try {
    const s = JSON.parse(text);
    return { version: 1, seq: s.seq ?? 0, entries: Array.isArray(s.entries) ? s.entries : [] };
  } catch {
    return { version: 1, seq: 0, entries: [] };
  }
}

// Temp file + rename. On Windows the rename fails with EPERM/EBUSY/EACCES while a status or wait reader has
// the file open, so it is retried (10 tries over about 1 s, tools/lib/atomic-file.mjs) before it throws.
export function writeQueue(hubDir, q) {
  mkdirSync(hubDir, { recursive: true });
  const done = q.entries.filter((e) => DONE.includes(e.state)).sort((a, b) => String(b.endedAt).localeCompare(String(a.endedAt))).slice(0, KEEP_DONE);
  const live = q.entries.filter((e) => !DONE.includes(e.state));
  const p = statePath(hubDir);
  writeFileAtomic(p, JSON.stringify({ ...q, entries: [...live, ...done] }, null, 2) + "\n", { tmp:`${p}.tmp` });
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
