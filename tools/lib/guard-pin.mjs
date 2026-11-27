// The main guard's per-head verdicts (docs/worktrees.md "Main guard"): `compiled-ok` (the Workbench compile
// passed, the full check still pending), `green` (the full check passed) or `red`. The guard records each one in
// <hub>/main-guard-state.json on its own machine and as a GitHub commit status (context `main-guard`) so any
// machine can read it. `world sync` and `wt new` pin to the newest first-parent commit of origin/main that is
// `compiled-ok` or `green`, never an unchecked head. Proven by tools/validation/guard-pin-selftest.mjs.

import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, renameSync, writeFileSync } from "node:fs";
import { join } from "node:path";

export const CONTEXT = "main-guard";
export const KEEP = 300;

export const statePathOf = (hub) => join(hub, "main-guard-state.json");
export const isChecked = (status) => status === "compiled-ok" || status === "green";

export function readHeads(hub) {
  try {
    const s = JSON.parse(readFileSync(statePathOf(hub), "utf8"));
    return { heads: s.heads ?? {} };
  } catch {
    return { heads: {} };
  }
}

// green and red are final for a head; compiled-ok never overwrites them.
export function recordHead(hub, sha, status, extra = {}) {
  if (!sha || !["compiled-ok", "green", "red"].includes(status)) throw new Error(`recordHead: bad ${sha} ${status}`);
  const s = readHeads(hub);
  const prev = s.heads[sha];
  if (prev && status === "compiled-ok" && prev.status !== "compiled-ok") return prev;
  s.heads[sha] = { status, at: new Date().toISOString(), ...extra };
  const keys = Object.keys(s.heads).sort((a, b) => String(s.heads[a].at).localeCompare(String(s.heads[b].at)));
  for (const k of keys.slice(0, Math.max(0, keys.length - KEEP))) delete s.heads[k];
  mkdirSync(hub, { recursive: true });
  const f = statePathOf(hub);
  writeFileSync(`${f}.tmp`, JSON.stringify(s, null, 2) + "\n");
  renameSync(`${f}.tmp`, f);
  return s.heads[sha];
}

// A stage-2 red that was an infra refusal, not a verdict (#2759): the head goes back to compiled-ok (stage 2 only
// ever runs on a compiled-ok head) with the reason kept. Any other entry is left alone; returns the entry or null.
export function clearInfraRed(hub, sha, why) {
  const s = readHeads(hub);
  const prev = s.heads[sha];
  if (!prev || prev.status !== "red" || prev.step === "compile") return null;
  s.heads[sha] = { status: "compiled-ok", at: prev.at, infra: why, wasRed: prev.step };
  mkdirSync(hub, { recursive: true });
  const f = statePathOf(hub);
  writeFileSync(`${f}.tmp`, JSON.stringify(s, null, 2) + "\n");
  renameSync(`${f}.tmp`, f);
  return s.heads[sha];
}

// GitHub commit statuses are the source of truth (every machine reads them through gh); the hub file is a cache.
// main-guard/compile: success = compiled-ok, failure = compile red. main-guard/suite: success = green (build, full
// suite, placement audit, pre-move e2e), failure = red. The legacy single context "main-guard" is still read.
export const COMPILE_CTX = "main-guard/compile";
export const SUITE_CTX = "main-guard/suite";
export const LEASE_CTX = "main-guard/lease";
export const LEASE_TTL_MS = 30 * 60 * 1000;

// What to post for a verdict: [{ context, state, description }].
export function guardToGh(status, step = "", note = "") {
  if (status === "compiled-ok") return [{ context: COMPILE_CTX, state: "success", description: "compiled-ok: Workbench compile passed" }];
  if (status === "green") return [{ context: SUITE_CTX, state: "success", description: "green: build, full suite, placement audit, pre-move e2e" }];
  if (step === "compile") return [{ context: COMPILE_CTX, state: "failure", description: "red: Workbench compile failed" }];
  return [{ context: SUITE_CTX, state: "failure", description: ("red: " + (step || "post-merge check") + " failed" + (note ? "; " + note : "")).slice(0, 140) }];
}

// Pure: a commit's statuses (any order; GitHub lists newest first) -> compiled-ok | green | red | null.
export function guardFromStatuses(list) {
  const newest = new Map();
  for (const st of list ?? []) {
    if (!st?.context) continue;
    const prev = newest.get(st.context);
    if (!prev || String(st.created_at ?? st.updated_at ?? "") > String(prev.created_at ?? prev.updated_at ?? "")) newest.set(st.context, st);
  }
  const c = newest.get(COMPILE_CTX)?.state;
  const t = newest.get(SUITE_CTX)?.state;
  if (c === "failure" || c === "error" || t === "failure" || t === "error") return "red";
  if (t === "success") return "green";
  if (c === "success") return "compiled-ok";
  const legacy = newest.get(CONTEXT);
  if (legacy) {
    if (legacy.state === "success") return "green";
    if (legacy.state === "failure" || legacy.state === "error") return "red";
    if (legacy.state === "pending" && /^compiled-ok/.test(legacy.description ?? "")) return "compiled-ok";
  }
  return null;
}
// Pure: shas newest first (first-parent of origin/main); statusOf(sha) -> status or null. The newest checked
// one, and how many commits it sits behind the head. null when none in the list is checked.
export function pickChecked(shas, statusOf) {
  for (let i = 0; i < shas.length; i++) {
    const st = statusOf(shas[i]);
    if (isChecked(st)) return { sha: shas[i], status: st, behind: i, head: shas[0] };
  }
  return null;
}

function ghStatuses(dir, sha) {
  const r = spawnSync("gh", ["api", `repos/{owner}/{repo}/commits/${sha}/statuses?per_page=100`], { cwd: dir, encoding: "utf8", windowsHide: true, timeout: 20000 });
  if (r.status !== 0) return undefined; // gh unavailable: the caller falls back to the cache
  try { return JSON.parse(r.stdout); } catch { return undefined; }
}

// The guard verdict of one commit from GitHub: a status, null (none posted) or undefined (gh failed).
export function ghStatus(dir, sha) {
  const list = ghStatuses(dir, sha);
  return list === undefined ? undefined : guardFromStatuses(list);
}

// The newest checked commit of origin/<base> as seen from `dir` (fetched by the caller): a GitHub verdict first
// for at most `remoteLookups` commits; the local hub file only when gh cannot answer.
export function resolveChecked(dir, { hub, base = "main", limit = 200, remoteLookups = 40, gh = ghStatus } = {}) {
  const r = spawnSync("git", ["-C", dir, "rev-list", "--first-parent", `-n${limit}`, `refs/remotes/origin/${base}`], { encoding: "utf8", windowsHide: true });
  const shas = String(r.stdout ?? "").split(/\r?\n/).filter(Boolean);
  const local = hub && existsSync(statePathOf(hub)) ? readHeads(hub).heads : {};
  let asked = 0;
  return pickChecked(shas, (sha) => {
    if (asked++ < remoteLookups) {
      const g = gh(dir, sha);
      if (g !== undefined && g !== null) return g; // a GitHub verdict wins; none posted or gh down: the cache
    }
    return local[sha]?.status ?? null;
  });
}

// The status that withdraws a stage-2 red set by an infra refusal: main-guard/suite pending (newest wins), so the
// commit reads compiled-ok again (guardFromStatuses) until the full check passes.
export function infraToGh(why) {
  return { context: SUITE_CTX, state: "pending", description: `infra, not red: ${why}; the full check is retried`.slice(0, 140) };
}
export function postInfra(dir, sha, why) {
  const g = infraToGh(why);
  return spawnSync("gh", ["api", "-X", "POST", `repos/{owner}/{repo}/statuses/${sha}`, "-f", `state=${g.state}`, "-f", `context=${g.context}`, "-f", `description=${g.description}`], { cwd: dir, encoding: "utf8", windowsHide: true, timeout: 20000 });
}

// Post a verdict on a commit (the guard). Returns the last spawn result; failures are logged by the caller.
export function postStatus(dir, sha, status, step = "", note = "") {
  let r = { status: 0 };
  for (const g of guardToGh(status, step, note)) {
    r = spawnSync("gh", ["api", "-X", "POST", `repos/{owner}/{repo}/statuses/${sha}`, "-f", `state=${g.state}`, "-f", `context=${g.context}`, "-f", `description=${g.description}`], { cwd: dir, encoding: "utf8", windowsHide: true, timeout: 20000 });
  }
  return r;
}

// ---- One guard for the repo: a lease in the main-guard/lease status on the repository's root commit, which never
// moves. Description "held <host> <pid> <iso>" or "released <host>". Newest status wins.
export function parseLease(st) {
  if (!st) return null;
  const m = /^(held|released) (\S+)(?: (\d+) (\S+))?/.exec(st.description ?? "");
  if (!m) return null;
  return { state: m[1], host: m[2], pid: m[3] ? Number(m[3]) : null, at: m[4] ?? st.created_at ?? null };
}

// The host name a lease carries: the machine name with whitespace runs as "-" (the lease description is
// space-separated). Until #2764 the cleanup was /s+/g, which turned every run of the letter "s" into "-":
// "lf-testbox" was written as "lf-te-tbox". The legacy form is still what an older guard writes, so a lease
// in either spelling is ours (sameLeaseHost); new leases use the real name.
export function leaseHostName(raw) {
  return String(raw).replace(/\s+/g, "-");
}
export function legacyLeaseHostName(raw) {
  return String(raw).replace(/s+/g, "-"); // lf-regex-ok: the pre-#2764 lease host spelling, kept so existing leases still match
}
export function sameLeaseHost(leaseHost, host) {
  return leaseHost === host || leaseHost === legacyLeaseHostName(host);
}

// Pure: may `host` take the lease? { ok, holder, why }.
export function leaseDecision(lease, host, now = Date.now(), ttl = LEASE_TTL_MS) {
  if (!lease || lease.state !== "held") return { ok: true, why: "free" };
  if (sameLeaseHost(lease.host, host)) return { ok: true, holder: lease, why: "ours" };
  const age = now - Date.parse(lease.at ?? "");
  if (!Number.isFinite(age) || age > ttl) return { ok: true, holder: lease, why: `stale lease of ${lease.host}` };
  return { ok: false, holder: lease, why: `the main guard runs on ${lease.host} (pid ${lease.pid}, lease renewed ${Math.round(age / 60000)} min ago)` };
}

export function rootCommit(dir) {
  const r = spawnSync("git", ["-C", dir, "rev-list", "--max-parents=0", "refs/remotes/origin/main"], { encoding: "utf8", windowsHide: true });
  return String(r.stdout ?? "").split(/\r?\n/).filter(Boolean).pop() ?? null;
}

// GitHub keeps at most 1000 statuses per commit and context ("This SHA and context has reached the maximum number of
// statuses", HTTP 422). The lease is one status per renewal (every 5 min) on one commit, so the single context
// `main-guard/lease` filled up on 2026-11-20 and every renewal after it failed in silence: the lease sat 65 h old under a
// live, working worker. A lease now goes to a context of the day (`main-guard/lease/20261122`: at most 288 renewals), and
// a reader takes the newest status of any context that is `main-guard/lease` or under it.
export const isLeaseCtx = (c) => c === LEASE_CTX || String(c ?? "").startsWith(`${LEASE_CTX}/`);
export const leaseContextFor = (now = Date.now(), n = 1) => `${LEASE_CTX}/${new Date(now).toISOString().slice(0, 10).replace(/-/g, "")}${n > 1 ? `-${n}` : ""}`;

// Pure: a commit's statuses (newest first, as the API lists them) -> the lease, null (none) .
export function leaseFromStatuses(list) {
  const mine = (list ?? []).filter((s) => isLeaseCtx(s?.context)).sort((a, b) => String(b.created_at).localeCompare(String(a.created_at)));
  return parseLease(mine[0]) ?? null;
}

export function readLease(dir) {
  const root = rootCommit(dir);
  if (!root) return undefined;
  const list = ghStatuses(dir, root);
  if (list === undefined) return undefined;
  return leaseFromStatuses(list);
}

// Post one lease status; returns the gh spawn result. `post` is injectable for the selftest.
function postLease(dir, root, ctx, state, description) {
  return spawnSync("gh", ["api", "-X", "POST", `repos/{owner}/{repo}/statuses/${root}`, "-f", `state=${state === "held" ? "pending" : "success"}`, "-f", `context=${ctx}`, "-f", `description=${description}`], { cwd: dir, encoding: "utf8", windowsHide: true, timeout: 20000 });
}

// What a failed post says, one line (gh prints GitHub's message on stderr or in the JSON on stdout).
export const postWhy = (r) => String(r?.stderr || r?.stdout || r?.error?.message || `exit ${r?.status ?? "?"}`).replace(/\s+/g, " ").trim().slice(0, 200);
export const isStatusCap = (r) => /maximum number of statuses/i.test(String(r?.stderr ?? "") + String(r?.stdout ?? ""));

// Write the lease and say whether it landed: { ok, ctx, why }. A context that is full moves to the next one of the day
// (`-2`, `-3`); any other failure is returned, never swallowed (the caller logs it).
export function writeLeaseChecked(dir, state, host, pid, { now = Date.now(), post = postLease, root = rootCommit(dir) } = {}) {
  if (!root) return { ok: false, ctx: null, why: "no root commit" };
  const description = state === "held" ? `held ${host} ${pid} ${new Date(now).toISOString()}` : `released ${host}`;
  let last = null;
  for (let n = 1; n <= 4; n++) {
    const ctx = leaseContextFor(now, n);
    last = post(dir, root, ctx, state, description);
    if (last?.status === 0) return { ok: true, ctx, why: "" };
    if (!isStatusCap(last)) break;
  }
  return { ok: false, ctx: null, why: postWhy(last) };
}

export function writeLease(dir, state, host, pid) {
  const r = writeLeaseChecked(dir, state, host, pid);
  return { status: r.ok ? 0 : 1, stderr: r.why, ...r };
}
