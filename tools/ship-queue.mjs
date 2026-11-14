// tools/ship-queue.mjs - the ship queue's worker and its commands (`tools\cli ship-queue ...`).
// `wt ship` pushes the branch, opens the PR (auto-merge off) and enqueues it (tools/lib/ship-queue.mjs);
// one worker per repo takes queued entries in FIFO order, as many at once as there are pool Workbench
// slots and as long as their files do not overlap, and for each:
//   1. merges origin/<base> into the worktree once (prs.mjs healMerge: merge drivers + regeneration);
//   2. runs `ci --remote` there (Workbench steps and the suite on pool machines; a branch with no
//      addons/ change runs `validate --all`, as `wt gate` does);
//   3. if main moved meanwhile, re-gates only when the new main commits change a file the PR changes
//      or a generated file (regateDecision), else pushes the gated head;
//   4. pushes and queues auto-merge, or comments the failure and the log tail on the PR and moves on.
// docs/worktrees.md "Ship queue". Every wait here is bounded (job budget, worker lifetime, wait budget).

import { spawn, spawnSync } from "node:child_process";
import { appendFileSync, closeSync, existsSync, mkdirSync, openSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import * as wt from "./wt.mjs";
import * as sq from "./lib/ship-queue.mjs";

const SELF = fileURLToPath(import.meta.url);
const JOB_BUDGET_MS = 100 * 60 * 1000; // one gate: ci measured 15-40 min under load; killed past this
const WORKER_LIFE_MS = 12 * 60 * 60 * 1000;
const WORKER_IDLE_MS = 3 * 60 * 1000; // an empty queue for this long ends the worker; the next ship starts one
const TICK_MS = 15 * 1000;

const hub = (root) => wt.ensureHub(root);
const now = () => new Date().toISOString();

// Read-modify-write the queue under the hub lock.
export function withQueue(root, fn) {
  const lock = wt.acquireLock(root, "ship-queue-state", { waitMs: 30000, pollMs: 100, stealMs: 60000 });
  try {
    const q = sq.readQueue(hub(root));
    const out = fn(q);
    sq.writeQueue(hub(root), q);
    return out;
  } finally {
    wt.releaseLock(root, "ship-queue-state");
  }
}

export function readQueue(root) {
  return sq.readQueue(hub(root));
}

function update(root, id, patch, note) {
  return withQueue(root, (q) => {
    const e = q.entries.find((x) => x.id === id);
    if (!e) return null;
    Object.assign(e, patch);
    if (note) e.history = [...(e.history ?? []), `${now()} ${note}`].slice(-20);
    return { ...e };
  });
}

// How many gates run at once: pool.json / lf.local.json "shipQueue": { "parallel": N }, else the sum of
// the Workbench limits of the ssh machines with the workbench cap (at least 1).
export async function gateSlots(root) {
  const pool = await import("./remote/pool.mjs");
  const cfg = pool.poolSettings(root)?.shipQueue ?? {};
  if (Number(cfg.parallel) > 0) return Number(cfg.parallel);
  const n = pool.loadMachines(root).filter((m) => m.via === "ssh" && !m.disabled && (m.caps ?? []).includes("workbench")).reduce((s, m) => s + (Number(m.workbenchLimit) > 0 ? Number(m.workbenchLimit) : 1), 0);
  return Math.max(1, n);
}

// ---------------------------------------------------------------- worker

function workerAlive(root) {
  const f = join(hub(root), "locks", "ship-queue-worker.lock");
  if (!existsSync(f)) return null;
  try {
    const info = JSON.parse(readFileSync(f, "utf8"));
    return info.pid && wt.pidAlive(info.pid) ? info.pid : null;
  } catch {
    return null;
  }
}

export function workerLog(root) {
  return join(wt.mainRootOf(root), "tmp", "logs", "ship-queue-worker.log");
}

// Start the worker when none runs. It runs this file's own copy (the checkout `wt ship` was called from).
export async function ensureWorker(root) {
  const pid = workerAlive(root);
  if (pid) return { pid, started: false };
  mkdirSync(dirname(workerLog(root)), { recursive: true });
  const { quietSpawnDetached } = await import("./lib/quiet-launch.mjs");
  const child = quietSpawnDetached(process.execPath, [SELF, "worker", wt.mainRootOf(root)], { cwd: dirname(dirname(SELF)), env: process.env });
  return { pid: child, started: true };
}

function wlog(root, s) {
  const line = `${now()} ${s}\n`;
  try {
    appendFileSync(workerLog(root), line);
  } catch {}
}

export async function runWorker(root) {
  try {
    wt.acquireLock(root, "ship-queue-worker", { meta: { purpose: "ship queue worker" } });
  } catch {
    return 0; // another worker has it
  }
  const t0 = Date.now();
  let idleSince = null;
  const jobs = new Map(); // id -> promise
  wlog(root, `worker ${process.pid} up`);
  try {
    // Entries a dead worker left running go back to the head of the queue.
    withQueue(root, (q) => {
      for (const e of q.entries) {
        if (sq.RUNNING.includes(e.state) && !(e.workerPid && e.workerPid !== process.pid && wt.pidAlive(e.workerPid))) {
          e.state = "queued";
          e.history = [...(e.history ?? []), `${now()} requeued: its worker ${e.workerPid} is gone`];
        }
      }
    });
    while (Date.now() - t0 < WORKER_LIFE_MS) {
      const slots = await gateSlots(root);
      const q = readQueue(root);
      const { start } = sq.pickRunnable(q.entries, { slots });
      for (const { entry, why } of start) {
        const claimed = update(root, entry.id, { state: "merging", startedAt: now(), workerPid: process.pid }, `start: ${why}`);
        if (!claimed) continue;
        wlog(root, `start ${entry.slug} (#${entry.pr}): ${why}`);
        jobs.set(entry.id, runJob(root, claimed).catch((e) => fail(root, claimed, `worker error: ${e?.stack ?? e}`)).finally(() => jobs.delete(entry.id)));
      }
      const live = readQueue(root).entries.filter((e) => !sq.DONE.includes(e.state));
      if (!live.length && !jobs.size) {
        idleSince ??= Date.now();
        if (Date.now() - idleSince > WORKER_IDLE_MS) break;
      } else idleSince = null;
      await Promise.race([...jobs.values(), new Promise((r) => setTimeout(r, TICK_MS))]);
    }
    await Promise.all(jobs.values());
  } finally {
    wlog(root, `worker ${process.pid} down`);
    wt.releaseLock(root, "ship-queue-worker");
  }
  return 0;
}

// One child process, its output appended to the entry's log, killed (process tree) past its budget.
function runLogged(log, exe, args, { cwd, budgetMs = JOB_BUDGET_MS, env = process.env } = {}) {
  return new Promise((res) => {
    appendFileSync(log, `\n$ ${[exe, ...args].join(" ")}   (in ${cwd})\n`);
    const fd = openSync(log, "a");
    const child = spawn(exe, args, { cwd, env, stdio: ["ignore", fd, fd], windowsHide: true });
    const timer = setTimeout(() => {
      appendFileSync(log, `\nTIMEOUT after ${Math.round(budgetMs / 1000)} s; killing the gate\n`);
      if (process.platform === "win32") spawnSync("taskkill", ["/PID", String(child.pid), "/T", "/F"], { windowsHide: true });
      else child.kill("SIGKILL");
    }, budgetMs);
    const done = (code) => {
      clearTimeout(timer);
      try {
        closeSync(fd);
      } catch {}
      res(code ?? 1);
    };
    child.on("exit", done);
    child.on("error", (e) => {
      appendFileSync(log, `spawn failed: ${e.message}\n`);
      done(1);
    });
  });
}

const changedFiles = (wtPath, base) => wt.git(wtPath, ["diff", "--name-only", `${base}...HEAD`]).stdout.split(/\r?\n/).filter(Boolean);

// The gate a branch gets: `ci --remote` when addons/ changed (or --full-gate), else `validate --all` (cheap, here).
export function gateCommand(files, fullGate = false) {
  return fullGate || files.some((f) => f.startsWith("addons/")) ? ["ci", "--remote"] : ["validate", "--all"];
}

async function runJob(root, entry) {
  const { wtPath, base, slug } = entry;
  const baseRef = `origin/${base}`;
  const log = entry.log;
  const say = (s) => appendFileSync(log, `${now()} ship-queue: ${s}\n`);
  mkdirSync(dirname(log), { recursive: true });
  say(`gate for ${slug} (PR #${entry.pr}) starts`);
  let attempts = entry.attempts ?? 0;
  const { healMerge } = await import("./prs.mjs");
  for (;;) {
    wt.git(wtPath, ["fetch", "origin", "--prune"]);
    if (wt.currentBranch(wtPath) !== entry.branch) return fail(root, entry, `worktree is on '${wt.currentBranch(wtPath)}', not ${entry.branch}`);
    update(root, entry.id, { state: "merging" });
    const m = healMerge(wtPath, { base: baseRef });
    if (m.status === "needs-human" || m.status === "refused") return fail(root, entry, `${baseRef} does not merge in automatically: ${m.reason}${m.conflicted?.length ? ` (${m.conflicted.join(", ")})` : ""}`);
    say(`merged ${baseRef}: ${m.status}${m.resolved?.length ? ` (resolved ${m.resolved.join(", ")})` : ""}`);
    const gatedBase = wt.git(wtPath, ["rev-parse", baseRef]).stdout.trim();
    const files = changedFiles(wtPath, baseRef);
    const cmd = gateCommand(files, entry.fullGate);
    attempts++;
    update(root, entry.id, { state: "gating", attempts, files, gatedBase }, `gate ${attempts}: ${cmd.join(" ")} on main ${gatedBase.slice(0, 7)}`);
    const t0 = Date.now();
    const code = await runLogged(log, process.execPath, [join(wtPath, "tools", "cli.mjs"), ...cmd], { cwd: wtPath, env: { ...process.env, LF_SHIP_QUEUE_JOB: entry.id } });
    const text = readFileSync(log, "utf8");
    const machine = [...text.matchAll(/ci: Workbench compile \+ build on ([\w.-]+)/g)].map((x) => x[1]).pop() ?? (cmd[0] === "ci" ? null : "this PC (validate)");
    update(root, entry.id, { machine, gateS: Math.round((Date.now() - t0) / 1000) }, `gate ${attempts} exit ${code} in ${Math.round((Date.now() - t0) / 1000)} s`);
    if (code !== 0) return fail(root, entry, `gate ${cmd.join(" ")} failed (exit ${code})`);
    wt.git(wtPath, ["fetch", "origin", "--prune"]);
    const newBase = wt.git(wtPath, ["rev-parse", baseRef]).stdout.trim();
    const mainFiles = newBase === gatedBase ? [] : wt.git(wtPath, ["diff", "--name-only", gatedBase, newBase]).stdout.split(/\r?\n/).filter(Boolean);
    const d = sq.regateDecision({ prFiles: changedFiles(wtPath, gatedBase), mainFiles, attempts });
    say(`after the gate: ${d.why}`);
    update(root, entry.id, {}, d.regate ? `re-gate: ${d.why}` : d.why);
    if (!d.regate) break;
  }
  update(root, entry.id, { state: "pushing" });
  const push = wt.git(wtPath, ["push", "-u", "origin", entry.branch]);
  if (push.status !== 0) return fail(root, entry, `push failed: ${(push.stderr || push.stdout).trim()}`);
  const head = wt.git(wtPath, ["rev-parse", "HEAD"]).stdout.trim();
  try {
    const out = wt.autoMergePrGh(wtPath, entry.pr);
    say(`pushed ${head.slice(0, 7)}; auto-merge queued${out ? ` (${out})` : ""}`);
  } catch (e) {
    return fail(root, entry, `pushed ${head.slice(0, 7)} but auto-merge failed: ${e.message}`);
  }
  update(root, entry.id, { state: "passed", endedAt: now(), head, why: `gated, pushed ${head.slice(0, 7)}, auto-merge queued` }, "passed");
  wlog(root, `passed ${slug} (#${entry.pr}) after ${attempts} gate(s)`);
}

// The failure goes to the PR (comment with the log tail) and the queue moves on.
function fail(root, entry, why) {
  const log = entry.log;
  try {
    appendFileSync(log, `${now()} ship-queue: FAIL ${why}\n`);
  } catch {}
  update(root, entry.id, { state: "failed", endedAt: now(), why }, `failed: ${why}`);
  wlog(root, `failed ${entry.slug} (#${entry.pr}): ${why}`);
  if (!entry.pr || process.env.LF_SHIP_QUEUE_NO_COMMENT === "1") return;
  let tail = "";
  try {
    const lines = readFileSync(log, "utf8").split(/\r?\n/);
    const keep = lines.filter((l) => /^(FAIL|SUMMARY|TIMEOUT|ERROR|SCRIPT|  - |ship-queue|.*\bFAIL\b)/.test(l.trim()) || /^\d{4}-.*ship-queue:/.test(l));
    tail = (keep.length ? keep : lines).slice(-40).join("\n");
  } catch {}
  const body = [
    `**Ship queue:** the gate for \`${entry.slug}\` failed; the PR is not merging. ${why}`,
    "",
    "```",
    tail.slice(-6000),
    "```",
    "",
    `Full log on the shipping PC: \`${log}\`. Fix, commit, and re-run \`tools\\cli wt ship ${entry.slug}\`.`,
  ].join("\n");
  const f = join(dirname(log), `${entry.id}-comment.md`);
  writeFileSync(f, body, "utf8");
  wt.run(wt.ghBin(), ["pr", "comment", String(entry.pr), "--body-file", f], { cwd: entry.wtPath });
}

// ---------------------------------------------------------------- wait / status / cancel

// Block until the slug's entry is passed, failed or cancelled; bounded by budgetMs. Exit code 0 on passed.
export async function waitFor(root, slug, { budgetMs = 3 * 60 * 60 * 1000, print = console.log } = {}) {
  const t0 = Date.now();
  let last = "";
  for (;;) {
    const e = sq.entryFor(readQueue(root), slug);
    if (!e) {
      print(`FAIL  ${slug} is not in the ship queue`);
      return 1;
    }
    const state = `${e.state}${e.machine ? ` on ${e.machine}` : ""}${e.attempts > 1 ? ` (gate ${e.attempts})` : ""}`;
    if (state !== last) {
      print(`ship-queue: ${slug} ${state}`);
      last = state;
    }
    if (sq.DONE.includes(e.state)) {
      const took = e.startedAt ? ` gate ${Math.round((Date.parse(e.endedAt) - Date.parse(e.startedAt)) / 1000)} s, in queue ${Math.round((Date.parse(e.endedAt) - Date.parse(e.enqueuedAt)) / 1000)} s` : "";
      print(`SUMMARY ship ${e.state === "passed" ? "ok" : "fail"} ${slug} PR #${e.pr}${took}: ${e.why ?? e.state} log=${e.log}`);
      return e.state === "passed" ? 0 : 1;
    }
    if (!workerAlive(root)) await ensureWorker(root);
    if (Date.now() - t0 > budgetMs) {
      print(`ship-queue: still ${e.state} after ${Math.round(budgetMs / 60000)} min; resume with 'tools\\cli ship-queue wait ${slug}' (log ${e.log})`);
      return 2;
    }
    await new Promise((r) => setTimeout(r, 10000));
  }
}

export async function printStatus(root, print = console.log) {
  const q = readQueue(root);
  const { wait } = sq.pickRunnable(q.entries, { slots: await gateSlots(root) });
  for (const l of sq.statusLines(q, { waits: wait })) print(l);
  const pid = workerAlive(root);
  print(`  worker: ${pid ? `pid ${pid}` : "not running (the next wt ship starts it)"}; log ${workerLog(root)}`);
}

export async function main(args, { root }) {
  const [sub, ...rest] = args;
  if (sub === "worker") return runWorker(rest[0] || root);
  if (sub === "status" || !sub) return printStatus(root).then(() => 0);
  if (sub === "wait" && rest[0]) return waitFor(root, rest[0]);
  if (sub === "cancel" && rest[0]) {
    const r = withQueue(root, (q) => {
      const e = q.entries.find((x) => x.slug === rest[0] && x.state === "queued");
      if (e) Object.assign(e, { state: "cancelled", endedAt: now(), why: "cancelled by hand" });
      return e;
    });
    console.log(r ? `ship-queue: ${rest[0]} cancelled` : `ship-queue: ${rest[0]} is not waiting in the queue (a running gate finishes)`);
    return r ? 0 : 1;
  }
  console.log("usage: tools\\cli ship-queue status | wait <slug> | cancel <slug>");
  return 1;
}

if (process.argv[1] && resolve(process.argv[1]).toLowerCase() === SELF.toLowerCase() && process.argv[2] === "worker") {
  runWorker(process.argv[3] || wt.mainRootOf(dirname(dirname(SELF)))).then((c) => process.exit(c));
}
