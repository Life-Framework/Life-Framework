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

// GitHub commit status <-> guard status.
export function guardToGh(status) {
  if (status === "green") return { state: "success", description: "green: full post-merge check passed" };
  if (status === "red") return { state: "failure", description: "red: post-merge check failed" };
  return { state: "pending", description: "compiled-ok: Workbench compile passed; full check pending" };
}
export function ghToGuard(st) {
  if (!st || st.context !== CONTEXT) return null;
  if (st.state === "success") return "green";
  if (st.state === "failure" || st.state === "error") return "red";
  if (st.state === "pending" && /^compiled-ok/.test(st.description ?? "")) return "compiled-ok";
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

export function ghStatus(dir, sha) {
  const r = spawnSync("gh", ["api", `repos/{owner}/{repo}/commits/${sha}/statuses?per_page=30`], { cwd: dir, encoding: "utf8", windowsHide: true, timeout: 20000 });
  if (r.status !== 0) return null;
  try {
    for (const st of JSON.parse(r.stdout)) {
      const g = ghToGuard(st);
      if (g) return g;
    }
  } catch {}
  return null;
}

// The newest checked commit of origin/<base> as seen from `dir` (fetched by the caller). The local state file
// first (the guard machine), then GitHub statuses for at most `remoteLookups` commits.
export function resolveChecked(dir, { hub, base = "main", limit = 200, remoteLookups = 40, gh = ghStatus } = {}) {
  const r = spawnSync("git", ["-C", dir, "rev-list", "--first-parent", `-n${limit}`, `refs/remotes/origin/${base}`], { encoding: "utf8", windowsHide: true });
  const shas = String(r.stdout ?? "").split(/\r?\n/).filter(Boolean);
  const local = hub && existsSync(statePathOf(hub)) ? readHeads(hub).heads : {};
  let asked = 0;
  return pickChecked(shas, (sha) => {
    if (local[sha]) return local[sha].status;
    if (asked++ >= remoteLookups) return null;
    return gh(dir, sha);
  });
}

// Post a status for a head (the guard). Failures are logged by the caller, never fatal.
export function postStatus(dir, sha, status) {
  const g = guardToGh(status);
  return spawnSync("gh", ["api", "-X", "POST", `repos/{owner}/{repo}/statuses/${sha}`, "-f", `state=${g.state}`, "-f", `context=${CONTEXT}`, "-f", `description=${g.description}`], { cwd: dir, encoding: "utf8", windowsHide: true, timeout: 20000 });
}
