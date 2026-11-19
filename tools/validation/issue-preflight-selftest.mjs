// tools/validation/issue-preflight-selftest.mjs
// inputs: tools/lib/issue-preflight.mjs, tools/commands/issue-check.mjs
//
// Proves `tools\cli issue-check` verdicts (tools/lib/issue-preflight.mjs) on fixtures shaped like the
// 2026-11-18 stale dispatches, with no GitHub:
//   1. a merged PR for #N (title ends "(#N)", or its own "Refs/Closes/Fixes #N" line), a main commit
//      whose subject ends "(#N)", a done label, a "fixed by" comment or the skip list -> LIKELY DONE;
//   2. #24930 is not #2493 (number boundary); a roadmap/outline PR citing it is only a hint (CHECK);
//   3. a done sub-issue it names -> CHECK, but a closed parent chapter or epic is ignored;
//   4. nothing found -> CLEAR; title keywords drop stop words and numbers.
//
//   5. 2026-11-19: PRs and commits that only mention #N (#3260, #3263, #3208, #3274, #3287, real text) -> CHECK.
//
// red-proof 2: 2026-11-19, on the unfixed lib (any citation was strong) the run printed seven
// `FAIL ... must be CHECK` lines (all five issues among them) and exited 1; with the fix it passes.
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

// 2026-11-19: issue-check called these five open cards LIKELY DONE because a merged PR or commit only
// mentioned the number. The fixtures carry the real PR text (title and the line that names the issue).
const open = (number, title) => ({ issue: { number, title, labels: [], state: "OPEN", comments: [], body: "" }, prs: [], commits: [], similar: [], named: [], skipList: new Set() });
const mention = (n, prs, commits = []) => ({ ...open(n, "card"), prs, commits });
const lumber = [
  { number: 3313, title: "Lumber: tree felling rules, design note and slice plan (#3259)", body: "Refs #3259\n- 9 `LF-Lumber_*` string rows, a `Lumber` system-catalog entry (LOCKED until #3260 adds a switch\nNothing in the world uses these rules yet. The axe action is #3260." },
  { number: 3548, title: "Felled map trees reach every client and late joiners (#3261)", body: "Refs #3261\n- `Publish(key)` and `Retract(key)` are what the axe action (#3260) and the regrow tick (#3262) will call." },
  { number: 3556, title: "Fell tree prompt: axe-held helper on the nearest tree, owner-side stamina (#3554)", body: "Refs #3554\nPlayers can now trigger the server fell from #3260/#3553 (#3554)." },
];
const v3260 = verdict(mention(3260, lumber, [{ sha: "1111111111111", subject: "Lumber rules for the axe slice #3260 builds on (#3259)" }]));
check(v3260.level === "CHECK", `#3260: design notes and sibling cards that name it must be CHECK, got ${v3260.level}`);
check(v3260.reasons.some((r) => r.includes("PR #3313")), "#3260: the CHECK must name the mentioning PR");
check(verdict(mention(3260, [{ number: 3553, title: "Fell a real map tree with an axe: hide, ledger, logs, XP (#3260)", body: "Refs #3260\nSlice #3260 (parent #3169): the server side of felling a real map tree." }])).level === "LIKELY DONE", "#3260: the PR that is for it must be LIKELY DONE");
check(verdict(mention(3263, [{ number: 3501, title: "The courier parcel hangs from the hand by its top edge instead of through the box", body: "Refs #3500\n**Options not built.** 2 (shared shoulder carry, #3263): that is an over-the-shoulder pose with a speed cap for logs and ore." }])).level === "CHECK", "#3263: an aside inside a sentence must be CHECK");
check(verdict(mention(3208, [{ number: 3564, title: "A house owner sees Make this my spawn on a dedicated client (#3375)", body: "Refs #3375\n#3208 (house sign) can reuse `OfferSet`. Doc row added in `docs/features/property.md`." }])).level === "CHECK", "#3208: 'can reuse' must be CHECK");
check(verdict(mention(3274, [{ number: 3519, title: "Town draft: Saint-Philippe hardware store on b59, general store drops the hardware rows (#3220)", body: "Refs #3220\n- `merchant-timber` (#3274) shares b59: the kerb in front of the door is left clear for its counter." }])).level === "CHECK", "#3274: a sibling card note must be CHECK");
check(verdict(mention(3287, [{ number: 3477, title: "Wheat field grid rules: cells, cut bitset, bar footprint, speed gate, hopper (#3285)", body: "Refs #3285\nA player notices nothing yet: the field entity (#3287), cut replication (#3288) and the reaper (#3289) build on it." }])).level === "CHECK", "#3287: 'build on it' must be CHECK");
check(verdict(mention(3287, [{ number: 1, title: "x", body: "Refs #3287" }])).level === "LIKELY DONE", "a Refs line must be LIKELY DONE");
check(verdict(mention(3287, [{ number: 1, title: "x", body: "Closes #3285, #3287\n" }])).level === "LIKELY DONE", "a Closes list with the number must be LIKELY DONE");
check(verdict(mention(3287, [{ number: 1, title: "x", body: "Fixes #3287 and the rest of the field" }])).level === "CHECK", "a closing word with trailing prose is not its own line");
check(verdict(mention(3287, [{ number: 1, title: "Field entity (#3286, #3287)", body: "" }])).level === "LIKELY DONE", "a title ending in a (#M, #N) group must be LIKELY DONE");
check(verdict(mention(3287, [{ number: 1, title: "Builds on #3287 (see notes)", body: "" }])).level === "CHECK", "a title that only mentions it must be CHECK");
check(verdict(mention(3287, [], [{ sha: "2222222222222", subject: "Field entity (#3287)" }])).level === "LIKELY DONE", "a commit whose subject ends in (#N) must be LIKELY DONE");
check(verdict({ ...open(3287, "card"), issue: { number: 3287, title: "card", labels: [{ name: "needs-playtest" }], state: "OPEN", comments: [] } }).level === "LIKELY DONE", "the needs-playtest label must stay LIKELY DONE");

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
