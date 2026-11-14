// Proves tools/agents/usage.mjs on fixtures/agent-usage (a lead, an lf-builder run, an older
// inputs: tools/**, agent-box/**, .githooks/**
// lf-scout run):
//   1. one API turn written as two records with the same message.id counts once, and a
//      <synthetic> record is not a turn;
//   2. cost-weighted tokens, peak and average context, the share past 150k and the handoff
//      simulation match hand-worked numbers;
//   3. shell commands get the right kind (test, poll, edit, search, git, script, session, wait);
//   4. a repeated Read is counted, a 120 s tool call is a wait, an image result is 1600 tokens;
//   5. --since drops the older run, --leads adds the lead, --top/--type limit the report.
// red-proof: 2026-11-14, with the message.id dedupe removed (every record a turn) it printed
// "FAIL agent-usage: turns: 5 != 4" and "FAIL agent-usage: cost: 385882 != 335380".
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { analyzeFile, bashKind, byType, collect, diagnose, main, parseSince, report } from "../agents/usage.mjs";

const failures = [];
const eq = (got, want, msg) => {
  if (got !== want) failures.push(`${msg}: ${got} != ${want}`);
};
const near = (got, want, msg) => {
  if (Math.abs(got - want) > 0.5) failures.push(`${msg}: ${got} != ${want}`);
};

const fx = join(dirname(fileURLToPath(import.meta.url)), "fixtures", "agent-usage", "projects", "C--fixture-Life-Framework");
const a1 = join(fx, "s1", "subagents", "agent-a1.jsonl");

// 1 + 2: the builder run, worked by hand
const r = await analyzeFile(a1, { agentType: "lf-builder", description: "Fix issue 1" });
eq(r.turns, 4, "turns");
// m1 2+1.25*40000+5*100 | m2 1+1250+4000+1000 | m3 1+250000+4100+250 | m5 1+125+24100+50
near(r.cost, 335380, "cost");
eq(r.peakCtx, 241101, "peak context");
near(r.avgCtx, (40002 + 41001 + 241001 + 241101) / 4, "average context");
near(r.hotCost, 254351 + 24276, "cost past 150k");
eq(r.startCtx, 40002, "start context");
eq(r.handoffs, 1, "handoffs");
// base 45002; m3 hands off (offset 195999): + 1.25*(45002+30000) + (254351-19599.9) + (24276-19599.9)
near(r.handoffCost, 50502 + 6251 + 93752.5 + 234751.1 + 4676.1, "handoff cost");
eq(r.outTokens, 360, "output tokens");
eq(r.models.join(","), "claude-sonnet-5-5", "models (synthetic dropped)");
near(r.wallMin * 60, 127, "wall seconds");

// 4: tools
eq(r.tools.read.calls, 2, "read calls");
near(r.tools.read.cost, 50502 + 254351 / 2, "read cost share");
near(r.tools["bash:poll"].cost, 254351 / 2, "poll cost share");
eq(r.tools["bash:test"].calls, 1, "test calls");
near(r.tools.text.cost, 24276, "text-only turn cost");
eq(r.repeatReads, 1, "repeated reads");
eq(r.reads, 2, "reads");
eq(r.waits, 1, "waits");
near(r.waitMin, 2, "wait minutes");
eq(r.tools["bash:poll"].result, 1600, "image result tokens");
eq(r.resultTokens, 1000 + 1 + 1000 + 1600, "result tokens");

// 3: command kinds
const kinds = {
  "cd /c/x && tools\\cli test --no-build --only LF_Test_A": "bash:test",
  "tools\\cli wt ship slug --body-file tmp/b.md": "bash:ship",
  "date; cat tmp/shots-before/batch.txt | tail": "bash:poll",
  "node agent-box/session.mjs up --profile full": "bash:session",
  "until [ ! -f x.lock ]; do sleep 30; done": "bash:wait",
  "cd /c/x; f=a.c; grep -n foo $f": "bash:search",
  "cd /c/x; sed -n 1,40p a.c": "bash:read",
  "sed -i 's/a/b/' a.c": "bash:edit",
  "cat > tmp/x.json <<'EOF'\n{}\nEOF": "bash:edit",
  "git log --oneline -3": "bash:git",
  "node -e 'console.log(1)'": "bash:script",
  "cd /c/x; for n in A B; do grep -c $n f; done": "bash:search",
  "tools\\cli logs LFDebug": "bash:log",
  "FF=tools/film/.bin/ffmpeg.exe; $FF -i a.mp4 b.mp4": "bash:media",
};
for (const [cmd, want] of Object.entries(kinds)) eq(bashKind(cmd), want, `kind of ${JSON.stringify(cmd)}`);

// diagnosis
const big = { ...r, turns: 400, avgCtx: 300000, waitMin: 60, waitCtx: 300000, waits: 10, compactions: 0 };
const flags = diagnose(big).join("; ");
if (!/scope too big: 400 turns/.test(flags)) failures.push(`diagnose missed the scope: ${flags}`);
if (!/waited 60 min/.test(flags)) failures.push(`diagnose missed the wait: ${flags}`);

// 5: collection, filters, report
const all = await collect([fx]);
eq(all.length, 2, "subagent runs");
const withLead = await collect([fx], { leads: true });
eq(withLead.length, 3, "runs with --leads");
const recent = await collect([fx], { since: parseSince("2026-11-10") });
eq(recent.map((x) => x.type).join(","), "lf-builder", "--since keeps the newer run");
eq((await collect([fx], { type: "lf-scout" })).length, 1, "--type");
eq(parseSince("2d", Date.parse("2026-11-14T00:00:00Z")), Date.parse("2026-11-12T00:00:00Z"), "relative --since");
eq(byType(all)[0].type, "lf-builder", "by-type order");
const text = report(all, { top: 1 });
if (!/top 1 by cost/.test(text) || /Find X/.test(text)) failures.push(`--top 1 report:\n${text}`);

const real = console.log;
let printed = "";
console.log = (...a) => (printed += a.join(" ") + "\n");
let code;
try {
  code = await main(["--dir", fx, "--top", "5", "--leads"]);
} finally {
  console.log = real;
}
eq(code, 0, "main exit code");
if (!/3 runs/.test(printed) || !/lead/.test(printed)) failures.push(`main output:\n${printed}`);

if (failures.length) {
  for (const f of failures) console.log(`FAIL agent-usage: ${f}`);
  process.exit(1);
}
console.log("agent-usage selftest: ok");
