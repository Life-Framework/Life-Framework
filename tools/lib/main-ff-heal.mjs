// Keeps a stray tracked edit in the main checkout from blocking its fast-forward (#3700).
//
// Twice on 2026-11-19 an agent's tracked edit in the main checkout (docs/features/sites.md, then tools/town/drops.mjs, both
// already merged through their PRs) made `git merge --ff-only origin/main` fail: the main guard stalled about 2 h the first
// time, and the checkout fell 173 commits behind the second. The guard's own fast-forward (tools/main-guard.mjs
// freshenMainCheckout) and the lead view (tools/commands/lead.mjs strayPass) call `healBlockedFastForward`:
//
//   - the fast-forward works: nothing else happens;
//   - it fails and tracked files are modified: every one that is not the maintainer's goes to `git stash push` under a label
//     that names the files (never discarded, `git stash pop` brings it back), the fast-forward is retried, and ONE alert
//     line says what was stashed;
//   - a Worlds/ file is the maintainer's hand work in the world-editor flow (docs/worktrees.md "Maintainer world edits"):
//     it is never stashed or touched, and an alert names it instead;
//   - an untracked file in the way is the lead's stray mover (`lead stray`), not this: the alert says so.
//
// The alert is also written to <hub>/main-ff-alert.json, which the lead view prints as a MAINFF line for 12 h.
import { mkdirSync, readFileSync, renameSync, writeFileSync } from "node:fs";
import { join } from "node:path";

export const ALERT_TTL_MS = 12 * 3600 * 1000;

// A path under a Worlds/ folder (addons/<addon>/Worlds/...): the maintainer's world-editor flow.
export function maintainerPath(p) {
  return /(^|\/)Worlds\//.test(String(p).replace(/\\/g, "/"));
}

// Tracked paths out of `git status --porcelain -z -uno`: a rename or copy carries its old path as a second record, which is skipped.
export function parseTracked(porcelain) {
  const out = [];
  const recs = String(porcelain ?? "").split("\0");
  for (let i = 0; i < recs.length; i++) {
    const rec = recs[i];
    if (rec.length < 4) continue;
    const code = rec.slice(0, 2);
    if (code === "??" || code === "!!") continue;
    out.push(rec.slice(3));
    if (code[0] === "R" || code[0] === "C" || code[1] === "R" || code[1] === "C") i++;
  }
  return out;
}

// Untracked paths out of `git status --porcelain -z -uall` (a rename's second record is skipped like parseTracked does).
export function parseUntracked(porcelain) {
  const out = [];
  const recs = String(porcelain ?? "").split("\0");
  for (let i = 0; i < recs.length; i++) {
    const rec = recs[i];
    if (rec.length < 4) continue;
    const code = rec.slice(0, 2);
    if (code === "??") out.push(rec.slice(3));
    else if (code[0] === "R" || code[0] === "C" || code[1] === "R" || code[1] === "C") i++;
  }
  return out;
}

// tracked: edited files; untracked: files origin/main now tracks that sit untracked in the checkout (#3795).
export function stashLabel(files, now = new Date(), untracked = []) {
  const all = [...files, ...untracked];
  const shown = all.slice(0, 3).join(", ");
  const more = all.length > 3 ? ` (+${all.length - 3} more)` : "";
  const what = untracked.length ? "stray edits and untracked copies of files main added, blocking" : "stray tracked edits blocking";
  return `lf-main-ff-heal ${now.toISOString()}: ${what} the fast-forward: ${shown}${more}`;
}

const firstLine = (r) => String(r?.stderr || r?.stdout || "").trim().split(/\r?\n/)[0].slice(0, 160);
const list = (files) => `${files.slice(0, 6).join(", ")}${files.length > 6 ? `, +${files.length - 6} more` : ""}`;

// The untracked paths that origin/main tracks (those make `git merge --ff-only` stop with "untracked working tree files would be
// overwritten"). Checked in chunks so a long list stays under the command-line limit.
function collidingUntracked(run, untracked) {
  const hit = new Set();
  for (let i = 0; i < untracked.length; i += 100) {
    const chunk = untracked.slice(i, i + 100);
    const r = run(["ls-tree", "-r", "-z", "--name-only", "origin/main", "--", ...chunk]);
    if (r.status !== 0) continue;
    for (const p of String(r.stdout ?? "").split("\0")) if (p) hit.add(p);
  }
  return untracked.filter((p) => hit.has(p));
}

// run(args) runs git in the main checkout and returns { status, stdout, stderr }. The caller has checked the branch is main.
// Returns { action: "ff" | "stashed" | "maintainer" | "blocked", files, untracked, kept, line }: `line` is the one alert line (absent for "ff").
export function healBlockedFastForward(mainRoot, run, { now = new Date() } = {}) {
  const ff = () => run(["merge", "--ff-only", "-q", "origin/main"]);
  const first = ff();
  if (first.status === 0) return { action: "ff", files: [], untracked: [], kept: [] };

  const st = run(["status", "--porcelain", "-z", "-uall"]);
  const tracked = st.status === 0 ? parseTracked(st.stdout) : [];
  const colliding = st.status === 0 ? collidingUntracked(run, parseUntracked(st.stdout)) : [];
  if (!tracked.length && !colliding.length) {
    return { action: "blocked", files: [], untracked: [], kept: [], line: `main checkout cannot fast-forward and it is not a tracked edit or an untracked copy of a file main added (${firstLine(first) || "no message"}); look at \`git status\` by hand` };
  }
  const kept = [...tracked, ...colliding].filter(maintainerPath);
  const files = tracked.filter((p) => !maintainerPath(p));
  const untracked = colliding.filter((p) => !maintainerPath(p));
  if (!files.length && !untracked.length) {
    return { action: "maintainer", files: [], untracked: [], kept, line: `main checkout cannot fast-forward: the maintainer's world edit(s) ${list(kept)} are in the way; left alone, ship or restore them by hand` };
  }
  const all = [...files, ...untracked];

  // -u so an untracked path in the pathspec is stashed too; the pathspec keeps every other untracked file where it is.
  const stash = run(["stash", "push", ...(untracked.length ? ["-u"] : []), "-m", stashLabel(files, now, untracked), "--", ...all]);
  if (stash.status !== 0) {
    return { action: "blocked", files: [], untracked: [], kept, line: `main checkout cannot fast-forward: stashing ${list(all)} failed (${firstLine(stash) || "no message"}); restore or move them by hand` };
  }
  const second = ff();
  if (second.status === 0) {
    const parts = [];
    if (files.length) parts.push(`${files.length} file(s) an agent edited by hand (${list(files)})`);
    if (untracked.length) parts.push(`${untracked.length} untracked file(s) that origin/main now tracks (${list(untracked)})`);
    return { action: "stashed", files, untracked, kept, line: `main checkout: stashed ${parts.join(" and ")} so it could fast-forward; \`git stash pop\` in the main checkout brings them back` };
  }
  if (kept.length) {
    return { action: "maintainer", files, untracked, kept, line: `main checkout: stashed ${all.length} file(s) (${list(all)}) but the maintainer's world edit(s) ${list(kept)} still block the fast-forward; left alone` };
  }
  return { action: "blocked", files, untracked, kept, line: `main checkout: stashed ${all.length} file(s) (${list(all)}) but it still cannot fast-forward (${firstLine(second) || "no message"})` };
}

// ---------------------------------------------------------------- the alert the lead view reads

const alertFile = (hub) => join(hub, "main-ff-alert.json");

export function writeAlert(hub, line, nowMs = Date.now()) {
  try {
    mkdirSync(hub, { recursive: true });
    const f = alertFile(hub);
    writeFileSync(`${f}.tmp`, JSON.stringify({ at: nowMs, line }) + "\n");
    renameSync(`${f}.tmp`, f);
    return true;
  } catch {
    return false;
  }
}

export function readAlert(hub) {
  try {
    const a = JSON.parse(readFileSync(alertFile(hub), "utf8"));
    return typeof a?.line === "string" && Number.isFinite(a.at) ? { at: a.at, line: a.line } : null;
  } catch {
    return null;
  }
}

export const alertFresh = (alert, nowMs = Date.now()) => !!alert && nowMs - alert.at <= ALERT_TTL_MS;

// Records `line` unless it is the same alert already on file (the guard ticks every minute): true when it is new.
export function noteAlert(hub, line, nowMs = Date.now()) {
  const prev = readAlert(hub);
  if (prev && prev.line === line && alertFresh(prev, nowMs)) return false;
  writeAlert(hub, line, nowMs);
  return true;
}
