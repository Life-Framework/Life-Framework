// Pure rules of the main guard (tools/main-guard.mjs, docs/worktrees.md "Main guard"): when to run, which merges
// may be blamed, the bisect, the flake rule, what may be reverted, and the status line. Proven offline by
// tools/validation/main-guard-selftest.mjs.

import { ownerOf } from "./world-test-judge.mjs";
import { sameLeaseHost } from "./guard-pin.mjs";

export const BATCH_MS = 10 * 60 * 1000; // a burst of merges is checked once, at most every 10 min
export const PRE_MOVE = ["e2e", "--pre-move", "502ebf39", "--mission", "playtest"];
export const PLAIN_E2E = ["e2e"]; // the same save round trip with no old build: tells a pre-move-only red from a real one (#2812)

// A pre-move e2e that fails twice while the plain e2e passes at the same head: a pass-2 check needs data the ref
// build's pass 1 cannot write (#2480, #2798). The merge is not broken, its check lacks an LF_E2E guard
// (validate-e2e-payload-guard): a fix card and a comment, no revert.
export function needsSkip(stepName, plainCode) {
  return stepName === "pre-move e2e" && plainCode === 0;
}

// The full post-merge check, one step per entry. `recheck(failing)` is the cheap command that reruns only what failed.
export const STEPS = [
  // --wait: a build lock held by another worktree queues (900 s) instead of failing at once (#2759).
  { name: "build", args: ["build", "--wait"], recheck: () => ["build", "--wait"] },
  { name: "suite", args: ["test", "--no-build", "--tier", "all"], recheck: (failing) => (failing.length ? ["test", "--no-build", "--no-cache", "--only", failing.join(",")] : ["test", "--no-build", "--no-cache", "--tier", "all"]) },
  { name: "placement audit", args: ["audit", "placement", "--strict"], recheck: () => ["audit", "placement", "--strict"] },
  { name: "grid placement audit", args: ["audit", "placement", "--grid", "--strict"], recheck: () => ["audit", "placement", "--grid", "--strict"] },
  { name: "pre-move e2e", args: PRE_MOVE, recheck: () => PRE_MOVE },
];

// Run now? On a new main head, but a burst is batched: no new run until BATCH_MS after the last one started.
export function shouldRun(state, head, now) {
  if (!head || head === state.lastChecked) return { run: false, why: "main unchanged" };
  const last = Date.parse(state.lastRunAt ?? "");
  if (Number.isFinite(last) && now - last < BATCH_MS) return { run: false, why: `batching: next run in ${Math.ceil((BATCH_MS - (now - last)) / 60000)} min` };
  return { run: true, why: `main moved to ${head.slice(0, 8)}` };
}

// "Merge pull request #1590 from Life-Framework/ws/x" -> 1590.
export function prOfMerge(subject) {
  const m = /^Merge pull request #(\d+)\b/.exec(String(subject ?? ""));
  return m ? Number(m[1]) : null;
}

// A revert PR or a revert commit: never reverted again.
export function isRevert(subject, branch = "") {
  return /^Revert\b/i.test(String(subject ?? "")) || /(^|\/)(ws\/)?revert-/i.test(String(branch ?? "")) || /\/revert-pr\d+/i.test(String(subject ?? ""));
}

// The merges the guard may blame, oldest first. merges: [{ sha, subject, beforeStart }] from
// `git log --first-parent --merges lastGreen..head` (any order); beforeStart is true when the merge was
// already on main when the guard started (never acted on).
export function candidates(merges) {
  return [...merges].filter((m) => !m.beforeStart).sort((a, b) => (a.order ?? 0) - (b.order ?? 0));
}

// The earliest candidate where `fails(c)` is true, by binary search, assuming the last one fails (main's head
// was red twice) and everything before the first candidate was green. `fails` is async; returns
// { culprit, tested: [{ sha, failed }] }, culprit null when the list is empty.
export async function bisect(list, fails) {
  const tested = [];
  if (!list.length) return { culprit: null, tested };
  let lo = 0;
  let hi = list.length - 1; // known (assumed) failing
  while (lo < hi) {
    const mid = Math.floor((lo + hi) / 2);
    const f = await fails(list[mid]);
    tested.push({ sha: list[mid].sha, failed: f });
    if (f) hi = mid;
    else lo = mid + 1;
  }
  return { culprit: list[lo], tested };
}

// After a red run: a failure counts only when the recheck at the same head fails too.
// A step that exits with an NTSTATUS (0x40010004 = the process was killed by a logoff or shutdown, 0xC000013A =
// Ctrl+C, ...), by the budget kill (124) or with no code, died of the machine, not of the commit: no recheck, no
// bisect, no revert (2026-11-15: a logoff killed `build`, the guard bisected and then failed on a dead checkout).
export function isEnvExit(code) {
  return code == null || code < 0 || code === 124 || code >= 0x40000000;
}

export function judgeRecheck(firstFailed, recheckFailed) {
  if (!firstFailed) return "green";
  return recheckFailed ? "red" : "flake";
}

// ---- INFRA (#2759): a step the machine refused, not one the commit failed. 2026-11-17 `build` printed only
// "FAIL lock 'build' is held ... (a headless Workbench build is running in another worktree)", the rerun hit the
// same lock, and "red twice" plus the bisect reverted #2369 (#2736). A refusal like that is no verdict: no recheck,
// no bisect, no revert, no red mark; the head is retried on a later tick.
export const INFRA_REFUSALS = [
  [/\block\b[^\n]*\bis held\b|\bis held by\b[^\n]*(?:wait budget|gave up)/i, "a lock was held by another job"],
  [/no free Workbench slot/i, "no free Workbench slot"],
  [/no pool machine (?:could|with)|no reachable machine/i, "no pool machine took the job"],
  [/session lane is held|no session lane|no lane within the wait/i, "no free lane"],
  [/Couldn't build device list|^\s*ENV\s/i, "the Workbench died before compiling"],
  [/Workbench not found|game install not found/i, "the toolchain is missing on the machine"],
  [/This is the machine \(Workbench could not register/i, "the Workbench could not refresh the resource database"],
];
// The compiler's or the suite's own words: with any of these in the transcript the step failed, whatever else it says.
export const REAL_FAILURE = /does not compile|Can't compile|SCRIPT\s*\(E\)|\bVME\b|\[LFTEST\] FAIL /i;

// The infra refusal in a transcript, or null. With FAIL (or ENV) lines, every one of them must be a refusal: a lock
// line beside a real finding is still red. Without any, the whole transcript is searched.
export function infraReason(text) {
  const t = String(text ?? "");
  if (REAL_FAILURE.test(t)) return null;
  const which = (s) => INFRA_REFUSALS.find(([re]) => re.test(s))?.[1] ?? null;
  const fails = t.split(/\r?\n/).filter((l) => /^\s*(?:FAIL|ENV)\b/.test(l));
  if (!fails.length) return which(t);
  const whys = fails.map(which);
  return whys.every(Boolean) ? whys[0] : null;
}

// One step result -> { verdict: ok | env | infra | red, why }. Only red may be rechecked, bisected or reverted.
// failing: the test names parsed from a suite run (a named failing test is always red). `build` exits 3 when no
// pool machine took it (tools/lib/wb-route.mjs): infra.
export function classifyStep(stepName, code, text, failing = []) {
  if (code === 0) return { verdict: "ok", why: "" };
  if (isEnvExit(code)) return { verdict: "env", why: `exit ${code} is the machine (logoff, shutdown or budget kill)` };
  if (failing.length) return { verdict: "red", why: "" };
  const why = infraReason(text);
  if (why) return { verdict: "infra", why };
  if (stepName === "build" && code === 3 && !REAL_FAILURE.test(String(text ?? ""))) return { verdict: "infra", why: "no pool machine took the build (exit 3)" };
  return { verdict: "red", why: "" };
}

// The red heads (stage 2 only; a compile red has its own verdict rule) whose recorded run was an infra refusal: the
// marks the guard set before #2759. heads: { sha: { status, step } }; logsOf(sha, step) -> the transcripts of that
// step's runs at that head (first run and recheck). Every transcript must be a refusal. -> [{ sha, step, why }]
export function infraRedHeads(heads, logsOf) {
  const out = [];
  for (const [sha, h] of Object.entries(heads ?? {})) {
    if (h?.status !== "red" || !h.step || h.step === "compile") continue;
    const texts = logsOf(sha, h.step) ?? [];
    if (!texts.length) continue;
    const vs = texts.map((t) => classifyStep(h.step, 1, t));
    if (vs.every((v) => v.verdict === "infra")) out.push({ sha, step: h.step, why: vs[0].why });
  }
  return out;
}

// Failing test names from a test run's output ([LFTEST] FAIL <name> ..., or the CLI's FAIL lines). The compact
// transcript the CLI prints can carry only the failing assertion lines (`[LFTEST]   - label (expected .., got ..)`)
// with no FAIL header above them (2026-11-17, #2249: the red suite had no parseable name and the bisect could not
// act). `tests` ([{ file, text }], the LF_Test_*.c sources) names the owner of such an orphan assertion by its label.
export function failingTests(text, tests = []) {
  const out = new Set();
  let headed = false; // an assertion line under a FAIL header belongs to that header's test
  for (const line of String(text ?? "").split(/\r?\n/)) {
    const m =/\[LFTEST\] FAIL (\S+)/.exec(line) || /^\s*FAIL\s+(LF_Test_\w+)/.exec(line);
    if (m) {
      out.add(m[1].replace(/[:,]$/, ""));
      headed = true;
      continue;
    }
    const a = /\[LFTEST\]\s+-\s+(.+?)\s*$/.exec(line);
    if (a && !headed && tests.length) {
      const owner = ownerOf(a[1], tests);
      if (owner) out.add(owner);
    }
  }
  return [...out];
}

// The FAIL lines of a run, as recorded at the red head (the "check lines" beside the test names): a log line per
// failing test or check, trimmed, at most `max`.
export function failLines(text, max = 12) {
  const out = [];
  for (const line of String(text ?? "").split(/\r?\n/)) {
    if (/\bFAIL\b/.test(line)) out.push(line.trim().slice(0, 200));
    if (out.length >= max) break;
  }
  return out;
}

// Which of the recorded failing tests `failed` (what one probe run failed) hits. A probe that fails other tests,
// or fails with no parseable test line (a test that does not exist yet, a test-plan problem), hits nothing.
export function setHits(set, failed) {
  const want = new Set(set ?? []);
  return [...new Set(failed ?? [])].filter((n) => want.has(n));
}

// The suite bisect (#2052). Red/green alone blamed the earliest red commit even when it failed other tests than
// today's (#2011, twice). Here a commit is red only when it fails one of the recorded tests `set` (the failing tests
// at the red head). probe(c) -> the failing test names of one run of `set` at c.
// Returns { culprit, tested, hits, missing, stop }. `stop` is a reason string when the guard must not revert:
//   - no candidate fails any recorded test (a different set: nothing is blamed),
//   - the earliest merge that fails them explains only part of the set (mixed causes: a later merge breaks the rest).
// The last candidate is assumed red by the bisect (the head failed twice), so a culprit it did not run is probed at
// the end; a last candidate that is not the head itself is probed first.
export async function bisectBySet(list, set, probe, { head = null } = {}) {
  const hits = new Map();
  const none = (stop) => ({ culprit: null, tested: [], hits, missing: [...set], stop });
  if (!list.length) return none("no merge since the last green head to blame");
  const last = list[list.length - 1];
  if (head && last.sha !== head) {
    const h = setHits(set, await probe(last));
    hits.set(last.sha, h);
    if (!h.length) return none(`mixed causes: none of the failing tests (${set.slice(0, 3).join(", ")}) fail at the newest merge ${last.sha.slice(0, 8)}`);
  }
  const b = await bisect(list, async (c) => {
    if (hits.has(c.sha)) return hits.get(c.sha).length > 0;
    const h = setHits(set, await probe(c));
    hits.set(c.sha, h);
    return h.length > 0;
  });
  // The bisect assumes its last candidate red without running it; the blame is only as good as that run, so an
  // untested culprit is probed now, and a pass there (a flake, or another cause) blames nothing.
  if (b.culprit && !hits.has(b.culprit.sha)) hits.set(b.culprit.sha, setHits(set, await probe(b.culprit)));
  const got = b.culprit ? hits.get(b.culprit.sha) : [];
  if (b.culprit && !got.length) return { ...b, culprit: null, hits, missing: [...set], stop: `mixed causes: none of the failing tests (${set.slice(0, 3).join(", ")}) fail at ${b.culprit.sha.slice(0, 8)} on the bisect rerun` };
  const missing = set.filter((n) => !got.includes(n));
  const r = { ...b, hits, missing, stop: null };
  if (b.culprit && missing.length) r.stop = `mixed causes: ${b.culprit.sha.slice(0, 8)} (${String(b.culprit.subject ?? "").slice(0, 50)}) fails only ${got.join(", ")}; ${missing.slice(0, 3).join(", ")} fail from another merge`;
  return r;
}

// May the guard revert this merge? Never a revert, never a merge from before the guard started, never one with
// no PR number, never twice, never while the guard runs stale code (codeDrift).
export function revertRefusal(c, state, { step, needsSkip: skip = false, codeDrift: drift = [] } = {}) {
  if (!c) return "no culprit";
  if (drift.length) return staleRefusal(drift);
  if (step && fixCardInstead(c, step, { needsSkip: skip })) return fixCardInstead(c, step, { needsSkip: skip });
  if (c.beforeStart) return `merge ${c.sha.slice(0, 8)} was on main before the guard started`;
  if (isRevert(c.subject, c.branch)) return `merge ${c.sha.slice(0, 8)} is a revert; the guard never reverts a revert`;
  const pr = prOfMerge(c.subject);
  if (!pr) return `merge ${c.sha.slice(0, 8)} is not a PR merge (${String(c.subject).slice(0, 60)})`;
  if ((state.reverted ?? []).some((r) => r.pr === pr)) return `PR #${pr} was already reverted`;
  return null;
}

export function revertTitle(pr, what) {
  return `Revert #${pr} (main guard: ${String(what).slice(0, 80)})`;
}

// The issues a PR body refs (Refs/Closes/Fixes/Resolves #N).
export function issuesOf(body) {
  return [...new Set([...String(body ?? "").matchAll(/\b(?:Refs|Closes|Fixes|Resolves|Closed|Fixed|Resolved)\s+#(\d+)/gi)].map((m) => Number(m[1])))];
}

// ---- Guard code freshness. 2026-11-18 the worker started at 23:13:12Z, 14 s before the main checkout pulled #2763
// (#2759: a held lock is INFRA), and a running worker never reloads its modules: it kept judging by exit code and
// falsely reverted #2881 (#2884) and #2887 (#2899) on `FAIL lock 'build' is held`. The worker records the blob ids of
// the code it loaded, compares them with origin/main every tick, never reverts while they differ, and restarts itself
// on the current code once the main checkout fast-forwards.
export const GUARD_CODE = ["tools/main-guard.mjs", "tools/lib/main-guard.mjs", "tools/lib/guard-pin.mjs"];

// Blob ids from `git ls-tree <ref> -- <paths>` output -> { path: id }.
export function parseLsTree(text) {
  const out = {};
  for (const l of String(text ?? "").split(/\r?\n/)) {
    const m = /^\d+ blob ([0-9a-f]{40})\t(.+)$/.exec(l);
    if (m) out[m[2]] = m[1];
  }
  return out;
}

// The guard files whose loaded blob is not origin/main's. An unknown id on either side counts as drift: the guard
// cannot prove it runs current code, so it must not revert.
export function codeDrift(loaded, current) {
  return GUARD_CODE.filter((p) => !loaded?.[p] || !current?.[p] || loaded[p] !== current[p]);
}

export function staleRefusal(drift) {
  return `the guard runs stale code (${drift.join(", ")} differ from origin/main); it restarts on current code before it reverts anything`;
}

// What a tick does about drift: none (fresh), wait (a stage-2 run is going; revertRefusal keeps it from reverting),
// restart (the main checkout now holds origin/main's guard code), stuck (the checkout could not fast-forward).
export function staleAction({ drift = [], busy = false, checkoutFresh = false } = {}) {
  if (!drift.length) return "none";
  if (busy) return "wait";
  return checkoutFresh ? "restart" : "stuck";
}

// One line for `main-guard status` and `lead`.
export function statusLine(state, { alive, now = Date.now(), off = false } = {}) {
  if (off) return "MAIN GUARD off (LF_MAIN_GUARD=0)";
  if (!alive) return `MAIN GUARD not running${state?.lastGreen ? `; last green ${state.lastGreen.slice(0, 8)}` : ""} (tools\\cli main-guard start)`;
  const ago = (t) => (t ? `${Math.max(0, Math.round((now - Date.parse(t)) / 60000))} min ago` : "never");
  const what = state.running ? `running ${state.running.step ?? "check"} at ${String(state.running.head ?? "").slice(0, 8)}` : "watching";
  const last = state.lastResult ? `, last ${state.lastResult.verdict} at ${String(state.lastResult.head ?? "").slice(0, 8)} ${ago(state.lastResult.at)}` : "";
  const rev = state.reverted?.length ? `, ${state.reverted.length} revert(s)` : "";
  return `MAIN GUARD ${what} (pid ${alive}); green ${state.lastGreen ? state.lastGreen.slice(0, 8) : "none yet"}${last}${rev}`;
}

// ---- Stage 1: the Workbench compile, first, on every new main head (batched to one run per ~3 min).
export const COMPILE_BATCH_MS = 3 * 60 * 1000;
export const COMPILE = ["ci", "--wb-only", "--compile-only"];

// `ci --wb-only --compile-only` exit code -> ok | fail | env. 3 = no pool machine took it, 4 = Workbench died
// short of RAM (tools/lib/wb-gate.mjs ENV_SHORT_CODE): the machine failed, not the scripts.
// A red verdict needs the compiler's own words in the transcript: any other non-zero exit (a sync that failed, a
// Workbench that never started; 2026-11-15 an 11 s "exit 1" on lf-cloud) is no verdict, never a revert.
export const COMPILE_ERROR = /does not compile|Can't compile|SCRIPT\s*\(E\)/i;
export function compileVerdict(code, text = null) {
  if (code === 0) return "ok";
  if (code === 3 || code === 4 || code == null) return "env";
  if (text !== null && !COMPILE_ERROR.test(String(text))) return "env";
  return "fail";
}

export function shouldCompile(state, head, now) {
  if (!head || head === state.lastCompiled || head === state.lastCompileRed) return { run: false, why: "main unchanged" };
  const last = Date.parse(state.lastCompileAt ?? "");
  if (Number.isFinite(last) && now - last < COMPILE_BATCH_MS) return { run: false, why: "batching compile" };
  return { run: true, why: `compile ${head.slice(0, 8)}` };
}

// The compile gate. compile(sha) -> "ok"|"fail"|"env". A failure is deterministic: no recheck, straight to the
// bisect over the merges since the last compiled-ok head, and the culprit is reverted (refusal permitting).
// Returns { verdict: "compiled-ok"|"red"|"env", culprit, refusal, tested }.
// base: the commit the bisect assumes compiles (the last compiled-ok head, else the last green one). When it was
// never compile-checked (baseChecked false) it is compiled first: red there too means the break predates every
// candidate, and nothing is blamed.
export async function compileGate({ head, merges, state, compile, base = null, baseChecked = true }) {
  const first = await compile(head);
  if (first === "ok") return { verdict: "compiled-ok", tested: [] };
  if (first === "env") return { verdict: "env", tested: [] };
  if (base && !baseChecked) {
    const b0 = await compile(base);
    if (b0 === "env") return { verdict: "env", tested: [] };
    if (b0 === "fail") return { verdict: "red", culprit: null, refusal: `the compile was already red at ${base.slice(0, 8)}, before every candidate merge`, tested: [{ sha: base, failed: true }] };
  }
  const ENV = Symbol("env");
  try {
    const b = await bisect(candidates(merges), async (c) => {
      const v = await compile(c.sha);
      if (v === "env") throw ENV;
      return v === "fail";
    });
    return { verdict: "red", culprit: b.culprit, refusal: revertRefusal(b.culprit, state, { step: "compile" }), tested: b.tested };
  } catch (e) {
    if (e === ENV) return { verdict: "env", tested: [] };
    throw e;
  }
}

// "Merge pull request #1867 from Life-Framework/ws/town-maintainer-edits" -> "ws/town-maintainer-edits".
export function branchOfMerge(subject) {
  const m = /^Merge pull request #\d+ from [^/\s]+\/(\S+)/.exec(String(subject ?? ""));
  return m ? m[1] : "";
}
export const MAINTAINER_BRANCH = /^(ws\/)?town-maintainer-[\w-]+$/;

// Never revert over town layout (#1872: town findings warn on every branch; a placement audit red is a fix card,
// not a revert) and never auto-revert the maintainer's town edits for anything but a compile break.
// Returns the reason when the guard must open a fix card instead, or null.
export function fixCardInstead(c, step, { needsSkip: skip = false } = {}) {
  if (!c || step === "compile") return null;
  if (skip && step === "pre-move e2e") return "the plain e2e passes at this head: the pre-move failure is a pass-2 check for data the ref build cannot write, a missing LF_E2E.RequiresPayload guard (#2812); fixed forward, never reverted";
  if (step === "placement audit" || step === "grid placement audit") return "a placement audit finding is fixed forward, never reverted (town layout warns, #1872)";
  const br = c.branch || branchOfMerge(c.subject);
  if (MAINTAINER_BRANCH.test(br)) return `#${prOfMerge(c.subject)} is the maintainer's town edit (${br}); only a compile break reverts it`;
  return null;
}

export function fixCard(c, step, what, failLog, opts = {}) {
  const pr = prOfMerge(c?.subject);
  return {
    title: `Main guard: ${step} red on main after #${pr} (${String(what).slice(0, 60)})`,
    body: [`The main guard saw **${what}** (${step}) fail twice on main; the bisect points at #${pr} (merge \`${c?.sha}\`).`, "", `It was not reverted: ${fixCardInstead(c, step, opts) ?? "see the guard log"}. Fix forward on a branch.`, "", `Log on the guard machine: \`${failLog}\``, "", "🤖 Generated with [Claude Code](https://claude.com/claude-code)"].join("\n"),
  };
}

// Reverting a merge with the repo's merge drivers on fails (they need a merge's sides). The guard's revert keeps
// ours for every driver-merged file (`-c merge.<name>.driver=true`) and regenerates them afterwards.
export function driverNames(gitconfigText) {
  return [...String(gitconfigText ?? "").matchAll(/^\s*\[merge "([^"]+)"\]/gm)].map((m) => m[1]);
}
export function driverOverrides(names) {
  return names.flatMap((n) => ["-c", `merge.${n}.driver=true`]);
}
export const REGEN_AFTER_REVERT = [
  ["tools/cli.mjs", "regen-tests"],
  ["tools/cli.mjs", "regen-localization"],
  ["tools/config/schema.mjs", "--write"],
];

// ---- The watchdog (#2754): a dead guard is noticed and restarted, and a stale one is shouted about.
// The worker renews its lease (the main-guard/lease status) every 5 min, but a long compile or a slow tick can delay
// one, so "down" is three missed renewals (15 min), not the 30 min after which `start` may take the lease over.
export const GUARD_DOWN_MS = 15 * 60 * 1000;
export const GUARD_WEDGED_MS = 45 * 60 * 1000; // a live pid with a lease this old is hung, not slow
export const RESTART_CAP = 3; // restarts per RESTART_WINDOW_MS before the watchdog stops and only alerts
export const RESTART_WINDOW_MS = 60 * 60 * 1000;

// Pure: the lease (guard-pin readLease: { state, host, pid, at } | null | undefined) -> what GitHub says about the guard.
// state: live | down (held, renewal overdue) | stopped (released on purpose) | none | unknown (gh failed).
export function guardHealth(lease, { now = Date.now(), downMs = GUARD_DOWN_MS } = {}) {
  if (lease === undefined) return { state: "unknown", minutes: 0 };
  if (!lease) return { state: "none", minutes: 0 };
  const at = Date.parse(lease.at ?? "");
  const minutes = Number.isFinite(at) ? Math.max(0, Math.round((now - at) / 60000)) : 0;
  if (lease.state !== "held") return { state: "stopped", minutes, host: lease.host };
  if (!Number.isFinite(at) || now - at > downMs) return { state: "down", minutes, host: lease.host, pid: lease.pid };
  return { state: "live", minutes, host: lease.host, pid: lease.pid };
}

// The loud line for `lead` and `wt new`, null when the guard is fine or unknown. behind = commits between the pinned
// compiled-ok head and origin/main (null when not known).
// alive = this machine's worker pid: a live pid with a lease that is late but not hung is a busy guard, not a dead one.
export function guardAlert(lease, { now = Date.now(), behind = null, downMs = GUARD_DOWN_MS, alive = null } = {}) {
  const h = guardHealth(lease, { now, downMs });
  if (alive && h.state === "down" && h.minutes * 60000 <= GUARD_WEDGED_MS) return null;
  const gap = behind === null || behind === undefined ? "" : `; you are ${behind} commit(s) behind main`;
  if (h.state === "down") return `main-guard down for ${h.minutes} min (last lease renewal on ${h.host}${gap}); \`tools\\cli main-guard start\`, or wait for the watchdog`;
  if (h.state === "stopped") return `main-guard stopped ${h.minutes} min ago${gap}; \`tools\\cli main-guard start\``;
  if (h.state === "none") return `main-guard has never held the lease${gap}; \`tools\\cli main-guard start\``;
  return null;
}

// Pure: what one watchdog tick does. alive = pid of this machine's worker or null; restarts = ISO times of the
// watchdog's recent restarts. -> { action: "none" | "restart" | "alert", why }.
export function watchDecision({ off = false, alive = null, lease, host, now = Date.now(), restarts = [], downMs = GUARD_DOWN_MS, wedgedMs = GUARD_WEDGED_MS } = {}) {
  if (off) return { action: "none", why: "LF_MAIN_GUARD=0" };
  const h = guardHealth(lease, { now, downMs });
  if (alive) {
    const stale = sameLeaseHost(lease?.host, host) && lease?.state === "held" && h.minutes * 60000 > wedgedMs;
    return stale ? { action: "alert", why: `worker pid ${alive} is alive but its lease is ${h.minutes} min old (hung?); not restarted` } : { action: "none", why: `worker pid ${alive} alive` };
  }
  if (h.state === "live" && !sameLeaseHost(lease.host, host)) return { action: "none", why: `${lease.host} runs the guard` };
  if (h.state === "stopped") return { action: "none", why: `stopped on purpose ${h.minutes} min ago (tools\\cli main-guard start)` };
  const recent = restarts.filter((t) => now - Date.parse(t) < RESTART_WINDOW_MS).length;
  if (recent >= RESTART_CAP) return { action: "alert", why: `${recent} restarts in the last hour and the guard keeps dying; not restarting` };
  // Our own pid is dead: ground truth, no need to wait for the lease to age. Another machine's lease must be overdue.
  if (h.state === "live" || h.state === "down") return { action: "restart", why: `worker dead (lease ${h.state}, ${h.minutes} min since renewal on ${h.host})` };
  return { action: "restart", why: h.state === "none" ? "no guard has ever held the lease" : "worker dead (lease unreadable, gh failed)" };
}
