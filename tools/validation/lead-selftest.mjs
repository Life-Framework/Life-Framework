// Proves tools/lib/lead.mjs, the logic behind `tools\cli lead` and `lead prune-safe`, on fixtures:
//   1. a FRESH worktree (branch tip == origin/main tip, no PR) is never merged-prunable, however idle:
//      only a merged PR whose head SHA is the branch tip counts (never git ancestry);
//   2. a merged PR with commits added after it, a PR for another base, and a same-named branch with a different tip
//      all keep the worktree; a dirty, locked or recently touched merged one is "merged-kept", not prunable;
//   3. a missing directory and a non-checkout are orphans; unregistered ws folders are found, registered ones not;
//   4. issues split into code / working / in-dev / playtest / human by label (human beats playtest beats in-dev),
//      and M2.5 is not M2;
//   5. PRs sort CONFLICTING first, and the conflict hint says generated-only versus needs-human;
//   6. the pool section keeps every machine: a fake slow machine (kept answering after its stage-one report, or
//      never) is cut at its own timeout and shows "slow" or "DOWN" with what it had, the rest print in full, the
//      overall budget line prints when it cuts someone, and a machine under its RAM floor draws a warning;
//   7. the ship queue line counts gating and queued entries and names the oldest wait.
// inputs: tools/**
// red-proof: 2026-11-14, with mergedVerdict changed to call any merged PR on the branch name a hit (the tip
// comparison removed) the checks "commits after the merged PR keep it" and "the reason is given" failed; restored.
// 2026-11-15, with gatherPool's per-machine timer removed (a slow machine waited for, as the old single
// `pool status` call did) the slow-machine checks failed (the section budget cut it at 2 s instead, the "machine" cut and its 0.2 s note failed); restored.
import {
  classifyWorktree, conflictHint, issueState, mergedVerdict, milestoneCounts, milestoneKey, milestoneLine,
  gatherPool, poolLine, poolLines, prLine, shipQueueLine, sortPrs, timeoutHealth, unregisteredDirs, worktreeSummary, IDLE_MS,
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

// 6. the pool section: parallel probes, a per-machine timeout, partial rows, a section budget, a RAM warning
const snap = (name, o) => ({ name, health: "up", freeGb: 8, totalGb: 15.6, floorGb: 1.5, tempC: null, lanes: 4, busy: 1, ...o });
const sleep = (ms, signal) => new Promise((res) => { const t = setTimeout(res, ms); signal?.addEventListener("abort", () => { clearTimeout(t); res(null); }); });
const fakes = {
  fast: async (m, { report }) => { report(snap(m.name, { freeGb: null })); return snap(m.name); },
  // answers the health probe, then the RAM and lane read hangs (lf-cloud on a bad day)
  slow: async (m, { report, signal }) => { report(snap(m.name, { freeGb: null, totalGb: null })); await sleep(4000, signal); return snap(m.name); },
  silent: async (m, { signal }) => { await sleep(4000, signal); return snap(m.name); },
  boom: async () => { throw new Error(["spawn failed", "second line"].join(String.fromCharCode(10))); },
};
const machines = ["fast", "slow", "silent", "boom"].map((name) => ({ name }));
const t0 = Date.now();
const g = await gatherPool({ machines, probe: (m, o) => fakes[m.name](m, o), machineMs: 200, budgetMs: 2000, fallback: (m) => (m.name === "silent" ? "DOWN" : "slow") });
const took = Date.now() - t0;
eq(g.snaps.map((m) => m.name), ["fast", "slow", "silent", "boom"], "every machine keeps a row, in pool order");
eq(g.snaps[0].health, "up", "the fast machine is up");
eq(g.snaps[0].freeGb, 8, "the fast machine has its full snapshot");
eq([g.snaps[1].health, g.snaps[1].cut, g.snaps[1].note], ["slow", "machine", "no RAM or lane answer in 0.2 s"], "a machine that stops after its health probe is slow, not dropped");
eq([g.snaps[2].health, g.snaps[2].cut, g.snaps[2].note], ["DOWN", "machine", "no answer in 0.2 s"], "a machine that never answers takes the fallback verdict");
eq([g.snaps[3].health, g.snaps[3].note], ["slow", "probe failed: spawn failed"], "a probe that throws is one row, first line only");
eq(took < 1500, true, "the section took the slow machine's timeout, not its 4 s");
eq(g.exceeded, false, "no machine was cut by the section budget");
const lines = poolLines({ snaps: g.snaps, queue: 2, exceeded: g.exceeded, budgetMs: 2000, elapsedMs: g.elapsedMs });
eq(lines[0], "POOL queue 2 waiting", "queue count heads the section");
eq(lines.length, 5, "a header and one row per machine, no warning");
eq(lines[2].includes("** slow: no RAM or lane answer in 0.2 s **"), true, "the slow row says why");
eq(lines[3].includes("** DOWN: no answer in 0.2 s **"), true, "the dead row says why");
eq(lines[2].includes("RAM ?  lanes ?/4 busy"), true, "a cut machine does not claim a lane count it never read");

const b2 = await gatherPool({ machines: machines.slice(0, 3), probe: (m, o) => fakes[m.name](m, o), machineMs: 5000, budgetMs: 150, fallback: () => "slow" });
eq(b2.exceeded, true, "the section budget cut a machine that was inside its own limit");
eq(b2.snaps.map((m) => m.cut ?? null), [null, "budget", "budget"], "only the silent ones were cut by the budget");
const bl = poolLines({ snaps: b2.snaps, queue: 0, exceeded: true, budgetMs: 150, elapsedMs: b2.elapsedMs });
eq(bl.at(-1).startsWith("  pool budget 0.2 s exceeded after 0.") && bl.at(-1).endsWith("s: partial rows for slow (slow), silent (slow)"), true, "the budget line is printed and names who was cut");

const low = poolLines({ snaps: [snap("local", { freeGb: 0.5 }), snap("lf-testbox", { freeGb: 4.5 })], queue: 0 });
eq(low.filter((l) => l.includes("WARN RAM")), ["  WARN RAM: local has 0.5 GB free, under its 1.5 GB floor (it takes no new job until RAM frees)"], "only the machine under its floor is warned about");
eq(poolLines({ snaps: [snap("local", { freeGb: 1.5 })], queue: 0 }).some((l) => l.includes("WARN")), false, "exactly at the floor is not under it");
eq(poolLines({ snaps: [snap("local", { freeGb: null, floorGb: null })], queue: 0 }).some((l) => l.includes("WARN")), false, "unknown RAM is not a warning");
eq(poolLine(snap("local")), "local       RAM 8/15.6 GB free  lanes 1/4 busy", "pool line");
eq(timeoutHealth({ okAt: 1000 }, 1000 + 4 * MIN, 5 * MIN), "slow", "answered 4 min ago: slow");
eq(timeoutHealth({ okAt: 1000 }, 1000 + 6 * MIN, 5 * MIN), "DOWN", "answered 6 min ago: down");
eq(timeoutHealth(null, 1000, 5 * MIN), "DOWN", "never answered: down");

// 7. ship queue line
const iso = (minAgo) => new Date(NOW - minAgo * MIN).toISOString();
const sq = { entries: [
  { slug: "tier-perf", pr: 1311, state: "gating", enqueuedAt: iso(40), startedAt: iso(20) },
  { slug: "b-late", pr: 1320, state: "queued", enqueuedAt: iso(3) },
  { slug: "a-early", pr: 1299, state: "queued", enqueuedAt: iso(14) },
  { slug: "old", pr: 1, state: "passed", enqueuedAt: iso(500), endedAt: iso(400) },
] };
eq(shipQueueLine(sq, NOW), "SHIP QUEUE 1 gating, 2 queued, oldest wait 14 min (a-early #1299)", "gating, queued and the oldest wait");
eq(shipQueueLine({ entries: [] }, NOW), "SHIP QUEUE 0 gating, 0 queued", "an empty queue");
eq(shipQueueLine({ entries: [{ slug: "x", state: "merging", enqueuedAt: iso(9) }] }, NOW), "SHIP QUEUE 1 gating, 0 queued", "a gating entry alone has no wait");

if (failures.length) {
  for (const f of failures) console.log(`FAIL lead: ${f}`);
  process.exit(1);
}
console.log("lead selftest: ok");
