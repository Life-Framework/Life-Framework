// pr-autoheal-selftest: the pure parts of tools/pr-autoheal.mjs (which PRs it looks at, which conflicts it may
// heal, which parked runs it approves: every parked pull_request run on a head it pushed) and the workflow's
// wiring. The end-to-end proof ran on GitHub with scratch PRs (#5035, #5040, 2026-11-27); this pins its rules.
import { readFileSync } from "node:fs";
import { conflictedPaths, withoutDriver, candidates, parkedRuns } from "../pr-autoheal.mjs";

let failed = 0;
const check = (c, m) => { if (!c) { console.log(`FAIL  ${m}`); failed++; } };

// merge-tree --name-only: tree oid, conflicted paths, blank line, messages.
const mt = "abc123\ndocs/features/atmosphere.md\ndocs/features/atmosphere.md\nsrc/a.c\n\nAuto-merging docs/features/atmosphere.md\nCONFLICT (content): ...\n";
check(conflictedPaths(mt).join(",") === "docs/features/atmosphere.md,src/a.c", `conflictedPaths: got ${conflictedPaths(mt)}`);
check(conflictedPaths("abc123\n").length === 0, "conflictedPaths: a tree line alone is no conflict");

const attr = "docs/features/atmosphere.md: merge: lf-markdown\nsrc/a.c: merge: unspecified\ntmp/u.md: merge: union\n";
check(withoutDriver(["docs/features/atmosphere.md"], attr).length === 0, "withoutDriver: an lf-* file is healable");
check(withoutDriver(["docs/features/atmosphere.md", "src/a.c"], attr).join() === "src/a.c", "withoutDriver: a plain file needs a human");
check(withoutDriver(["tmp/u.md"], attr).join() === "tmp/u.md", "withoutDriver: merge=union is not one of ours (GitHub ignores it)");

const prs = [
  { number: 1, headRefName: "ws/a", isDraft: false, isCrossRepository: false, labels: [] },
  { number: 2, headRefName: "ws/b", isDraft: true, isCrossRepository: false, labels: [] },
  { number: 3, headRefName: "ws/c", isDraft: false, isCrossRepository: true, labels: [] },
  { number: 4, headRefName: "ws/d", isDraft: false, isCrossRepository: false, labels: [{ name: "no-autoheal" }] },
  { number: 5, headRefName: "ws/test-merge-x", isDraft: true, isCrossRepository: false, labels: [] },
];
check(candidates(prs).map((p) => p.number).join() === "1",`candidates: got ${candidates(prs).map((p) => p.number)}`);
check(candidates(prs, { headPrefix: "ws/test-merge-" }).map((p) => p.number).join() === "5", "candidates: the experiment prefix takes its drafts only");

const runs = [
  { id: 10, name: "validate", event: "pull_request", head_sha: "s1", conclusion: "action_required" },
  { id: 11, name: "validate", event: "workflow_dispatch", head_sha: "s1", conclusion: "action_required" },
  { id: 12, name: "validate", event: "pull_request", head_sha: "s2", conclusion: "action_required" },
  { id: 13, name: "validate", event: "pull_request", head_sha: "s1", conclusion: null },
];
check(parkedRuns(runs, ["s1"]).map((r) => r.id).join() === "10", "parkedRuns: only the parked pull_request run on a head this script pushed");

const wf = readFileSync(new URL("../../.github/workflows/pr-autoheal.yml", import.meta.url), "utf8");
check(/push:\s*\n\s*branches: \[main\]/.test(wf), "workflow: runs on every push to main");
check(/actions: write/.test(wf) && /contents: write/.test(wf), "workflow: can push and approve runs");
check(!/test-merge/.test(wf), "workflow: no scratch-experiment trigger left in");
check(/node tools\/pr-autoheal\.mjs/.test(wf), "workflow: runs tools/pr-autoheal.mjs");

if (failed) process.exit(1);
console.log("PASS  pr-autoheal selftest");
