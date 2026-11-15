// Logic behind `tools\cli lead` (tools/commands/lead.mjs): no git, no gh, no ssh; the one clock it uses is
// gatherPool's timers, and its probe is injected, so tools/validation/lead-selftest.mjs proves it on fixtures.
import { conflictKind } from "../prs.mjs";
import { DONE, RUNNING } from "./ship-queue.mjs";

export const IDLE_MS = 30 * 60 * 1000;

// ---------------------------------------------------------------- milestones

// "M2 Living Town" is M2; "M2.5 Showcase" is not.
export function milestoneKey(title) {
  const m = /^(M\d+)(?:\s|$)/.exec(String(title ?? ""));
  return m ? m[1] : null;
}

// The state of one open issue, by its labels. A human gate beats a playtest, a playtest beats in-dev.
export function issueState(labels) {
  const names = new Set((labels ?? []).map((l) => (typeof l === "string" ? l : l.name)));
  if (names.has("needs-human")) return "human";
  if (names.has("needs-playtest")) return "playtest";
  if (names.has("in-dev")) return "indev";
  if (names.has("agent:working")) return "working";
  return "code";
}

// issues: [{ number, labels, milestone: { title } | null }] -> { M2: { total, code, working, indev, playtest, human }, ... }
export function milestoneCounts(issues, keys = ["M2", "M3"]) {
  const out = {};
  for (const k of keys) out[k] = { total: 0, code: 0, working: 0, indev: 0, playtest: 0, human: 0 };
  for (const i of issues) {
    const k = milestoneKey(i.milestone?.title);
    if (!k || !out[k]) continue;
    out[k].total++;
    out[k][issueState(i.labels)]++;
  }
  return out;
}

export function milestoneLine(key, c) {
  const code = c.code + c.working;
  const work = c.working ? ` (${c.working} being built)` : "";
  return `${key} ${c.total} open: code ${code}${work} | in-dev ${c.indev} | playtest ${c.playtest} | human ${c.human}`;
}

// ---------------------------------------------------------------- pull requests

export function ageText(ms) {
  const m = Math.max(0, Math.round(ms / 60000));
  if (m < 60) return `${m}m`;
  const h = Math.floor(m / 60);
  return h < 48 ? `${h}h` : `${Math.floor(h / 24)}d`;
}

export function checksWord(pr) {
  const runs = pr.statusCheckRollup ?? [];
  if (!runs.length) return "no checks";
  let fail = 0, pending = 0;
  for (const c of runs) {
    const s = c.__typename === "StatusContext" ? c.state : (c.status === "COMPLETED" ? c.conclusion : "PENDING");
    if (["SUCCESS", "NEUTRAL", "SKIPPED"].includes(s)) continue;
    if (["FAILURE", "ERROR", "CANCELLED", "TIMED_OUT", "ACTION_REQUIRED", "STARTUP_FAILURE"].includes(s)) fail++;
    else pending++;
  }
  if (fail) return `FAIL ${fail}/${runs.length}`;
  if (pending) return `pending ${pending}/${runs.length}`;
  return "pass";
}

export function isConflicting(pr) {
  return pr.mergeable === "CONFLICTING" || pr.mergeStateStatus === "DIRTY";
}

// Conflicting first, then what is stuck (BLOCKED, UNSTABLE, BEHIND, UNKNOWN), then CLEAN; oldest first within a group.
export function sortPrs(prs) {
  const rank = (p) => (isConflicting(p) ? 0 : p.mergeStateStatus === "CLEAN" ? 2 : 1);
  return prs.slice().sort((a, b) => rank(a) - rank(b) || String(a.createdAt).localeCompare(String(b.createdAt)));
}

// files: the conflicted paths `git merge-tree` reports (null when it could not tell).
export function conflictHint(files) {
  if (!files) return "conflicts: ? (branch not fetched)";
  if (!files.length) return "no textual conflict (GitHub is stale)";
  const human = files.filter((f) => !conflictKind(f));
  if (!human.length) return `generated-only (${files.length}): prs heal fixes it`;
  const more = human.length > 1 ? ` +${human.length - 1}` : "";
  return `NEEDS HUMAN: ${human[0].replace(/^.*\//, "")}${more}`;
}

export function prLine(pr, now, hint) {
  const state = isConflicting(pr) ? "CONFLICT" : pr.isDraft ? "DRAFT" : pr.mergeStateStatus;
  const branch = String(pr.headRefName).replace(/^ws\//, "");
  const tail = hint ? `  ${hint}` : "";
  return `#${pr.number} ${branch.slice(0, 26).padEnd(26)} ${String(state).padEnd(8)} ${checksWord(pr).padEnd(11)} ${ageText(now - Date.parse(pr.createdAt))}${tail}`;
}

// ---------------------------------------------------------------- worktrees

// A merged PR whose head branch is this one, whose base is the worktree's base, and whose head SHA is the
// branch tip. NEVER git ancestry: a freshly created worktree's branch is an ancestor of origin/main and has
// no PR, so it is not merged. `prs` is the merged-PR list: [{ number, headRefName, headRefOid, baseRefName }].
export function mergedVerdict({ branch, tip, base = "main" }, prs) {
  if (!tip) return { merged: false, via: "no branch" };
  const mine = (prs ?? []).filter((p) => p.headRefName === branch && (!p.baseRefName || p.baseRefName === base));
  const hit = mine.find((p) => p.headRefOid === tip);
  if (hit) return { merged: true, via: `merged PR #${hit.number}` };
  if (mine.length) return { merged: false, via: "commits after the merged PR" };
  return { merged: false, via: "no merged PR" };
}

// e: { slug, branch, base, tip, exists, isGit, dirty, lastMs, busy }. `kept` says why a merged one is not prunable.
export function classifyWorktree(e, mergedPrs, now = Date.now(), idleMs = IDLE_MS) {
  if (!e.exists) return { cls: "orphan", why: "registered, directory missing" };
  if (!e.isGit) return { cls: "orphan", why: "directory is not a git checkout" };
  const idleFor = now - (e.lastMs || 0);
  const m = mergedVerdict(e, mergedPrs);
  if (m.merged) {
    if (e.dirty) return { cls: "merged-kept", why: "uncommitted changes", via: m.via };
    if (e.busy) return { cls: "merged-kept", why: e.busy, via: m.via };
    if (idleFor < idleMs) return { cls: "merged-kept", why: `touched ${Math.round(idleFor / 60000)} min ago`, via: m.via };
    return { cls: "prunable", via: m.via };
  }
  if (idleFor < idleMs) return { cls: "active", why: "" };
  return { cls: "idle", why: m.via };
}

// Folders named like worktrees that git and the registry know nothing about.
// dirs: folder paths on disk; known: paths git lists, the registry holds, or the warm pool owns.
export function unregisteredDirs(dirs, known) {
  const norm = (p) => String(p).replace(/\\/g, "/").replace(/\/+$/, "").toLowerCase();
  const have = new Set(known.map(norm));
  return dirs.filter((d) => !have.has(norm(d)));
}

export function worktreeSummary(rows, orphanDirs) {
  const by = { active: [], idle: [], prunable: [], "merged-kept": [], orphan: [] };
  for (const r of rows) (by[r.cls] ?? by.idle).push(r);
  return { by, orphanDirs: orphanDirs ?? [] };
}

// ---------------------------------------------------------------- pool

// A snapshot is { name, health: up|slow|DOWN, freeGb, totalGb, floorGb, tempC, lanes, busy, note?, cut? } (one machine,
// built by poolSnapshot in tools/remote/pool.mjs). Each machine is read in its own process: a slow ssh answer
// costs that machine one "slow" row, never the whole section.
//
// gatherPool runs probe(m, { report, signal }) for every machine at once. probe reports a partial snapshot after
// each stage (the health probe, then the RAM and lock read) and resolves with the final one. A machine still silent
// after machineMs, or any machine when budgetMs of the whole section is spent, is cut off and shows what it had:
// "slow" when fallback(m) says it answered lately, "DOWN" otherwise, never a missing row.
// Returns { snaps, exceeded, elapsedMs }; exceeded is true when the section budget (not a machine's own limit) cut someone.
export async function gatherPool({ machines, probe, machineMs = 9000, budgetMs = 12000, fallback = () => "slow" }) {
  const t0 = Date.now();
  const cutters = [];
  let exceeded = false;
  const one = (m) => new Promise((resolve) => {
    let last = null;
    let done = false;
    const ac = new AbortController();
    const timers = [];
    const finish = (snap) => {
      if (done) return;
      done = true;
      for (const t of timers) clearTimeout(t);
      ac.abort();
      resolve(snap);
    };
    const cut = (why, limitMs) => {
      const secs = Math.round(limitMs / 100) / 10;
      if (last?.health) finish({ ...last, health: last.health === "DOWN" ? "DOWN" : "slow", freeGb: last.freeGb ?? null, busy: null, note: `no RAM or lane answer in ${secs} s`, cut: why });
      else finish({ name: m.name, health: fallback(m), freeGb: null, totalGb: null, floorGb: null, tempC: null, lanes: null, busy: null, note: `no answer in ${secs} s`, cut: why });
    };
    cutters.push((why, limitMs) => { const open = !done; if (open) cut(why, limitMs); return open; });
    timers.push(setTimeout(() => cut("machine", machineMs), machineMs));
    Promise.resolve()
      .then(() => probe(m, { report: (snap) => { if (!done) last = snap; }, signal: ac.signal }))
      .then((snap) => finish(snap ?? (last ? { ...last, note: "probe ended early" } : { name: m.name, health: fallback(m), freeGb: null, totalGb: null, floorGb: null, tempC: null, lanes: null, busy: null, note: "probe failed" })))
      .catch((e) => finish({ name: m.name, health: fallback(m), freeGb: null, totalGb: null, floorGb: null, tempC: null, lanes: null, busy: null, note: `probe failed: ${String(e?.message ?? e).split(/\r?\n/)[0].slice(0, 60)}` }));
  });
  const budgetTimer = setTimeout(() => {
    for (const c of cutters) if (c("budget", budgetMs)) exceeded = true;
  }, budgetMs);
  try {
    const snaps = await Promise.all(machines.map(one));
    return { snaps, exceeded, elapsedMs: Date.now() - t0 };
  } finally {
    clearTimeout(budgetTimer);
  }
}

// What a machine that answered nothing in time is called: "slow" when it answered within recentMs (a busy
// machine, the lf-cloud 7 s ssh case), "DOWN" when it never did or not lately. entry is its health.json record.
export function timeoutHealth(entry, now, recentMs) {
  return Number.isFinite(entry?.okAt) && now - entry.okAt < recentMs ? "slow" : "DOWN";
}

export function poolLine(m) {
  const ram = m.freeGb === null ? "RAM ?" : `RAM ${m.freeGb}/${m.totalGb} GB free`;
  const busy = m.busy === null ? "?" : m.busy;
  const lanes = m.lanes === null ? `${busy} busy` : `lanes ${busy}/${m.lanes} busy`;
  const temp = m.tempC === null ? "" : `  ${m.tempC}C`;
  const bad = m.health === "up" ? "" : `  ** ${m.health}${m.note ? `: ${m.note}` : ""} **`;
  return `${m.name.padEnd(11)} ${ram}  ${lanes}${temp}${bad}`;
}

// A machine under its free-RAM floor takes no new job (the pool holds it), so the lead is told.
export function lowRam(snaps) {
  return snaps.filter((m) => Number.isFinite(m.freeGb) && Number.isFinite(m.floorGb) && m.freeGb < m.floorGb);
}

export function poolLines({ snaps, queue, exceeded = false, budgetMs = 0, elapsedMs = 0 }) {
  const lines = [`POOL queue ${queue ?? "?"} waiting`, ...snaps.map((m) => `  ${poolLine(m)}`)];
  for (const m of lowRam(snaps)) lines.push(`  WARN RAM: ${m.name} has ${m.freeGb} GB free, under its ${m.floorGb} GB floor (it takes no new job until RAM frees)`);
  if (exceeded) {
    const cut = snaps.filter((m) => m.cut === "budget").map((m) => `${m.name} (${m.health})`);
    lines.push(`  pool budget ${Math.round(budgetMs / 100) / 10} s exceeded after ${(elapsedMs / 1000).toFixed(1)} s: partial rows for ${cut.join(", ")}`);
  }
  return lines;
}

// ---------------------------------------------------------------- ship queue

// One line from the ship queue file: how many gates run, how many wait, and the longest wait (queued since).
// q is { entries: [{ slug, pr, state, enqueuedAt }] }, as tools/lib/ship-queue.mjs readQueue returns it.
export function shipQueueLine(q, now) {
  const live = (q?.entries ?? []).filter((e) => !DONE.includes(e.state));
  const gating = live.filter((e) => RUNNING.includes(e.state)).length;
  const queued = live.filter((e) => e.state === "queued").sort((a, b) => Date.parse(a.enqueuedAt) - Date.parse(b.enqueuedAt));
  const oldest = queued.find((e) => Number.isFinite(Date.parse(e.enqueuedAt)));
  const wait = oldest ? `, oldest wait ${Math.max(0, Math.round((now - Date.parse(oldest.enqueuedAt)) / 60000))} min (${oldest.slug}${oldest.pr ? ` #${oldest.pr}` : ""})` : "";
  return `SHIP QUEUE ${gating} gating, ${queued.length} queued${wait}`;
}
