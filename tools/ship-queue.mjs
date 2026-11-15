// tools/ship-queue.mjs - the ship queue's worker and its commands (`tools\cli ship-queue ...`).
// `wt ship` pushes the branch, opens the PR (auto-merge off) and enqueues it (tools/lib/ship-queue.mjs);
// one worker per repo takes queued entries in FIFO order, as many at once as there are pool Workbench
// slots (a validate-only gate takes none) and as long as their files do not overlap, and for each:
//   1. merges origin/<base> into the worktree once (prs.mjs healMerge: merge drivers + regeneration);
//   2. runs `ci --remote` there (Workbench steps and the suite on pool machines; a branch with no
//      addons/ change runs `validate --all`, as `wt gate` does);
//   3. if main moved meanwhile, re-gates only when the new main commits change a file the PR changes
//      or a generated file (regateDecision), else pushes the gated head;
//   4. pushes and queues auto-merge, or comments the failure and the log tail on the PR and moves on.
// docs/worktrees.md "Ship queue". Every wait here is bounded (job budget, worker lifetime, wait budget).

import { spawn, spawnSync } from "node:child_process";
import { appendFileSync, closeSync, existsSync, mkdirSync, openSync, readFileSync, renameSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import * as wt from "./wt.mjs";
import * as sq from "./lib/ship-queue.mjs";
import { sleepSync } from "./lib/atomic-file.mjs";

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

// The ship status outside the queue file: <worktree>/tmp/ship-status.json (every update; what an agent or a
// human reads beside the code), and the `ship-gate` commit status on the PR head (on a state change; what
// GitHub shows). Neither may fail a gate: errors go to the worker log. docs/worktrees.md "Async ship".
export function publish(root, e, { changed = true } = {}) {
  writeStatusFile(e);
  if (changed) postCommitStatus(root, e);
}

export function statusFilePath(wtPath) {
  return join(wtPath, "tmp", "ship-status.json");
}

export function writeStatusFile(e) {
  if (!e?.wtPath || !existsSync(e.wtPath)) return;
  try {
    mkdirSync(join(e.wtPath, "tmp"), { recursive: true });
    const f = statusFilePath(e.wtPath);
    writeFileSync(`${f}.tmp`, JSON.stringify({ line: sq.statusLine(e), phase: sq.phaseOf(e), updatedAt: now(), entry: e }, null, 2) + "\n", "utf8");
    renameSync(`${f}.tmp`, f);
  } catch {}
}

export function postCommitStatus(root, e) {
  if (process.env.LF_SHIP_QUEUE_NO_STATUS === "1" || !e?.head || !e?.wtPath) return;
  const gh = wt.ghBin();
  if (!gh) return;
  const s = sq.commitStatusFor(e);
  const args = ["api", "-X", "POST", `repos/{owner}/{repo}/statuses/${e.head}`, "-f", `state=${s.state}`, "-f", `context=${s.context}`, "-f", `description=${s.description}`];
  if (e.prUrl) args.push("-f", `target_url=${e.prUrl}`);
  const r = wt.run(gh, args, { cwd: existsSync(e.wtPath) ? e.wtPath : wt.mainRootOf(root) });
  if (r.status !== 0) wlog(root, `commit status for ${e.slug} (${e.head.slice(0, 7)}) not posted: ${(r.stderr || r.stdout || "").trim().slice(0, 200)}`);
}

// A poll that must not die on a read that kept failing (the file held open for over a second): null, and
// the caller tries again next tick.
function tryReadQueue(root) {
  try {
    return readQueue(root);
  } catch (e) {
    wlog(root, `queue read failed, trying again next tick: ${e?.code ?? ""} ${e?.message ?? e}`);
    return null;
  }
}

// A queue write is bookkeeping: it records what a gate did, it is never part of the gate. Every attempt
// reads the queue afresh and retries the rename on a Windows sharing error (tools/lib/atomic-file.mjs), and
// a write that still fails after `attempts` tries is logged and returns null; it never throws, so a status
// reader holding the file open cannot fail a PR's gate (the 11-minute gate that died on EPERM).
// Terminal writes (passed, failed) try longer: the entry stays "pushing" until they land.
export function update(root, id, patch, note, { attempts = 3, pauseMs = 500, sleep = sleepSync, log = (s) => wlog(root, s) } = {}) {
  for (let i = 1; ; i++) {
    try {
      let before = null;
      const out = withQueue(root, (q) => {
        const e = q.entries.find((x) => x.id === id);
        if (!e) return null;
        before = { state: e.state, machine: e.machine };
        Object.assign(e, patch);
        if (note) e.history = [...(e.history ?? []), `${now()} ${note}`].slice(-20);
        return { ...e };
      });
      // publishing never fails a gate (publish swallows its own errors)
      if (out) publish(root, out, { changed: !before || before.state !== out.state || before.machine !== out.machine });
      return out;
    } catch (err) {
      if (i >= attempts) {
        log(`queue write for ${id} failed ${i} times, the gate result is kept: ${err?.code ?? ""} ${err?.message ?? err}${patch?.state ? ` (wanted state ${patch.state})` : ""}`);
        return null;
      }
      sleep(pauseMs * i);
    }
  }
}

const TERMINAL = { attempts: 8, pauseMs: 750 };

// How many Workbench gates run at once, and how many validate-only gates beside them (tools/lib/ship-queue.mjs
// slotCount / lightCount). Override: "shipQueue": { "parallel": N, "light": N } in pool.json or the "pool" key
// of lf.local.json; else the sum of the Workbench limits of the ssh machines with the workbench cap, where a
// machine's own "workbenchLimit" (in its pool.machines entry) counts, 1 when unset, at least 1 in all.
export async function gateLimits(root) {
  const pool = await import("./remote/pool.mjs");
  const cfg = pool.poolSettings(root).shipQueue;
  return { slots: sq.slotCount({ cfg, machines: pool.loadMachines(root) }), light: sq.lightCount({ cfg }) };
}

export async function gateSlots(root) {
  return (await gateLimits(root)).slots;
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

// quiet-launch reports a process that ends within 3 s as a failed launch. For the worker, exit 0 in
// that window is the normal "another worker holds the lock" answer (runWorker returns 0).
export const FAST_EXIT_OK = /exited within \d+s with code 0/;

// Start the worker when none runs. It runs this file's own copy (the checkout `wt ship` was called from).
// A worker that was already up, or that won the lock a moment before this one, is a success:
// { pid, started: false, already: true }, and the caller prints "worker already running (pid N)".
// opts.alive / opts.spawn / opts.sleep are for the selftest.
export async function ensureWorker(root, opts = {}) {
  const alive = opts.alive ?? (() => workerAlive(root));
  const pid = alive();
  if (pid) return { pid, started: false, already: true, message: workerRunningText(pid) };
  if (!opts.spawn) mkdirSync(dirname(workerLog(root)), { recursive: true });
  let spawnDetached = opts.spawn;
  if (!spawnDetached) {
    const { quietSpawnDetached } = await import("./lib/quiet-launch.mjs");
    spawnDetached = quietSpawnDetached;
  }
  try {
    const child = spawnDetached(process.execPath, [SELF, "worker", wt.mainRootOf(root)], { cwd: dirname(dirname(SELF)), env: process.env });
    return { pid: child, started: true, already: false };
  } catch (e) {
    if (!FAST_EXIT_OK.test(String(e?.message ?? e))) throw e;
    // The new worker saw the lock taken and left: wait for the lock file to show its holder.
    const sleep = opts.sleep ?? ((ms) => new Promise((r) => setTimeout(r, ms)));
    let holder = alive();
    for (let i = 0; i < 5 && !holder; i++) {
      await sleep(400);
      holder = alive();
    }
    return { pid: holder ?? null, started: false, already: true, message: workerRunningText(holder) };
  }
}

export function workerRunningText(pid) {
  return pid ? `worker already running (pid ${pid})` : "worker already running";
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
    try {
      withQueue(root, (q) => {
        for (const e of q.entries) {
          if (sq.RUNNING.includes(e.state) && !(e.workerPid && e.workerPid !== process.pid && wt.pidAlive(e.workerPid))) {
            e.state = "queued";
            e.history = [...(e.history ?? []), `${now()} requeued: its worker ${e.workerPid} is gone`];
          }
        }
      });
    } catch (e) {
      wlog(root, `startup requeue skipped: ${e?.code ?? ""} ${e?.message ?? e}`);
    }
    while (Date.now() - t0 < WORKER_LIFE_MS) {
      const { slots, light } = await gateLimits(root);
      const q = tryReadQueue(root);
      const { start } = q ? sq.pickRunnable(q.entries, { slots, lightSlots: light }) : { start: [] };
      for (const { entry, why } of start) {
        const claimed = update(root, entry.id, { state: "merging", startedAt: now(), workerPid: process.pid }, `start: ${why}`);
        if (!claimed) continue;
        wlog(root, `start ${entry.slug} (#${entry.pr}): ${why}`);
        jobs.set(entry.id, runJob(root, claimed).catch((e) => fail(root, claimed, `worker error: ${e?.stack ?? e}`)).finally(() => jobs.delete(entry.id)));
      }
      const live = (tryReadQueue(root)?.entries ?? [{ state: "unknown" }]).filter((e) => !sq.DONE.includes(e.state));
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

// The gate a branch gets: `validate --all` (cheap, here) when nothing under addons/ changed; else `ci --remote`,
// tiered unless --full-gate: ci classifies the merged diff again and runs the T1 data-only gate when it holds
// (tools/lib/gate-tier.mjs, docs/worktrees.md "Gate tiers").
// pin: the machine flags `wt ship` was given (--remote <name> / --machine <name> / --local), in place
// of the bare --remote. Agents do not pin; a human or the lead can.
export function gateCommand(files, fullGate = false, pin = []) {
  const where = pin.length ? pin : ["--remote"];
  if (fullGate) return ["ci", ...where];
  return files.some((f) => f.startsWith("addons/")) ? ["ci", "--tiered", ...where] : ["validate", "--all"];
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
    const cmd = gateCommand(files, entry.fullGate, entry.pin ?? []);
    attempts++;
    update(root, entry.id, { state: "gating", attempts, files, gatedBase }, `gate ${attempts}: ${cmd.join(" ")} on main ${gatedBase.slice(0, 7)}`);
    const t0 = Date.now();
    const startAt = readFileSync(log, "utf8").length;
    const code = await runLogged(log, process.execPath, [join(wtPath, "tools", "cli.mjs"), ...cmd], { cwd: wtPath, env: { ...process.env, LF_SHIP_QUEUE_JOB: entry.id } });
    const text = readFileSync(log, "utf8").slice(startAt);
    const machine = [...text.matchAll(/ci: Workbench compile \+ build on ([\w.-]+)/g)].map((x) => x[1]).pop() ?? (cmd[0] === "ci" ? null : "this PC (validate)");
    update(root, entry.id, { machine, gateS: Math.round((Date.now() - t0) / 1000) }, `gate ${attempts} exit ${code} in ${Math.round((Date.now() - t0) / 1000)} s`);
    if (code !== 0) {
      // The pool, not the PR: no machine answered, or a shard was placed nowhere. Back in the queue at its
      // old place, at most MAX_ENV_RETRIES times; then it fails like any gate.
      const env = sq.environmentFailure(text);
      const retries = (entry.envRetries ?? 0) + 1;
      if (env && retries <= sq.MAX_ENV_RETRIES) {
        say(`gate ${attempts} failed on the pool, not the PR (${env}); back in the queue (retry ${retries} of ${sq.MAX_ENV_RETRIES})`);
        update(root, entry.id, { state: "queued", envRetries: retries, attempts: attempts - 1, notBefore: new Date(Date.now() + 3 * 60 * 1000).toISOString() }, `requeued: ${env}`);
        wlog(root, `requeued ${slug} (#${entry.pr}): ${env}`);
        return;
      }
      return fail(root, entry, `gate ${cmd.join(" ")} failed (exit ${code})${env ? `; the pool failed it ${retries} times (${env})` : ""}`);
    }
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
  update(root, entry.id, { state: "passed", endedAt: now(), head, why: `gated, pushed ${head.slice(0, 7)}, auto-merge queued` }, "passed", TERMINAL);
  wlog(root, `passed ${slug} (#${entry.pr}) after ${attempts} gate(s)`);
}

// The failure goes to the PR (comment with the log tail) and the queue moves on.
function fail(root, entry, why) {
  const log = entry.log;
  try {
    appendFileSync(log, `${now()} ship-queue: FAIL ${why}\n`);
  } catch {}
  update(root, entry.id, { state: "failed", endedAt: now(), why }, `failed: ${why}`, TERMINAL);
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

// Block until the slug's entry is passed, failed or cancelled; bounded by budgetMs. Exit code 0 on passed,
// 1 on failed / cancelled / not queued, 2 when the budget ran out with the gate still queued or running.
// quiet: print only the closing SUMMARY line (`tools\cli wait ship`, which an agent runs in <10 min chunks).
export async function waitFor(root, slug, { budgetMs = 3 * 60 * 60 * 1000, print = console.log, quiet = false, pollMs = 10000, resume = `tools\\cli ship-queue wait ${slug}`, read = () => readQueue(root), keepWorker = async () => { if (!workerAlive(root)) await ensureWorker(root); } } = {}) {
  const t0 = Date.now();
  let last = "";
  for (;;) {
    let q = null;
    try {
      q = read();
    } catch (err) {
      wlog(root, `queue read failed, trying again next tick: ${err?.code ?? ""} ${err?.message ?? err}`);
    }
    if (!q) {
      // the file stayed locked for over a second: not an answer about the PR, look again
      if (Date.now() - t0 > budgetMs) {
        print(`ship-queue: could not read the queue after ${Math.round(budgetMs / 60000)} min; resume with 'tools\\cli ship-queue wait ${slug}'`);
        return 2;
      }
      await new Promise((r) => setTimeout(r, 2000));
      continue;
    }
    const e = sq.entryFor(q, slug);
    if (!e) {
      print(quiet ? `SUMMARY ship fail ${slug}: not in the ship queue (run 'tools\\cli wt ship ${slug}')` : `FAIL  ${slug} is not in the ship queue`);
      return 1;
    }
    const state = `${e.state}${e.machine ? ` on ${e.machine}` : ""}${e.attempts > 1 ? ` (gate ${e.attempts})` : ""}`;
    if (state !== last && !quiet) print(`ship-queue: ${slug} ${state}`);
    last = state;
    if (sq.DONE.includes(e.state)) {
      const took = e.startedAt ? ` gate ${Math.round((Date.parse(e.endedAt) - Date.parse(e.startedAt)) / 1000)} s, in queue ${Math.round((Date.parse(e.endedAt) - Date.parse(e.enqueuedAt)) / 1000)} s` : "";
      print(`SUMMARY ship ${e.state === "passed" ? "ok" : "fail"} ${slug} PR #${e.pr}${took}: ${e.why ?? e.state} log=${e.log}`);
      return e.state === "passed" ? 0 : 1;
    }
    await keepWorker();
    if (Date.now() - t0 >= budgetMs) {
      const line = sq.statusLine(e).replace(/^ship-status \S+ /, "");
      print(quiet ? `SUMMARY ship ${sq.phaseOf(e)} ${slug} after ${Math.round((Date.now() - t0) / 1000)} s wait; ${line}; re-run '${resume}'` : `ship-queue: still ${e.state} after ${Math.round(budgetMs / 60000)} min; resume with '${resume}' (log ${e.log})`);
      return 2;
    }
    await new Promise((r) => setTimeout(r, Math.min(pollMs, Math.max(0, budgetMs - (Date.now() - t0)) + 50)));
  }
}

// `tools\cli wt ship-status <slug>`: one line, exit 0 when passed, queued or running, 1 when failed or unknown.
export async function shipStatus(root, slug, { print = console.log } = {}) {
  const q = readQueue(root);
  const e = sq.entryFor(q, slug);
  let waitWhy = null;
  if (e?.state === "queued") {
    const { slots, light } = await gateLimits(root);
    waitWhy = sq.pickRunnable(q.entries, { slots, lightSlots: light }).wait.find((w) => w.entry.id === e.id)?.why ?? null;
    if (!workerAlive(root)) waitWhy = `${waitWhy ? `${waitWhy}; ` : ""}no worker running (the next wt ship or wait starts one)`;
  }
  print(sq.statusLine(e, { slug, waitWhy }));
  return e && sq.phaseOf(e) !== "failed" ? 0 : 1;
}

export async function printStatus(root, print = console.log) {
  const q = readQueue(root);
  const { slots, light } = await gateLimits(root);
  const { wait } = sq.pickRunnable(q.entries, { slots, lightSlots: light });
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
      return e ? { ...e } : null;
    });
    if (r) publish(root, r);
    console.log(r ? `ship-queue: ${rest[0]} cancelled` : `ship-queue: ${rest[0]} is not waiting in the queue (a running gate finishes)`);
    return r ? 0 : 1;
  }
  console.log("usage: tools\\cli ship-queue status | wait <slug> | cancel <slug>");
  return 1;
}

if (process.argv[1] && resolve(process.argv[1]).toLowerCase() === SELF.toLowerCase() && process.argv[2] === "worker") {
  runWorker(process.argv[3] || wt.mainRootOf(dirname(dirname(SELF)))).then((c) => process.exit(c));
}
