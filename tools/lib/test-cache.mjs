// Test pass cache: a suite that already passed on byte-identical inputs is not run again.
//
// The key is a sha256 over:
//   - git tree hashes of `addons/` (tracked plus untracked non-ignored files: scripts,
//     resources, .meta), `tools/test/` (test-e2e.ps1 decides pass/fail) and
//     `tools/lib/addons.mjs` (decides the -addons list), built from a throwaway copy of the
//     index the way testbox.snapshotCommit does, so git does the hashing;
//   - the blob hash of every addon's resourceDatabase.rdb (ignored by git, read at boot);
//   - the selection: tier, the resolved --only list (order kept), --world, --core-only;
//   - the launch addon list (LF_GAME_ADDONS can change it);
//   - this PC's dedicated server build id (the pool only sends a run to a machine with the
//     same build unless it is pinned);
//   - KEY_VERSION, bumped when the key's meaning changes.
// The machine is not in the key: the pool already lets a gate land on any machine, and the
// one timing test (LF_Test_PerfBudget) is calibrated per machine in reference ops.
//
// Only passes are stored, one file per key under <main>/tmp/test-cache/ (shared by every
// worktree), written atomically. Anything unreadable is a miss.

import { createHash, randomBytes } from "node:crypto";
import { spawnSync } from "node:child_process";
import { copyFileSync, existsSync, mkdirSync, readFileSync, renameSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";

export const KEY_VERSION = 1;
export const KEY_PATHS = ["addons", "tools/test", "tools/lib/addons.mjs"];

function gitOut(root, args, env = {}) {
  const r = spawnSync("git", args, { cwd: root, encoding: "utf8", windowsHide: true, env: { ...process.env, ...env } });
  if (r.status !== 0) throw new Error(`git ${args.join(" ")}: ${(r.stderr || "").trim()}`);
  return r.stdout.trim();
}

// { "<path>": "<git object id>" } for each KEY_PATHS entry, including untracked non-ignored files.
export function treeHashes(root, paths = KEY_PATHS) {
  const tmpDir = join(root, "tmp", "test-cache-index");
  mkdirSync(tmpDir, { recursive: true });
  const index = join(tmpDir, `key-${process.pid}-${randomBytes(4).toString("hex")}.index`);
  const realIndex = resolve(root, gitOut(root, ["rev-parse", "--git-path", "index"]));
  if (existsSync(realIndex)) copyFileSync(realIndex, index);
  const env = { GIT_INDEX_FILE: index };
  try {
    const present = paths.filter((p) => existsSync(join(root, p)));
    if (present.length) gitOut(root, ["add", "-A", "--", ...present], env);
    // Paths that vanished from disk must leave the index too, or a deletion would not change the key.
    const gone = paths.filter((p) => !existsSync(join(root, p)));
    if (gone.length) gitOut(root, ["rm", "-r", "-q", "--cached", "--ignore-unmatch", "--", ...gone], env);
    const tree = gitOut(root, ["write-tree"], env);
    const out = {};
    for (const p of paths) {
      const r = spawnSync("git", ["rev-parse", `${tree}:${p}`], { cwd: root, encoding: "utf8", windowsHide: true });
      out[p] = r.status === 0 ? r.stdout.trim() : "none";
    }
    return out;
  } finally {
    rmSync(index, { force: true });
  }
}

// { "<addon>": "<blob hash>" | "none" } for addons/<name>/resourceDatabase.rdb.
export function rdbHashes(root, addons) {
  const out = {};
  for (const name of [...addons].sort()) {
    const f = join(root, "addons", name, "resourceDatabase.rdb");
    out[name] = existsSync(f) ? gitOut(root, ["hash-object", "--no-filters", f]) : "none";
  }
  return out;
}

// Everything that can change a result, as a plain object (stored beside the pass for audit).
export function keyInputs(root, { tier, only = [], world = null, coreOnly = false, build = null, addons = [], trees, rdb } = {}) {
  return {
    v: KEY_VERSION,
    trees: trees ?? treeHashes(root),
    rdb: rdb ?? rdbHashes(root, addons),
    tier,
    only: [...only],
    world: world || null,
    coreOnly: !!coreOnly,
    addons: [...addons],
    build: build ?? "unknown",
  };
}

export function cacheKey(inputs) {
  return createHash("sha256").update(JSON.stringify(inputs)).digest("hex");
}

export function cacheDir(mainRoot) {
  return join(mainRoot, "tmp", "test-cache");
}

// Why this run may not use the cache, or null when it may.
export function skipReason({ tier, opts = {}, env = process.env } = {}) {
  if (env.LF_TEST_CACHE === "0") return "LF_TEST_CACHE=0";
  if (opts.noCache) return "--no-cache";
  if (opts.red) return "red-proof run";
  if (opts.dev) return "dev prints the run's debug lines";
  if (tier === "persistence") return "persistence tier";
  if (env.LF_PLACE_AUDIT_IN) return "placement audit run";
  return null;
}

// The stored pass for a key, or null (missing, corrupt, not a pass, wrong key).
export function lookup(mainRoot, key) {
  try {
    const rec = JSON.parse(readFileSync(join(cacheDir(mainRoot), `${key}.json`), "utf8"));
    if (!rec || rec.key !== key || rec.code !== 0 || !(rec.failed === 0 || rec.failed === undefined)) return null;
    return rec;
  } catch {
    return null;
  }
}

// Stores a pass. Never throws: a cache that cannot be written is a cache miss next time.
export function store(mainRoot, key, rec) {
  if (rec.code !== 0) return false;
  try {
    const dir = cacheDir(mainRoot);
    mkdirSync(dir, { recursive: true });
    const tmp = join(dir, `.${key}.${process.pid}.${randomBytes(4).toString("hex")}.tmp`);
    writeFileSync(tmp, JSON.stringify({ ...rec, key }, null, 1) + "\n", "utf8");
    renameSync(tmp, join(dir, `${key}.json`));
    return true;
  } catch {
    return false;
  }
}

export function reuseLine(rec) {
  const count = rec.total !== undefined ? `${rec.passed}/${rec.total} passed` : "passed";
  const where = [rec.machine ? `on ${rec.machine}` : "", rec.worktree ? `in ${rec.worktree}` : ""].filter(Boolean).join(" ");
  return `test: CACHED pass, same inputs as the run at ${rec.at} ${where} (${count}, tier=${rec.tier}${rec.only?.length ? ` only=${rec.only.join(",")}` : ""}); key ${String(rec.key).slice(0, 12)}; --no-cache forces a run`;
}

// The summaries.jsonl row a reuse writes, so stamp readers still see a result.
export function cachedSummary(rec, sha) {
  return { sha, tier: rec.stampTier ?? rec.tier, passed: rec.passed ?? null, failed: 0, total: rec.total ?? null, consoleLog: rec.consoleLog ?? "", host: rec.machine ?? null, cached: true, cachedFrom: rec.at, key: rec.key };
}
