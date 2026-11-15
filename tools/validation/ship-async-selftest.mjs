// Proves the async ship (docs/worktrees.md "Async ship") without the pool, GitHub or a game: `wt ship` returns once
// queued unless --wait, the one-line status, the ship-gate commit status mapping, the preflight's merge-tree
// conflict split (on a real temp git repo), `wait ship`'s budget and single SUMMARY line, the status file, and
// that `prs heal` leaves a PR in the ship queue alone.
// inputs: tools/lib/ship-queue.mjs, tools/ship-queue.mjs, tools/commands/wait.mjs, tools/prs.mjs, tools/cli.mjs
// red-proof 2026-11-14 (ship-async), each broken alone: shipWaits() returning true by default "wt ship returns once
// queued by default"; commitStatusFor() mapping failed to "pending" "a failed gate posts failure"; conflictPaths()
// keeping the messages after the blank line "merge-tree on a real repo: conflicted paths parsed"; waitFor() without the
// quiet guard on state lines "wait ship prints exactly one line"; liveShipEntry() ignoring the state "prs heal skips a
// PR whose ship entry is gating".

import { spawnSync } from "node:child_process";
import { existsSync, mkdtempSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import * as sq from "../lib/ship-queue.mjs";
import { waitFor, writeStatusFile, statusFilePath } from "../ship-queue.mjs";
import { parseArgs, DEFAULT_BUDGET_S } from "../commands/wait.mjs";
import { liveShipEntry } from "../prs.mjs";

let failed = 0;
const check = (ok, msg) => {
  if (ok) console.log(`PASS  ${msg}`);
  else {
    failed++;
    console.log(`FAIL  ${msg}`);
  }
};

// 1. wt ship returns once queued; --wait (or LF_SHIP_WAIT=1) blocks; --async / --no-wait win over the env.
check(sq.shipWaits([], {}) === false, "wt ship returns once queued by default");
check(sq.shipWaits(["--wait"], {}) === true, "--wait blocks until the gate ends");
check(sq.shipWaits([], { LF_SHIP_WAIT: "1" }) === true, "LF_SHIP_WAIT=1 blocks");
check(sq.shipWaits(["--async"], { LF_SHIP_WAIT: "1" }) === false && sq.shipWaits(["--no-wait"], {}) === false, "--async / --no-wait never block");

// 2. One-line status for each phase.
const base = { slug: "s", pr: 7, log: "C:/x/ship-s.log", enqueuedAt: "2026-11-14T10:00:00Z" };
const at = "2026-11-14T10:30:00Z";
const L = (e) => sq.statusLine(e, { now: at });
check(/^ship-status s queued PR #7: queued 30 min; waits for b log=C:\/x\/ship-s\.log$/.test(sq.statusLine({ ...base, state: "queued" }, { now: at, waitWhy: "waits for b" })), `queued line: ${sq.statusLine({ ...base, state: "queued" }, { now: at, waitWhy: "waits for b" })}`);
check(/^ship-status s running PR #7: gating on lf-cloud for 45 s \(gate 2\) log=/.test(L({ ...base, state: "gating", machine: "lf-cloud", startedAt: "2026-11-14T10:29:15Z", attempts: 2 })), "running line names the step, machine, time and re-gate");
check(/^ship-status s passed PR #7: gate 20 min, gated, pushed abc log=/.test(L({ ...base, state: "passed", startedAt: "2026-11-14T10:05:00Z", endedAt: "2026-11-14T10:25:00Z", why: "gated, pushed abc" })), "passed line");
check(/^ship-status s failed PR #7: .*ci failed \(exit 1\) log=/.test(L({ ...base, state: "failed", why: "gate ci failed (exit 1)" })), "failed line carries the why");
check(sq.phaseOf({ state: "cancelled" }) === "failed" && sq.phaseOf({ state: "merging" }) === "running" && sq.phaseOf({ state: "pushing" }) === "running", "cancelled reads failed; merging and pushing read running");
check(/unknown: not in the ship queue/.test(sq.statusLine(null, { slug: "nope" })), "an unknown slug says so");

// 3. The ship-gate commit status.
const cs = (e) => sq.commitStatusFor(e);
check(cs({ state: "queued" }).state === "pending" && cs({ state: "gating" }).state === "pending", "queued and gating post pending");
check(cs({ state: "passed", why: "ok" }).state === "success", "a passed gate posts success");
check(cs({ state: "failed", why: "x" }).state === "failure", "a failed gate posts failure");
check(cs({ state: "cancelled" }).state === "error", "a cancelled entry posts error");
check(cs({ state: "failed", why: "y".repeat(400) }).description.length <= 140 && cs({ state: "queued" }).context === "ship-gate", "description fits GitHub's 140 and the context is ship-gate");

// 4. Preflight conflict split, on a real git repo: a driver-merged file is the queue's job, a source file the agent's.
const tmp = mkdtempSync(join(tmpdir(), "lf-ship-async-"));
const g = (...a) => spawnSync("git", a, { cwd: tmp, encoding: "utf8" });
g("init", "-q", "-b", "main");
g("config", "user.email", "t@t");
g("config", "user.name", "t");
mkdirSync(join(tmp, "docs"));
mkdirSync(join(tmp, "src"));
writeFileSync(join(tmp, "docs", "features.md"), "a\n");
writeFileSync(join(tmp, "src", "a.c"), "a\n");
writeFileSync(join(tmp, "src", "b.c"), "b\n");
g("add", "-A");
g("commit", "-qm", "base");
g("checkout", "-qb", "ws/x");
writeFileSync(join(tmp, "docs", "features.md"), "ours\n");
writeFileSync(join(tmp, "src", "a.c"), "ours\n");
g("commit", "-qam", "ours");
g("checkout", "-q", "main");
writeFileSync(join(tmp, "docs", "features.md"), "theirs\n");
writeFileSync(join(tmp, "src", "a.c"), "theirs\n");
writeFileSync(join(tmp, "src", "b.c"), "theirs\n");
g("commit", "-qam", "theirs");
const mt = g("merge-tree", "--write-tree", "--name-only", "ws/x", "main");
const paths = sq.conflictPaths(mt.stdout);
check(mt.status === 1 && paths.sort().join(",") === "docs/features.md,src/a.c", `merge-tree on a real repo: conflicted paths parsed (got ${paths.join(",")}, exit ${mt.status})`);
check(sq.humanConflicts(paths).join(",") === "src/a.c", "only the source file needs the agent; docs/features.md is driver-merged");
check(sq.conflictPaths("abc123\n").length === 0, "a clean merge-tree output has no paths");

// 5. wait ship: argument parsing, the budget, one SUMMARY line.
check(parseArgs(["ship", "x"]).budgetS === DEFAULT_BUDGET_S && DEFAULT_BUDGET_S < 600, "wait ship defaults to a budget under the 10 min tool cap");
check(parseArgs(["ship", "x", "--budget", "30"]).budgetS === 30, "--budget sets the seconds");
check(!!parseArgs(["ship", "x", "--budget", "soon"]).error && !!parseArgs(["ship"]).error && !!parseArgs(["build", "x"]).error, "bad arguments are refused with usage");

const qOf = (...entries) => ({ seq: 9, entries });
async function waitLines(seq, opts) {
  const out = [];
  let i = 0;
  const code = await waitFor("unused", "s", { quiet: true, pollMs: 5, read: () => seq[Math.min(i++, seq.length - 1)], keepWorker: async () => {}, print: (l) => out.push(l), ...opts });
  return { code, out };
}
{
  const running = { ...base, id: "s-1", seq: 1, state: "gating", startedAt: at, attempts: 1 };
  const passed = { ...running, state: "passed", endedAt: "2026-11-14T10:40:00Z", why: "gated" };
  const r1 = await waitLines([qOf(running), qOf({ ...running, state: "pushing" }), qOf(passed)], { budgetMs: 5000 });
  check(r1.code === 0 && r1.out.length === 1 && /^SUMMARY ship ok s PR #7 gate 600 s/.test(r1.out[0]), `wait ship prints exactly one line and exits 0 on a pass (${JSON.stringify(r1.out)})`);
  const r2 = await waitLines([qOf({ ...running, state: "failed", endedAt: at, why: "gate ci failed" })], { budgetMs: 5000 });
  check(r2.code === 1 && /^SUMMARY ship fail s .*gate ci failed/.test(r2.out[0]), "a failed gate exits 1 with its why");
  const r3 = await waitLines([qOf(running)], { budgetMs: 0, resume: "tools\\cli wait ship s" });
  check(r3.code === 2 && r3.out.length === 1 && /^SUMMARY ship running s .*re-run 'tools\\cli wait ship s'/.test(r3.out[0]), `the budget ends with exit 2 and a re-run hint (${r3.out[0]})`);
  const r4 = await waitLines([qOf()], { budgetMs: 1000 });
  check(r4.code === 1 && /^SUMMARY ship fail s: not in the ship queue/.test(r4.out[0]), "a slug not in the queue fails at once");
}

// 6. The status file in the worktree.
{
  const wtPath = join(tmp, "wt");
  mkdirSync(wtPath);
  writeStatusFile({ ...base, wtPath, state: "gating", startedAt: at });
  const f = statusFilePath(wtPath);
  const j = existsSync(f) ? JSON.parse(readFileSync(f, "utf8")) : {};
  check(j.phase === "running" && /^ship-status s running/.test(j.line ?? ""), "tmp/ship-status.json holds the phase and the status line");
}

// 7. prs heal leaves a PR in the ship queue to the worker.
{
  const q = qOf({ branch: "ws/a", state: "gating" }, { branch: "ws/b", state: "failed" }, { branch: "ws/c", state: "queued" });
  check(!!liveShipEntry(q, "ws/a") && !!liveShipEntry(q, "ws/c"), "prs heal skips a PR whose ship entry is gating or queued");
  check(!liveShipEntry(q, "ws/b") && !liveShipEntry(q, "ws/z"), "a failed or absent entry leaves the PR to heal");
}

// 8. The wiring in cli.mjs: the queue path asks shipWaits, the old non-terminal default-wait is gone, ship-status exists.
{
  const cli = readFileSync(new URL("../cli.mjs", import.meta.url), "utf8");
  const body = cli.slice(cli.indexOf("async function shipViaQueue"), cli.indexOf("async function wtShipCmd"));
  check(body.includes("sql.shipWaits(flags, process.env)") && !body.includes("process.stdout.isTTY"), "wt ship's queue path waits only on --wait (no isTTY default)");
  check(body.indexOf("--no-preflight") >= 0 && body.indexOf("--no-preflight") < body.indexOf('["push"'), "the preflight runs before the push");
  check(cli.includes('"ship-status": async'), "wt ship-status is wired");
}

console.log(failed ? `ship-async-selftest: ${failed} failed` : "ship-async-selftest: ok");
process.exit(failed ? 1 : 0);
