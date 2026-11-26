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
// Stage 0 (#2889, tools/lib/main-guard.mjs validateGate): `validate --all` on every new head before the compile; a red
// is rerun, bisected to the merge that introduced it, and that PR gets a comment, the staff a line and a fix card; only a
// hard data-safety check (HARD_CHECKS) reverts. A red explained by an earlier guard revert still on main (the follow-up
// of a reverted PR) is commented, never reverted. Never reverts a revert, never touches a merge from before it started, never pushes main. LF_MAIN_GUARD=0 stops it.
// Newest first (2026-11-22): a red at an older head is first tried at the newest compiled-ok head (mg.newestFirst); a pass
// there drops the bisect, and a revert is refused when a newer head already passes or the culprit is a large merge
// (mg.isLargePr). The lease renewal is checked and logged; the heartbeat (beatAt/activityAt) lets the watchdog renew a
// stale lease for a worker that still progresses (mg.watchDecision).
// One worker per repo (hub lock); state in <hub>/main-guard.json; log tmp/logs/main-guard.log.

import { spawn, spawnSync } from "node:child_process";
import { appendFileSync, copyFileSync, existsSync, mkdirSync, openSync, closeSync, readdirSync, readFileSync, renameSync, statSync, unlinkSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import * as wt from "./wt.mjs";
import { wtDirFor } from "./lib/local-config.mjs";
import * as mg from "./lib/main-guard.mjs";
import { healBlockedFastForward, noteAlert } from "./lib/main-ff-heal.mjs";
import * as rdbCache from "./lib/rdb-cache.mjs";
import * as pin from "./lib/guard-pin.mjs";
import { hostname } from "node:os";

const HOST = pin.leaseHostName(process.env.LF_MACHINE_NAME || hostname());

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

// Guard code freshness (mg.GUARD_CODE): the blob ids this process loaded, read once at import, against origin/main.
function diskCodeIds(dir) {
  const r = git(dir, ["hash-object", "--", ...mg.GUARD_CODE]);
  const ids = r.status === 0 ? r.stdout.trim().split(/\r?\n/) : [];
  const out = {};
  mg.GUARD_CODE.forEach((p, i) => { if (/^[0-9a-f]{40}$/.test(ids[i] ?? "")) out[p] = ids[i]; });
  return out;
}
const LOADED_CODE = diskCodeIds(dirname(dirname(SELF)));
const originCodeIds = (dir) => mg.parseLsTree(git(dir, ["ls-tree", "origin/main", "--", ...mg.GUARD_CODE]).stdout);
const codeDriftNow = (dir) => mg.codeDrift(LOADED_CODE, originCodeIds(dir));

// Fast-forward the main checkout to origin/main (only on branch main, never forced) and say whether its guard code on
// disk now matches origin/main's.
function freshenMainCheckout(root) {
  const mr = mainRoot(root);
  if (git(mr, ["symbolic-ref", "--short", "HEAD"]).stdout.trim() === "main") {
    // A stray tracked edit an agent left there is stashed (never discarded) so the checkout can move; the maintainer's
    // Worlds/ edits are left alone and alerted (#3700).
    const heal = healBlockedFastForward(mr, (args) => git(mr, args));
    if (heal.line) {
      if (noteAlert(hub(root), heal.line)) log(root, `ALERT ${heal.line}`);
    }
  }
  return mg.codeDrift(diskCodeIds(mr), originCodeIds(mr)).length === 0;
}

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
// dropRdb: delete the run worktree's rdb first, so a recheck or a bisect probe never starts from the (possibly
// corrupt) rdb the first run restored from the shared cache (#3586).
// The step logs of the children running now: the worker's progress heartbeat is their newest write (activityNow).
const activeLogs = new Set();
function activityNow() {
  let newest = 0;
  for (const f of activeLogs) { try { newest = Math.max(newest, statSync(f).mtimeMs); } catch {} }
  return new Date(newest || Date.now()).toISOString();
}

function runCli(root, dir, args, tag, extraEnv = {}, dropRdb = false) {
  if (dropRdb) rdbCache.dropAllRdbs(dir);
  const file = join(mainRoot(root), "tmp", "logs", `main-guard-${tag}-${Date.now()}.log`);
  mkdirSync(dirname(file), { recursive: true });
  activeLogs.add(file);
  return new Promise((res) => {
    const fd = openSync(file, "a");
    const child = spawn(process.execPath, [join(dir, "tools", "cli.mjs"), ...args], { cwd: dir, env: { ...process.env, LF_PRIORITY: "high", LF_MAIN_GUARD_JOB: "1", ...extraEnv }, stdio: ["ignore", fd, fd], windowsHide: true });
    const timer = setTimeout(() => { try { child.kill(); } catch {} }, STEP_BUDGET_MS);
    const done = (code) => {
      clearTimeout(timer);
      activeLogs.delete(file);
      try { closeSync(fd); } catch {}
      let text = "";
      try { text = readFileSync(file, "utf8"); } catch {}
      res({ code: code ?? 1, text, log: file });
    };
    child.on("exit", done);
    child.on("error", () => done(1));
  });
}

// A flake leaves its evidence: the raw transcripts the failing run named (`log=...` in its SUMMARY line, in the guard's
// worktree, rotated within hours) are copied to <main>/tmp/logs/flakes/ (newest 80 kept), so the failing assertion is
// still there when someone audits the flake log days later. Returns the copied paths, relative to <main>.
function keepFlakeEvidence(root, dir, h8, stepName, text) {
  const kept = [];
  try {
    const out = join(mainRoot(root), "tmp", "logs", "flakes");
    mkdirSync(out, { recursive: true });
    for (const rel of mg.rawLogPaths(text)) {
      const from = resolve(dir, rel);
      if (!existsSync(from) || statSync(from).size > 20 * 1024 * 1024) continue;
      const to = join(out, `${h8}-${stepName}-${rel.split(/[\\/]/).pop()}`);
      copyFileSync(from, to);
      kept.push(join("tmp", "logs", "flakes", to.split(/[\\/]/).pop()));
    }
    const all = readdirSync(out).map((n) => ({ n, t: statSync(join(out, n)).mtimeMs })).sort((a, b) => b.t - a.t);
    for (const old of all.slice(80)) unlinkSync(join(out, old.n));
  } catch {}
  return kept;
}

// The merges since the last green head, oldest first, each marked beforeStart when it was on main when the guard started.
function mergesSince(dir, from, head, startHead) {
  const r = git(dir, ["log", "--first-parent", "--merges", "--reverse", "--format=%H%x09%s", `${from}..${head}`]);
  if (r.status !== 0) return [];
  return mg.parseMerges(r.stdout, (sha) => !!startHead && git(dir, ["merge-base", "--is-ancestor", sha, startHead]).status === 0);
}

// A guard revert still on main: its merge subject names the revert branch (revert() makes ws/revert-pr<N>-guard).
function revertLanded(dir, pr) {
  const r = git(dir, ["log", "origin/main", "--first-parent", "--merges", "-F", `--grep=ws/revert-pr${pr}-guard`, "--format=%H", "-1"]);
  return r.status === 0 && !!r.stdout.trim();
}
// The follow-up of a reverted PR (lib explainedByEarlierRevert): merged after it, touching its files.
function earlierRevertFor(dir, c, state) {
  const filesOf = (sha) => { const r = git(dir, ["diff", "--name-only", `${sha}^1`, sha]); return r.status === 0 ? r.stdout.split(/\r?\n/).filter(Boolean) : []; };
  return mg.explainedByEarlierRevert(c, state, {
    landed: (pr) => revertLanded(dir, pr),
    isAfter: (revertedSha, sha) => revertedSha !== sha && git(dir, ["merge-base", "--is-ancestor", revertedSha, sha]).status === 0,
    filesOf,
  });
}

// A comment on a PR, from a file (a body on the command line loses its quotes on Windows).
function commentOnPr(root, dir, pr, body, tag) {
  const cf = join(mainRoot(root), "tmp", "logs", `main-guard-${tag}-${pr}-${Date.now()}.md`);
  writeFileSync(cf, body);
  const r = gh(dir, ["pr", "comment", String(pr), "--body-file", cf]);
  if (r.status !== 0) log(root, `comment on #${pr} failed: ${String(r.stderr || "").trim().slice(0, 160)}`);
}

// The files one merge changed (the guard's first-parent view), for the large-PR rule (mg.isLargePr).
function mergeFiles(dir, sha) {
  const r = git(dir, ["diff", "--name-only", `${sha}^1`, sha]);
  return r.status === 0 ? r.stdout.split(/\r?\n/).filter(Boolean) : [];
}

// Newest first (mg.newestFirst): the newest compiled-ok head that descends from `sha`, or null. A requested check of an
// exact commit (pinned) is never moved to a newer head.
const newerThanFn = (root, dir, pinned) => (sha) => {
  if (pinned) return null;
  const n = readState(root).lastCompiled;
  if (!n || n === sha) return null;
  return git(dir, ["merge-base", "--is-ancestor", sha, n]).status === 0 ? n : null;
};

// Run just the failing checks of `step` at one commit: { verdict: ok | red | env, failing, why, code, text, log }. "ok"
// means none of the recorded failing tests fails there (another test failing is that head's own business).
const probeFn = (root, dir, step) => async (sha, set) => {
  checkout(dir, sha);
  const x = await runCli(root, dir, step.recheck(set), `probe-${sha.slice(0, 8)}`, mg.RECHECK_ENV, true);
  const failed = step.name === "suite" ? mg.failingTests(x.text, testSources(dir)) : [];
  const v = mg.classifyStep(step.name, x.code, x.text, failed);
  const base = { code: x.code, text: x.text, log: x.log };
  let out;
  if (v.verdict === "env" || v.verdict === "infra") out = { ...base, verdict: "env", why: v.why, failing: [] };
  else if (x.code === 0) out = { ...base, verdict: "ok", failing: [] };
  else if (step.name === "suite" && set.length) {
    const hits = mg.setHits(set, failed);
    out = hits.length ? { ...base, verdict: "red", failing: hits } : { ...base, verdict: "ok", failing: [] };
  } else out = { ...base, verdict: "red", failing: failed };
  log(root, `  probe ${sha.slice(0, 8)} ${step.name}: ${out.verdict}${out.verdict === "red" ? ` (${out.failing.slice(0, 3).join(", ") || step.name})` : out.verdict === "env" ? ` (${out.why})` : ""} (log ${x.log})`);
  return out;
};

async function checkHead(root, dir, head, { pinned = false } = {}) {
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
    log(root, `${head.slice(0, 8)} ${step.name} RED (exit ${r.code}, ${what})`);
    // Newest first: a newer compiled-ok head runs the failing checks before this old head is rechecked or bisected.
    const reproduced = new Set();
    const nf = await mg.newestFirst({
      head,
      set: failing,
      newerThan: newerThanFn(root, dir, pinned),
      probe: probeFn(root, dir, step),
      run: (cur, set, guard, probeOnce) => redAt(root, dir, step, cur, cur === head ? r : null, set, guard, probeOnce, { pinned, reproduced }),
    });
    if (nf.kind === "env") {
      log(root, `${head.slice(0, 8)} ${step.name}: newer head ${nf.sha.slice(0, 8)} gave no verdict (${nf.why}); nothing judged, retried on a later tick`);
      writeState(root, { running: null, lastResult: { head, verdict: "env", step: step.name, what: nf.why, at: now() } });
      return;
    }
    if (nf.kind === "fixed") {
      const by8 = nf.by.slice(0, 8);
      log(root, `${head.slice(0, 8)} ${step.name} red (${what}) is fixed at the newer head ${by8}: no bisect, no revert; ${by8} gets its full check next`);
      for (const sha of reproduced) mark(root, dir, sha, "red", { step: step.name, what, note: `fixed at ${by8}; not bisected` });
      writeState(root, { running: null, lastRunAt: null, lastResult: { head, verdict: "superseded", step: step.name, what, by: nf.by, at: now() } });
      return;
    }
    if (nf.value === "done" || nf.head !== head) return;
    // "flake": the same head passed on the rerun; go on with the next step.
  }
  mark(root, dir, head, "green");
  writeState(root, { running: null, lastChecked: head, lastGreen: head, lastResult: { head, verdict: "green", at: now() } });
  log(root, `${head.slice(0, 8)} green`);
}

// A red step at one head (cur), after newest-first found no newer head to try first: rerun it at the same head (a pass
// is a flake), bisect the merges since the last green head, and revert or open a fix card. Returns "flake" or "done".
// `r` is the first run at cur (a newer head's is the probe's); guard() throws Superseded when a still newer head appears.
async function redAt(root, dir, step, cur, r, set, guard, probeOnce, { pinned, reproduced }) {
  const state = readState(root);
  const h8 = cur.slice(0, 8);
  if (!r) r = await probeOnce(cur, set);
  const failing = set;
  const what = failing.length ? failing.slice(0, 3).join(", ") : step.name;
  checkout(dir, cur);
  writeState(root, { running: { head: cur, step: step.name, at: now() } });
  log(root, `${h8} ${step.name} rechecking at the same head (${what}; log ${r.log})`);
  const again = await runCli(root, dir, step.recheck(failing), `${h8}-recheck`, mg.RECHECK_ENV, true);
  const v2 = mg.classifyStep(step.name, again.code, again.text, step.name === "suite" ? mg.failingTests(again.text, testSources(dir)) : []);
  if (v2.verdict === "env" || v2.verdict === "infra") {
    log(root, `${h8} ${step.name} recheck: ${v2.verdict.toUpperCase()} (${v2.why}), not the commit; no revert, retried on a later tick`);
    writeState(root, { running: null, lastResult: { head: cur, verdict: v2.verdict, step: step.name, what: v2.why, at: now() } });
    return "done";
  }
  const verdict = mg.judgeRecheck(true, again.code !== 0);
  if (verdict === "flake") {
    try { appendFileSync(flakeLog(root), `${now()} ${h8} ${step.name}: ${what} failed once, passed on recheck (${r.log})\n`); } catch {}
    log(root, `${h8} ${step.name}: flake (${what}), no revert`);
    return "flake";
  }
  reproduced.add(cur);
  await guard(); // a newer head may have arrived during the recheck
  // #2812: a pre-move e2e red is blamed on a PR only when the plain e2e fails too. A pass-2 check for data the ref
  // build's pass 1 cannot write fails the pre-move run alone; that PR gets a fix card and a comment, no revert.
  let needsSkip = false;
  if (step.name === "pre-move e2e") {
    log(root, `${h8} pre-move e2e reproduced; running the plain e2e to tell a pre-move-only failure from a real one`);
    const plain = await runCli(root, dir, mg.PLAIN_E2E, `${h8}-plain-e2e`, mg.RECHECK_ENV, true);
    const pv = mg.classifyStep("e2e", plain.code, plain.text);
    if (pv.verdict === "env" || pv.verdict === "infra") {
      log(root, `${h8} plain e2e: ${pv.verdict.toUpperCase()} (${pv.why}), not the commit; no revert, retried on a later tick`);
      writeState(root, { running: null, lastResult: { head: cur, verdict: pv.verdict, step: step.name, what: pv.why, at: now() } });
      return "done";
    }
    needsSkip = mg.needsSkip(step.name, plain.code);
    log(root, `${h8} plain e2e ${plain.code === 0 ? "passes: needs-skip, the pre-move failure is not a broken save" : "fails too: a real failure"} (log ${plain.log})`);
  }
  // Reproduced twice: find the merge that broke it.
  const rerun = step.recheck(failing);
  const list = mg.candidates(mergesSince(dir, state.lastGreen ?? state.startHead, cur, state.startHead));
  log(root, `${h8} ${step.name} reproduced (${what}); bisecting ${list.length} merge(s) since ${(state.lastGreen ?? state.startHead ?? "").slice(0, 8)}`);
  writeState(root, { running: { head: cur, step: `bisect ${step.name}`, at: now() } });
  // The recorded red set (#2052): the failing tests and FAIL lines at this head. The suite bisect treats a commit as
  // red only when it fails one of these tests; other steps (build, audit, e2e) have no test set and stay red/green.
  const redLines = step.name === "suite" ? mg.failLines(r.text) : [];
  if (step.name === "suite") log(root, `${h8} suite red set: ${failing.length ? failing.join(", ") : "(no test names parsed)"}${redLines.length ? ` | ${redLines.slice(0, 3).join(" | ")}` : ""}`);
  let b;
  if (step.name === "suite" && !failing.length) {
    b = { culprit: null, stop: "no failing test name could be read from the suite output, so no merge can be matched to it" };
  } else if (step.name === "suite") {
    b = await mg.bisectBySet(list, failing, async (c) => {
      await guard();
      checkout(dir, c.sha);
      const x = await runCli(root, dir, rerun, `bisect-${c.sha.slice(0, 8)}`, mg.RECHECK_ENV, true);
      const failed = mg.failingTests(x.text, testSources(dir));
      const xv = mg.classifyStep(step.name, x.code, x.text, failed);
      if (xv.verdict === "env" || xv.verdict === "infra") throw new Error(`bisect ${c.sha.slice(0, 8)}: ${xv.verdict} (${xv.why}), not the commit; nothing is reverted`);
      const hit = mg.setHits(failing, failed);
      log(root, `  bisect ${c.sha.slice(0, 8)} ${hit.length ? `red (${hit.slice(0, 3).join(", ")})` : x.code === 0 ? "green" : `green for the set (exit ${x.code}, other failures: ${failed.slice(0, 3).join(", ") || "none parsed"})`} (${c.subject.slice(0, 60)})`);
      return failed;
    }, { head: cur });
  } else {
    b = await mg.bisect(list, async (c) => {
      await guard();
      checkout(dir, c.sha);
      const x = await runCli(root, dir, rerun, `bisect-${c.sha.slice(0, 8)}`, mg.RECHECK_ENV, true);
      const xv = mg.classifyStep(step.name, x.code, x.text);
      if (xv.verdict === "env" || xv.verdict === "infra") throw new Error(`bisect ${c.sha.slice(0, 8)}: ${xv.verdict} (${xv.why}), not the commit; nothing is reverted`);
      log(root, `  bisect ${c.sha.slice(0, 8)} ${x.code === 0 ? "green" : "red"} (${c.subject.slice(0, 60)})`);
      return x.code !== 0;
    });
  }
  checkout(dir, cur);
  // Stale guard code judged this run (2026-11-18, #2884, #2899): no red mark, no revert; the restart rechecks the head.
  const drift = codeDriftNow(dir);
  if (drift.length) {
    log(root, `${h8} ${step.name} red, not acted on: ${mg.staleRefusal(drift)}`);
    writeState(root, { running: null });
    return "done";
  }
  // The size of the culprit (mg.isLargePr): a feature-sized merge gets a fix card, not a revert.
  if (b.culprit && !b.stop) b.culprit.files = mg.countFiles(mergeFiles(dir, b.culprit.sha));
  const earlier = b.culprit && !b.stop ? earlierRevertFor(dir, b.culprit, readState(root)) : null;
  const refusal = b.stop ?? mg.revertRefusal(b.culprit, readState(root), { step: step.name, needsSkip }) ?? (earlier ? mg.earlierRefusal(mg.prOfMerge(b.culprit.subject), earlier) : null);
  if (refusal) {
    log(root, `${h8} ${step.name} red, no revert: ${refusal}`);
    const pr = b.culprit ? mg.prOfMerge(b.culprit.subject) : null;
    if (earlier && b.culprit) commentOnPr(root, dir, pr, `**Main guard:** ${what} fails on main after this PR merged, but ${mg.earlierRefusal(pr, earlier)}. Not reverted. Log on the guard machine: \`${r.log}\``, "explained");
    else if (!b.stop && b.culprit && mg.isLargePr(b.culprit)) commentOnPr(root, dir, pr, `**Main guard:** ${what} fails on main after this PR merged, and the bisect points here. The PR changes ${b.culprit.files} files, so the guard opens a fix card instead of a revert (it reverts a merge this large only for a compile break). Fix forward on a branch. Log on the guard machine: \`${r.log}\``, "large");
    if (mg.fixCardInstead(b.culprit, step.name, { needsSkip })) openFixCard(root, dir, b.culprit, step.name, what, r.log, { needsSkip });
    mark(root, dir, cur, "red", { step: step.name, what, note: b.stop ? refusal : "" });
    writeState(root, { running: null, lastChecked: cur, lastResult: { head: cur, verdict: "red", step: step.name, what, at: now(), note: refusal, failing, failLines: redLines } });
    return "done";
  }
  // A newer head that already passes the failing checks means the fix is on main: no revert.
  const fixedBy = await mg.newerFixes({ head: cur, set: failing, newerThan: newerThanFn(root, dir, pinned), probe: probeOnce });
  if (fixedBy) {
    const note = `a newer head ${fixedBy.slice(0, 8)} already passes the failing checks; culprit #${mg.prOfMerge(b.culprit.subject)} is not reverted`;
    log(root, `${h8} ${step.name} red, no revert: ${note}`);
    mark(root, dir, cur, "red", { step: step.name, what, note });
    writeState(root, { running: null, lastRunAt: null, lastChecked: cur, lastResult: { head: cur, verdict: "red", step: step.name, what, at: now(), note, failing, failLines: redLines } });
    return "done";
  }
  await revert(root, dir, b.culprit, what, r.log);
  mark(root, dir, cur, "red", { step: step.name, what });
  writeState(root, { running: null, lastChecked: cur, lastResult: { head: cur, verdict: "red", step: step.name, what, at: now(), culprit: mg.prOfMerge(b.culprit.subject), failing, failLines: redLines } });
  return "done";
}

// Stage 0: validate --all on a new head (#2889), in the compile worktree, before the compile. It never marks the head red
// on GitHub (the compiled-ok pin is about the build, not about a validator), and it never blocks the compile or stage 2.
async function validateHead(root, vdir, head) {
  const h8 = head.slice(0, 8);
  writeState(root, { lastValidateAt: now(), validating: { head, at: now() } });
  let lastLog = "";
  try {
    const state = readState(root);
    const run = async (sha, only) => {
      checkout(vdir, sha);
      const x = await runCli(root, vdir, mg.VALIDATE, `${sha.slice(0, 8)}-validate${only ? "-probe" : ""}`, only ? { [mg.VALIDATE_ONLY_ENV]: only.join(",") } : {});
      lastLog = x.log;
      const failed = mg.failingChecks(x.text);
      const v = mg.classifyStep("validate", x.code, x.text, failed);
      if (only) log(root, `  validate ${sha.slice(0, 8)} ${x.code === 0 ? "green" : v.verdict === "red" ? `red (${failed.slice(0, 3).join(", ") || "no check named"})` : v.verdict.toUpperCase()} for ${only.slice(0, 3).join(", ")}`);
      return { verdict: v.verdict, failed, why: v.why, text: x.text, log: x.log };
    };
    let top = null;
    const from = state.lastValidated ?? state.lastGreen ?? state.startHead;
    const g = await mg.validateGate({
      head,
      merges: mergesSince(vdir, from, head, state.startHead),
      state,
      run: async (sha, only) => { const x = await run(sha, only); if (sha === head && !only) top = x; return x; },
      earlier: (c) => earlierRevertFor(vdir, c, state),
    });
    if (g.verdict === "green") {
      writeState(root, { lastValidated: head, lastValidateRed: null, validateRed: null });
      log(root, `${h8} validate green`);
      return;
    }
    if (g.verdict === "env" || g.verdict === "unnamed") {
      log(root, `${h8} validate: no verdict (${g.why}); retried on a later tick`);
      writeState(root, { lastValidateAt: new Date(Date.now() + 7 * 60 * 1000).toISOString() }); // no verdict: back off to ~10 min between tries
      return;
    }
    if (g.verdict === "flake") {
      try { appendFileSync(flakeLog(root), `${now()} ${h8} validate: ${g.set.join(", ")} failed once, passed on recheck (${lastLog})\n`); } catch {}
      log(root, `${h8} validate: flake (${g.set.join(", ")}), no action; retried on a later tick`);
      writeState(root, { lastValidateAt: new Date(Date.now() + 7 * 60 * 1000).toISOString() }); // no verdict: back off to ~10 min between tries
      return;
    }
    if (g.verdict === "known") {
      log(root, `${h8} validate still red (${g.set.join(", ")}), already flagged${g.flagged.pr ? ` on #${g.flagged.pr}` : ""}`);
      writeState(root, { lastValidateRed: head });
      return;
    }
    // red: the set, the merge that introduced it, and what to do about it.
    const out = mg.checkOutput(top?.text, g.set);
    const checks = g.set;
    const drift = codeDriftNow(vdir);
    if (drift.length) { log(root, `${h8} validate red (${checks.join(", ")}), not acted on: ${mg.staleRefusal(drift)}`); return; }
    const c = g.culprit;
    const pr = c ? mg.prOfMerge(c.subject) : null;
    log(root, `${h8} validate RED (${checks.join(", ")}); ${c ? `introduced by #${pr} (${c.sha.slice(0, 8)}): ${g.action}` : `no merge to blame: ${g.stop}`}${g.refusal && c ? ` (${g.refusal})` : ""}`);
    writeState(root, { lastValidateRed: head, validateRed: { head, checks, pr, action: g.action, at: now() } });
    if (!c) { await staffLine(root, `main guard: validate is red on main at ${h8} (${checks.slice(0, 3).join(", ")}); no single merge to blame (${g.stop})`); return; }
    if (g.action === "revert") {
      await revert(root, vdir, c, `validate ${g.hits.join(", ")}`, lastLog);
      return;
    }
    if (g.action === "explained") {
      commentOnPr(root, vdir, pr, mg.validateComment(c, checks, out, lastLog, { earlier: g.earlier }), "validate-explained");
      await staffLine(root, `main guard: validate is red on main (${checks.slice(0, 3).join(", ")}) after #${pr}, which follows the reverted #${g.earlier.pr}; not reverted, ship it with the re-land`);
      return;
    }
    commentOnPr(root, vdir, pr, mg.validateComment(c, checks, out, lastLog), "validate");
    const card = mg.validateFixCard(c, checks, out, lastLog);
    const bf = join(mainRoot(root), "tmp", "logs", `main-guard-validate-card-${pr}-${Date.now()}.md`);
    writeFileSync(bf, card.body);
    const r = gh(vdir, ["issue", "create", "--title", card.title, "--body-file", bf]);
    log(root, `validate fix card: ${r.status === 0 ? (r.stdout || "").trim() : `create failed: ${(r.stderr || "").trim().slice(0, 200)}`}`);
    await staffLine(root, `main guard: validate is red on main (${checks.slice(0, 3).join(", ")}), introduced by #${pr}; commented, fix card ${r.status === 0 ? (r.stdout || "").trim() : "failed"}`);
  } finally {
    writeState(root, { validating: null });
  }
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

function openFixCard(root, dir, c, stepName, what, failLog, opts = {}) {
  const card = mg.fixCard(c, stepName, what, failLog, opts);
  const bf = join(mainRoot(root), "tmp", "logs", `main-guard-fixcard-${mg.prOfMerge(c.subject)}-${Date.now()}.md`);
  writeFileSync(bf, card.body);
  const r = gh(dir, ["issue", "create", "--title", card.title, "--body-file", bf]);
  log(root, `fix card: ${r.status === 0 ? (r.stdout || "").trim() : `create failed: ${(r.stderr || "").trim().slice(0, 200)}`}`);
  if (opts.needsSkip) {
    const pr = mg.prOfMerge(c.subject);
    const cf = join(mainRoot(root), "tmp", "logs", `main-guard-needs-skip-${pr}.md`);
    writeFileSync(cf, `**Main guard:** the pre-move e2e failed on main after this PR merged, but the plain e2e passes, so this is not a broken save: a pass-2 check needs data the pinned ref build (502ebf39) cannot write. Guard it with LF_E2E.RequiresPayload("<payload>") (Tests/LF_E2E.c; validate-e2e-payload-guard names the rule). Not reverted. Log: ${failLog}`);
    gh(dir, ["pr", "comment", String(pr), "--body-file", cf]);
  }
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
  const first = pin.writeLeaseChecked(dir, "held", HOST, process.pid);
  if (!first.ok) log(root, `lease write FAILED at start: ${first.why}`);
  writeState(root, { running: null, pid: process.pid, beatAt: now(), activityAt: now(), leaseErr: first.ok ? null : first.why });
  // The lease is renewed on its own timer: the loop below awaits a compile for many minutes, and a lease renewed only
  // between ticks went 16 min stale under a live worker (#2754), which the watchdog and `lead` read as a dead guard.
  let leaseLost = null;
  let restart = false;
  let staleSaidAt = 0;
  writeState(root, { code: LOADED_CODE, codeFrom: dirname(dirname(SELF)) });
  // A renewal that fails is logged, never swallowed (2026-11-22: GitHub refused every one from 2026-11-20 on, 422 "maximum
  // number of statuses", and nothing said so). The watchdog renews a stale lease for a worker that still progresses.
  let leaseFails = 0;
  const beat = setInterval(() => {
    try {
      const cur = pin.readLease(dir);
      if (cur && cur.state === "held" && !pin.sameLeaseHost(cur.host, HOST)) { leaseLost = cur.host; return; }
      const w = pin.writeLeaseChecked(dir, "held", HOST, process.pid);
      if (w.ok) {
        if (leaseFails) log(root, `lease renewal ok again after ${leaseFails} failure(s) (${w.ctx})`);
        leaseFails = 0;
        if (readState(root).leaseErr) writeState(root, { leaseErr: null });
      } else {
        leaseFails++;
        if (leaseFails === 1 || leaseFails % 6 === 0) log(root, `lease renewal FAILED (${leaseFails} in a row): ${w.why}; the watchdog renews it while this worker progresses`);
        writeState(root, { leaseErr: w.why });
      }
    } catch (e) { log(root, `lease renewal failed: ${e?.message ?? e}`); }
  }, 5 * 60 * 1000);
  beat.unref?.();
  // The local heartbeat (mg.workerProgress): the event loop runs (beatAt) and the step it waits on writes (activityAt).
  const pulse = setInterval(() => { try { writeState(root, { beatAt: now(), activityAt: activityNow() }); } catch {} }, 60 * 1000);
  pulse.unref?.();
  for (;;) {
    if (leaseLost) { log(root, `lease taken by ${leaseLost}; worker exits`); break; }
    if (off()) { log(root, "LF_MAIN_GUARD=0: worker exits"); break; }
    if (readState(root).stop) { writeState(root, { stop: false }); log(root, "stop requested: worker exits"); break; }
    try {
      git(dir, ["fetch", "origin", "--prune"]);
      const head = git(dir, ["rev-parse", "origin/main"]).stdout.trim();
      // Stale code judges nothing: wait for a running stage 2 (it cannot revert), then restart on origin/main's code.
      const drift = codeDriftNow(dir);
      if (drift.length) {
        const act = mg.staleAction({ drift, busy: !!stage2, checkoutFresh: !stage2 && freshenMainCheckout(root) });
        if (act === "restart") { log(root, `guard code changed on origin/main (${drift.join(", ")}); restarting on ${head.slice(0, 8)}`); restart = true; break; }
        if (act === "stuck" && Date.now() - staleSaidAt > 30 * 60 * 1000) {
          staleSaidAt = Date.now();
          log(root, `STALE: ${mg.staleRefusal(drift)}, and the main checkout cannot fast-forward; checking nothing until it does`);
        }
        await new Promise((r) => setTimeout(r, TICK_MS));
        continue;
      }
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
          stage2 = checkHead(root, dir, req, { pinned: true }).catch((e) => { log(root, `stage 2 failed: ${e?.message ?? e}`); writeState(root, { running: null }); }).finally(() => { stage2 = null; });
        } else if (v === "fail") mark(root, cdir, req, "red", { step: "compile" });
        else { log(root, `${req.slice(0, 8)}: no compile verdict; request kept`); writeState(root, { request: req }); }
      }
      // Stage 0: validate the newest head (cheap, no Workbench); a failure here is flagged, it does not hold the compile.
      if (mg.shouldValidate(readState(root), head, Date.now()).run) {
        try { await validateHead(root, cdir, head); } catch (e) { log(root, `validate stage failed: ${e?.message ?? e}`); writeState(root, { validating: null }); }
      }
      // Stage 1 next: compile the newest head.
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
  clearInterval(pulse);
  try { unlinkSync(lockPath(root)); } catch {}
  if (restart) {
    // The lease stays held (same host), so the new worker takes it over as ours with no gap another machine could use.
    const code = await main(["start"], { root });
    log(root, `self-restart ${code === 0 ? `ok (pid ${workerAlive(root)})` : `FAILED (exit ${code}); the watchdog retries`}`);
    return 0;
  }
  pin.writeLease(dir, "released", HOST);
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
  const st = readState(root);
  const d = mg.watchDecision({ off: off(), alive, lease, host: HOST, restarts: w.restarts ?? [], local: { beatAt: st.beatAt, activityAt: st.activityAt } });
  console.log(`main-guard watch: ${d.action}: ${d.why}`);
  if (dryRun || d.action === "none") return 0;
  log(root, `watch: ${d.action}: ${d.why}`);
  const say = async (text) => {
    if (Date.now() - Date.parse(w.alertAt ?? "") < 30 * 60 * 1000) return;
    w.alertAt = now();
    await staffLine(root, text);
  };
  if (d.action === "alert") { await say(`main guard watchdog: ${d.why}`); save(); return 1; }
  if (d.action === "renew") {
    const rw = pin.writeLeaseChecked(mr, "held", HOST, alive);
    log(root, `watch: renew: ${rw.ok ? `lease renewed on ${rw.ctx} for pid ${alive}` : `FAILED: ${rw.why}`}`);
    if (!rw.ok) { await say(`main guard watchdog: worker pid ${alive} is progressing but its lease cannot be written: ${rw.why}`); save(); return 1; }
    return 0;
  }
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
    // The worker runs the main checkout's code, fast-forwarded to origin/main first (never a worktree's copy, never a
    // checkout behind origin/main: 2026-11-18 a worker that started 14 s before the pull reverted two PRs falsely).
    const mr = mainRoot(root);
    if (!freshenMainCheckout(root)) {
      const drift = mg.codeDrift(diskCodeIds(mr), originCodeIds(mr));
      console.log(`FAIL  main-guard: the main checkout's guard code (${drift.join(", ")}) is not origin/main's and it cannot fast-forward; update ${mr} to origin/main, then start again`);
      return 1;
    }
    const entry = join(mr, "tools", "main-guard.mjs");
    const { quietSpawnDetached } = await import("./lib/quiet-launch.mjs");
    const il = await import("./lib/interactive-launch.mjs");
    try {
      // Over ssh a detached child dies with the session (2026-11-16: three workers logged "starts" and vanished), so the
      // worker starts through a one-shot Interactive task on the console desktop, which outlives the ssh job.
      if (il.interactiveNeeded()) il.interactiveSpawnDetached(process.execPath, [entry, "worker", mr], { cwd: mr, marker: "main-guard.mjs" });
      else quietSpawnDetached(process.execPath, [entry, "worker", mr], { cwd: mr, env: process.env });
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
    if (pin.sameLeaseHost(pin.readLease(mainRoot(root))?.host, HOST)) pin.writeLease(mainRoot(root), "released", HOST);
    console.log(`main-guard: stopped (pid ${pid})`);
    return 0;
  }
  if (sub === "status" || !sub) {
    const alive = workerAlive(root);
    if (alive) {
      console.log(mg.statusLine(readState(root), { alive, off: off() }));
      const ws = readState(root);
      const p = mg.workerProgress({ beatAt: ws.beatAt, activityAt: ws.activityAt });
      console.log(`  worker: ${p.state} (${p.why})${ws.leaseErr ? `; its lease writes fail: ${ws.leaseErr}` : ""}`);
    }
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
    else {
      console.log(`  github: ${who}`);
      const dr = mg.codeDrift(readState(root).code, originCodeIds(mr));
      console.log(`  code: ${dr.length ? `STALE (${dr.join(", ")} differ from origin/main; the worker restarts itself when idle, or reverts nothing)` : "origin/main's"}`);
    }
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
