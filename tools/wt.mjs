// tools/wt.mjs — Life Framework parallel-worktree library.
//
// The main checkout is the world-editor copy and is reserved for the user.
// Every agent works in its own linked git worktree (Life-Framework-ws-<slug>
// at branch ws/<slug>). This module owns the coordination state that makes
// that safe and automatic:
//
//   - the hub (git-ignored, <main-checkout>/tmp/wt/) holds the worktree
//     registry and per-worktree port allocation, so two worktrees never boot
//     the test server on the same ports;
//   - atomic lock files serialize the one genuinely shared resource (the
//     headless Workbench build) and state mutation (wt new);
//   - PR creation + auto-merge go through the `gh` CLI when available and fall
//     back to the GitHub REST API when GITHUB_TOKEN is set.
//
// All functions are resolved against any checkout: git-common-dir is the same
// from the main checkout and from every linked worktree, so the hub path is
// stable no matter where a command is invoked.

import { spawn, spawnSync } from "node:child_process";
import { closeSync, copyFileSync, existsSync, mkdirSync, openSync, readdirSync, readFileSync, renameSync, rmSync, statfsSync, statSync, unlinkSync, writeFileSync } from "node:fs";
import { dirname, join, normalize, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { findReparsePoints, recycleWorktree, removeWorktree, safeRemoveTree } from "./link-safety.mjs";
import { localSetting } from "./lib/local-config.mjs";
import { ADDONS } from "./lib/addons.mjs";
import { sessionDirOf } from "./lib/session-lease.mjs";
import * as pinLib from "./lib/guard-pin.mjs";
import { refuseMessage } from "./lib/disk-guard.mjs";
import { isMachineWide, machineLockDir } from "./lib/machine-locks.mjs";

const IS_WIN = process.platform === "win32";
const GH_ON_PATH = IS_WIN ? "gh.cmd" : "gh";
const GH_FALLBACK = "C:/Program Files/GitHub CLI/gh.exe";

// Resolve the GitHub CLI executable: PATH first, then the winget install path.
export function ghBin() {
  if (run(GH_ON_PATH, ["--version"]).status === 0) return GH_ON_PATH;
  if (existsSync(GH_FALLBACK)) return GH_FALLBACK;
  return null;
}

export function ghAvailable() {
  return ghBin() !== null;
}

export function run(cmd, args, opts = {}) {
  const res = spawnSync(cmd, args, { windowsHide: true, encoding: "utf8", stdio: ["ignore", "pipe", "pipe"], ...opts });
  if (res.error) {
    res.status = res.status ?? 1;
    res.stdout = res.stdout ?? "";
    res.stderr = res.stderr ?? `failed to spawn ${cmd}: ${res.error.message}`;
  }
  res.stdout = res.stdout ?? "";
  res.stderr = res.stderr ?? "";
  return res;
}

export function git(root, args) {
  return run("git", ["-C", root, ...args]);
}

export function gitOk(root, args) {
  return git(root, args).status === 0;
}

function sleep(ms) {
  Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
}

// ---------------------------------------------------------------- git geometry

// A checkout's common dir never moves while a process runs, and the pool queue asks for it on every
// poll (hundreds of git spawns in one wait), so a success is cached per root; a failure is not.
const commonDirs = new Map();
export function gitCommonDir(root) {
  const hit = commonDirs.get(root);
  if (hit) return hit;
  const res = git(root, ["rev-parse", "--git-common-dir"]);
  if (res.status !== 0) throw new Error(`not a git checkout: ${root}`);
  const dir = resolve(root, res.stdout.trim());
  commonDirs.set(root, dir);
  return dir;
}

// True for the checkout that owns the .git dir (the world-editor copy).
export function isMainCheckout(root) {
  return normalize(gitCommonDir(root)) === normalize(join(root, ".git"));
}

// The main checkout root, from any checkout of the repo.
export function mainRootOf(root) {
  return dirname(gitCommonDir(root));
}

export function currentBranch(root) {
  const res = git(root, ["branch", "--show-current"]);
  return res.status === 0 ? res.stdout.trim() : "";
}

export function listWorktrees(root) {
  const res = git(root, ["worktree", "list", "--porcelain"]);
  const entries = [];
  let cur = null;
  for (const line of res.stdout.split(/\r?\n/)) {
    if (!line.trim()) {
      if (cur) entries.push(cur);
      cur = null;
      continue;
    }
    if (line.startsWith("worktree ")) cur = { path: line.slice(9).trim() };
    else if (line.startsWith("branch ")) cur.branch = line.slice(7).trim().replace("refs/heads/", "");
    else if (line.startsWith("HEAD ")) cur.head = line.slice(5).trim();
  }
  if (cur) entries.push(cur);
  return entries;
}

// ---------------------------------------------------------------- hub state

export function hubDirOf(root) {
  return join(mainRootOf(root), "tmp", "wt");
}

export function ensureHub(root) {
  const hub = hubDirOf(root);
  mkdirSync(join(hub, "locks"), { recursive: true });
  return hub;
}

// Where lock `name` lives. Most locks are in this checkout's hub. A lock that guards a UDP port
// (server-2001, server-test-<k>) is machine-wide: on a PC that is also a pool box for another PC, the
// box clone's hub holds it, because the remote holder writes it there (tools/lib/machine-locks.mjs).
export function lockFileOf(root, name) {
  const shared = isMachineWide(name) ? machineLockDir(mainRootOf(root)) : null;
  if (shared) {
    mkdirSync(shared, { recursive: true });
    return join(shared, `${name}.lock`);
  }
  return join(ensureHub(root), "locks", `${name}.lock`);
}

// The lock file this checkout wrote before slot locks were shared, or null. A holder started by older
// code is still a holder: acquirers treat a live one as busy until it ends.
function legacyLockFile(root, name) {
  const own = join(ensureHub(root), "locks", `${name}.lock`);
  return lockFileOf(root, name) === own ? null : own;
}

function legacyHeld(file) {
  if (!file) return false;
  try {
    const info = JSON.parse(readFileSync(file, "utf8"));
    return !!info.pid && info.pid !== process.pid && pidAlive(info.pid);
  } catch {
    return false;
  }
}

export function readState(root) {
  const p = join(hubDirOf(root), "state.json");
  if (!existsSync(p)) return { version: 1, worktrees: {}, nextPortIndex: 0, warm: {} };
  // A read can meet the rename of a concurrent write (EPERM/EBUSY on Windows): retry briefly.
  for (let i = 0; ; i++) {
    try {
      const s = JSON.parse(readFileSync(p, "utf8"));
      s.worktrees = s.worktrees ?? {};
      s.nextPortIndex = s.nextPortIndex ?? 0;
      s.warm = s.warm ?? {};
      return s;
    } catch (e) {
      if (i < 20 && ["EPERM", "EBUSY", "ENOENT"].includes(e.code)) {
        sleep(25);
        continue;
      }
      return { version: 1, worktrees: {}, nextPortIndex: 0, warm: {} };
    }
  }
}

// Mutate state.json only under the 'wt' lock (withWtLock). The write is a temp
// file plus a rename, so a reader never sees half a file.
export function writeState(root, state) {
  const hub = ensureHub(root);
  const dest = join(hub, "state.json");
  const tmp = join(hub, `state.json.${process.pid}.tmp`);
  writeFileSync(tmp, JSON.stringify(state, null, 2) + "\n", "utf8");
  for (let i = 0; ; i++) {
    try {
      renameSync(tmp, dest);
      return;
    } catch (e) {
      if (i >= 40) throw e;
      sleep(25);
    }
  }
}

// Runs fn(state) under the 'wt' lock on a fresh read, then writes the state.
// The lock covers registry bookkeeping and git metadata steps (each well under
// a second), never a checkout, so concurrent `wt new` calls overlap.
export function withWtLock(root, fn, { waitMs = 120000 } = {}) {
  acquireLock(root, "wt", { waitMs, pollMs: 100 });
  try {
    const state = readState(root);
    const out = fn(state);
    writeState(root, state);
    return out;
  } finally {
    releaseLock(root, "wt");
  }
}

// Bring the registry in line with `git worktree list` (first run imports the
// worktrees that predate the hub; drift is repaired by writing markers).
// No addon file I/O here: `wt list` calls this. The resource database is
// copied by `wt new` and before a server boot (cli test/dev/serve, wt e2e).
export function reconcileState(root) {
  const list = listWorktrees(root);
  try {
    return withWtLock(root, (state) => reconcileInto(state, list), { waitMs: 15000 });
  } catch {
    // The lock is held longer than a bookkeeping step takes: show the registry
    // as it is rather than write it unlocked.
    return readState(root);
  }
}

// Warm-pool trees are detached (no branch), so this loop never registers them.
function reconcileInto(state, list) {
  for (const e of list) {
    if (!e.branch || e.branch === "main" || !existsSync(e.path)) continue;
    const slug = e.branch.startsWith("ws/") ? e.branch.slice(3) : e.branch;
    if (!/^[a-z0-9]+(-[a-z0-9]+)*$/.test(slug)) continue;
    if (!state.worktrees[slug]) {
      const ports = allocatePorts(state);
      state.worktrees[slug] = {
        slug,
        path: e.path,
        branch: e.branch,
        ports,
        created: new Date().toISOString(),
        status: "active",
      };
    } else {
      state.worktrees[slug].path = e.path;
      state.worktrees[slug].branch = e.branch;
    }
    // A `wt new` in flight writes its own marker (with priority) when it finishes.
    if (state.worktrees[slug].status !== "creating") writeMarker(e.path, slug);
  }
  return state;
}

// ---------------------------------------------------------------- marker / slug

export function markerPath(root) {
  return join(root, "tmp", "wt.json");
}

// priority (pool queue order: high|normal|low) is kept across rewrites. The file is written only when its content
// changes: `wt list` reconciles every worktree, and rewriting all the markers made each worktree look freshly
// touched, so `lead prune-safe` pruned nothing (2026-11-17). Returns true when it wrote.
export function writeMarker(wtRoot, slug, priority) {
  const keep = priority ?? readMarker(wtRoot)?.priority;
  const text = JSON.stringify(keep ? { slug, priority: keep } : { slug }) + "\n";
  const p = markerPath(wtRoot);
  try {
    if (readFileSync(p, "utf8") === text) return false; // unchanged: leave the mtime alone
  } catch { /* no marker yet */ }
  mkdirSync(join(wtRoot, "tmp"), { recursive: true });
  writeFileSync(p, text, "utf8");
  return true;
}

export function readMarker(root) {
  const p = markerPath(root);
  if (!existsSync(p)) return null;
  try {
    return JSON.parse(readFileSync(p, "utf8"));
  } catch {
    return null;
  }
}

export function slugOfRoot(root) {
  if (isMainCheckout(root)) return null;
  const m = readMarker(root);
  if (m && m.slug) return m.slug;
  const branch = currentBranch(root);
  if (branch && branch.startsWith("ws/")) return branch.slice(3);
  return null;
}

// ---------------------------------------------------------------- ports

export function defaultPorts() {
  return { index: 0, gamePort: 2001, a2sPort: 17777 };
}

// resourceDatabase.rdb is gitignored and required for --no-build world tests.
// Copy it from the main checkout when the worktree copy is missing, or smaller and older.
// A worktree build that registers a new resource writes a newer copy that can be smaller
// than main's; overwriting it dropped the new GUID ("Wrong GUID" on a new prefab).
// Called by `wt new`, and before a server boot by ensureResourceDatabase in cli.mjs,
// which also refreshes a copy that misses a .meta (tools/lib/rdb-fresh.mjs).
// Never commit the destination.
export function copyResourceDatabase(wtRoot, mainRoot = mainRootOf(wtRoot)) {
  let copied = false;
  for (const { name } of ADDONS) if (copyAddonResourceDatabase(wtRoot, mainRoot, name)) copied = true;
  return copied;
}

// One addon's rdb: main's copy is only ever copied when it exists, so an addon
// with no rdb (the empty pack) is skipped.
function copyAddonResourceDatabase(wtRoot, mainRoot, name) {
  const src = join(mainRoot, "addons", name, "resourceDatabase.rdb");
  const dest = join(wtRoot, "addons", name, "resourceDatabase.rdb");
  if (resolve(src) === resolve(dest)) return false;
  if (!existsSync(src)) return false;
  const srcSize = statSync(src).size;
  if (existsSync(dest)) {
    const destStat = statSync(dest);
    if (destStat.size >= srcSize) return false;
    if (destStat.mtimeMs > statSync(src).mtimeMs) return false;
  }
  mkdirSync(dirname(dest), { recursive: true });
  copyFileSync(src, dest);
  return true;
}

export function allocatePorts(state) {
  const index = state.nextPortIndex ?? 0;
  state.nextPortIndex = index + 1;
  return { index, gamePort: 2001 + index * 10, a2sPort: 17777 + index * 10 };
}

// The port pair a command in `root` must boot its test server on. Registered
// worktrees get their allocated pair; unregistered / main checkouts get the
// defaults (main is guarded against heavy commands anyway).
export function portsForRoot(root) {
  if (isMainCheckout(root)) return defaultPorts();
  const slug = slugOfRoot(root);
  if (slug) {
    const e = readState(root).worktrees?.[slug];
    if (e?.ports?.gamePort) return e.ports;
  }
  return defaultPorts();
}

export function requireWorktree(root, slug) {
  const e = readState(root).worktrees?.[slug];
  if (!e) throw new Error(`unknown worktree '${slug}' (see: cli wt list)`);
  if (!existsSync(e.path)) throw new Error(`worktree dir missing: ${e.path} (re-checkout or prune it)`);
  return e;
}

// ---------------------------------------------------------------- locks

export function pidAlive(pid) {
  try {
    process.kill(pid, 0);
    return true;
  } catch {
    return false;
  }
}

// acquireLock: exclusive-create lock file. waitMs=0 fails fast when held;
// waitMs>0 polls until it frees or the budget is spent. A lock whose owner
// process is gone is stolen. Age alone only expires malformed locks; a live
// build may legitimately run for the full 15-minute Workbench timeout.
// callerPids: while queued, give up when one of these processes is gone, so a queued run whose caller
// timed out never takes the slot later.
export function acquireLock(root, name, { waitMs = 0, stealMs = 1200000, pollMs = 500, meta = {}, callerPids = [] } = {}) {
  const lockFile = lockFileOf(root, name);
  const legacy = legacyLockFile(root, name);
  const started = Date.now();
  for (;;) {
    if (!legacyHeld(legacy) && tryTakeLock(lockFile, name, meta, stealMs)) return { name, file: lockFile };
    lockWaitCheck(name, lockFile, started, waitMs, callerPids);
    sleep(pollMs);
  }
}

// acquireLockAsync: acquireLock for a process that runs other async work at the same time. The sync
// version sleeps with Atomics.wait, which freezes the whole event loop: in `ci` the build's 15-minute
// wait on the `build` lock ran beside `validate --full`, so every check that had already finished sat
// unreaped and reported the same 908.8 s (2026-11-14). lock-async-selftest proves the loop keeps ticking.
export async function acquireLockAsync(root, name, { waitMs = 0, stealMs = 1200000, pollMs = 500, meta = {}, callerPids = [] } = {}) {
  const lockFile = lockFileOf(root, name);
  const legacy = legacyLockFile(root, name);
  const started = Date.now();
  for (;;) {
    if (!legacyHeld(legacy) && tryTakeLock(lockFile, name, meta, stealMs)) return { name, file: lockFile };
    lockWaitCheck(name, lockFile, started, waitMs, callerPids);
    await new Promise((r) => setTimeout(r, pollMs));
  }
}

// One attempt: true when this process now holds the lock. A lock whose owner is gone is removed first.
// On Windows an exclusive create of a name another process is deleting (a stale lock being stolen, a
// holder releasing) fails with EPERM, EACCES or EBUSY until the delete finishes, a few milliseconds: that
// is contention, not an error. It used to be thrown: four processes churning one lock got 5400 EPERM throws
// against 880 acquisitions (the machine-wide server, build and Workbench locks).
const LOCK_TRANSIENT = ["EPERM", "EACCES", "EBUSY"];

function tryTakeLock(lockFile, name, meta, stealMs) {
  let transient = 0;
  for (;;) {
    try {
      const fd = openSync(lockFile, "wx");
      try {
        writeFileSync(fd, JSON.stringify({ ...meta, pid: process.pid, at: new Date().toISOString(), name }));
      } finally {
        closeSync(fd);
      }
      return true;
    } catch (e) {
      if (LOCK_TRANSIENT.includes(e.code) && transient++ < 400) {
        sleep(5);
        continue;
      }
      if (e.code !== "EEXIST") throw e;
      try {
        const st = statSync(lockFile);
        let stale = false;
        try {
          const info = JSON.parse(readFileSync(lockFile, "utf8"));
          stale = !info.pid || !pidAlive(info.pid);
        } catch {
          stale = Date.now() - st.mtimeMs > stealMs;
        }
        if (!stale) return false;
        try {
          unlinkSync(lockFile);
        } catch {}
      } catch {
        // The lock vanished between open and stat: try again.
      }
    }
  }
}

function lockWaitCheck(name, lockFile, started, waitMs, callerPids) {
  if (waitMs <= 0 || Date.now() - started > waitMs) {
    throw new Error(`lock '${name}' is held (${lockFile}); retry with --wait to queue`);
  }
  if (callerPids.some((p) => p && !pidAlive(p))) throw new Error(`lock '${name}': the caller is gone; not queuing any longer`);
}

// transferLock: hand a held lock to another process (a detached server), so
// the lock stays held while that process lives and goes stale when it exits.
export function transferLock(root, name, pid) {
  const file = lockFileOf(root, name);
  const info = JSON.parse(readFileSync(file, "utf8"));
  if (info.pid !== process.pid) throw new Error(`lock '${name}' is not held by this process`);
  writeFileSync(file, JSON.stringify({ ...info, pid, at: new Date().toISOString() }));
}

// Takes a lock over from a process that holds it (a server about to restart),
// so no queued run can grab it in the gap. Throws when fromPid does not hold it.
export function adoptLock(root, name, fromPid) {
  const file = lockFileOf(root, name);
  const info = JSON.parse(readFileSync(file, "utf8"));
  if (info.pid !== fromPid) throw new Error(`lock '${name}' is held by pid ${info.pid}, not ${fromPid}`);
  writeFileSync(file, JSON.stringify({ ...info, pid: process.pid, at: new Date().toISOString() }));
}

export function releaseLock(root, name, owner = process.pid) {
  const file = lockFileOf(root, name);
  try {
    const info = JSON.parse(readFileSync(file, "utf8"));
    if (info.pid !== owner) return;
  } catch {
    return;
  }
  try {
    unlinkSync(file);
  } catch {}
}

// ---------------------------------------------------------------- worktree lifecycle

// The branch a worktree was cut from and ships to. Worktrees without recorded
// metadata (created before --base existed) stay on main.
export function baseOf(entry) {
  return entry?.base || "main";
}

// Base of the worktree checked out at `root` (main when unregistered).
export function baseOfRoot(root) {
  const slug = slugOfRoot(root);
  return slug ? baseOf(readState(root).worktrees?.[slug]) : "main";
}

export function checkBaseName(base) {
  if (!/^[A-Za-z0-9][A-Za-z0-9._/-]*$/.test(base) || base.includes("..")) throw new Error(`invalid base branch '${base}'`);
}

function defaultPin(root) {
  try {
    return pinLib.resolveChecked(root, { hub: hubDirOf(root), remoteLookups: 15 });
  } catch {
    return null;
  }
}

export function wtNew(root, slug, { waitMs = 0, base = "main", priority, warm = true, unchecked = false, resolvePin = defaultPin, freeGb = freeDiskGbAt } = {}) {
  checkBaseName(base);
  if (!/^[a-z0-9]+(-[a-z0-9]+)*$/.test(slug)) {
    throw new Error(`invalid slug '${slug}': lowercase letters, digits and single hyphens only`);
  }
  if (slug === "main") throw new Error("slug 'main' is reserved");
  const t0 = Date.now();
  const phases = {};
  const mark = (name, since) => { phases[name] = Date.now() - since; return Date.now(); };
  const main = mainRootOf(root);
  const branch = `ws/${slug}`;
  const dir = join(dirname(main), `Life-Framework-ws-${slug}`);
  // A checkout that runs the drive dry fails half way and leaves a broken tree: refuse before touching anything.
  const tooFull = refuseMessage(freeGb(dirname(main)), "the drive holding the worktrees");
  if (tooFull) throw new Error(tooFull);
  // Fetch outside the lock: it is network time, and concurrent fetches are safe
  // (a ref-lock clash leaves the refs a moment older, as before).
  git(root, ["fetch", "origin", "--prune"]);
  mark("fetch", t0);
  if (!gitOk(root, ["show-ref", "--verify", `refs/remotes/origin/${base}`])) {
    throw new Error(`base branch origin/${base} does not exist; push it first`);
  }
  const lockWait = Math.max(waitMs, 120000);
  // main: start from the newest commit the main guard marked compiled-ok or green, never an unchecked head
  // (--unchecked-main overrides). No checked commit found: origin/main, with a note.
  let start = "origin/" + base;
  let pinned = null;
  let pinNote = null;
  if (base === "main" && !unchecked) {
    const p = resolvePin(root);
    if (p) {
      start = p.sha;
      pinned = p.sha.slice(0, 8) + " (" + p.status + ", " + p.behind + " commit(s) behind origin/main)";
    } else pinNote = "no compiled-ok commit from the main guard found; based on origin/main";
  }

  // Phase 1, locked: reserve the slug and its ports, then either claim a warm
  // tree and create the branch, or register a fresh worktree without checking
  // it out. Both are git metadata only (well under a second).
  let claimed = null;
  let ports = null;
  const t1 = Date.now();
  withWtLock(root, (state) => {
    const prev = state.worktrees[slug];
    if (prev) {
      const dead = prev.status === "creating" && !(prev.pid && pidAlive(prev.pid)) && !existsSync(prev.path);
      if (!dead) throw new Error(`worktree '${slug}' is already registered`);
      git(root, ["branch", "-D", prev.branch]); // the branch of a `wt new` that died before checkout
      delete state.worktrees[slug];
    }
    if (gitOk(root, ["show-ref", "--verify", `refs/heads/${branch}`])) {
      throw new Error(`branch ${branch} already exists; prune it or pick another slug`);
    }
    if (existsSync(dir)) throw new Error(`directory already exists: ${dir}`);
    claimed = warm ? takeWarm(root, state, slug) : null;
    if (claimed) {
      const br = git(root, ["branch", branch, start]);
      // A throw here skips the state write, so the warm tree stays ready.
      if (br.status !== 0) throw new Error(`git branch ${branch} failed: ${br.stderr || br.stdout}`);
    } else {
      addFreshNoCheckout(root, dir, branch, base, false, start);
    }
    ports = allocatePorts(state);
    state.worktrees[slug] = {
      slug,
      path: dir,
      branch,
      ports,
      base,
      ...(priority ? { priority } : {}),
      created: new Date().toISOString(),
      status: "creating",
      pid: process.pid,
    };
  }, { waitMs: lockWait });
  const lockMs = Date.now() - t1;
  let t2 = mark("lock", t1);

  // Phase 2, unlocked: the slow part (checkout), overlapping other `wt new` calls.
  let fromWarm = false;
  try {
    if (claimed && claimWarm(root, claimed, dir, branch, phases)) fromWarm = true;
    else {
      if (claimed) {
        // The warm tree failed its checks or would not move: hand it to the
        // filler to discard, and make this one fresh on the branch already created.
        withWtLock(root, (state) => {
          const w = state.warm[claimed.id];
          if (w) w.status = "broken";
          addFreshNoCheckout(root, dir, branch, base, true);
        }, { waitMs: lockWait });
      }
      const co = git(dir, ["reset", "--hard", "-q"]);
      if (co.status !== 0) throw new Error(`checkout of ${dir} failed: ${co.stderr || co.stdout}`);
    }
    t2 = mark("checkout", t2);
    writeMarker(dir, slug, priority);
    copyResourceDatabase(dir, main);
    t2 = mark("marker+rdb", t2);
  } catch (e) {
    abandonNew(root, slug, dir, branch);
    throw e;
  }
  // Phase 3, locked: the entry becomes exactly what a fresh `wt new` wrote.
  withWtLock(root, (state) => {
    const e = state.worktrees[slug];
    if (e) {
      e.status = "active";
      delete e.pid;
    }
    if (fromWarm) delete state.warm[claimed.id];
  }, { waitMs: lockWait });
  mark("finish", t2);
  kickWarmFill(root);
  return { slug, path: dir, branch, ports, base, pinned, pinNote, priority, warm: fromWarm, ms: Date.now() - t0, lockMs, phases };
}

// `git worktree add --no-checkout`: registers the worktree and its branch
// (same upstream tracking as `worktree add -b <branch> origin/<base>`) without
// writing the files, which `git reset --hard` does afterwards, unlocked.
function addFreshNoCheckout(root, dir, branch, base, branchExists, start = `origin/${base}`) {
  const args = branchExists
    ? ["worktree", "add", "--no-checkout", dir, branch]
    : ["worktree", "add", "--no-checkout", dir, "-b", branch, start];
  const add = git(root, args);
  if (add.status !== 0) throw new Error(`git worktree add failed: ${add.stderr || add.stdout}`);
}

// Undo a `wt new` that failed after phase 1: the half-made tree goes through
// recycleWorktree (junction-safe), then the branch and the reservation.
function abandonNew(root, slug, dir, branch) {
  try {
    if (existsSync(dir)) recycleWorktree(mainRootOf(root), dir);
  } catch { /* leave it for wt prune --force */ }
  try {
    withWtLock(root, (state) => {
      if (existsSync(dir)) {
        const e = state.worktrees[slug];
        if (e) delete e.pid; // keep the entry so `wt prune <slug> --force` can find the leftover
        return;
      }
      git(root, ["branch", "-D", branch]);
      delete state.worktrees[slug];
    }, { waitMs: 60000 });
  } catch { /* the reservation's pid is dead now; the next `wt new <slug>` clears it */ }
}

// ---------------------------------------------------------------- warm pool
//
// K pre-checked-out worktrees on detached origin/main, so `wt new` skips the
// ~20-30 s checkout. They live beside the agent worktrees as
// Life-Framework-warm-<id> and are recorded in state.warm (never in
// state.worktrees): detached, so `wt list`, reconcileState, prs, pool prune
// and validate-worktrees never see them as agent worktrees. A claim moves the
// tree to Life-Framework-ws-<slug> with `git worktree move` (a directory rename)
// so the final path is the one a fresh worktree gets; a warm tree has never
// booted a server, so it holds no game-addons junction, and the claim refuses
// one that does. Every discard goes through recycleWorktree.

export function warmSettings(root) {
  const num = (key, dflt) => {
    const v = Number(localSetting(root, key));
    return Number.isFinite(v) && localSetting(root, key) !== undefined ? v : dflt;
  };
  // A warm tree is a full checkout (~1.5 GB here); fill only above this much
  // free disk, so the pool never eats the space tests and sessions need
  // (pool minFreeDiskGb is 5).
  return { k: Math.max(0, Math.floor(num("LF_WT_WARM", 3))), minFreeGb: num("LF_WT_WARM_MIN_FREE_GB", 12) };
}

export function freeDiskGbAt(path) {
  try {
    const s = statfsSync(path);
    return (Number(s.bavail) * Number(s.bsize)) / 1073741824;
  } catch {
    return null;
  }
}

function warmLog(root, msg) {
  try {
    const f = join(ensureHub(root), "warm.log");
    writeFileSync(f, `${new Date().toISOString()} [${process.pid}] ${msg}\n`, { flag: "a" });
  } catch { /* logging is best effort */ }
}

// Under the lock: the oldest ready warm tree, marked claimed by this process.
function takeWarm(root, state, slug) {
  const ready = Object.values(state.warm ?? {})
    .filter((w) => w.status === "ready" && existsSync(w.path))
    .sort((a, b) => String(a.created).localeCompare(String(b.created)));
  const w = ready[0];
  if (!w) return null;
  w.status = "claimed";
  w.slug = slug;
  w.pid = process.pid;
  w.claimedAt = new Date().toISOString();
  return { ...w };
}

// Unlocked: check the claimed tree is pristine, move it to its final path and
// put it on the new branch (a fast-forward from whatever main it was made at).
// false = not used (still at the warm path; the caller falls back to fresh).
function claimWarm(root, w, dir, branch, phases = {}) {
  if (!existsSync(w.path)) return false;
  let t = Date.now();
  if (findReparsePoints(w.path).length) {
    warmLog(root, `claim refused ${w.path}: it holds a link`);
    return false;
  }
  const st = git(w.path, ["status", "--porcelain", "--ignored"]);
  if (st.status !== 0 || st.stdout.trim()) {
    warmLog(root, `claim refused ${w.path}: not pristine`);
    return false;
  }
  phases.checks = Date.now() - t;
  t = Date.now();
  const mv = git(mainRootOf(root), ["worktree", "move", w.path, dir]);
  phases.move = Date.now() - t;
  t = Date.now();
  if (mv.status !== 0 || !existsSync(dir)) {
    warmLog(root, `claim could not move ${w.path}: ${(mv.stderr || mv.stdout).trim()}`);
    if (existsSync(dir)) throw new Error(`git worktree move left ${dir} behind: ${(mv.stderr || mv.stdout).trim()}`);
    return false;
  }
  const co = git(dir, ["checkout", "-q", branch]);
  if (co.status !== 0) throw new Error(`checkout ${branch} in ${dir} failed: ${co.stderr || co.stdout}`);
  phases.switch = Date.now() - t;
  warmLog(root, `claimed ${w.id} as ${dir}`);
  return true;
}

// Start the background filler (tools/wt-warm.mjs fill), detached and hidden,
// with its cwd in the main checkout so it never pins a worktree directory.
export function kickWarmFill(root) {
  if (warmSettings(root).k <= 0 || process.env.LF_WT_WARM_KICK === "0") return false;
  try {
    const script = fileURLToPath(new URL("./wt-warm.mjs", import.meta.url));
    const main = mainRootOf(root);
    const child = spawn(process.execPath, [script, "fill", main], { cwd: main, detached: true, stdio: "ignore", windowsHide: true });
    child.unref();
    return true;
  } catch {
    return false;
  }
}

// Under the lock: entries to discard (dead filler or claimer, broken), and
// entries whose tree is gone. Marks the discards so no one claims them.
function reapWarm(state) {
  const discard = [];
  for (const [id, w] of Object.entries(state.warm ?? {})) {
    const owner = w.pid && pidAlive(w.pid);
    if (w.status === "ready" && !existsSync(w.path)) delete state.warm[id];
    else if (w.status === "claimed" && !owner && !existsSync(w.path)) delete state.warm[id];
    else if (w.status === "broken" || ((w.status === "preparing" || w.status === "claimed" || w.status === "discarding") && !owner)) {
      w.status = "discarding";
      w.pid = process.pid;
      discard.push({ ...w });
    }
  }
  return discard;
}

function discardWarm(root, list) {
  const main = mainRootOf(root);
  for (const w of list) {
    let gone = false;
    try {
      gone = recycleWorktree(main, w.path);
    } catch (e) {
      warmLog(root, `discard ${w.path} failed: ${e.message}`);
    }
    withWtLock(root, (state) => {
      if (gone) delete state.warm[w.id];
      else if (state.warm[w.id]) state.warm[w.id].status = "broken";
    });
    warmLog(root, `${gone ? "discarded" : "could not discard"} ${w.path}`);
  }
}

// Fill the pool to K. One filler at a time (lock 'wt-warm'); trees are made one
// after another at below-normal priority, and only while free disk stays above
// the floor. Returns a summary.
export function wtWarmFill(root, { log = () => {} } = {}) {
  try {
    acquireLock(root, "wt-warm", { waitMs: 0 });
  } catch {
    return { skipped: "another filler is running" };
  }
  const made = [];
  try {
    const main = mainRootOf(root);
    for (;;) {
      const { k, minFreeGb } = warmSettings(root);
      const discard = withWtLock(root, (state) => reapWarm(state));
      if (discard.length) discardWarm(root, discard);
      const live = Object.values(readState(root).warm).filter((w) => w.status === "ready" || w.status === "preparing").length;
      if (live >= k) break;
      const free = freeDiskGbAt(dirname(main));
      if (free !== null && free < minFreeGb) {
        warmLog(root, `fill stopped: ${free.toFixed(1)} GB free is under the ${minFreeGb} GB warm floor`);
        log(`warm: ${free.toFixed(1)} GB free is under the ${minFreeGb} GB floor (LF_WT_WARM_MIN_FREE_GB); not filling`);
        break;
      }
      const id = Date.now().toString(36);
      const path = join(dirname(main), `Life-Framework-warm-${id}`);
      withWtLock(root, (state) => {
        const add = git(root, ["worktree", "add", "--detach", "--no-checkout", path, "origin/main"]);
        if (add.status !== 0) throw new Error(`git worktree add failed: ${add.stderr || add.stdout}`);
        state.warm[id] = { id, path, status: "preparing", pid: process.pid, created: new Date().toISOString() };
      });
      const t = Date.now();
      const co = git(path, ["reset", "--hard", "-q"]);
      if (co.status !== 0) {
        withWtLock(root, (state) => { if (state.warm[id]) state.warm[id].status = "broken"; });
        warmLog(root, `prepare ${path} failed: ${(co.stderr || co.stdout).trim()}`);
        break;
      }
      const head = git(path, ["rev-parse", "HEAD"]).stdout.trim();
      withWtLock(root, (state) => {
        const w = state.warm[id];
        if (!w) return;
        w.status = "ready";
        w.head = head;
        delete w.pid;
      });
      warmLog(root, `ready ${path} at ${head.slice(0, 8)} in ${((Date.now() - t) / 1000).toFixed(1)} s`);
      log(`warm: ready ${path}`);
      made.push(path);
    }
  } finally {
    releaseLock(root, "wt-warm");
  }
  return { made };
}

// Remove every warm tree that is not being prepared or claimed right now.
export function wtWarmDrain(root) {
  const list = withWtLock(root, (state) => {
    const out = reapWarm(state);
    for (const w of Object.values(state.warm)) {
      if (w.status !== "ready") continue;
      w.status = "discarding";
      w.pid = process.pid;
      out.push({ ...w });
    }
    return out;
  });
  discardWarm(root, list);
  return list.map((w) => w.path);
}

export function warmStatus(root) {
  const s = warmSettings(root);
  return { ...s, freeGb: freeDiskGbAt(dirname(mainRootOf(root))), trees: Object.values(readState(root).warm ?? {}) };
}

export function wtSync(root, slug) {
  const e = requireWorktree(root, slug);
  git(root, ["fetch", "origin", "--prune"]);
  const cur = currentBranch(e.path);
  if (cur !== e.branch) {
    const co = git(e.path, ["checkout", e.branch]);
    if (co.status !== 0) throw new Error(`checkout ${e.branch} failed: ${co.stderr || co.stdout}`);
  }
  const dirty = git(e.path, ["status", "--porcelain"]).stdout.trim();
  if (dirty) throw new Error(`worktree ${slug} has uncommitted changes; commit or stash first`);
  const base = baseOf(e);
  const merge = git(e.path, ["merge", `origin/${base}`, "--no-edit", "--no-ff"]);
  if (merge.status !== 0) {
    const unmerged = git(e.path, ["diff", "--name-only", "--diff-filter=U"]).stdout.trim();
    throw new Error(`merge origin/${base} into ${e.branch} failed: ${mergeFailureDetail(unmerged, merge.stderr || merge.stdout)}`);
  }
  return { slug, branch: e.branch, path: e.path, base };
}

// One line on purpose: the compact console summary keeps only the first line of a FAIL message,
// so the conflicting files must sit on it.
export function mergeFailureDetail(unmerged, raw) {
  const files = String(unmerged || "").split(/\r?\n/).map((s) => s.trim()).filter(Boolean);
  if (files.length) return `conflicts in: ${files.join(", ")} (resolve them, commit, and re-run)`;
  return String(raw || "").trim();
}

// True when merging `theirs` into the worktree branch would conflict. Uses
// `git merge-tree --write-tree` (no checkout, no index change). merge-tree may
// not run the custom drivers in tools/merge/, so a "conflict" here only means
// "try the real merge"; wtSync then applies the drivers.
export function mergeWouldConflict(wtRoot, theirs = "origin/main") {
  const res = git(wtRoot, ["merge-tree", "--write-tree", "--name-only", "HEAD", theirs]);
  if (res.status === 0) return false;
  if (res.status === 1) return true;
  throw new Error(`git merge-tree failed: ${res.stderr || res.stdout}`);
}

// Heads of merged PRs for a branch, via gh. A squash merge leaves the branch tip out of origin/<base>'s
// history, so ancestry alone calls it unmerged. Returns [] when gh is missing or fails.
export function mergedPrHeads(root, branch) {
  const gh = ghBin();
  if (!gh) return [];
  const res = run(gh, ["pr", "list", "--head", branch, "--state", "merged", "--json", "headRefOid", "--limit", "20"], { cwd: root });
  if (res.status !== 0) return [];
  try {
    return JSON.parse(res.stdout).map((p) => p.headRefOid).filter(Boolean);
  } catch {
    return [];
  }
}

// Is this branch's work in origin/<base>? True when it is an ancestor, when a merged PR's head SHA is the
// branch tip (squash or rebase merge), or when the branch's files match origin/<base>. Unmerged work is false.
// opts.mergedHeads(root, branch) replaces the gh lookup (selftest).
export function branchMerged(root, branch, base, { mergedHeads = mergedPrHeads } = {}) {
  const ref = `origin/${base}`;
  if (!gitOk(root, ["rev-parse", "--verify", "--quiet", `refs/heads/${branch}`])) return { merged: false, via: "missing branch" };
  if (gitOk(root, ["merge-base", "--is-ancestor", branch, ref])) return { merged: true, via: "ancestor" };
  const tip = git(root, ["rev-parse", branch]).stdout.trim();
  if (tip && mergedHeads(root, branch).includes(tip)) return { merged: true, via: "merged PR" };
  const mb = git(root, ["merge-base", branch, ref]);
  if (mb.status === 0) {
    const files = git(root, ["diff", "--name-only", `${mb.stdout.trim()}..${branch}`]).stdout.split(/\r?\n/).filter(Boolean);
    if (files.length && gitOk(root, ["diff", "--quiet", branch, ref, "--", ...files])) return { merged: true, via: "same files as " + ref };
  }
  return { merged: false, via: "not merged" };
}

// Why a worktree must not be pruned right now, or null: a live lock or session that names it.
export function worktreeBusy(root, e) {
  const want = normalize(resolve(e.path)).toLowerCase();
  const dirs = [join(hubDirOf(root), "locks"), machineLockDir(mainRootOf(root))].filter(Boolean);
  for (const dir of dirs) {
    if (!existsSync(dir)) continue;
    for (const f of readdirSync(dir)) {
      try {
        const info = JSON.parse(readFileSync(join(dir, f), "utf8"));
        if (info.root && normalize(resolve(info.root)).toLowerCase() === want && info.pid && pidAlive(info.pid)) return `lock ${f}`;
      } catch { /* malformed lock: not ours to read */ }
    }
  }
  const sess = join(sessionDirOf(e.path), "session.json");
  if (existsSync(sess)) {
    try {
      const s = JSON.parse(readFileSync(sess, "utf8"));
      if (s.serverPid && pidAlive(s.serverPid)) return "live session";
    } catch { return "session.json unreadable"; }
  }
  return null;
}

// Newest sign of life in a worktree: its index, HEAD, or HEAD log.
function lastTouched(e) {
  let newest = 0;
  const gd = git(e.path, ["rev-parse", "--absolute-git-dir"]).stdout.trim();
  for (const f of [join(gd, "index"), join(gd, "HEAD"), join(gd, "logs", "HEAD")]) {
    try { newest = Math.max(newest, statSync(f).mtimeMs); } catch { /* absent */ }
  }
  return newest;
}

// One batched gh call: the newest merged PRs with their head branch, head SHA, base and merge time.
// `wt prune --merged` matches worktrees against this list. Returns { ok, prs } or { ok: false, error };
// a missing gh, a failed call, a timeout or unreadable output is an error, never an empty list, so the
// caller prunes nothing instead of falling back to ancestry.
export const MERGED_PR_LIMIT = 500;
export function mergedPrList(root, { timeoutMs = 45000 } = {}) {
  const gh = ghBin();
  if (!gh) return { ok: false, error: "gh not found" };
  const res = run(gh, ["pr", "list", "--state", "merged", "--json", "headRefName,headRefOid,baseRefName,mergedAt,number",
    "--limit", String(MERGED_PR_LIMIT)], { cwd: root, timeout: timeoutMs });
  if (res.error && res.error.code === "ETIMEDOUT") return { ok: false, error: `gh pr list timed out after ${Math.round(timeoutMs / 1000)} s` };
  if (res.status !== 0) return { ok: false, error: `gh pr list failed: ${(res.stderr || res.stdout).trim().slice(0, 200)}` };
  try {
    const prs = JSON.parse(res.stdout || "[]");
    if (!Array.isArray(prs)) return { ok: false, error: "gh pr list returned no list" };
    return { ok: true, prs };
  } catch {
    return { ok: false, error: "gh pr list output was not JSON" };
  }
}

// Is this worktree's work in a merged PR, exactly? Pure over the PR list. Merged means a merged PR whose head
// branch is e.branch, whose base is the worktree's base, and whose head SHA is the branch tip: the branch has
// no commit the PR did not carry. Ancestry of origin/<base> says nothing here (a fresh branch is an ancestor).
// Returns { merged, via } where via is the reason either way.
export function prMergedVerdict(root, e, prs) {
  const base = baseOf(e);
  const tip = git(root, ["rev-parse", "--verify", "--quiet", `refs/heads/${e.branch}`]).stdout.trim();
  if (!tip) return { merged: false, via: "missing branch" };
  const mine = prs.filter((p) => p.headRefName === e.branch && (!p.baseRefName || p.baseRefName === base));
  const hit = mine.find((p) => p.headRefOid === tip);
  if (hit) return { merged: true, via: `merged PR #${hit.number}` };
  if (mine.length) {
    const newest = mine.slice().sort((x, y) => String(y.mergedAt).localeCompare(String(x.mergedAt)))[0];
    return { merged: false, via: `PR #${newest.number} merged, but the branch has commits after it` };
  }
  if (gitOk(root, ["merge-base", "--is-ancestor", e.branch, `origin/${base}`])) return { merged: false, via: "no merged PR (ancestor only)" };
  return { merged: false, via: "no merged PR (not in origin/" + base + ")" };
}

// Every worktree `wt prune --merged` would remove, and the ones it skips with the reason. Only a merged PR
// qualifies a worktree (see prMergedVerdict); uncommitted changes, a lock or session and recent activity
// still keep it. When gh cannot list the PRs, nothing is taken and `ghError` says why. Warm-pool trees live in
// state.warm, not state.worktrees, so they are never seen here. opts.mergedPrs(root) replaces the gh call.
export function pruneCandidates(root, { idleMs = 30 * 60 * 1000, now = Date.now(), mergedPrs = mergedPrList } = {}) {
  git(root, ["fetch", "origin", "--prune"]);
  const state = readState(root);
  const take = [];
  const skip = [];
  const list = mergedPrs(root);
  if (!list.ok) {
    for (const [slug, e] of Object.entries(state.worktrees ?? {})) skip.push({ slug, why: `${e.status === "creating" ? "being created" : "merged PRs unknown"}` });
    return { take, skip, ghError: list.error };
  }
  for (const [slug, e] of Object.entries(state.worktrees ?? {})) {
    if (e.status === "creating") { skip.push({ slug, why: "being created" }); continue; }
    if (!existsSync(e.path)) { skip.push({ slug, why: "directory missing" }); continue; }
    const m = prMergedVerdict(root, e, list.prs);
    if (!m.merged) { skip.push({ slug, why: m.via }); continue; }
    if (git(e.path, ["status", "--porcelain"]).stdout.trim()) { skip.push({ slug, why: "uncommitted changes" }); continue; }
    const busy = worktreeBusy(root, e);
    if (busy) { skip.push({ slug, why: busy }); continue; }
    const idle = now - lastTouched(e);
    if (idle < idleMs) { skip.push({ slug, why: `touched ${Math.round(idle / 60000)} min ago` }); continue; }
    take.push({ slug, via: m.via });
  }
  return { take, skip };
}

// Parses --refs / --closes: an issue number >= 1. Absent means no issue (null); "0" or text throws.
export function parseIssueFlag(flags) {
  for (const name of ["--refs", "--closes"]) {
    const i = flags.indexOf(name);
    if (i === -1) continue;
    const v = flags[i + 1];
    if (v === undefined || !/^[1-9]\d*$/.test(v)) {
      throw new Error(`${name} takes an issue number of 1 or more, got '${v ?? ""}'. With no issue, leave the flag out`);
    }
    return v;
  }
  return null;
}

export function wtPrune(root, slug, { force = false, mergedHeads = mergedPrHeads } = {}) {
  // The checks and the delete run unlocked (a delete takes seconds); the lock
  // covers only the registry and branch bookkeeping, so `wt new` never queues
  // behind a prune.
  const e = readState(root).worktrees?.[slug];
  if (!e) throw new Error(`unknown worktree '${slug}' (see: cli wt list)`);
  if (e.status === "creating" && e.pid && pidAlive(e.pid)) throw new Error(`worktree '${slug}' is still being created (pid ${e.pid})`);
  const forget = () => withWtLock(root, (state) => {
    git(root, ["branch", "-D", e.branch]);
    delete state.worktrees[slug];
  }, { waitMs: 600000 });

  if (!existsSync(e.path)) {
    if (!force) throw new Error(`worktree dir missing: ${e.path} (use --force to remove stale state)`);
    forget();
    return { slug, path: e.path, branch: e.branch };
  }

  if (!force) {
    git(root, ["fetch", "origin", "--prune"]);
    const base = baseOf(e);
    if (!branchMerged(root, e.branch, base, { mergedHeads }).merged) {
      throw new Error(`branch ${e.branch} is not merged into origin/${base}; use --force to discard unmerged work`);
    }
    const dirty = git(e.path, ["status", "--porcelain"]).stdout.trim();
    if (dirty) throw new Error(`worktree ${slug} has uncommitted changes; use --force to discard`);
  }
  // Never call git worktree remove directly: with --force it follows the
  // game-addons junctions and empties the Steam installs.
  const rm = removeWorktree(mainRootOf(root), e.path, { force });
  if (rm.status !== 0) {
    const known = listWorktrees(root).some((w) => normalize(w.path) === normalize(e.path));
    if (!force || known) throw new Error(`git worktree remove failed: ${rm.stderr || rm.stdout}`);
    // Git can unregister a worktree before failing to remove its ignored
    // Workbench caches. Complete the explicit --force cleanup in that case.
    safeRemoveTree(e.path);
  }
  forget();
  kickWarmFill(root);
  return { slug, path: e.path, branch: e.branch };
}

// ---------------------------------------------------------------- PR + merge

export function originOwnerRepo(root) {
  const res = git(root, ["remote", "get-url", "origin"]);
  if (res.status !== 0) return null;
  const url = res.stdout.trim();
  const m = url.match(/github\.com[:/]([^/]+)\/([^/]+?)(?:\.git)?$/);
  if (!m) return null;
  return { owner: m[1], repo: m[2] };
}

export function commitSubjects(root, range) {
  const res = git(root, ["log", range, "--reverse", "--format=%s"]);
  return res.status === 0 ? res.stdout.split(/\r?\n/).filter(Boolean) : [];
}

// The body always goes through a file: a generated body (a world ship's change list) runs to tens of KB, and as a
// command-line argument it fails with spawnSync ENAMETOOLONG on Windows (~32 KB limit). The file lives in
// <root>/tmp (scratch, gitignored) under a unique name and is removed afterwards. `deps` is for the selftest.
export function withBodyFile(root, body, fn, tag = "pr-body-gh") {
  const dir = join(root, "tmp");
  mkdirSync(dir, { recursive: true });
  const file = join(dir, `${tag}-${process.pid}-${Date.now()}.md`);
  writeFileSync(file, String(body ?? ""), "utf8");
  try {
    return fn(file);
  } finally {
    rmSync(file, { force: true });
  }
}

export function createPrGh(root, { head, title, body, base = "main" }, deps = {}) {
  const gh = deps.gh ?? ghBin();
  const exec = deps.run ?? run;
  const res = withBodyFile(root, body, (file) => exec(gh, ["pr", "create", "--base", base, "--head", head, "--title", title, "--body-file", file], { cwd: root }));
  if (res.status !== 0) throw new Error(`gh pr create failed: ${res.stderr || res.stdout}`);
  const out = res.stdout.trim();
  const url = (out.match(/https?:\/\/\S+/i) || [])[0] || out.split(/\r?\n/).pop();
  const number = parseInt((url.match(/\/pull\/(\d+)/) || [])[1], 10);
  return { url, number };
}

// The body of PR #number, or null when gh cannot read it.
export function prBodyGh(root, number) {
  const res = run(ghBin(), ["pr", "view", String(number), "--json", "body", "--jq", ".body"], { cwd: root });
  return res.status === 0 ? res.stdout.replace(/\r?\n$/, "") : null;
}

// Replaces the body of PR #number (through --body-file, like createPrGh).
export function setPrBodyGh(root, number, body) {
  const res = withBodyFile(root, body, (file) => run(ghBin(), ["pr", "edit", String(number), "--body-file", file], { cwd: root }));
  if (res.status !== 0) throw new Error(`gh pr edit failed: ${res.stderr || res.stdout}`);
}

// The open PR for a head branch, or null. Lets a re-run of `wt ship` reuse it.
export function findOpenPrGh(root, branch) {
  const gh = ghBin();
  const res = run(gh, ["pr", "list", "--head", branch, "--state", "open", "--json", "number,url", "--limit", "1"], { cwd: root });
  if (res.status !== 0) return null;
  try {
    const list = JSON.parse(res.stdout || "[]");
    return list.length ? { url: list[0].url, number: list[0].number } : null;
  } catch {
    return null;
  }
}

// Queue the PR to merge itself once required checks and reviews pass. The
// repo deletes the head branch on merge (delete_branch_on_merge).
export function autoMergePrGh(root, number) {
  const gh = ghBin();
  const res = run(gh, ["pr", "merge", String(number), "--auto", "--merge"], { cwd: root });
  let out = (res.stdout + res.stderr).trim();
  // A base with no required checks (an unprotected branch) is "clean" at once, so auto-merge is refused: merge now.
  if (res.status !== 0 && /clean status/i.test(out)) {
    const watch = run(gh, ["pr", "checks", String(number), "--watch", "--fail-fast"], { cwd: root });
    if (watch.status !== 0) throw new Error(`PR checks did not pass; not merging: ${(watch.stdout + watch.stderr).trim().slice(-400)}`);
    const now = run(gh, ["pr", "merge", String(number), "--merge"], { cwd: root });
    out = (now.stdout + now.stderr).trim();
    if (now.status !== 0) throw new Error(`gh pr merge failed: ${out}`);
    return out || "merged after checks passed (base has no required checks)";
  }
  if (res.status !== 0 && !/already/i.test(out)) throw new Error(`gh pr merge --auto failed: ${out}`);
  return out;
}

async function githubFetch(owner, repo, path, { method = "GET", body, token } = {}) {
  const res = await fetch(`https://api.github.com/repos/${owner}/${repo}${path}`, {
    method,
    headers: {
      Authorization: `Bearer ${token}`,
      "Content-Type": "application/json",
      "User-Agent": "life-framework-cli",
      "X-GitHub-Api-Version": "2022-11-28",
    },
    body: body ? JSON.stringify(body) : undefined,
  });
  const text = await res.text();
  return { status: res.status, json: text ? JSON.parse(text) : null };
}

export async function createPrRest(root, { head, title, body, token, base = "main" }) {
  const or = originOwnerRepo(root);
  if (!or) throw new Error("origin is not a github.com remote; cannot use REST fallback (install gh)");
  const res = await githubFetch(or.owner, or.repo, "/pulls", {
    method: "POST",
    body: { title, head, base, body },
    token,
  });
  if (res.status >= 300) throw new Error(`REST create PR failed (${res.status}): ${JSON.stringify(res.json)}`);
  return { url: res.json.html_url, number: res.json.number };
}

export async function mergePrRest(root, number, branch, token) {
  const or = originOwnerRepo(root);
  const res = await githubFetch(or.owner, or.repo, `/pulls/${number}/merge`, {
    method: "PUT",
    body: { merge_method: "merge" },
    token,
  });
  if (res.status >= 300) {
    const msg = res.json?.message ?? "";
    if (/already|no commits|conflict|not mergeable/i.test(msg)) return { already: true };
    throw new Error(`REST merge PR failed (${res.status}): ${JSON.stringify(res.json)}`);
  }
  git(root, ["push", "origin", "--delete", branch]);
  return { already: false };
}
