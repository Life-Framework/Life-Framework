// Selftest for the worktree root override (tools/lib/local-config.mjs wtParent / wtDirFor, tools/wt.mjs wtNew).
// A fixture repo with a bare origin: `wt new` puts the worktree beside the main checkout by default, under
// LF_WT_ROOT (env) or lf.local.json { "wt": { "root" } } when set, and a worktree already at the old place is
// still found there.
// inputs: tools/wt.mjs, tools/lib/local-config.mjs
// red-proof (2026-11-17): with wtNew using dirname(main) again this printed "wt new ignored LF_WT_ROOT: ...".
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, realpathSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import * as wt from "../wt.mjs";
import { wtDirFor, wtParent } from "../lib/local-config.mjs";

process.env.LF_WT_WARM = "0";
process.env.LF_WT_WARM_KICK = "0";
delete process.env.LF_WT_ROOT;

const failures = [];
const ok = (cond, msg) => { if (!cond) failures.push(msg); };
const base = realpathSync(mkdtempSync(join(tmpdir(), "lf-wt-root-")));
const g = (cwd, args) => {
  const r = spawnSync("git", ["-c", "user.name=t", "-c", "user.email=t@t", ...args], { cwd, windowsHide: true, encoding: "utf8" });
  if (r.status !== 0) throw new Error(`git ${args.join(" ")}: ${r.stderr}`);
  return r.stdout.trim();
};

try {
  const origin = join(base, "origin.git");
  const main = join(base, "Life-Framework");
  g(base, ["init", "-q", "--bare", "-b", "main", origin]);
  g(base, ["clone", "-q", origin, main]);
  writeFileSync(join(main, "a.txt"), "one\n");
  writeFileSync(join(main, ".gitignore"), "tmp/\nlf.local.json\n");
  g(main, ["add", "a.txt", ".gitignore"]);
  g(main, ["commit", "-q", "-m", "one"]);
  g(main, ["push", "-q", "origin", "HEAD:main"]);
  g(main, ["fetch", "-q", "origin"]);

  ok(wtParent(main) === base, `default root is ${wtParent(main)}, want ${base}`);
  const a = wt.wtNew(main, "alpha", { unchecked: true });
  ok(resolve(a.dir ?? a.path ?? "") === join(base, "Life-Framework-ws-alpha") && existsSync(join(base, "Life-Framework-ws-alpha", "a.txt")), "wt new without a root is not beside the main checkout");

  const root = join(base, "elsewhere", "worktrees");
  process.env.LF_WT_ROOT = root;
  ok(wtParent(main) === resolve(root), `env root is ${wtParent(main)}`);
  const b = wt.wtNew(main, "beta", { unchecked: true });
  ok(existsSync(join(root, "Life-Framework-ws-beta", "a.txt")), `wt new ignored LF_WT_ROOT: ${JSON.stringify(b)}`);
  ok(!existsSync(join(base, "Life-Framework-ws-beta")), "wt new with a root also made the old-place folder");
  ok(wtDirFor(main, "alpha") === join(base, "Life-Framework-ws-alpha"), "a worktree at the old place is not found there once a root is set");
  ok(wtDirFor(main, "gamma") === join(root, "Life-Framework-ws-gamma"), "a new slug does not resolve under the root");

  delete process.env.LF_WT_ROOT;
  const cfgRoot = join(base, "cfg-root");
  writeFileSync(join(main, "lf.local.json"), JSON.stringify({ wt: { root: cfgRoot } }));
  ok(wtParent(main) === resolve(cfgRoot), `lf.local.json wt.root gives ${wtParent(main)}`);
} catch (e) {
  failures.push(String(e?.stack ?? e));
} finally {
  rmSync(base, { recursive: true, force: true });
}
if (failures.length) {
  console.log("wt-root-selftest: FAIL");
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}
console.log("wt-root-selftest: OK (default beside main, LF_WT_ROOT and lf.local.json wt.root, old-place worktrees still found)");
