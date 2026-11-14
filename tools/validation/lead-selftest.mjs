// Proves tools/lib/lead.mjs, the logic behind `tools\cli lead` and `lead prune-safe`, on fixtures:
//   1. a FRESH worktree (branch tip == origin/main tip, no PR) is never merged-prunable, however idle:
//      only a merged PR whose head SHA is the branch tip counts (never git ancestry);
//   2. a merged PR with commits added after it, a PR for another base, and a same-named branch with a different tip
//      all keep the worktree; a dirty, locked or recently touched merged one is "merged-kept", not prunable;
//   3. a missing directory and a non-checkout are orphans; unregistered ws folders are found, registered ones not;
//   4. issues split into code / working / in-dev / playtest / human by label (human beats playtest beats in-dev),
//      and M2.5 is not M2;
//   5. PRs sort CONFLICTING first, and the conflict hint says generated-only versus needs-human;
//   6. `pool status` text parses to machines, lanes, RAM, temperature and queue.
// inputs: tools/**
// red-proof: 2026-11-14, with mergedVerdict changed to call any merged PR on the branch name a hit (the tip
// comparison removed) the checks "commits after the merged PR keep it" and "the reason is given" failed; restored.
import {
  classifyWorktree, conflictHint, issueState, mergedVerdict, milestoneCounts, milestoneKey, milestoneLine,
  parsePoolStatus, poolLine, prLine, sortPrs, unregisteredDirs, worktreeSummary, IDLE_MS,
} from "../lib/lead.mjs";

const failures = [];
const eq = (got, want, msg) => {
  if (JSON.stringify(got) !== JSON.stringify(want)) failures.push(`${msg}: ${JSON.stringify(got)} != ${JSON.stringify(want)}`);
};

const NOW = Date.parse("2026-11-14T12:00:00Z");
const MIN = 60000;
const merged = [
  { number: 10, headRefName: "ws/done", headRefOid: "aaa", baseRefName: "main" },
  { number: 11, headRefName: "ws/more", headRefOid: "bbb", baseRefName: "main" },
  { number: 12, headRefName: "ws/other-base", headRefOid: "ccc", baseRefName: "m3" },
];
const wtree = (o) => ({ slug: "x", branch: "ws/x", base: "main", tip: "fff", exists: true, isGit: true, dirty: false, lastMs: NOW - 120 * MIN, busy: null, ...o });
const cls = (o) => classifyWorktree(wtree(o), merged, NOW).cls;

// 1. a fresh worktree: no PR, nothing touched for hours, branch == main. Never prunable.
eq(cls({ branch: "ws/fresh", tip: "main-tip", lastMs: NOW - 600 * MIN }), "idle", "fresh worktree idle for hours is idle, not prunable");
eq(cls({ branch: "ws/fresh", tip: "main-tip", lastMs: NOW - 2 * MIN }), "active", "fresh worktree just touched is active");
eq(mergedVerdict({ branch: "ws/fresh", tip: "main-tip" }, []).merged, false, "fresh worktree: no merged PRs at all");
eq(mergedVerdict({ branch: "ws/fresh", tip: "main-tip" }, undefined).merged, false, "an unknown PR list never merges anything");

// 2. only the exact PR head counts
eq(cls({ branch: "ws/done", tip: "aaa" }), "prunable", "merged PR, tip == head, clean, idle: prunable");
eq(cls({ branch: "ws/more", tip: "zzz" }), "idle", "commits after the merged PR keep it");
eq(mergedVerdict({ branch: "ws/more", tip: "zzz" }, merged).via, "commits after the merged PR", "the reason is given");
eq(cls({ branch: "ws/other-base", tip: "ccc", base: "main" }), "idle", "a PR into another base does not count");
eq(cls({ branch: "ws/other-base", tip: "ccc", base: "m3" }), "prunable", "...but does for a worktree based on it");
eq(cls({ branch: "ws/done", tip: "aaa", dirty: true }), "merged-kept", "merged but dirty is kept");
eq(cls({ branch: "ws/done", tip: "aaa", busy: "live session" }), "merged-kept", "merged but locked is kept");
eq(cls({ branch: "ws/done", tip: "aaa", lastMs: NOW - 10 * MIN }), "merged-kept", "merged but touched 10 min ago is kept");
eq(cls({ branch: "ws/done", tip: "aaa", lastMs: NOW - IDLE_MS - MIN }), "prunable", "merged and idle past 30 min is prunable");
eq(cls({ branch: undefined, tip: undefined }), "idle", "a detached head is never prunable");

// 3. orphans
eq(cls({ exists: false }), "orphan", "registered but the directory is missing");
eq(cls({ isGit: false }), "orphan", "a directory that is not a checkout");
eq(unregisteredDirs(["C:/r/L-ws-a", "C:\\r\\L-ws-b", "C:/r/L-ws-c/"], ["c:/r/l-ws-a", "C:/r/L-ws-c"]), ["C:\\r\\L-ws-b"], "only the unregistered folder");
const sum = worktreeSummary([{ cls: "active" }, { cls: "idle" }, { cls: "prunable" }, { cls: "orphan" }], ["d"]);
eq([sum.by.active.length, sum.by.idle.length, sum.by.prunable.length, sum.by.orphan.length, sum.orphanDirs.length], [1, 1, 1, 1, 1], "summary buckets");

// 4. milestones
eq(milestoneKey("M2 Living Town"), "M2", "M2 title");
eq(milestoneKey("M2.5 Showcase and Outreach"), null, "M2.5 is not M2");
eq(milestoneKey("Development"), null, "other milestone");
eq(issueState([{ name: "needs-human" }, { name: "needs-playtest" }, { name: "in-dev" }]), "human", "human beats playtest and in-dev");
eq(issueState([{ name: "needs-playtest" }, { name: "in-dev" }]), "playtest", "playtest beats in-dev");
eq(issueState([{ name: "in-dev" }]), "indev", "in-dev");
eq(issueState([{ name: "agent:working" }]), "working", "being built");
eq(issueState([{ name: "bug" }]), "code", "needs code");
const issues = [
  { labels: [{ name: "bug" }], milestone: { title: "M2 Living Town" } },
  { labels: [{ name: "agent:working" }], milestone: { title: "M2 Living Town" } },
  { labels: [{ name: "in-dev" }], milestone: { title: "M2 Living Town" } },
  { labels: [{ name: "needs-playtest" }], milestone: { title: "M3 Framework Release" } },
  { labels: [{ name: "needs-human" }], milestone: { title: "M3 Framework Release" } },
  { labels: [], milestone: { title: "M2.5 Showcase and Outreach" } },
  { labels: [], milestone: null },
];
const mc = milestoneCounts(issues);
eq(milestoneLine("M2", mc.M2), "M2 3 open: code 2 (1 being built) | in-dev 1 | playtest 0 | human 0", "M2 line");
eq(milestoneLine("M3", mc.M3), "M3 2 open: code 0 | in-dev 0 | playtest 1 | human 1", "M3 line");

// 5. PRs
const pr = (n, o) => ({ number: n, headRefName: `ws/p${n}`, mergeStateStatus: "CLEAN", mergeable: "MERGEABLE", createdAt: "2026-11-14T10:00:00Z", statusCheckRollup: [], ...o });
const sorted = sortPrs([pr(1), pr(2, { mergeStateStatus: "BLOCKED" }), pr(3, { mergeStateStatus: "DIRTY", mergeable: "CONFLICTING" }), pr(4, { createdAt: "2026-11-14T09:00:00Z" })]);
eq(sorted.map((p) => p.number), [3, 2, 4, 1], "conflicting first, then stuck, then clean oldest first");
eq(conflictHint(["addons/LifeFramework/Scripts/Game/Tests/LF_TestRegistrations.generated.c", "docs/features.md"]).startsWith("generated-only (2)"), true, "generated files only");
eq(conflictHint(["docs/features.md", "addons/LifeFramework/Scripts/Game/Feature/Bank/LF_Bank.c"]), "NEEDS HUMAN: LF_Bank.c", "a source file needs a human");
eq(conflictHint(null).startsWith("conflicts: ?"), true, "unknown branch");
const line = prLine(pr(7, { mergeStateStatus: "DIRTY", mergeable: "CONFLICTING", statusCheckRollup: [{ __typename: "CheckRun", status: "COMPLETED", conclusion: "FAILURE" }] }), NOW, "generated-only (1): prs heal fixes it");
eq(/^#7 p7 +CONFLICT +FAIL 1\/1 +2h +generated-only/.test(line), true, "PR line");

// 6. pool status text, as printed by `tools\cli pool status`
const text = [
  "local  this PC (Gaming-Box)  up  build 26257316  RAM 2.2/15.6 GB free (floor 1.5)  up to 2 test + 2 session lane(s) (port range allows 64 + 32), as RAM allows  (3 lane(s) free or stale)",
  "    test     server-test-1   e2e-pre-move     e2e        age  27m  idle    -  run 1621s of 90s budget",
  "lf-testbox  ssh lf-testbox  slow (answered 9 s ago)  build 26257316  RAM 4.1/15.2 GB free (floor 1.5)  38.1 \u00b0C (holds new jobs at 90)  up to 5 test + 2 session lane(s) (port range allows 64 + 32), as RAM allows  (7 lane(s) free or stale)",
  "lf-cloud  ssh lf-cloud  DOWN",
  "workbench (this PC): 0/1 busy",
  "queue: 3 waiting (high > normal > low)",
].join("\n");
const pool = parsePoolStatus(text);
eq(pool.queue, 3, "queue length");
eq(pool.machines.map((m) => [m.name, m.health, m.freeGb, m.totalGb, m.tempC, m.lanes, m.busy]), [
  ["local", "up", 2.2, 15.6, null, 4, 1],
  ["lf-testbox", "slow", 4.1, 15.2, 38.1, 7, 0],
  ["lf-cloud", "DOWN", null, null, null, null, 0],
], "machines");
eq(poolLine(pool.machines[0]), "local       RAM 2.2/15.6 GB free  lanes 1/4 busy", "pool line");
eq(poolLine(pool.machines[2]).includes("DOWN"), true, "a down machine says so");

if (failures.length) {
  for (const f of failures) console.log(`FAIL lead: ${f}`);
  process.exit(1);
}
console.log("lead selftest: ok");
