// Pure logic behind `tools\cli lead` (tools/commands/lead.mjs): no git, no gh, no clock of its own,
// so tools/validation/lead-selftest.mjs can prove it on fixtures.
import { conflictKind } from "../prs.mjs";

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

// Reads the text `tools\cli pool status` prints: machine head lines (no indent), busy lane rows (indented),
// and the queue line. Returns { machines: [{ name, health, freeGb, totalGb, tempC, lanes, busy }], queue }.
export function parsePoolStatus(text) {
  const machines = [];
  let queue = null;
  let cur = null;
  for (const raw of String(text).split(/\r?\n/)) {
    const q = /^queue:\s*(\d+)\s+waiting/.exec(raw);
    if (q) { queue = Number(q[1]); cur = null; continue; }
    if (/^\s+\S/.test(raw)) {
      if (cur && !/stale/i.test(raw)) cur.busy++;
      continue;
    }
    const health = /\s\s(up|DOWN|slow(?: \([^)]*\))?)(?:\s\s|$)/.exec(raw);
    if (!health || !/\s\s(this PC|ssh )/.test(raw)) { cur = null; continue; }
    const ram = /RAM ([\d.]+)\/([\d.]+) GB free/.exec(raw);
    const temp = /(\d+(?:\.\d+)?) \S{1,2}C \(holds/.exec(raw);
    const split = /up to (\d+) test \+ (\d+) session/.exec(raw);
    const shared = /(\d+) shared slot/.exec(raw);
    cur = {
      name: raw.split(/\s\s/)[0].trim(),
      health: health[1].replace(/\s*\(.*\)$/, ""),
      freeGb: ram ? Number(ram[1]) : null,
      totalGb: ram ? Number(ram[2]) : null,
      tempC: temp ? Number(temp[1]) : null,
      lanes: split ? Number(split[1]) + Number(split[2]) : shared ? Number(shared[1]) : null,
      busy: 0,
    };
    machines.push(cur);
  }
  return { machines, queue };
}

export function poolLine(m) {
  const ram = m.freeGb === null ? "RAM ?" : `RAM ${m.freeGb}/${m.totalGb} GB free`;
  const lanes = m.lanes === null ? `${m.busy} busy` : `lanes ${m.busy}/${m.lanes} busy`;
  const temp = m.tempC === null ? "" : `  ${m.tempC}C`;
  const bad = m.health === "up" ? "" : `  ** ${m.health} **`;
  return `${m.name.padEnd(11)} ${ram}  ${lanes}${temp}${bad}`;
}
