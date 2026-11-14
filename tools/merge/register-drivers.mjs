// Registers the merge drivers: exactly ONE `include.path` in the shared git config, pointing at
// the main checkout's tools/merge/drivers.gitconfig. Run by .githooks/pre-commit.
//
// Worktrees share the main repo's .git/config. The hook used to append the worktree's own absolute
// path, so every worktree added a line (640 lines, 591 of them for deleted worktrees, 79 KB) and
// concurrent writers hit "could not write config file ... Permission denied".
//
// The one canonical value is <main checkout>/tools/merge/drivers.gitconfig, main checkout being
// the parent of `git rev-parse --git-common-dir`. Any other include.path ending in
// tools/merge/drivers.gitconfig (a worktree, a deleted worktree, a duplicate) is removed. Other
// include lines are left alone. Reading never writes: a config that is already right is not touched.
// A busy config (another process holds config.lock) is retried, then skipped with a warning; this
// script never fails a commit.
import { spawnSync } from "node:child_process";
import { existsSync } from "node:fs";
import { basename, dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const SUFFIX = "tools/merge/drivers.gitconfig";
const norm = (p) => p.split("\\").join("/");
const isDriversPath = (p) => norm(p).endsWith(SUFFIX);
const sleep = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);

function git(cwd, args) {
  const r = spawnSync("git", args, { cwd, encoding: "utf8", windowsHide: true });
  return { status: r.status, out: (r.stdout || "").trim(), err: (r.stderr || "").trim() };
}

// The main checkout of the repo that `cwd` belongs to, or null (bare repo, odd layout).
export function mainCheckout(cwd) {
  const r = git(cwd, ["rev-parse", "--path-format=absolute", "--git-common-dir"]);
  if (r.status !== 0 || !r.out) return null;
  const common = norm(resolve(r.out));
  return basename(common) === ".git" ? norm(dirname(common)) : null;
}

export function driversIncludes(cwd) {
  const r = git(cwd, ["config", "--local", "--get-all", "include.path"]);
  return r.status === 0 ? r.out.split(/\r?\n/).filter(isDriversPath) : [];
}

// Retries a config write that loses a lock race. Returns true on success.
function writeConfig(cwd, args) {
  let last = "";
  for (let attempt = 0; attempt < 5; attempt++) {
    const r = git(cwd, ["config", "--local", ...args]);
    if (r.status === 0 || r.status === 5) return true; // 5: --unset-all matched nothing
    last = r.err;
    sleep(150 + attempt * 150);
  }
  console.warn(`register-drivers: config busy, left as is (${last.split("\n")[0]})`);
  return false;
}

// Makes the config right. Returns { canonical, removed, added } or { skipped: reason }.
export function registerDrivers(cwd) {
  const main = mainCheckout(cwd);
  if (!main) return { skipped: "no main checkout" };
  const canonical = `${main}/${SUFFIX}`;
  if (!existsSync(canonical)) return { skipped: `${canonical} missing` };
  const have = driversIncludes(cwd);
  const stale = have.filter((p) => p !== canonical);
  const result = { canonical, removed: 0, added: false };
  if (stale.length) {
    // Drop every drivers include, then re-add the one. One atomic-ish pass beats N unsets.
    if (!writeConfig(cwd, ["--unset-all", "include.path", "[/\\\\]tools[/\\\\]merge[/\\\\]drivers\\.gitconfig$"])) return result;
    result.removed = stale.length;
    result.added = true;
    writeConfig(cwd, ["--add", "include.path", canonical]);
  } else if (have.length === 0) {
    result.added = writeConfig(cwd, ["--add", "include.path", canonical]);
  }
  return result;
}

if (process.argv[1] && resolve(process.argv[1]) === resolve(fileURLToPath(import.meta.url))) {
  try {
    const r = registerDrivers(process.cwd());
    if (r.removed) console.log(`register-drivers: removed ${r.removed} stale include.path line(s)`);
  } catch (e) {
    console.warn(`register-drivers: ${e.message}`);
  }
  process.exit(0);
}
