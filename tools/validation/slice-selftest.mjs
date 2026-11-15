// Proves `tools\cli slice` (docs/process/roadmap-pipeline.md) on fixtures, with no GitHub:
// inputs: tools/lib/slice.mjs, tools/commands/slice.mjs, tools/validation/fixtures/slice/**
//   1. the #1005 worked-example plan parses into six good cards; a card written to an issue body reads back the same,
//      with plan keys replaced by issue numbers;
//   2. a bad plan is refused: two parallel slices owning the same file (shared append-only files excepted), an over-budget
//      slice, a missing acceptance check, risk below T2 or off Opus, and a dependency cycle;
//   3. readiness: a dependency counts as done once it is merged (in-dev, needs-playtest, verified or closed); agent:working
//      is running, not ready; ready slices sort by critical path (longest remaining chain first), release blockers ahead on a tie;
//   4. the brief carries goal, acceptance, owned files, the hard timebox with the split rule, the tier's gate, the agent for
//      the model and, for risky work, the independent reviewer;
//   5. the ledger: record refuses an incomplete row, appends good ones; stats shows first-pass, rework and median minutes
//      per model and type;
//   6. the command: list --fixture prints the queue and the ready:running ratio, new --dry-run writes the cards file and
//      exits 0, a bad plan exits 1, all on quiet SUMMARY lines.
// red-proof: with the merged test in statusOf forced false, "S3 is ready once S2 is in-dev" failed; with the sort's path
// term removed, "the longest chain (X, 90m) goes before a short blocker" failed; with isShared forced false, "features.md is not a
// conflict" failed. All restored. record-merged (2026-11-15): the new cases were written first and failed (mergeVerdict
// did not exist; record took --outcome merged with an open PR); then mergeVerdict and the check in slice record went in.
import { mkdtempSync, readFileSync, writeFileSync, existsSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { main } from "../commands/slice.mjs";
import { brief, cardBody, checkPlan, conflicts, criticalPaths, ledgerRow, mergeVerdict, parseCard, parsePlan, readyOrder, renderStats, stats, statusOf, suggestModel } from "../lib/slice.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const FIX = join(HERE, "fixtures", "slice");
const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};

// 1. the worked example
const plan = parsePlan(readFileSync(join(FIX, "plan-1005.md"), "utf8"));
check(plan.length === 6, `the #1005 plan has six slices (got ${plan.length})`);
check(checkPlan(plan).length === 0, `the #1005 plan is clean: ${checkPlan(plan).join("; ")}`);
const numbers = new Map(plan.map((s, i) => [s.id, 101 + i]));
const issues = plan.map((s) => ({ number: numbers.get(s.id), title: s.title, state: "OPEN", labels: [], body: cardBody(s, 1005, numbers) }));
const back = parseCard(issues[4].body);
check(back.parent === 1005 && back.key === "S5" && back.model === "opus" && back.tier === "T2", "a card body reads back its fields");
check(back.risk.join() === "persistence" && back.blocker === true && back.minutes === 45 && back.calls === 60, "risk, blocker and budget read back");
check(JSON.stringify(back.depends) === "[102]", `plan keys become issue numbers (got ${JSON.stringify(back.depends)})`);
check(back.owns[0] === "addons/LifeFramework/Scripts/Game/Persistence/LF_ContentAlias*.c", "owns reads back without backticks");

// 2. a bad plan
const bad = checkPlan(parsePlan(readFileSync(join(FIX, "plan-bad.md"), "utf8"))).join("\n");
check(bad.includes("dependency cycle"), "a cycle is refused");
const badNoCycle = parsePlan(readFileSync(join(FIX, "plan-bad.md"), "utf8")).map((s) => (s.id === "C" ? { ...s, card: { ...s.card, depends: [] } } : s));
const conf = conflicts(badNoCycle).join("\n");
check(/A and B .*Feature\/Bank/.test(conf), `two parallel slices owning one file conflict (got ${conf})`);
check(/C: budget must be at most 45 min/.test(bad), "an over-budget slice is refused");
check(/C: acceptance/.test(bad), "a slice with no acceptance check is refused");
check(/C: risk persistence needs tier T2/.test(bad), "risky work below T2 is refused");
check(suggestModel(parseCard("## Card\n- type: data\n- risk: money")) === "opus" && suggestModel(parseCard("## Card\n- type: docs")) === "haiku", "routing: risk goes to opus, docs to haiku");
check(conflicts([plan[0], plan[1]]).length === 0, "disjoint slices do not conflict");
const mk = (id, owns, extra = "") => ({ id, title: id, state: "open", labels: [], card: parseCard(`## Card
- owns: ${owns}
- budget: 30 min
${extra}`) });
check(conflicts([mk("P", "`docs/features.md`, `tools/p.mjs`"), mk("Q", "`docs/features.md`, `tools/q.mjs`")]).length === 0, "features.md is not a conflict");
const chain = [mk("X", "`a`"), mk("Y", "`b`", "- depends: X"), mk("Z", "`c`", "- depends: Y"), mk("W", "`d`", "- release-blocker: yes")];
check(readyOrder(chain).map((s) => s.id).join() === "X,W", `the longest chain (X, 90m) goes before a short blocker (W, 30m) (got ${readyOrder(chain).map((s) => s.id)})`);

// 3. readiness and priority
const live = issues.map((i) => ({ ...i, id: i.number, card: parseCard(i.body), state: "open", labels: [] }));
live[1].labels = ["in-dev"]; // S2 merged
live[0].labels = ["agent:working"]; // S1 running
check(statusOf(live[1]) === "merged" && statusOf(live[0]) === "running", "in-dev is merged, agent:working is running");
check(readyOrder(live.map((s, i) => (i === 2 ? { ...s, labels: ["hold"] } : s))).every((s) => s.card.key !== "S3"), "a held slice is never ready");
const order = readyOrder(live).map((s) => s.card.key);
check(order.includes("S3"), "S3 is ready once S2 is in-dev");
check(!order.includes("S6") && !order.includes("S1") && !order.includes("S2"), `blocked, running and merged slices are not ready (got ${order})`);
check(order[0] === "S5", `S5 (path 90, blocker) leads the queue (got ${order})`);
check(order.join() === "S5,S3,S4", `queue is S5,S3,S4 (got ${order})`);
const cp = criticalPaths(live);
check(cp.get(105).path === 90 && cp.get(103).path === 90 && cp.get(104).path === 45 && cp.get(102).path === 0 + 90, `critical paths (got ${[...cp].map(([k, v]) => k + ":" + v.path)})`);
check(cp.get(103).blocker && !cp.get(104).blocker, "a slice that feeds a release blocker is flagged");

// 4. the brief
const b5 = brief(issues[4], parseCard(issues[4].body));
for (const want of ["lf-hard", "Timebox: 45 min / 60 calls", "SPLIT", "LF_ContentAlias", "LF_Test_ContentAliasLoad", "wt ship slice-105 --refs 105", "independent reviewer", "slice record 105 --model opus", "merged, in-dev"]) {
  check(b5.includes(want), `the brief says ${want}`);
}
const b2 = brief(issues[1], parseCard(issues[1].body));
check(b2.includes("lf-builder") && b2.includes("light gate") && !b2.includes("independent reviewer"), "a T1 data brief has the light gate and no reviewer");

// 5. the ledger
check(ledgerRow({ model: "gpt", outcome: "merged", gate: "pass", minutes: 3 }).length === 1, "record refuses an unknown model");
const rows = [
  { model: "sonnet", type: "capability", outcome: "merged", gate: "pass", minutes: 30, wait: 2 },
  { model: "sonnet", type: "capability", outcome: "merged", gate: "fail", minutes: 50, wait: 4 },
  { model: "sonnet", type: "data", outcome: "split", gate: "pass", minutes: 45 },
  { model: "opus", type: "system", outcome: "merged", gate: "pass", minutes: 40 },
];
const st = stats(rows);
const son = st.model.find((g) => g.key === "sonnet");
check(son.n === 3 && Math.abs(son.firstPass - 2 / 3) < 1e-9 && Math.abs(son.rework - 1 / 3) < 1e-9 && son.minutes === 45 && son.wait === 3, `sonnet stats (got ${JSON.stringify(son)})`);
check(st.type.find((g) => g.key === "capability").minutes === 40, "median minutes per type");

// 6. the command, quiet
const tmp = mkdtempSync(join(tmpdir(), "lf-slice-"));
const fixture = join(tmp, "issues.json");
writeFileSync(fixture, JSON.stringify({ issues: live.map((s) => ({ number: s.number, title: s.title, state: "OPEN", labels: s.labels.map((name) => ({ name })), body: s.body })) }));
const capture = async (args, root = tmp) => {
  const out = [];
  const [log, err] = [console.log, console.error];
  console.log = (...a) => out.push(a.join(" "));
  console.error = (...a) => out.push(a.join(" "));
  try {
    return { code: await main(args, { root }), text: out.join("\n") };
  } finally {
    console.log = log;
    console.error = err;
  }
};
const ls = await capture(["list", "1005", "--fixture", fixture]);
check(ls.code === 0 && ls.text.includes("ready 3, running 1") && /1\. #105 .*BLOCKER/.test(ls.text), `list prints the queue (got ${ls.text})`);
const lr = await capture(["list", "1005", "--ready", "--fixture", fixture]);
check(lr.text.split("\n").length === 4, `--ready prints only the queue (got ${lr.text.split("\n").length} lines)`);
const br = await capture(["brief", "103", "--fixture", fixture]);
check(br.code === 0 && br.text.includes("Slice #103"), "brief prints from the fixture");
const dry = await capture(["new", "1005", "--plan", join(FIX, "plan-1005.md"), "--dry-run"]);
check(dry.code === 0 && dry.text.startsWith("SUMMARY slice new ok dry-run 6") && existsSync(join(tmp, "tmp", "slices-1005.cards.md")), `new --dry-run writes cards (got ${dry.text})`);
const tpl = await capture(["new", "1005"]);
check(tpl.code === 0 && existsSync(join(tmp, "tmp", "slices-1005.md")), "new without a plan writes the template");
const tplCheck = await capture(["new", "1005", "--plan", join(tmp, "tmp", "slices-1005.md"), "--dry-run"]);
check(tplCheck.code === 0, `the template itself passes the plan check (got ${tplCheck.text})`);
const nb = await capture(["new", "1005", "--plan", join(FIX, "plan-bad.md"), "--dry-run"]);
check(nb.code === 1 && nb.text.startsWith("SUMMARY slice new fail"), "a bad plan exits 1 on one SUMMARY line");
// a later wave: `depends: #N` names a live sub-issue, and owns is checked against the running ones
const wave = (key, owns, depends) =>
  `# Slice ${key}: Wave card ${key}\n## Card\n- type: tooling\n- tier: T0\n- model: sonnet\n- machines: none\n- budget: 30 min / 40 calls\n- owns: \`${owns}\`\n- depends: ${depends}\n- acceptance: test LF_Test_Wave${key}\n- risk: none\n- release-blocker: no\n\n## Capability\nOne wave card.\n`;
const waveRun = async (name, text) => {
  writeFileSync(join(tmp, name), text);
  return capture(["new", "1005", "--plan", join(tmp, name), "--dry-run", "--fixture", fixture]);
};
const wOk = await waveRun("wave-ok.md", wave("N1", "tools/wave/n1.mjs", "#102"));
check(wOk.code === 0, `a wave card may depend on a live sub-issue by number (got ${wOk.text})`);
const wClash = await waveRun("wave-clash.md", wave("N2", "addons/LifeFramework/Scripts/Game/Feature/Robbery/LF_RobberySystemToggle.c", "none"));
check(wClash.code === 1 && /#101 and N2|N2 and #101/.test(wClash.text), `a wave card owning a running slice's file is refused (got ${wClash.text})`);
const wGone = await waveRun("wave-gone.md", wave("N3", "tools/wave/n3.mjs", "#999"));
check(wGone.code === 1 && wGone.text.includes("N3 depends on #999"), `a dependency on an unknown issue is refused (got ${wGone.text})`);
const ledger = join(tmp, "ledger.jsonl");
// record --outcome merged asks gh whether the slice's PR merged (fixtures: --pr-fixture { prs: [...] }, no GitHub).
const prFile = (name, prs) => {
  const f = join(tmp, name);
  writeFileSync(f, JSON.stringify({ prs }));
  return f;
};
const mergedPr = prFile("pr-merged.json", [{ number: 900, state: "MERGED", mergedAt: "2026-11-15T10:00:00Z", headRefName: "ws/slice-105", body: "Refs #105" }]);
const openPr = prFile("pr-open.json", [{ number: 901, state: "OPEN", mergedAt: null, headRefName: "ws/slice-106", body: "Refs #106" }]);
const noPr = prFile("pr-none.json", []);
const rec = (issue, outcome, pr, extra = []) => capture(["record", String(issue), "--model", "sonnet", "--outcome", outcome, "--gate", "pass", "--minutes", "30", "--pr-fixture", pr, "--ledger", ledger, ...extra]);
const ledgerCount = () => (existsSync(ledger) ? readFileSync(ledger, "utf8").trim().split("\n").filter(Boolean).length : 0);
const r1 = await capture(["record", "105", "--model", "opus", "--outcome", "merged", "--gate", "pass", "--minutes", "41", "--type", "system", "--pr-fixture", mergedPr, "--ledger", ledger]);
const r2 = await capture(["record", "103", "--model", "sonnet", "--ledger", ledger]);
check(r1.code === 0 && r2.code === 1 && ledgerCount() === 1, "record appends good rows and refuses incomplete ones");
const ss = await capture(["stats", "--ledger", ledger]);
check(ss.text.includes("by model") && /opus .*first-pass 100%.*median 41m/.test(ss.text), `stats reads the ledger (got ${ss.text})`);
const rOpen = await rec(106, "merged", openPr);
check(rOpen.code === 1 && ledgerCount() === 1 && rOpen.text.includes("handoff") && rOpen.text.includes("#901"), `record refuses merged while the PR is open and suggests handoff (got ${rOpen.text})`);
const rNone = await rec(107, "merged", noPr);
check(rNone.code === 1 && ledgerCount() === 1 && rNone.text.includes("handoff"), `record refuses merged when no PR is found (got ${rNone.text})`);
const rForce = await rec(106, "merged", openPr, ["--force"]);
check(rForce.code === 0 && ledgerCount() === 2, `--force records merged anyway (got ${rForce.text})`);
const rHand = await rec(106, "handoff", openPr);
const rHandNone = await rec(108, "handoff", noPr);
check(rHand.code === 0 && rHandNone.code === 0 && ledgerCount() === 4, "a handoff needs no merged PR");
const pr = (number, state, head, body, mergedAt = null) => ({ number, state, mergedAt, headRefName: head, body });
check(mergeVerdict([pr(1, "MERGED", "ws/slice-105", "", "2026-11-15T10:00:00Z")], 105).ok, "mergeVerdict: a merged PR on ws/slice-<N> passes");
check(mergeVerdict([pr(2, "MERGED", "ws/other", "Closes #105", "2026-11-15T10:00:00Z")], 105).ok, "mergeVerdict: a merged PR whose body refs the issue passes");
check(!mergeVerdict([pr(3, "MERGED", "ws/other", "Refs #1050", "2026-11-15T10:00:00Z")], 105).ok, "mergeVerdict: #1050 is not #105");
check(!mergeVerdict([pr(4, "CLOSED", "ws/slice-105", "Refs #105")], 105).ok, "mergeVerdict: a closed, unmerged PR does not pass");
check(mergeVerdict([pr(5, "CLOSED", "ws/slice-105", "Refs #105"), pr(6, "MERGED", "ws/slice-105-b", "Refs #105", "2026-11-15T10:00:00Z")], 105).ok, "mergeVerdict: one merged PR among several passes");
check(renderStats([]).startsWith("no slices recorded"), "empty ledger says so");

if (failures.length) {
  console.log(`SUMMARY slice-selftest fail ${failures.length}`);
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}
console.log("SUMMARY slice-selftest ok");
