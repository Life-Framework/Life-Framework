// tools/main-guard.mjs - the post-merge main guard (docs/worktrees.md "Main guard"). The slim ship gate runs
// validate --full + LOGIC; this worker runs the rest on main after the merge:
//   1. every new origin/main head (a burst batched to one run per ~10 min): build, the full suite, the
//      placement audit and the pre-move e2e, in its own worktree (Life-Framework-ws-main-guard-run), each
//      heavy run through the pool at high priority;
//   2. a red step is rerun at the same head (only the failing tests): a one-off is a flake, logged to
//      tmp/logs/main-guard-flakes.log, and nothing is reverted;
//   3. a red that reproduces: bisect the first-parent PR merges since the last green head (only merges made
//      after the guard started), rerunning only what failed at each candidate; a suite red is matched by its failing
//      test names (a commit is red only if it fails one of them) and stops with "mixed causes", no revert, when no
//      single merge explains the set (#2052);
//   4. the culprit merge is reverted with `git revert -m 1` on a ws/revert-pr<N>-guard branch, a PR titled
//      "Revert #N (main guard: <what failed>)" with auto-merge (GitHub's required validate check gates it),
//      a comment on the original PR, its issues reopened as `regression`, one line to the staff webhook.
// Never reverts a revert, never touches a merge from before it started, never pushes main. LF_MAIN_GUARD=0 stops it.
// One worker per repo (hub lock); state in <hub>/main-guard.json; log tmp/logs/main-guard.log.

import { spawn, spawnSync } from "node:child_process";
import { appendFileSync, existsSync, mkdirSync, openSync, closeSync, readdirSync, readFileSync, renameSync, unlinkSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import * as wt from "./wt.mjs";
import { wtDirFor } from "./lib/local-config.mjs";
import * as mg from "./lib/main-guard.mjs";
import * as pin from "./lib/guard-pin.mjs";
import { hostname } from "node:os";

const HOST = (process.env.LF_MACHINE_NAME || hostname()).replace(/s+/g, "-");

const SELF = fileURLToPath(import.meta.url);
const TICK_MS = 60 * 1000;
const STEP_BUDGET_MS = 90 * 60 * 1000;
const RUN_SLUG = "main-guard-run";
const COMPILE_SLUG = "main-guard-compile";
const now = () => new Date().toISOString();

const hub = (root) => wt.ensureHub(root);
const mainRoot = (root) => wt.mainRootOf(root);
export const statePath = (root) => join(hub(root), "main-guard.json");
const lockPath = (root) => join(hub(root), "locks", "main-guard-worker.lock");
export const logPath = (root) => join(mainRoot(root), "tmp", "logs", "main-guard.log");
const flakeLog = (root) => join(mainRoot(root), "tmp", "logs", "main-guard-flakes.log");
const off = () => process.env.LF_MAIN_GUARD === "0";

function log(root, s) {
  try {
    mkdirSync(dirname(logPath(root)), { recursive: true });
    appendFileSync(logPath(root), `${now()} ${s}\n`);
  } catch {}
}

export function readState(root) {
  try {
    return JSON.parse(readFileSync(statePath(root), "utf8"));
  } catch {
    return {};
  }
}

function writeState(root, patch) {
  const s = { ...readState(root), ...patch };
  const f = statePath(root);
  writeFileSync(`${f}.tmp`, JSON.stringify(s, null, 2) + "\n");
  renameSync(`${f}.tmp`, f);
  return s;
}

export function workerAlive(root) {
  try {
    const info = JSON.parse(readFileSync(lockPath(root), "utf8"));
    return info.pid && wt.pidAlive(info.pid) ? info.pid : null;
  } catch {
    return null;
  }
}

const git = (cwd, args) => spawnSync("git", ["-C", cwd, ...args], { encoding: "utf8", windowsHide: true, maxBuffer: 64 * 1024 * 1024 });
const gh = (cwd, args) => {
  const bin = wt.ghBin();
  if (!bin) return { status: 1, stdout: "", stderr: "gh not found" };
  return spawnSync(bin, args, { cwd, encoding: "utf8", windowsHide: true, timeout: 120000 });
};

function runDir(root, slug = RUN_SLUG) {
  return wtDirFor(root, slug);
}

// A head's verdict: <hub>/main-guard-state.json and the GitHub commit status `main-guard` (what world sync and
// wt new pin to on any machine).
function mark(root, dir, sha, status, extra) {
  try { pin.recordHead(hub(root), sha, status, extra); } catch (e) { log(root, `state write failed: ${e.message}`); }
  const r = pin.postStatus(dir, sha, status, extra?.step, extra?.note);
  if (r.status !== 0) log(root, `commit status ${status} on ${sha.slice(0, 8)} not posted: ${String(r.stderr || "").trim().slice(0, 160)}`);
}

// The guard's own worktree, made once with `wt new` (ports, junctions, rdb), then moved by detached checkouts.
function ensureRunWorktree(root, slug = RUN_SLUG) {
  const RUN_SLUG = slug;
  const dir = runDir(root, slug);
  if (existsSync(join(dir, ".git"))) return dir;
  const r = spawnSync(process.execPath, [join(mainRoot(root), "tools", "cli.mjs"), "wt", "new", RUN_SLUG], { cwd: mainRoot(root), encoding: "utf8", windowsHide: true, timeout: 600000 });
  if (r.status !== 0 || !existsSync(join(dir, ".git"))) throw new Error(`wt new ${RUN_SLUG} failed: ${(r.stdout + r.stderr).trim().split(/\r?\n/).slice(-3).join(" | ")}`);
  return dir;
}

// The LF_Test_*.c sources of the checkout (core and every pack): names the test behind a failing assertion line when the
// compact suite output has no FAIL header (mg.failingTests).
function testSources(dir) {
  const out = [];
  let addons = [];
  try { addons = readdirSync(join(dir, "addons")); } catch { return out; }
  for (const a of addons) {
    const t = join(dir, "addons", a, "Scripts", "Game", "Tests");
    let files = [];
    try { files = readdirSync(t); } catch { continue; }
    for (const f of files) {
      if (!/^LF_Test_\w+\.c$/.test(f)) continue;
      try { out.push({ file: f, text: readFileSync(join(t, f), "utf8") }); } catch {}
    }
  }
  return out;
}

function checkout(dir, sha) {
  let last = "";
  for (let i = 0; i < 3; i++) {
    git(dir, ["reset", "-q", "--hard"]);
    const r = git(dir, ["checkout", "-q", "--detach", sha]);
    if (r.status === 0) return;
    // An empty stderr was a spawn that never ran git (killed, no buffer): name the real reason.
    last = (r.stderr || r.stdout || "").trim() || `git exited ${r.status ?? "-"}${r.signal ? ` (${r.signal})` : ""}${r.error ? `: ${r.error.message}` : ""}`;
    Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, 3000);
  }
  throw new Error(`checkout ${sha.slice(0, 8)} in ${dir}: ${last} (after 3 tries)`);
}

// One cli command in the run worktree, at high pool priority (LF_PRIORITY: `build` takes no --priority flag and read it as an addon), logged to its own file; resolves { code, text, log }.
function runCli(root, dir, args, tag) {
  const file = join(mainRoot(root), "tmp", "logs", `main-guard-${tag}-${Date.now()}.log`);
  mkdirSync(dirname(file), { recursive: true });
  return new Promise((res) => {
    const fd = openSync(file, "a");
    const child = spawn(process.execPath, [join(dir, "tools", "cli.mjs"), ...args], { cwd: dir, env: { ...process.env, LF_PRIORITY: "high", LF_MAIN_GUARD_JOB: "1" }, stdio: ["ignore", fd, fd], windowsHide: true });
    const timer = setTimeout(() => { try { child.kill(); } catch {} }, STEP_BUDGET_MS);
    const done = (code) => {
      clearTimeout(timer);
      try { closeSync(fd); } catch {}
      let text = "";
      try { text = readFileSync(file, "utf8"); } catch {}
      res({ code: code ?? 1, text, log: file });
    };
    child.on("exit", done);
    child.on("error", () => done(1));
  });
}

// The merges since the last green head, oldest first, each marked beforeStart when it was on main when the guard started.
function mergesSince(dir, from, head, startHead) {
  const r = git(dir, ["log", "--first-parent", "--merges", "--reverse", "--format=%H%x09%s", `${from}..${head}`]);
  if (r.status !== 0) return [];
  return r.stdout.split(/\r?\n/).filter(Boolean).map((l, i) => {
    const [sha, subject] = l.split("\t");
    const beforeStart = !!startHead && git(dir, ["merge-base", "--is-ancestor", sha, startHead]).status === 0;
    return { sha, subject, beforeStart, order: i };
  });
}

async function checkHead(root, dir, head) {
  const state = readState(root);
  writeState(root, { lastRunAt: now(), running: { head, step: "checkout", at: now() } });
  checkout(dir, head);
  for (const step of mg.STEPS) {
    if (off()) return;
    writeState(root, { running: { head, step: step.name, at: now() } });
    log(root, `${head.slice(0, 8)} ${step.name}: ${step.args.join(" ")}`);
    const r = await runCli(root, dir, step.args, `${head.slice(0, 8)}-${step.name.replace(/\W+/g, "-")}`);
    if (r.code === 0) continue;
    const failing = step.name === "suite" ? mg.failingTests(r.text, testSources(dir)) : [];
    // env (the machine died) and infra (a held lock, no Workbench slot or pool machine; #2759) are no verdict: no
    // recheck, no bisect, no revert, no red mark; lastChecked stays, so a later tick runs this head again.
    const v1 = mg.classifyStep(step.name, r.code, r.text, failing);
    if (v1.verdict !== "red") {
      log(root, `${head.slice(0, 8)} ${step.name}: ${v1.verdict.toUpperCase()} (${v1.why}), not the commit; no recheck, no revert, retried on a later tick (log ${r.log})`);
      writeState(root, { running: null, lastResult: { head, verdict: v1.verdict, step: step.name, what: v1.why, at: now() } });
      return;
    }
    const what = failing.length ? failing.slice(0, 3).join(", ") : step.name;
    log(root, `${head.slice(0, 8)} ${step.name} RED (exit ${r.code}, ${what}); rechecking at the same head (log ${r.log})`);
    const again = await runCli(root, dir, step.recheck(failing), `${head.slice(0, 8)}-recheck`);
    const v2 = mg.classifyStep(step.name, again.code, again.text, step.name === "suite" ? mg.failingTests(again.text, testSources(dir)) : []);
    if (v2.verdict === "env" || v2.verdict === "infra") {
      log(root, `${head.slice(0, 8)} ${step.name} recheck: ${v2.verdict.toUpperCase()} (${v2.why}), not the commit; no revert, retried on a later tick`);
      writeState(root, { running: null, lastResult: { head, verdict: v2.verdict, step: step.name, what: v2.why, at: now() } });
      return;
    }
    const verdict = mg.judgeRecheck(true, again.code !== 0);
    if (verdict === "flake") {
      try { appendFileSync(flakeLog(root), `${now()} ${head.slice(0, 8)} ${step.name}: ${what} failed once, passed on recheck (${r.log})\n`); } catch {}
      log(root, `${head.slice(0, 8)} ${step.name}: flake (${what}), no revert`);
      continue;
    }
    // Reproduced twice: find the merge that broke it.
    const recheck = step.recheck(failing);
    const list = mg.candidates(mergesSince(dir, state.lastGreen ?? state.startHead, head, state.startHead));
    log(root, `${head.slice(0, 8)} ${step.name} reproduced (${what}); bisecting ${list.length} merge(s) since ${(state.lastGreen ?? state.startHead ?? "").slice(0, 8)}`);
    writeState(root, { running: { head, step: `bisect ${step.name}`, at: now() } });
    // The recorded red set (#2052): the failing tests and FAIL lines at this head. The suite bisect treats a commit as
    // red only when it fails one of these tests; other steps (build, audit, e2e) have no test set and stay red/green.
    const redLines = step.name === "suite" ? mg.failLines(r.text) : [];
    if (step.name === "suite") log(root, `${head.slice(0, 8)} suite red set: ${failing.length ? failing.join(", ") : "(no test names parsed)"}${redLines.length ? ` | ${redLines.slice(0, 3).join(" | ")}` : ""}`);
    let b;
    if (step.name === "suite" && !failing.length) {
      b = { culprit: null, stop: "no failing test name could be read from the suite output, so no merge can be matched to it" };
    } else if (step.name === "suite") {
      b = await mg.bisectBySet(list, failing, async (c) => {
        checkout(dir, c.sha);
        const x = await runCli(root, dir, recheck, `bisect-${c.sha.slice(0, 8)}`);
        const failed = mg.failingTests(x.text, testSources(dir));
        const xv = mg.classifyStep(step.name, x.code, x.text, failed);
        if (xv.verdict === "env" || xv.verdict === "infra") throw new Error(`bisect ${c.sha.slice(0, 8)}: ${xv.verdict} (${xv.why}), not the commit; nothing is reverted`);
        const hit = mg.setHits(failing, failed);
        log(root, `  bisect ${c.sha.slice(0, 8)} ${hit.length ? `red (${hit.slice(0, 3).join(", ")})` : x.code === 0 ? "green" : `green for the set (exit ${x.code}, other failures: ${failed.slice(0, 3).join(", ") || "none parsed"})`} (${c.subject.slice(0, 60)})`);
        return failed;
      }, { head });
    } else {
      b = await mg.bisect(list, async (c) => {
        checkout(dir, c.sha);
        const x = await runCli(root, dir, recheck, `bisect-${c.sha.slice(0, 8)}`);
        const xv = mg.classifyStep(step.name, x.code, x.text);
        if (xv.verdict === "env" || xv.verdict === "infra") throw new Error(`bisect ${c.sha.slice(0, 8)}: ${xv.verdict} (${xv.why}), not the commit; nothing is reverted`);
        log(root, `  bisect ${c.sha.slice(0, 8)} ${x.code === 0 ? "green" : "red"} (${c.subject.slice(0, 60)})`);
        return x.code !== 0;
      });
    }
    checkout(dir, head);
    const refusal = b.stop ?? mg.revertRefusal(b.culprit, readState(root), { step: step.name });
    if (refusal) {
      log(root, `${head.slice(0, 8)} ${step.name} red, no revert: ${refusal}`);
      if (mg.fixCardInstead(b.culprit, step.name)) openFixCard(root, dir, b.culprit, step.name, what, r.log);
      mark(root, dir, head, "red", { step: step.name, what, note: b.stop ? refusal : "" });
      writeState(root, { running: null, lastChecked: head, lastResult: { head, verdict: "red", step: step.name, what, at: now(), note: refusal, failing, failLines: redLines } });
      return;
    }
    await revert(root, dir, b.culprit, what, r.log);
    mark(root, dir, head, "red", { step: step.name, what });
    writeState(root, { running: null, lastChecked: head, lastResult: { head, verdict: "red", step: step.name, what, at: now(), culprit: mg.prOfMerge(b.culprit.subject), failing, failLines: redLines } });
    return;
  }
  mark(root, dir, head, "green");
  writeState(root, { running: null, lastChecked: head, lastGreen: head, lastResult: { head, verdict: "green", at: now() } });
  log(root, `${head.slice(0, 8)} green`);
}

// The stage-2 reds the guard set on an infra refusal before #2759 (2026-11-17: 7587bfd5, a held build lock): read
// back from the step logs, put back to compiled-ok in the hub file, and withdrawn on GitHub (main-guard/suite
// pending), so promote and the compiled-ok pin no longer skip them. Runs at worker start and as
// `main-guard reclassify [--dry-run]`.
export function reclassifyInfra(root, { dryRun = false, say = (s) => log(root, s) } = {}) {
  const logs = join(mainRoot(root), "tmp", "logs");
  let names = [];
  try { names = readdirSync(logs); } catch {}
  const logsOf = (sha, step) => {
    const pre = `main-guard-${sha.slice(0, 8)}-`;
    const tags = [`${pre}${step.replace(/\W+/g, "-")}-`, `${pre}recheck-`];
    return names.filter((n) => tags.some((t) => n.startsWith(t) && /^\d+\.log$/.test(n.slice(t.length)))).map((n) => { try { return readFileSync(join(logs, n), "utf8"); } catch { return ""; } });
  };
  const found = mg.infraRedHeads(pin.readHeads(hub(root)).heads, logsOf);
  for (const f of found) {
    say(`reclassify ${f.sha.slice(0, 8)}: ${f.step} red was infra (${f.why})${dryRun ? " [dry run]" : "; now compiled-ok, main-guard/suite pending"}`);
    if (dryRun) continue;
    pin.clearInfraRed(hub(root), f.sha, f.why);
    const r = pin.postInfra(mainRoot(root), f.sha, f.why);
    if (r.status !== 0) say(`  status on ${f.sha.slice(0, 8)} not posted: ${String(r.stderr || "").trim().slice(0, 160)}`);
    const st = readState(root);
    if (st.lastResult?.head === f.sha && st.lastResult.verdict === "red") writeState(root, { lastResult: { ...st.lastResult, verdict: "infra", note: f.why } });
  }
  return found;
}

function openFixCard(root, dir, c, stepName, what, failLog) {
  const card = mg.fixCard(c, stepName, what, failLog);
  const bf = join(mainRoot(root), "tmp", "logs", `main-guard-fixcard-${mg.prOfMerge(c.subject)}-${Date.now()}.md`);
  writeFileSync(bf, card.body);
  const r = gh(dir, ["issue", "create", "--title", card.title, "--body-file", bf]);
  log(root, `fix card: ${r.status === 0 ? (r.stdout || "").trim() : `create failed: ${(r.stderr || "").trim().slice(0, 200)}`}`);
}

async function compileRouted(root, cdir, sha) {
  const file = join(mainRoot(root), "tmp", "logs", `main-guard-compile-${sha.slice(0, 8)}-${Date.now()}.log`);
  mkdirSync(dirname(file), { recursive: true });
  const wbRoute = await import("./lib/wb-route.mjs");
  const say = (l) => { try { appendFileSync(file, String(l) + String.fromCharCode(10)); } catch {} log(root, `  compile ${sha.slice(0, 8)}: ${String(l).split(String.fromCharCode(10))[0].slice(0, 300)}`); };
  process.env.LF_PRIORITY = "high";
  try {
    // The remote transcript is printed to stdout by the pool sync; tee it into the log to judge the verdict.
    const orig = process.stdout.write.bind(process.stdout);
    process.stdout.write = (chunk, ...rest) => { try { appendFileSync(file, chunk); } catch {} return orig(chunk, ...rest); };
    let r;
    try {
      r = await wbRoute.routeWorkbench(cdir, { purpose: "main guard compile", suffix: "-compile", args: ["ci", "--wb-only", "--compile-only", "--local"], log: say, runLocal: async () => runCli(root, cdir, ["ci", "--wb-only", "--compile-only", "--local"], `compile-${sha.slice(0, 8)}`).then((y) => y.code) });
    } finally {
      process.stdout.write = orig;
    }
    let text = "";
    try { text = readFileSync(file, "utf8"); } catch {}
    return { code: r.code, log: file, text };
  } catch (e) {
    say(`route failed: ${e?.message ?? e}`);
    return { code: 3, log: file };
  }
}

// Stage 1: the Workbench compile on a new head, in its own worktree, before anything else. A red compile is
// deterministic: bisect at once (no recheck) and revert the culprit.
async function compileHead(root, cdir, head) {
  const state = readState(root);
  writeState(root, { lastCompileAt: now(), compiling: { head, at: now() } });
  const compile = async (sha) => {
    checkout(cdir, sha);
    // The guard's own routing (not the checked-out commit's cli, which may predate it): send the compile to a pool
    // machine with the workbench cap; the remote runs that commit's `ci --wb-only --compile-only --local`.
    const x = await compileRouted(root, cdir, sha);
    const v = mg.compileVerdict(x.code, x.text ?? null);
    log(root, `  compile ${sha.slice(0, 8)}: ${v} (exit ${x.code}, ${x.log})`);
    return v;
  };
  const from = state.lastCompiled ?? state.lastGreen ?? state.startHead;
  const g = await mg.compileGate({ head, merges: mergesSince(cdir, from, head, state.startHead), state, compile, base: from, baseChecked: !!state.lastCompiled });
  writeState(root, { compiling: null });
  if (g.verdict === "env") { log(root, `${head.slice(0, 8)} compile: no verdict (pool machine), retry next tick`); return; }
  if (g.verdict === "compiled-ok") {
    mark(root, cdir, head, "compiled-ok");
    writeState(root, { lastCompiled: head });
    log(root, `${head.slice(0, 8)} compiled-ok`);
    return;
  }
  mark(root, cdir, head, "red", { step: "compile" });
  writeState(root, { lastCompileRed: head });
  if (g.refusal) { log(root, `${head.slice(0, 8)} compile red, no revert: ${g.refusal}`); return; }
  log(root, `${head.slice(0, 8)} compile red; bisected to ${g.culprit.sha.slice(0, 8)} (${g.culprit.subject.slice(0, 60)}); reverting`);
  await revert(root, cdir, g.culprit, "Workbench compile", "compile logs under tmp/logs/main-guard-compile-*");
}

function readDriverNames(dir) {
  try { return mg.driverNames(readFileSync(join(dir, "tools", "merge", "drivers.gitconfig"), "utf8")); } catch { return []; }
}

async function revert(root, dir, c, what, failLog) {
  const pr = mg.prOfMerge(c.subject);
  const title = mg.revertTitle(pr, what);
  const branch = `ws/revert-pr${pr}-guard`;
  git(dir, ["fetch", "origin", "--prune"]);
  git(dir, ["checkout", "-q", "-B", branch, "origin/main"]);
  // The repo's merge drivers (tools/merge/drivers.gitconfig) need a merge's sides and fail under `git revert`
  // ("lf-regen: cannot identify the merge sides"; #1847 and #1856 were left unreverted that way). For the revert
  // each driver keeps ours (`true`); the generated files are then rebuilt from the reverted sources.
  const rv = git(dir, [...mg.driverOverrides(readDriverNames(dir)), "revert", "-m", "1", "--no-edit", c.sha]);
  if (rv.status !== 0) {
    git(dir, ["revert", "--abort"]);
    log(root, `revert of #${pr} (${c.sha.slice(0, 8)}) does not apply on main: ${(rv.stderr || rv.stdout).trim().slice(0, 200)}; left to a human`);
    return;
  }
  for (const args of mg.REGEN_AFTER_REVERT) spawnSync(process.execPath, args, { cwd: dir, encoding: "utf8", windowsHide: true, timeout: 300000 });
  git(dir, ["add", "-A"]);
  git(dir, ["commit", "--amend", "-q", "-m", `${title}\n\nReverts #${pr} (merge ${c.sha}): the main guard saw ${what} fail twice at main and bisected it to this merge.\n\nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`]);
  const push = git(dir, ["push", "-u", "origin", `${branch}:${branch}`]);
  if (push.status !== 0) {
    log(root, `revert of #${pr}: push failed: ${(push.stderr || push.stdout).trim().slice(0, 200)}`);
    return;
  }
  const body = [`Reverts #${pr} (merge \`${c.sha}\`).`, "", `The main guard (docs/worktrees.md "Main guard") ran the full post-merge check on main: **${what}** failed, failed again on a rerun, and the bisect over the merges since the last green head put it on #${pr}. The revert gates on GitHub's \`validate\` check only.`, "", `Log on the guard machine: \`${failLog}\`.`, "", "🤖 Generated with [Claude Code](https://claude.com/claude-code)"].join("\n");
  const bf = join(mainRoot(root), "tmp", "logs", `main-guard-revert-${pr}.md`);
  writeFileSync(bf, body);
  const created = gh(dir, ["pr", "create", "--base", "main", "--head", branch, "--title", title, "--body-file", bf]);
  const url = (created.stdout || "").trim().split(/\r?\n/).pop();
  log(root, `revert PR for #${pr}: ${created.status === 0 ? url : `create failed: ${(created.stderr || "").trim().slice(0, 200)}`}`);
  if (created.status === 0) gh(dir, ["pr", "merge", url, "--auto", "--merge"]);
  // The original PR: a comment, and its issues back to regression.
  const cf = join(mainRoot(root), "tmp", "logs", `main-guard-comment-${pr}.md`);
  writeFileSync(cf, `**Main guard:** after this PR merged, ${what} failed twice on main and the bisect over the merges since the last green head points here. Reverted in ${created.status === 0 ? url : "a revert PR (creation failed, see the guard log)"}. Fix on a branch and ship again.\n\nLog on the guard machine: \`${failLog}\``);
  gh(dir, ["pr", "comment", String(pr), "--body-file", cf]);
  const v = gh(dir, ["pr", "view", String(pr), "--json", "body", "-q", ".body"]);
  for (const n of mg.issuesOf(v.stdout)) {
    gh(dir, ["issue", "reopen", String(n)]);
    gh(dir, ["issue", "edit", String(n), "--add-label", "regression", "--remove-label", "in-dev,needs-playtest"]);
  }
  await staffLine(root, `main guard: reverted #${pr} (${what} failed twice on main) ${created.status === 0 ? url : ""}`.trim());
  writeState(root, { reverted: [...(readState(root).reverted ?? []), { pr, sha: c.sha, at: now(), revertPr: created.status === 0 ? url : null, what }] });
  git(dir, ["checkout", "-q", "--detach", "origin/main"]);
}

// One line to the staff webhook when one is configured: LF_STAFF_WEBHOOK_URL, or "mainGuard": { "webhookUrl" } in
// lf.local.json. The URL is never printed or logged.
async function staffLine(root, text) {
  let url = process.env.LF_STAFF_WEBHOOK_URL || "";
  if (!url) {
    try { url = JSON.parse(readFileSync(join(mainRoot(root), "lf.local.json"), "utf8"))?.mainGuard?.webhookUrl || ""; } catch {}
  }
  if (!url) return log(root, "no staff webhook configured; the revert is in this log only");
  try {
    const res = await fetch(url, { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify({ content: text, text }) });
    log(root, res.ok ? "staff webhook: posted" : `staff webhook refused (HTTP ${res.status})`);
  } catch (e) {
    log(root, `staff webhook post failed (${e?.name ?? "error"})`);
  }
}

export async function runWorker(root) {
  mkdirSync(dirname(lockPath(root)), { recursive: true });
  const holder = workerAlive(root);
  if (holder && holder !== process.pid) return 0;
  writeFileSync(lockPath(root), JSON.stringify({ pid: process.pid, at: now() }));
  log(root, `worker pid ${process.pid} starts`);
  const dir = ensureRunWorktree(root);
  const cdir = ensureRunWorktree(root, COMPILE_SLUG);
  let stage2 = null;
  git(dir, ["fetch", "origin", "--prune"]);
  const head0 = git(dir, ["rev-parse", "origin/main"]).stdout.trim();
  const st = readState(root);
  // The first start pins the start head: merges on main before it are never blamed. The guard has not
  // checked it, so the first run proves it (lastGreen) before anything can be bisected.
  if (!st.startHead) writeState(root, { startedAt: now(), startHead: head0, lastChecked: null, lastGreen: null, reverted: st.reverted ?? [] });
  try { reclassifyInfra(root); } catch (e) { log(root, `reclassify failed: ${e?.message ?? e}`); }
  // One guard for the repo: the lease (main-guard/lease status on the root commit) must be free, stale or ours.
  const lease = pin.readLease(dir);
  const ld = pin.leaseDecision(lease, HOST);
  if (lease === undefined) log(root, "lease: gh could not read it; starting on the local lock only");
  else if (!ld.ok) { log(root, `lease refused: ${ld.why}; worker exits`); try { unlinkSync(lockPath(root)); } catch {} return 1; }
  log(root, `lease: ${lease === undefined ? "unreadable" : ld.why}`);
  pin.writeLease(dir, "held", HOST, process.pid);
  writeState(root, { running: null, pid: process.pid });
  // The lease is renewed on its own timer: the loop below awaits a compile for many minutes, and a lease renewed only
  // between ticks went 16 min stale under a live worker (#2754), which the watchdog and `lead` read as a dead guard.
  let leaseLost = null;
  const beat = setInterval(() => {
    try {
      const cur = pin.readLease(dir);
      if (cur && cur.state === "held" && cur.host !== HOST) { leaseLost = cur.host; return; }
      pin.writeLease(dir, "held", HOST, process.pid);
    } catch (e) { log(root, `lease renewal failed: ${e?.message ?? e}`); }
  }, 5 * 60 * 1000);
  beat.unref?.();
  for (;;) {
    if (leaseLost) { log(root, `lease taken by ${leaseLost}; worker exits`); break; }
    if (off()) { log(root, "LF_MAIN_GUARD=0: worker exits"); break; }
    if (readState(root).stop) { writeState(root, { stop: false }); log(root, "stop requested: worker exits"); break; }
    try {
      git(dir, ["fetch", "origin", "--prune"]);
      const head = git(dir, ["rev-parse", "origin/main"]).stdout.trim();
      // A requested check (`main-guard check <sha>`, e.g. a release candidate) goes first: its compile, then stage 2
      // on exactly that commit, whatever main's head is.
      const req = readState(root).request;
      if (req && !stage2) {
        writeState(root, { request: null });
        log(root, `requested check of ${req.slice(0, 8)}`);
        checkout(cdir, req);
        const x = await compileRouted(root, cdir, req);
        const v = mg.compileVerdict(x.code, x.text ?? null);
        log(root, `  compile ${req.slice(0, 8)}: ${v} (exit ${x.code}, ${x.log})`);
        if (v === "ok") {
          mark(root, cdir, req, "compiled-ok");
          stage2 = checkHead(root, dir, req).catch((e) => { log(root, `stage 2 failed: ${e?.message ?? e}`); writeState(root, { running: null }); }).finally(() => { stage2 = null; });
        } else if (v === "fail") mark(root, cdir, req, "red", { step: "compile" });
        else { log(root, `${req.slice(0, 8)}: no compile verdict; request kept`); writeState(root, { request: req }); }
      }
      // Stage 1 first: compile the newest head.
      const c = mg.shouldCompile(readState(root), head, Date.now());
      if (c.run) await compileHead(root, cdir, head);
      // Stage 2 in the background, only ever on the newest compiled-ok head.
      const s = readState(root);
      const d = mg.shouldRun(s, s.lastCompiled, Date.now());
      if (!stage2 && d.run) {
        log(root, `run: ${d.why} (compiled-ok)`);
        stage2 = checkHead(root, dir, s.lastCompiled).catch((e) => { log(root, `stage 2 failed: ${e?.message ?? e}`); writeState(root, { running: null }); }).finally(() => { stage2 = null; });
      }
    } catch (e) {
      log(root, `tick failed: ${e?.message ?? e}`);
      writeState(root, { running: null });
    }
    await new Promise((r) => setTimeout(r, TICK_MS));
  }
  clearInterval(beat);
  pin.writeLease(dir, "released", HOST);
  try { unlinkSync(lockPath(root)); } catch {}
  return 0;
}

// One watchdog tick (#2754), run every 5 min by the LF-MainGuardWatch scheduled task: a dead worker is started again,
// a hung one or a crash loop only alerts. Restarts are remembered in <hub>/main-guard-watch.json (3 per hour at most).
// Alerts go to the log and the staff webhook, at most one per 30 min.
export async function watch(root, dryRun = false) {
  const mr = mainRoot(root);
  const f = join(hub(root), "main-guard-watch.json");
  let w = {};
  try { w = JSON.parse(readFileSync(f, "utf8")); } catch {}
  const save = () => { try { writeFileSync(f, JSON.stringify(w, null, 2) + "\n"); } catch {} };
  wt.git(mr, ["fetch", "origin", "--prune"]);
  const lease = pin.readLease(mr);
  const alive = workerAlive(root);
  const d = mg.watchDecision({ off: off(), alive, lease, host: HOST, restarts: w.restarts ?? [] });
  console.log(`main-guard watch: ${d.action}: ${d.why}`);
  if (dryRun || d.action === "none") return 0;
  log(root, `watch: ${d.action}: ${d.why}`);
  const say = async (text) => {
    if (Date.now() - Date.parse(w.alertAt ?? "") < 30 * 60 * 1000) return;
    w.alertAt = now();
    await staffLine(root, text);
  };
  if (d.action === "alert") { await say(`main guard watchdog: ${d.why}`); save(); return 1; }
  w.restarts = [...(w.restarts ?? []), now()].slice(-10);
  save();
  const code = await main(["start"], { root });
  const up = workerAlive(root);
  log(root, `watch: restart ${up ? `ok (pid ${up})` : "FAILED"}`);
  await say(up ? `main guard watchdog: the guard was down (${d.why}); restarted, pid ${up}` : `main guard watchdog: the guard was down (${d.why}) and the restart FAILED (exit ${code}); see ${logPath(root)}`);
  save();
  return up ? 0 : 1;
}

export async function main(args, { root }) {
  const [sub] = args;
  if (sub === "worker") return runWorker(root);
  if (sub === "start") {
    if (off()) { console.log("main-guard: LF_MAIN_GUARD=0, not starting"); return 1; }
    const pid = workerAlive(root);
    if (pid) { console.log(`main-guard: already running (pid ${pid})`); return 0; }
    if (readState(root).stop) writeState(root, { stop: false });
    {
      wt.git(mainRoot(root), ["fetch", "origin", "--prune"]);
      const ld = pin.leaseDecision(pin.readLease(mainRoot(root)), HOST);
      if (!ld.ok) { console.log(`FAIL  main-guard: ${ld.why}. Only one guard runs; stop that one first.`); return 1; }
    }
    const { quietSpawnDetached } = await import("./lib/quiet-launch.mjs");
    const il = await import("./lib/interactive-launch.mjs");
    try {
      // Over ssh a detached child dies with the session (2026-11-16: three workers logged "starts" and vanished), so the
      // worker starts through a one-shot Interactive task on the console desktop, which outlives the ssh job.
      if (il.interactiveNeeded()) il.interactiveSpawnDetached(process.execPath, [SELF, "worker", mainRoot(root)], { cwd: dirname(dirname(SELF)), marker: "main-guard.mjs" });
      else quietSpawnDetached(process.execPath, [SELF, "worker", mainRoot(root)], { cwd: dirname(dirname(SELF)), env: process.env });
    } catch (e) {
      console.log(`FAIL  main-guard: worker did not start: ${e?.message ?? e} (log ${logPath(root)})`);
      return 1;
    }
    for (let i = 0; i < 20 && !workerAlive(root); i++) await new Promise((r) => setTimeout(r, 500));
    console.log(mg.statusLine(readState(root), { alive: workerAlive(root) }));
    return workerAlive(root) ? 0 : 1;
  }
  if (sub === "reclassify") {
    const found = reclassifyInfra(root, { dryRun: args.includes("--dry-run"), say: (s) => { console.log(s); log(root, s); } });
    if (!found.length) console.log("main-guard reclassify: no red head was an infra refusal");
    return 0;
  }
  if (sub === "check") {
    const want = args[1];
    if (!want) { console.log("usage: tools\cli main-guard check <sha>   compile, then the full check, on exactly that main commit"); return 1; }
    wt.git(mainRoot(root), ["fetch", "origin", "--prune"]);
    const sha = wt.git(mainRoot(root), ["rev-parse", `${want}^{commit}`]).stdout.trim();
    if (!sha || wt.git(mainRoot(root), ["merge-base", "--is-ancestor", sha, "origin/main"]).status !== 0) { console.log(`FAIL  ${want} is not a commit on origin/main`); return 1; }
    writeState(root, { request: sha });
    console.log(`main-guard: check of ${sha.slice(0, 8)} requested; it runs next (${workerAlive(root) ? "worker running" : "start the guard"}). Statuses: main-guard/compile, main-guard/suite on ${sha.slice(0, 8)}`);
    return 0;
  }
  if (sub === "watch") return watch(root, args.includes("--dry-run"));
  if (sub === "stop") {
    const pid = workerAlive(root);
    if (!pid) { console.log("main-guard: not running"); return 0; }
    writeState(root, { stop: true });
    try { process.kill(pid); } catch {}
    try { unlinkSync(lockPath(root)); } catch {}
    writeState(root, { stop: false, running: null });
    if (pin.readLease(mainRoot(root))?.host === HOST) pin.writeLease(mainRoot(root), "released", HOST);
    console.log(`main-guard: stopped (pid ${pid})`);
    return 0;
  }
  if (sub === "status" || !sub) {
    const alive = workerAlive(root);
    if (alive) console.log(mg.statusLine(readState(root), { alive, off: off() }));
    // From GitHub, so every machine sees the same guard: who holds the lease, main's head, the newest checked commit.
    const mr = mainRoot(root);
    wt.git(mr, ["fetch", "origin", "--prune"]);
    const lease = pin.readLease(mr);
    const head = wt.git(mr, ["rev-parse", "origin/main"]).stdout.trim();
    const hs = pin.ghStatus(mr, head);
    const ck = pin.resolveChecked(mr, { hub: wt.hubDirOf(mr), remoteLookups: 30 });
    const leaseAge = lease?.at ? Date.now() - Date.parse(lease.at) : 0;
    const who = lease === undefined ? "lease unknown (gh failed)" : !lease || lease.state !== "held" ? "no guard holds the lease" : leaseAge > pin.LEASE_TTL_MS ? `STALE lease of ${lease.host} (pid ${lease.pid}, renewed ${Math.round(leaseAge / 60000)} min ago; no live guard, \`main-guard start\` takes it over)` : `running on ${lease.host} (pid ${lease.pid}, lease renewed ${Math.round((Date.now() - Date.parse(lease.at)) / 60000)} min ago)`;
    if (!alive) console.log(`MAIN GUARD ${who}`);
    else console.log(`  github: ${who}`);
    console.log(`  main ${head.slice(0, 8)}: ${hs ?? "unchecked"}; newest compiled-ok or better: ${ck ? `${ck.sha.slice(0, 8)} (${ck.status}, ${ck.behind} behind)` : "none"}`);
    console.log(`  log ${logPath(root)}; flakes ${flakeLog(root)}`);
    return 0;
  }
  console.log("usage: tools\\cli main-guard start | stop | status | watch [--dry-run]");
  return 1;
}

if (process.argv[1] && resolve(process.argv[1]).toLowerCase() === SELF.toLowerCase() && process.argv[2] === "worker") {
  runWorker(process.argv[3] || wt.mainRootOf(dirname(dirname(SELF)))).then((c) => process.exit(c));
}
