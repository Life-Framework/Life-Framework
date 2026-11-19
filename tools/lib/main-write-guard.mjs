// Keeps agents from writing into the lead's main checkout (#2785).
//
// On 2026-11-17/18 three agents wrote a localization diff and several LF_Test_*.c files into C:\LF\agents\Life-Framework
// instead of their worktree, and each one blocked the lead's `git pull --ff-only`. Two cheap pieces stop it:
//
//   1. A Claude Code PreToolUse hook (.claude/settings.json -> tools/hooks/main-write-guard.mjs) calls `decideWrite`
//      for Write, Edit, MultiEdit and NotebookEdit (and, #3700, a Bash or PowerShell command that writes a file) and denies
//      a file under the main checkout's tracked areas when the session is an agent working in a worktree. Pure fs, no git
//      process: about 60 ms a call.
//   2. `tools\cli lead` calls `findStrays`/`moveStrays` and moves untracked leftovers out of the main checkout. A tracked
//      edit that blocks the fast-forward is stashed by tools/lib/main-ff-heal.mjs (#3700).
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
const SHELL_TOOLS = new Set(["Bash", "PowerShell"]);

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

// ---------------------------------------------------------------- shell commands (#3700)

// The Write/Edit hook cannot see a shell write, and twice on 2026-11-19 a `sed -i` / script edit in the main checkout
// (docs/features/sites.md, tools/town/drops.mjs) blocked its fast-forward. `shellWriteTargets` reads a Bash or PowerShell
// command line and returns the paths it writes: a `>` / `>>` redirect, `tee`, an in-place `sed`/`perl`, Set-Content /
// Add-Content / Out-File / New-Item / Remove-Item / Move-Item / Rename-Item, rm, mv, touch, and the destination of cp /
// Copy-Item. It is a heuristic over quoted tokens, not a shell: a script that writes by itself (`node x.mjs`) is not seen
// (the lead's stray pass is the net for those), and a read (`cat`, `grep`, a `cp` source) never counts. Relative paths
// resolve against the cwd the command starts in, followed through `cd`/`pushd`/`Set-Location` in the same command line.
const WRITE_ANY = new Set([
  "tee", "set-content", "add-content", "out-file", "new-item", "remove-item", "move-item", "rename-item", "clear-content",
  "rm", "del", "erase", "ri", "mv", "move", "ren", "rename", "touch", "mkdir", "rmdir", "truncate", "patch", "sc", "ac", "ni", "mi", "rni",
]);
const COPY = new Set(["cp", "copy", "copy-item", "cpi"]);
const CD = new Set(["cd", "chdir", "pushd", "set-location", "sl"]);

// Splits on unquoted ; && || | & and newlines; each token is { text, quoted }. A `>&2` / `2>&1` stays in its segment.
function shellSegments(command) {
  const segs = [];
  let tokens = [];
  let cur = "";
  let quoted = false;
  let started = false;
  let quote = "";
  const endToken = () => {
    if (started) tokens.push({ text: cur, quoted });
    cur = ""; quoted = false; started = false;
  };
  const endSeg = () => { endToken(); if (tokens.length) segs.push(tokens); tokens = []; };
  const s = String(command);
  for (let i = 0; i < s.length; i++) {
    const c = s[i];
    if (quote) {
      if (c === quote) quote = "";
      else if (c === "\\" && quote === '"' && s[i + 1] === '"') { cur += '"'; i++; }
      else cur += c;
      continue;
    }
    if (c === '"' || c === "'") { quote = c; quoted = true; started = true; continue; }
    if (c === ";" || c === "\n" || c === "|") { endSeg(); continue; }
    if (c === "&") {
      if (s[i - 1] === ">" || s[i + 1] === ">") { cur += c; started = true; continue; } // >&2, &>file
      endSeg();
      continue;
    }
    if (/\s/.test(c)) { endToken(); continue; }
    if (c === "`" && s[i + 1]) { cur += s[i + 1]; started = true; i++; continue; } // PowerShell escape
    cur += c;
    started = true;
  }
  endSeg();
  return segs;
}

const unwrap = (t) => t.replace(/^[(@{]+/, "").replace(/[)}]+$/, "");
const isFlag = (t) => /^-{1,2}[A-Za-z]/.test(t.text) && !t.quoted;
const isPathLike = (t) => t.length > 0 && !/[$*?%<>|]/.test(t) && !t.startsWith("~");

// startCwd: where the command starts. Returns [{ path, cwd }]: each written path as typed plus the cwd it resolves from
// (null when a `cd $var` made it unknown, so a relative path is skipped rather than guessed).
export function shellWriteTargets(command, startCwd) {
  const out = [];
  let cwd = startCwd ?? null;
  for (const seg of shellSegments(command)) {
    const names = seg.map((t) => (t.quoted ? "" : unwrap(t.text).replace(/^.*[\\/]/, "").toLowerCase().replace(/\.exe$/, "")));
    const first = names.find((n) => n && n !== "sudo" && n !== "&" && n !== "command") ?? "";

    if (CD.has(first)) {
      const arg = seg.slice(names.indexOf(first) + 1).find((t) => !isFlag(t));
      if (arg && isPathLike(unwrap(arg.text)) && arg.text !== "-") cwd = cwd ? norm(isAbsolute(arg.text) || /^\/[a-zA-Z]\//.test(arg.text) ? arg.text : resolve(cwd, unwrap(arg.text))) : /^([a-zA-Z]:|\/)/.test(arg.text) ? norm(arg.text) : null;
      else cwd = null;
      continue;
    }

    // redirects: `> f`, `>> f`, `1>f`, `2> f` (not `2>&1`, not /dev/null)
    for (let i = 0; i < seg.length; i++) {
      const t = seg[i];
      if (t.quoted) continue;
      const m = /^\d?&?>{1,2}(.*)$/.exec(t.text);
      if (!m || /^&\d?$/.test(m[1])) continue;
      const target = m[1] ? m[1] : seg[i + 1]?.text;
      if (target && isPathLike(unwrap(target)) && !/^\/dev\//.test(target) && !/^\$?null$/i.test(target)) out.push({ path: unwrap(target), cwd });
    }

    const vi = names.indexOf(first);
    const args = seg.filter((t, i) => i !== vi && !isFlag(t) && (t.quoted || !/^\d?&?>/.test(t.text)));
    const inPlace = (first === "sed" || first === "perl") && seg.some((t) => !t.quoted && (/^-[a-zA-Z]*i[a-zA-Z.]*$/.test(t.text) || /^--in-place/.test(t.text)));
    if (WRITE_ANY.has(first) || inPlace) {
      for (const t of args) if (isPathLike(unwrap(t.text))) out.push({ path: unwrap(t.text), cwd });
    } else if (COPY.has(first)) {
      const di = seg.findIndex((t) => !t.quoted && /^-destination$/i.test(t.text));
      const dest = di >= 0 ? seg[di + 1] : args[args.length - 1];
      if (dest && isPathLike(unwrap(dest.text))) out.push({ path: unwrap(dest.text), cwd });
    }
  }
  return out;
}

// ---------------------------------------------------------------- the decision

// The worktree an agent session works in: the cwd's linked worktree, else the session's live `wt new` claim; null for the
// lead or the maintainer working in the main checkout itself.
function agentWorktree(input, repo, ctx) {
  if (repo.linked) return repo.root;
  const sessionId = input.session_id || basename(String(input.transcript_path ?? ""), ".jsonl");
  return liveClaim(repo.mainRoot, sessionId, ctx.now)?.path ?? null;
}

// The deny for `abs` (a normalised path), or null when it is not a guarded path of the main checkout.
function denyFor(abs, repo, input, ctx, how) {
  if (!isInside(abs, repo.mainRoot)) return null;
  const rel = norm(abs).slice(norm(repo.mainRoot).length).replace(/^\/+/, "");
  if (!GUARDED.includes(rel.split("/")[0])) return null;
  if (repo.linked && isInside(abs, repo.root)) return null; // a worktree nested inside the main folder
  const worktree = agentWorktree(input, repo, ctx);
  if (!worktree) return null; // the lead or the maintainer working in the main checkout itself
  const reason =
    `Refused: ${norm(abs)} is inside the lead's main checkout (${norm(repo.mainRoot)}), and this session works in the worktree ${worktree}. ` +
    `${how} ${worktree}/${rel} instead (use absolute paths under your worktree; never a relative path). ` +
    `Stray edits there block the lead's git pull (#2785, #3700).`;
  return { block: true, reason, worktree, rel };
}

// input: the hook payload { tool_name, tool_input, cwd, session_id, transcript_path }.
// ctx: { env, projectDir, now }. Returns { block: false } or { block: true, reason, worktree, rel }.
export function decideWrite(input, ctx = {}) {
  const env = ctx.env ?? process.env;
  if (env.LF_ALLOW_MAIN_WRITE === "1") return { block: false };
  const tool = input?.tool_name;
  const cwd = input?.cwd || ctx.projectDir || process.cwd();

  if (SHELL_TOOLS.has(tool)) {
    const command = input.tool_input?.command;
    if (typeof command !== "string" || !command) return { block: false };
    const targets = shellWriteTargets(command, cwd);
    if (!targets.length) return { block: false }; // no write verb: no file system work at all
    const repo = repoOf(cwd) ?? repoOf(ctx.projectDir);
    if (!repo) return { block: false };
    for (const t of targets) {
      let abs;
      if (isAbsolute(t.path) || /^\/[a-zA-Z]\//.test(t.path)) abs = norm(t.path);
      else if (t.cwd) abs = norm(resolve(t.cwd, t.path));
      else continue;
      const d = denyFor(abs, repo, input, ctx, "Run that command on");
      if (d) return d;
    }
    return { block: false };
  }

  if (!WRITE_TOOLS.has(tool)) return { block: false };
  const ti = input.tool_input ?? {};
  const target = ti.file_path ?? ti.notebook_path ?? ti.path;
  if (!target || typeof target !== "string") return { block: false };

  const repo = repoOf(cwd) ?? repoOf(ctx.projectDir);
  if (!repo) return { block: false };
  const abs = isAbsolute(target) || /^\/[a-zA-Z]\//.test(target) ? norm(target) : norm(resolve(cwd, target));
  return denyFor(abs, repo, input, ctx, "Write") ?? { block: false };
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
