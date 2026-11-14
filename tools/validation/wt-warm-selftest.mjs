// Selftest for the warm worktree pool and the short 'wt' lock (tools/wt.mjs).
// A fixture repo with a bare origin: fill two warm trees, move origin/main on,
// claim one (stale, so the claim fast-forwards), make one fresh, and check the
// two registry entries and markers match; a warm tree holding a junction is
// refused, falls back to fresh, and is recycled without touching the junction
// target; prune and drain leave nothing behind.
// inputs: tools/wt.mjs, tools/wt-warm.mjs, tools/link-safety.mjs, tools/lib/local-config.mjs
// red-proof (2026-11-14): with claimWarm's link and pristine checks removed, the
// junction tree was moved into Life-Framework-ws-gamma and this printed "a warm
// tree holding a junction is not claimed" and "the refused tree is marked broken".
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, realpathSync, rmSync, symlinkSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { unlinkReparsePoints } from "../link-safety.mjs";
import * as wt from "../wt.mjs";

process.env.LF_WT_WARM = "2";
process.env.LF_WT_WARM_MIN_FREE_GB = "0";
process.env.LF_WT_WARM_KICK = "0";

const failures = [];
const ok = (cond, msg) => { if (!cond) failures.push(msg); };
const base = realpathSync(mkdtempSync(join(tmpdir(), "lf-wt-warm-")));
const g = (cwd, args) => {
  const r = spawnSync("git", ["-c", "user.name=t", "-c", "user.email=t@t", ...args], { cwd, windowsHide: true, encoding: "utf8" });
  if (r.status !== 0) throw new Error(`git ${args.join(" ")}: ${r.stderr}`);
  return r.stdout.trim();
};

try {
  const origin = join(base, "origin.git");
  const main = join(base, "Life-Framework");
  const other = join(base, "other");
  g(base, ["init", "-q", "--bare", "-b", "main", origin]);
  g(base, ["clone", "-q", origin, main]);
  writeFileSync(join(main, "a.txt"), "one\n");
  writeFileSync(join(main, ".gitignore"), "tmp/\n");
  g(main, ["add", "a.txt", ".gitignore"]);
  g(main, ["commit", "-q", "-m", "one"]);
  g(main, ["push", "-q", "origin", "HEAD:main"]);
  g(main, ["fetch", "-q", "origin"]);

  // Fill: two detached trees, recorded only in state.warm.
  const fill = wt.wtWarmFill(main);
  ok(fill.made?.length === 2, `fill made ${fill.made?.length} trees, want 2`);
  let st = wt.reconcileState(main);
  ok(Object.keys(st.worktrees).length === 0, "warm trees are not registered as agent worktrees");
  ok(Object.values(st.warm).filter((w) => w.status === "ready").length === 2, "two warm trees are ready");
  ok(wt.pruneCandidates(main, { mergedPrs: () => ({ ok: true, prs: [] }) }).take.length === 0, "prune --merged never takes a warm tree");

  // origin/main moves on: the claim must fast-forward, not recreate.
  g(base, ["clone", "-q", origin, other]);
  writeFileSync(join(other, "b.txt"), "two\n");
  g(other, ["add", "b.txt"]);
  g(other, ["commit", "-q", "-m", "two"]);
  g(other, ["push", "-q", "origin", "HEAD:main"]);
  const tip = g(other, ["rev-parse", "HEAD"]);

  const a = wt.wtNew(main, "alpha", { priority: "low" });
  ok(a.warm === true, "alpha came from the warm pool");
  ok(a.path === join(base, "Life-Framework-ws-alpha"), `alpha path ${a.path}`);
  ok(g(a.path, ["rev-parse", "HEAD"]) === tip, "a stale warm tree is fast-forwarded to origin/main on claim");
  ok(g(a.path, ["branch", "--show-current"]) === "ws/alpha", "alpha is on ws/alpha");
  ok(existsSync(join(a.path, "b.txt")), "alpha has the new commit's file");
  const b = wt.wtNew(main, "beta", { warm: false, priority: "low" });
  ok(b.warm === false, "beta is a fresh checkout");
  ok(g(b.path, ["rev-parse", "HEAD"]) === tip, "beta is at origin/main");
  for (const r of [a, b]) {
    ok(g(main, ["config", `branch.ws/${r.slug}.merge`]) === "refs/heads/main", `${r.slug} tracks origin/main`);
    ok(g(r.path, ["status", "--porcelain"]) === "", `${r.slug} is clean`);
  }
  st = wt.readState(main);
  const keys = (e) => Object.keys(e).sort().join(",");
  ok(keys(st.worktrees.alpha) === keys(st.worktrees.beta), `registry entries differ: ${keys(st.worktrees.alpha)} vs ${keys(st.worktrees.beta)}`);
  ok(st.worktrees.alpha.status === "active" && !("pid" in st.worktrees.alpha), "alpha's entry is final");
  ok(st.worktrees.alpha.ports.index !== st.worktrees.beta.ports.index, "alpha and beta have their own ports");
  const marker = (p) => readFileSync(join(p, "tmp", "wt.json"), "utf8");
  ok(marker(a.path) === marker(b.path).replace("beta", "alpha"), "alpha's tmp/wt.json is what a fresh worktree writes");
  ok(wt.slugOfRoot(a.path) === "alpha", "slugOfRoot finds alpha");
  ok(wt.portsForRoot(a.path).gamePort === st.worktrees.alpha.ports.gamePort, "portsForRoot finds alpha's ports");

  // A warm tree that holds a junction is refused; the new worktree is fresh and
  // the tree is recycled without following the link.
  const install = join(base, "steam", "addons", "core");
  mkdirSync(install, { recursive: true });
  const sentinel = join(install, "core.gproj");
  writeFileSync(sentinel, "sentinel");
  const left = Object.values(st.warm).find((w) => w.status === "ready");
  const links = join(left.path, "server", "profile", "t", "game-addons");
  mkdirSync(links, { recursive: true });
  symlinkSync(install, join(links, "core"), "junction");
  const c = wt.wtNew(main, "gamma");
  ok(c.warm === false, "a warm tree holding a junction is not claimed");
  ok(existsSync(join(c.path, "a.txt")), "gamma fell back to a fresh checkout");
  ok(wt.readState(main).warm[left.id]?.status === "broken", "the refused tree is marked broken");
  process.env.LF_WT_WARM = "0";
  wt.wtWarmFill(main);
  ok(!existsSync(left.path), "the broken warm tree was recycled");
  ok(existsSync(sentinel), "recycling the broken warm tree kept the junction target");

  // Prune and drain.
  for (const slug of ["alpha", "beta", "gamma"]) {
    const r = wt.wtPrune(main, slug, { force: true });
    ok(!existsSync(r.path), `${slug} directory removed`);
    ok(spawnSync("git", ["-C", main, "show-ref", "--verify", `refs/heads/ws/${slug}`], { windowsHide: true }).status !== 0, `${slug} branch removed`);
  }
  process.env.LF_WT_WARM = "1";
  wt.wtWarmFill(main);
  const drained = wt.wtWarmDrain(main);
  ok(drained.length === 1, `drain removed ${drained.length} trees, want 1`);
  st = wt.readState(main);
  ok(Object.keys(st.worktrees).length === 0 && Object.keys(st.warm).length === 0, "registry is empty after prune and drain");
  ok(existsSync(sentinel), "junction target survived every removal");
} catch (e) {
  failures.push(`threw: ${e.stack || e.message}`);
} finally {
  unlinkReparsePoints(base);
  rmSync(base, { recursive: true, force: true });
}

if (failures.length) {
  console.log("wt-warm-selftest: FAIL");
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}
console.log("wt-warm-selftest: OK (warm fill, stale claim, fresh fallback, junction refusal, prune, drain)");
