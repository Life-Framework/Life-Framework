// tools/validation/issue-preflight-selftest.mjs
// inputs: tools/lib/issue-preflight.mjs, tools/commands/issue-check.mjs
//
// Proves `tools\cli issue-check` verdicts (tools/lib/issue-preflight.mjs) on fixtures shaped like the
// 2026-11-18 stale dispatches, with no GitHub:
//   1. a merged PR or main commit citing #N, a done label, a "fixed by" comment or the skip list -> LIKELY DONE;
//   2. #24930 is not #2493 (number boundary); a roadmap/outline PR citing it is only a hint (CHECK);
//   3. a done sub-issue it names -> CHECK, but a closed parent chapter or epic is ignored;
//   4. nothing found -> CLEAR; title keywords drop stop words and numbers.
//
// red-proof: 2026-11-18, with citesIssue() matching `#${n}` without the digit boundary the run printed
// `FAIL #24930 must not count as a citation of #2493` and exited 1; restored, it passes.
import { citesIssue, keywords, namedIssues, skipListNumbers, verdict } from "../lib/issue-preflight.mjs";

const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};
const base = (over = {}) => ({ issue: { number: 2493, title: "Map shares: perf gate misses the 5 s sweep", labels: [], state: "OPEN", comments: [], body: "", ...over.issue }, prs: [], commits: [], similar: [], named: [], skipList: new Set(), ...over, issue: { number: 2493, title: "Map shares", labels: [], state: "OPEN", comments: [], ...(over.issue || {}) } });

check(citesIssue("fixes (#2493)", 2493) && citesIssue("Refs #2493\n", 2493), "a plain #N citation must count");
check(!citesIssue("Refs #24930", 2493), "#24930 must not count as a citation of #2493");

check(verdict(base()).level === "CLEAR", "nothing found must be CLEAR");
check(verdict(base({ prs: [{ number: 2499, title: "Map review", body: "Refs #2493" }] })).level === "LIKELY DONE", "a merged PR citing the issue must be LIKELY DONE");
check(verdict(base({ prs: [{ number: 9, title: "x", body: "Refs #24930" }] })).level === "CLEAR", "a PR citing a longer number must not count");
check(verdict(base({ commits: [{ sha: "4c3a95b8f0000", subject: "Map review (#2493)" }] })).level === "LIKELY DONE", "a main commit citing it must be LIKELY DONE");
check(verdict(base({ issue: { labels: [{ name: "needs-playtest" }] } })).level === "LIKELY DONE", "a done label must be LIKELY DONE");
check(verdict(base({ issue: { comments: [{ body: "This was already fixed by the gate rework" }] } })).level === "LIKELY DONE", "an 'already fixed' comment must be LIKELY DONE");
check(verdict(base({ skipList: new Set([2493]) })).level === "LIKELY DONE", "the skip list must make it LIKELY DONE");
check(verdict(base({ prs: [{ number: 1841, title: "Roadmap: the bus goes to Backlog", body: "#2493" }] })).level === "CHECK", "a roadmap PR citing it is only a hint");
check(verdict(base({ similar: [{ number: 2531, title: "Map share sweep perf", mergedAt: "2026-11-17T10:00:00Z" }] })).level === "CHECK", "a similar merged PR must be CHECK");
check(verdict(base({ named: [{ number: 2246, title: "Map performance gate", state: "CLOSED", labels: [] }] })).level === "CHECK", "a done named issue must be CHECK");
check(verdict(base({ named: [{ number: 1015, title: "M5 chapter: Story: The relocated", state: "CLOSED", labels: [] }] })).level === "CLEAR", "a closed parent chapter must be ignored");
check(verdict(base({ named: [{ number: 77, title: "Open thing", state: "OPEN", labels: [{ name: "agent:working" }] }] })).level === "CLEAR", "an open named issue must not count");

const kw = keywords("Events: a late joiner gets the current phase, and events count players near their place");
check(kw.length === 4 && !kw.includes("the") && kw.includes("players"), `keywords picked ${JSON.stringify(kw)}`);
check(!keywords("Fix 2493 (review 2026-11-17) map").some((w) => /[0-9]/.test(w)), "keywords must drop numbers and parentheses");
check(JSON.stringify(namedIssues("after #3016, see #2493 and #3016", 2493)) === "[3016]", "namedIssues must skip itself and repeats");

const skip = skipListNumbers("ALREADY-FIXED: 129 145 2497(likely)\nPARTLY: 1794 (#640 rejoin open)\nALREADY-FIXED (2026-11-18): 640 rejoin at logout position (PRs 2068, 2078)\n");
check(skip.has(129) && skip.has(2497) && skip.has(640) && !skip.has(1794) && !skip.has(2068), `skip list parsed ${JSON.stringify([...skip])}`);

if (failures.length) {
  for (const f of failures) console.log(`FAIL ${f}`);
  process.exit(1);
}
console.log("issue-preflight-selftest: OK (citations with a number boundary, done labels, comments, skip list, planning PRs, similar PRs, named issues, keywords)");
