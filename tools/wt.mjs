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

import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, openSync, readFileSync, statSync, unlinkSync, writeFileSync } from "node:fs";
import { dirname, join, normalize, resolve } from "node:path";

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
  const res = spawnSync(cmd, args, { encoding: "utf8", stdio: ["ignore", "pipe", "pipe"], ...opts });
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

export function gitCommonDir(root) {
  const res = git(root, ["rev-parse", "--git-common-dir"]);
  if (res.status !== 0) throw new Error(`not a git checkout: ${root}`);
  return resolve(root, res.stdout.trim());
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

export function readState(root) {
  const p = join(hubDirOf(root), "state.json");
  if (!existsSync(p)) return { version: 1, worktrees: {}, nextPortIndex: 0 };
  try {
    const s = JSON.parse(readFileSync(p, "utf8"));
    s.worktrees = s.worktrees ?? {};
    s.nextPortIndex = s.nextPortIndex ?? 0;
    return s;
  } catch {
    return { version: 1, worktrees: {}, nextPortIndex: 0 };
  }
}

export function writeState(root, state) {
  const hub = ensureHub(root);
  writeFileSync(join(hub, "state.json"), JSON.stringify(state, null, 2) + "\n", "utf8");
}

// Bring the registry in line with `git worktree list` (first run imports the
// worktrees that predate the hub; drift is repaired by writing markers).
export function reconcileState(root) {
  const state = readState(root);
  for (const e of listWorktrees(root)) {
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
    writeMarker(e.path, slug);
  }
  writeState(root, state);
  return state;
}

// ---------------------------------------------------------------- marker / slug

export function markerPath(root) {
  return join(root, "tmp", "wt.json");
}

export function writeMarker(wtRoot, slug) {
  mkdirSync(join(wtRoot, "tmp"), { recursive: true });
  writeFileSync(markerPath(wtRoot), JSON.stringify({ slug }) + "\n", "utf8");
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

function pidAlive(pid) {
  try {
    process.kill(pid, 0);
    return true;
  } catch {
    return false;
  }
}

// acquireLock: exclusive-create lock file. waitMs=0 fails fast when held;
// waitMs>0 polls until it frees or the budget is spent. A lock whose owner
// process is gone (or that outlives stealMs) is stolen.
export function acquireLock(root, name, { waitMs = 0, stealMs = 600000, pollMs = 500 } = {}) {
  const hub = ensureHub(root);
  const lockFile = join(hub, "locks", `${name}.lock`);
  const started = Date.now();
  for (;;) {
    try {
      openSync(lockFile, "wx");
      writeFileSync(lockFile, JSON.stringify({ pid: process.pid, at: new Date().toISOString(), name }));
      return { name, file: lockFile };
    } catch (e) {
      if (e.code !== "EEXIST") throw e;
      try {
        const st = statSync(lockFile);
        let stale = Date.now() - st.mtimeMs > stealMs;
        if (!stale) {
          try {
            const info = JSON.parse(readFileSync(lockFile, "utf8"));
            if (info.pid && !pidAlive(info.pid)) stale = true;
          } catch {}
        }
        if (stale) {
          try {
            unlinkSync(lockFile);
          } catch {}
          continue;
        }
      } catch {
        continue;
      }
      if (waitMs <= 0 || Date.now() - started > waitMs) {
        throw new Error(`lock '${name}' is held (${lockFile}); retry with --wait to queue`);
      }
      sleep(pollMs);
    }
  }
}

export function releaseLock(root, name) {
  const file = join(ensureHub(root), "locks", `${name}.lock`);
  try {
    const info = JSON.parse(readFileSync(file, "utf8"));
    if (info.pid !== process.pid) return;
  } catch {
    return;
  }
  try {
    unlinkSync(file);
  } catch {}
}

// ---------------------------------------------------------------- worktree lifecycle

export function wtNew(root, slug, { waitMs = 0 } = {}) {
  if (!/^[a-z0-9]+(-[a-z0-9]+)*$/.test(slug)) {
    throw new Error(`invalid slug '${slug}': lowercase letters, digits and single hyphens only`);
  }
  if (slug === "main") throw new Error("slug 'main' is reserved");
  const lock = acquireLock(root, "wt", { waitMs });
  try {
    const state = readState(root);
    if (state.worktrees[slug]) throw new Error(`worktree '${slug}' is already registered`);
    const branch = `ws/${slug}`;
    if (gitOk(root, ["show-ref", "--verify", `refs/heads/${branch}`])) {
      throw new Error(`branch ${branch} already exists; prune it or pick another slug`);
    }
    const main = mainRootOf(root);
    const dir = join(dirname(main), `Life-Framework-ws-${slug}`);
    if (existsSync(dir)) throw new Error(`directory already exists: ${dir}`);
    git(root, ["fetch", "origin", "--prune"]);
    const add = git(root, ["worktree", "add", dir, "-b", branch, "origin/main"]);
    if (add.status !== 0) throw new Error(`git worktree add failed: ${add.stderr || add.stdout}`);
    const ports = allocatePorts(state);
    state.worktrees[slug] = {
      slug,
      path: dir,
      branch,
      ports,
      created: new Date().toISOString(),
      status: "active",
    };
    writeState(root, state);
    writeMarker(dir, slug);
    return { slug, path: dir, branch, ports };
  } finally {
    releaseLock(root, "wt");
  }
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
  const merge = git(e.path, ["merge", "origin/main", "--no-edit", "--no-ff"]);
  if (merge.status !== 0) {
    throw new Error(`merge origin/main into ${e.branch} failed:\n${merge.stderr || merge.stdout}`);
  }
  return { slug, branch: e.branch, path: e.path };
}

export function wtPrune(root, slug, { force = false } = {}) {
  const e = requireWorktree(root, slug);
  if (!force) {
    git(root, ["fetch", "origin", "--prune"]);
    if (!gitOk(root, ["merge-base", "--is-ancestor", e.branch, "origin/main"])) {
      throw new Error(`branch ${e.branch} is not merged into origin/main; use --force to discard unmerged work`);
    }
    const dirty = git(e.path, ["status", "--porcelain"]).stdout.trim();
    if (dirty) throw new Error(`worktree ${slug} has uncommitted changes; use --force to discard`);
  }
  const rm = git(mainRootOf(root), ["worktree", "remove", e.path, ...(force ? ["--force"] : [])]);
  if (rm.status !== 0) throw new Error(`git worktree remove failed: ${rm.stderr || rm.stdout}`);
  git(root, ["branch", "-D", e.branch]);
  const state = readState(root);
  delete state.worktrees[slug];
  writeState(root, state);
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

export function createPrGh(root, { head, title, body }) {
  const gh = ghBin();
  const res = run(gh, ["pr", "create", "--base", "main", "--head", head, "--title", title, "--body", body], { cwd: root });
  if (res.status !== 0) throw new Error(`gh pr create failed: ${res.stderr || res.stdout}`);
  const out = res.stdout.trim();
  const url = (out.match(/https?:\/\/\S+/i) || [])[0] || out.split(/\r?\n/).pop();
  const number = parseInt((url.match(/\/pull\/(\d+)/) || [])[1], 10);
  return { url, number };
}

export function mergePrGh(root, number) {
  const gh = ghBin();
  const res = run(gh, ["pr", "merge", String(number), "--merge", "--delete-branch"], { cwd: root });
  if (res.status !== 0) {
    const out = res.stdout + res.stderr;
    if (/already|no commits|closed|not mergeable/i.test(out)) return { already: true, output: out };
    throw new Error(`gh pr merge failed: ${out}`);
  }
  return { already: false };
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

export async function createPrRest(root, { head, title, body, token }) {
  const or = originOwnerRepo(root);
  if (!or) throw new Error("origin is not a github.com remote; cannot use REST fallback (install gh)");
  const res = await githubFetch(or.owner, or.repo, "/pulls", {
    method: "POST",
    body: { title, head, base: "main", body },
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