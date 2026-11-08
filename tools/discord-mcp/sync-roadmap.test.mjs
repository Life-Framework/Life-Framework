import assert from "node:assert/strict";
import test from "node:test";
import { archivedThreadName, buildPillars, issuePillar, issueSystem, renderPillarPost } from "./sync-roadmap.mjs";

const m1 = { number: 1, title: "M1: Playtest Town" };
const m2 = { number: 2, title: "M2" };
const issue = (number, title, { state = "open", topic = "Law & Order", parent = "Police & Response", assignees = [] } = {}) => ({
  number,
  title,
  state,
  labels: topic ? [{ name: `topic:${topic}` }, { name: "enhancement" }] : [],
  body: parent ? `Parent: ${parent}\n\nPart of **M1 Playtest Town**.` : "No parent line.",
  assignees: assignees.map((login) => ({ login })),
  html_url: `https://github.com/Life-Framework/Life-Framework/issues/${number}`,
});

test("pillar and system come from the topic label and the Parent line, like the website", () => {
  assert.equal(issuePillar(issue(1, "x")), "Law & Order");
  assert.equal(issuePillar(issue(1, "x", { topic: null })), "Unsorted");
  assert.equal(issuePillar({ labels: [{ name: "topic:A" }, { name: "topic:B" }] }), "Multiple topics");
  assert.equal(issueSystem(issue(1, "x")), "Police & Response");
  assert.equal(issueSystem({ body: "  parent: **Town Foundation**" }), "Town Foundation");
  assert.equal(issueSystem(issue(1, "x", { parent: null })), "General");
});

test("pillars follow the website order and systems follow their earliest issue", () => {
  const pillars = buildPillars([
    { milestone: m1, issue: issue(30, "World item", { topic: "World & Travel", parent: "Travel" }) },
    { milestone: m2, issue: issue(5, "Later law", { parent: "Moderation" }) },
    { milestone: m1, issue: issue(9, "Early law") },
  ]);
  assert.deepEqual(
    pillars.map((p) => p.name),
    ["Law & Order", "World & Travel"],
  );
  assert.deepEqual(
    pillars[0].systems.map((s) => s.name),
    ["Police & Response", "Moderation"],
  );
});

test("a pillar post leads with open work, names who is on an issue, and explains /claim", () => {
  const [pillar] = buildPillars([
    { milestone: m1, issue: issue(10, "M1-01 Done thing", { state: "closed" }) },
    { milestone: m1, issue: issue(11, "M1-02 Taken thing", { assignees: ["alex", "sam"] }) },
    { milestone: m2, issue: issue(12, "M2 epic: Free thing (M2)") },
  ]);
  const post = renderPillarPost(pillar, "2026-11-08T05:17:00.000Z");

  assert.equal(post.name, "Law & Order");
  assert.equal(post.embed.title, "🛡️ Law & Order");
  assert.equal(post.embed.url, "https://life-framework.org/roadmap");
  const d = post.embed.description;
  assert.match(d, /1\/3 done \(33%\)/);
  assert.match(d, /1 needs a hand/);
  assert.match(d, /\*\*Police & Response\*\* · 1\/3/);
  assert.ok(d.indexOf("Free thing") < d.indexOf("Taken thing"), "open issues come first");
  assert.ok(d.indexOf("Taken thing") < d.indexOf("Done thing"), "done issues come last");
  assert.doesNotMatch(d, /epic:|\(M2\)/, "milestone markers are stripped from titles");
  assert.match(d, /🙋 `M2` \[Free thing\]/);
  assert.match(d, /\[alex\]\(https:\/\/github\.com\/alex\), \[sam\]\(https:\/\/github\.com\/sam\)/);
  assert.match(d, /comment `\/claim`/);
  assert.equal(post.embed.footer.text, "Synced from GitHub · 2026-11-08");
});

test("a huge pillar stays inside Discord's embed limit and links to the full tree", () => {
  const entries = Array.from({ length: 120 }, (_, i) => ({
    milestone: m1,
    issue: issue(i + 1, `M1-${i + 1} A long issue title that takes up a fair amount of the Discord card budget`, { parent: `System ${i % 12}` }),
  }));
  const post = renderPillarPost(buildPillars(entries)[0]);
  assert.ok(post.embed.description.length <= 4096);
  assert.match(post.embed.description, /more issue\(s\) not shown/);
  assert.match(post.embed.description, /\[See the whole tree\]\(https:\/\/life-framework\.org\/roadmap\)$/);
});

test("retired posts are marked once and stay within Discord's title limit", () => {
  assert.equal(archivedThreadName("Law & Order"), "[ARCHIVED] Law & Order");
  assert.equal(archivedThreadName("[ARCHIVED] Law & Order"), "[ARCHIVED] Law & Order");
  assert.ok(archivedThreadName("x".repeat(120)).length <= 100);
});
