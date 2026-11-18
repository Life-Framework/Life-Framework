// Keeps agents from writing into the lead's main checkout (#2785).
//
// On 2026-11-17/18 three agents wrote a localization diff and several LF_Test_*.c files into C:\LF\agents\Life-Framework
// instead of their worktree, and each one blocked the lead's `git pull --ff-only`. Two cheap pieces stop it:
//
//   1. A Claude Code PreToolUse hook (.claude/settings.json -> tools/hooks/main-write-guard.mjs) calls `decideWrite`
//      for Write, Edit, MultiEdit and NotebookEdit and denies a file under the main checkout's tracked areas when the
//      session is an agent working in a worktree. Pure fs, no git process: about 60 ms a call.
//   2. `tools\cli lead` calls `findStrays`/`moveStrays` and moves untracked leftovers out of the main checkout.
//
// "The session is an agent" is either of two facts, because most agents are launched with the main checkout as their
// cwd and only `cd` into the worktree for shell commands:
//   - the cwd is a linked worktree, or
//   - `wt new` ran in this session: it writes <hub>/session-claims/<sessionId>.json (the session id is the one the
//     hook receives, and CLAUDE_CODE_SESSION_ID in the tool environment).
// A session with neither fact (the lead, the maintainer working in the main checkout) is never blocked. A claim ends
// when its worktree folder is gone (`wt prune`) or after CLAIM_TTL_MS. LF_ALLOW_MAIN_WRITE=1 turns the hook off.
import { existsSync, mkdirSync, readFileSync, readdirSync, rmSync, statSync, writeFileSync } from "node:fs";
import { basename, dirname, isAbsolute, join, relative, resolve } from "node:path";

// Top-level folders of the main checkout an agent must reach only through its worktree. `tmp/` and the local config
// files are deliberately not here.
export const GUARDED = ["addons", "tools", "docs", "website", "agent-box", ".claude", ".githooks", ".github"];
export const CLAIM_TTL_MS = 48 * 3600 * 1000;
const WRITE_TOOLS = new Set(["Write", "Edit", "MultiEdit", "NotebookEdit"]);

// ---------------------------------------------------------------- paths

// Backslashes to slashes, the Git Bash `/c/x` form to `C:/x`, `.`/`..` folded, no trailing slash; case-folded for compares.
export function norm(p) {
  let s = String(p ?? "").replace(/\\/g, "/");
  const m = /^\/([a-zA-Z])(\/|$)/.exec(s);
  if (m) s = `${m[1].toUpperCase()}:/${s.slice(m[0].length)}`;
  s = resolve(s).replace(/\\/g, "/");
  return s.length > 3 ? s.replace(/\/+$/, "") : s;
}
const key = (p) => norm(p).toLowerCase();

export function isInside(child, parent) {
  const c = key(child);
  const p = key(parent);
  return c === p || c.startsWith(p.endsWith("/") ? p : `${p}/`);
}

// The checkout that holds `start` (walking up to a `.git`): { root, linked, mainRoot } or null.
// A linked worktree's `.git` is a file `gitdir: <main>/.git/worktrees/<name>`; the main checkout's is a folder.
export function repoOf(start) {
  if (!start) return null;
  let dir = norm(start);
  for (let i = 0; i < 64; i++) {
    const git = join(dir, ".git");
    if (existsSync(git)) {
      try {
        if (statSync(git).isDirectory()) return { root: dir, linked: false, mainRoot: dir };
        const m = /^gitdir:\s*(.+)$/m.exec(readFileSync(git, "utf8"));
        if (!m) return null;
        const gitdir = norm(m[1].trim());
        // <main>/.git/worktrees/<name> -> <main>
        if (basename(dirname(gitdir)) !== "worktrees") return null;
        return { root: dir, linked: true, mainRoot: norm(dirname(dirname(dirname(gitdir)))) };
      } catch {
        return null;
      }
    }
    const up = dirname(dir);
    if (up === dir) return null;
    dir = up;
  }
  return null;
}

// ---------------------------------------------------------------- session claims

const safeId = (id) => String(id ?? "").replace(/[^A-Za-z0-9_-]/g, "").slice(0, 80);
export const claimDir = (mainRoot) => join(mainRoot, "tmp", "wt", "session-claims");

// `wt new` calls this: this session now owns the worktree `path`. Best effort; never throws.
export function recordClaim(mainRoot, { sessionId, slug, path, now = Date.now() }) {
  const id = safeId(sessionId);
  if (!id) return false;
  try {
    const dir = claimDir(mainRoot);
    mkdirSync(dir, { recursive: true });
    const file = join(dir, `${id}.json`);
    let worktrees = [];
    try {
      worktrees = JSON.parse(readFileSync(file, "utf8")).worktrees ?? [];
    } catch { /* first claim */ }
    worktrees = worktrees.filter((w) => w.slug !== slug);
    worktrees.push({ slug, path: norm(path), at: now });
    writeFileSync(file, JSON.stringify({ sessionId: id, worktrees }, null, 2) + "\n");
    // Housekeeping: claims nobody refreshed for a week go.
    for (const f of readdirSync(dir)) {
      try {
        if (now - statSync(join(dir, f)).mtimeMs > 7 * 24 * 3600 * 1000) rmSync(join(dir, f), { force: true });
      } catch { /* raced with another agent */ }
    }
    return true;
  } catch {
    return false;
  }
}

// The newest live claim of this session: a worktree folder that still exists and was claimed within the TTL.
export function liveClaim(mainRoot, sessionId, now = Date.now()) {
  const id = safeId(sessionId);
  if (!id) return null;
  try {
    const rec = JSON.parse(readFileSync(join(claimDir(mainRoot), `${id}.json`), "utf8"));
    const live = (rec.worktrees ?? []).filter((w) => now - w.at <= CLAIM_TTL_MS && existsSync(w.path));
    live.sort((a, b) => b.at - a.at);
    return live[0] ?? null;
  } catch {
    return null;
  }
}

// ---------------------------------------------------------------- the decision

// input: the hook payload { tool_name, tool_input, cwd, session_id, transcript_path }.
// ctx: { env, projectDir, now }. Returns { block: false } or { block: true, reason, worktree, rel }.
export function decideWrite(input, ctx = {}) {
  const env = ctx.env ?? process.env;
  if (env.LF_ALLOW_MAIN_WRITE === "1") return { block: false };
  if (!WRITE_TOOLS.has(input?.tool_name)) return { block: false };
  const ti = input.tool_input ?? {};
  const target = ti.file_path ?? ti.notebook_path ?? ti.path;
  if (!target || typeof target !== "string") return { block: false };

  const cwd = input.cwd || ctx.projectDir || process.cwd();
  const repo = repoOf(cwd) ?? repoOf(ctx.projectDir);
  if (!repo) return { block: false };
  const abs = isAbsolute(target) || /^\/[a-zA-Z]\//.test(target) ? norm(target) : norm(resolve(cwd, target));
  if (!isInside(abs, repo.mainRoot)) return { block: false };
  const rel = norm(abs).slice(norm(repo.mainRoot).length).replace(/^\/+/, "");
  if (!GUARDED.includes(rel.split("/")[0])) return { block: false };
  if (repo.linked && isInside(abs, repo.root)) return { block: false }; // a worktree nested inside the main folder

  let worktree = repo.linked ? repo.root : null;
  if (!worktree) {
    const sessionId = input.session_id || basename(String(input.transcript_path ?? ""), ".jsonl");
    worktree = liveClaim(repo.mainRoot, sessionId, ctx.now)?.path ?? null;
  }
  if (!worktree) return { block: false }; // the lead or the maintainer working in the main checkout itself

  const reason =
    `Refused: ${norm(abs)} is inside the lead's main checkout (${norm(repo.mainRoot)}), and this session works in the worktree ${worktree}. ` +
    `Write ${worktree}/${rel} instead (use absolute paths under your worktree; never a relative path). ` +
    `Stray files there block the lead's git pull (#2785).`;
  return { block: true, reason, worktree, rel };
}

// ---------------------------------------------------------------- strays (the lead's side)

// Untracked files under GUARDED in the main checkout, from `git status --porcelain -z -uall`; modified tracked files are
// reported but never moved. `porcelain` is the raw NUL-separated output.
export function parseStatus(porcelain) {
  const untracked = [];
  const modified = [];
  for (const rec of String(porcelain ?? "").split("\0")) {
    if (rec.length < 4) continue;
    const code = rec.slice(0, 2);
    const path = rec.slice(3);
    if (!GUARDED.includes(path.split("/")[0])) continue;
    if (code === "??") untracked.push(path);
    else if (code.trim()) modified.push(path);
  }
  return { untracked, modified };
}

// Files older than `minAgeMs` only: a file written a moment ago may be a write in flight or the maintainer's own.
export function findStrays(mainRoot, status, { now = Date.now(), minAgeMs = 2 * 60 * 1000 } = {}) {
  const move = [];
  const fresh = [];
  for (const rel of status.untracked) {
    let age = Infinity;
    try {
      age = now - statSync(join(mainRoot, rel)).mtimeMs;
    } catch { continue; } // gone already
    (age >= minAgeMs ? move : fresh).push(rel);
  }
  return { move, fresh, modified: status.modified };
}

// Where moved files go: <lf root>/lead/stray beside the agents folder when that exists (C:\LF\lead\stray), else <main>/tmp/stray.
export function strayRootOf(mainRoot, env = process.env) {
  if (env.LF_LEAD_STRAY_DIR) return resolve(env.LF_LEAD_STRAY_DIR);
  const lf = dirname(dirname(mainRoot));
  if (existsSync(join(lf, "lead"))) return join(lf, "lead", "stray");
  return join(mainRoot, "tmp", "stray");
}

// Moves each file to <strayRoot>/<stamp>/<rel>. Never overwrites, never deletes anything but the moved original. Returns the moved list.
export function moveStrays(mainRoot, files, strayRoot, { stamp = new Date().toISOString().replace(/[:.]/g, "-") } = {}) {
  const moved = [];
  for (const rel of files) {
    const from = join(mainRoot, rel);
    const to = join(strayRoot, stamp, rel);
    try {
      mkdirSync(dirname(to), { recursive: true });
      writeFileSync(to, readFileSync(from), { flag: "wx" });
      rmSync(from, { force: true });
      moved.push({ rel, to });
    } catch { /* unreadable or exists: leave it for the next pass */ }
  }
  return moved;
}
