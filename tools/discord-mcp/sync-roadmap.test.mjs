import assert from "node:assert/strict";
import test from "node:test";
import {
  displayTitle,
  issuePillar,
  issueState,
  issueSystem,
  renderIssueCard,
  threadIssueNumber,
  threadName,
  wantedTags,
} from "./sync-roadmap.mjs";

const m1 = { number: 1, title: "M1: Playtest Town" };
const body = `Parent: Police & Response

Part of **M1 Playtest Town**. Wave 2.

## Why
The duty desk has no components, so nobody can go on duty.

## Work
- Clock in / clock out at the desk.
- Wanted board shows posters.
  - nested detail that is skipped

## Depends on
- M1-00 (#85)
- #83 (wanted list RPC)

## Acceptance proof
- **Suite:** WORLD police/station. Red-proof.
- **Clip:** Clock in ÔåÆ board.
`;
const issue = (overrides = {}) => ({
  number: 96,
  title: "M1-11 Police station on the street: duty desk and wanted board",
  state: "open",
  labels: [{ name: "topic:Law & Order" }, { name: "scope" }],
  body,
  assignees: [],
  html_url: "https://github.com/Life-Framework/Life-Framework/issues/96",
  ...overrides,
});

test("pillar, system, and state come from the topic label, Parent line, and assignees", () => {
  assert.equal(issuePillar(issue()), "Law & Order");
  assert.equal(issuePillar(issue({ labels: [] })), null);
  assert.equal(issueSystem(issue()), "Police & Response");
  assert.equal(issueSystem(issue({ body: "" })), "General");
  assert.equal(issueState(issue()), "open");
  assert.equal(issueState(issue({ assignees: [{ login: "alex" }] })), "in-progress");
  assert.equal(issueState(issue({ state: "closed" })), "done");
});

test("thread names carry the issue number as their key and fit Discord's limit", () => {
  assert.equal(threadName(issue()), "#96 Police station on the street: duty desk and wanted board");
  assert.equal(threadIssueNumber(threadName(issue())), 96);
  assert.equal(threadIssueNumber("Somebody's own post"), null);
  assert.ok(threadName(issue({ title: "x".repeat(200) })).length <= 100);
  assert.equal(displayTitle("M2 epic: Medic (downed state) (M2)"), "Medic (downed state)");
});

test("an open issue's card explains the work, the proof, what it waits on, and how to join", () => {
  const dep = issue({ number: 85, title: "M1-00 Split the street into site layers", state: "closed", html_url: "https://github.com/x/y/issues/85" });
  const card = renderIssueCard(issue(), m1, new Map([[85, dep]]), "2026-11-08T05:17:00.000Z");
  const d = card.embed.description;

  assert.equal(card.state, "open");
  assert.equal(card.pillar, "Law & Order");
  assert.equal(card.embed.title, "Police station on the street: duty desk and wanted board");
  assert.match(d, /🙋 \*\*Needs a hand\*\* · M1 · Law & Order \/ Police & Response/);
  assert.match(d, /\*\*Why\*\*\nThe duty desk has no components/);
  assert.match(d, /• Clock in \/ clock out at the desk\./);
  assert.match(d, /Roadmap_Police_Desk|duty desk has no components/);
  assert.doesNotMatch(d, /nested detail/);
  assert.match(d, /\*\*Done when\*\*\n• Suite: WORLD police\/station/);
  assert.match(d, /Clock in → board/, "mis-encoded arrows are repaired");
  assert.match(d, /✅ \[#85 Split the street into site layers\]\(https:\/\/github\.com\/x\/y\/issues\/85\)/);
  assert.match(d, /• #83/);
  assert.match(d, /Comment `\/claim` on the GitHub issue/);
  assert.match(d, /talk it through here in this thread/);
  assert.ok(d.length <= 4096);
  assert.equal(card.embed.footer.text, "GitHub #96 · synced 2026-11-08");
});

test("a claimed issue names its people and a done issue drops the call to join", () => {
  const claimed = renderIssueCard(issue({ assignees: [{ login: "alex" }, { login: "sam" }] }), m1);
  assert.match(claimed.embed.description, /\*\*On it\*\*\n\[alex\]\(https:\/\/github\.com\/alex\), \[sam\]/);
  const done = renderIssueCard(issue({ state: "closed", assignees: [{ login: "alex" }] }), m1);
  assert.match(done.embed.description, /\*\*Built by\*\*/);
  assert.match(done.embed.description, /What was built/);
  assert.doesNotMatch(done.embed.description, /\/claim/);
});

test("forum tags: pillars then statuses, existing IDs kept, hand-made tags preserved", () => {
  const tags = wantedTags([
    { id: "1", name: "Law & Order", emoji_name: "🛡️" },
    { id: "9", name: "Question" },
  ]);
  assert.equal(tags[0].id, "1");
  assert.equal(tags[0].name, "Law & Order");
  assert.ok(tags.some((t) => t.name === "Needs a hand" && !t.id));
  assert.ok(tags.some((t) => t.name === "Question" && t.id === "9"));
  assert.ok(tags.length <= 20);
});
