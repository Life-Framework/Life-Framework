// tools/main-guard.mjs - the post-merge main guard (docs/worktrees.md "Main guard"). The slim ship gate runs
// validate --full + LOGIC; this worker runs the rest on main after the merge:
//   1. every new origin/main head (a burst batched to one run per ~10 min): build, the full suite, the
//      placement audit and the pre-move e2e, in its own worktree (Life-Framework-ws-main-guard-run), each
//      heavy run through the pool at high priority;
//   2. a red step is rerun at the same head (only the failing tests): a one-off is a flake, logged to
//      tmp/logs/main-guard-flakes.log, and nothing is reverted;
//   3. a red that reproduces: bisect the first-parent PR merges since the last green head (only merges made
//      after the guard started), rerunning only what failed at each candidate;
//   4. the culprit merge is reverted with `git revert -m 1` on a ws/revert-pr<N>-guard branch, a PR titled
//      "Revert #N (main guard: <what failed>)" with auto-merge (GitHub's required validate check gates it),
//      a comment on the original PR, its issues reopened as `regression`, one line to the staff webhook.
// Never reverts a revert, never touches a merge from before it started, never pushes main. LF_MAIN_GUARD=0 stops it.
// One worker per repo (hub lock); state in <hub>/main-guard.json; log tmp/logs/main-guard.log.

import { spawn, spawnSync } from "node:child_process";
import { appendFileSync, existsSync, mkdirSync, openSync, closeSync, readFileSync, renameSync, unlinkSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import * as wt from "./wt.mjs";
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
  return join(dirname(mainRoot(root)), `Life-Framework-ws-${slug}`);
}

// A head's verdict: <hub>/main-guard-state.json and the GitHub commit status `main-guard` (what world sync and
// wt new pin to on any machine).
function mark(root, dir, sha, status, extra) {
  try { pin.recordHead(hub(root), sha, status, extra); } catch (e) { log(root, `state write failed: ${e.message}`); }
  const r = pin.postStatus(dir, sha, status, extra?.step);
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

function checkout(dir, sha) {
  git(dir, ["reset", "-q", "--hard"]);
  const r = git(dir, ["checkout", "-q", "--detach", sha]);
  if (r.status !== 0) throw new Error(`checkout ${sha.slice(0, 8)}: ${(r.stderr || r.stdout).trim()}`);
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
    const failing = step.name === "suite" ? mg.failingTests(r.text) : [];
    const what = failing.length ? failing.slice(0, 3).join(", ") : step.name;
    log(root, `${head.slice(0, 8)} ${step.name} RED (exit ${r.code}, ${what}); rechecking at the same head (log ${r.log})`);
    const again = await runCli(root, dir, step.recheck(failing), `${head.slice(0, 8)}-recheck`);
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
    const b = await mg.bisect(list, async (c) => {
      checkout(dir, c.sha);
      const x = await runCli(root, dir, recheck, `bisect-${c.sha.slice(0, 8)}`);
      log(root, `  bisect ${c.sha.slice(0, 8)} ${x.code === 0 ? "green" : "red"} (${c.subject.slice(0, 60)})`);
      return x.code !== 0;
    });
    checkout(dir, head);
    const refusal = mg.revertRefusal(b.culprit, readState(root), { step: step.name });
    if (refusal) {
      log(root, `${head.slice(0, 8)} ${step.name} red, no revert: ${refusal}`);
      if (mg.fixCardInstead(b.culprit, step.name)) openFixCard(root, dir, b.culprit, step.name, what, r.log);
      mark(root, dir, head, "red", { step: step.name, what });
      writeState(root, { running: null, lastChecked: head, lastResult: { head, verdict: "red", step: step.name, what, at: now(), note: refusal } });
      return;
    }
    await revert(root, dir, b.culprit, what, r.log);
    mark(root, dir, head, "red", { step: step.name, what });
    writeState(root, { running: null, lastChecked: head, lastResult: { head, verdict: "red", step: step.name, what, at: now(), culprit: mg.prOfMerge(b.culprit.subject) } });
    return;
  }
  mark(root, dir, head, "green");
  writeState(root, { running: null, lastChecked: head, lastGreen: head, lastResult: { head, verdict: "green", at: now() } });
  log(root, `${head.slice(0, 8)} green`);
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
  const say = (l) => { try { appendFileSync(file, `${l}
`); } catch {} };
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
  // One guard for the repo: the lease (main-guard/lease status on the root commit) must be free, stale or ours.
  const lease = pin.readLease(dir);
  const ld = pin.leaseDecision(lease, HOST);
  if (lease === undefined) log(root, "lease: gh could not read it; starting on the local lock only");
  else if (!ld.ok) { log(root, `lease refused: ${ld.why}; worker exits`); try { unlinkSync(lockPath(root)); } catch {} return 1; }
  pin.writeLease(dir, "held", HOST, process.pid);
  let leaseAt = Date.now();
  writeState(root, { running: null, pid: process.pid });
  for (;;) {
    if (Date.now() - leaseAt > 5 * 60 * 1000) {
      const cur = pin.readLease(dir);
      if (cur && cur.state === "held" && cur.host !== HOST) { log(root, `lease taken by ${cur.host}; worker exits`); break; }
      pin.writeLease(dir, "held", HOST, process.pid);
      leaseAt = Date.now();
    }
    if (off()) { log(root, "LF_MAIN_GUARD=0: worker exits"); break; }
    if (readState(root).stop) { writeState(root, { stop: false }); log(root, "stop requested: worker exits"); break; }
    try {
      git(dir, ["fetch", "origin", "--prune"]);
      const head = git(dir, ["rev-parse", "origin/main"]).stdout.trim();
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
  pin.writeLease(dir, "released", HOST);
  try { unlinkSync(lockPath(root)); } catch {}
  return 0;
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
    try {
      quietSpawnDetached(process.execPath, [SELF, "worker", mainRoot(root)], { cwd: dirname(dirname(SELF)), env: process.env });
    } catch (e) {
      console.log(`FAIL  main-guard: worker did not start: ${e?.message ?? e} (log ${logPath(root)})`);
      return 1;
    }
    for (let i = 0; i < 20 && !workerAlive(root); i++) await new Promise((r) => setTimeout(r, 500));
    console.log(mg.statusLine(readState(root), { alive: workerAlive(root) }));
    return workerAlive(root) ? 0 : 1;
  }
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
    const who = lease === undefined ? "lease unknown (gh failed)" : !lease || lease.state !== "held" ? "no guard holds the lease" : `running on ${lease.host} (pid ${lease.pid}, lease renewed ${Math.round((Date.now() - Date.parse(lease.at)) / 60000)} min ago)`;
    if (!alive) console.log(`MAIN GUARD ${who}`);
    else console.log(`  github: ${who}`);
    console.log(`  main ${head.slice(0, 8)}: ${hs ?? "unchecked"}; newest compiled-ok or better: ${ck ? `${ck.sha.slice(0, 8)} (${ck.status}, ${ck.behind} behind)` : "none"}`);
    console.log(`  log ${logPath(root)}; flakes ${flakeLog(root)}`);
    return 0;
  }
  console.log("usage: tools\\cli main-guard start | stop | status");
  return 1;
}

if (process.argv[1] && resolve(process.argv[1]).toLowerCase() === SELF.toLowerCase() && process.argv[2] === "worker") {
  runWorker(process.argv[3] || wt.mainRootOf(dirname(dirname(SELF)))).then((c) => process.exit(c));
}
