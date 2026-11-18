// Proves the waiting/working split of `tools\cli slice record` and `slice stats` (#3200) on a fixture worktree, with no
// GitHub and no game. inputs: tools/lib/slice.mjs, tools/commands/slice.mjs
//   1. a worktree's tmp/logs yields the waits per step: explicit `waited N s` (pool lines, a look summary, a session line)
//      counts, a log with no waited value falls back to its `(N s)` figures, a line naming no step is ignored;
//   2. record writes that waitMin map onto the ledger row; stats skips an older row that has none;
//   3. stats prints waiting and working minutes per card type, and working is the minutes less the waits.
// red-proof: #3200 (2026-11-18): before the split existed the run crashed on `slice.waitMinutes is not a function`; then the
// lib and record change went in and the run printed `SUMMARY slice-wait-selftest ok`. The fixture is written at runtime.
import { mkdirSync, appendFileSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import * as slice from "../lib/slice.mjs";
import { main } from "../commands/slice.mjs";
import { scratchDir } from "../lib/scratch.mjs";

const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};

const tmp = scratchDir("lf-slice-wait-");
const root = join(tmp, "wt");
const logDir = join(root, "tmp", "logs");
mkdirSync(logDir, { recursive: true });

// 1. the waits per step
const files = {
  "test-slice-3200.log": "pool: test only on lf-testbox: exit 0, waited 60 s, ran 14.2 s\npool: e2e on lf-testbox: exit 0, waited 90 s, ran 70 s\n",
  "session-slice-3200.log": "pool: session up on lf-testbox: exit 0, waited 180 s, ran 40 s\n",
  "look-slice-3200.log": "SUMMARY look ok 2 pictures, waited 42 s\n",
  "validate-slice-3200.log": "PASS  validate/a.mjs (30.0 s)\nPASS  validate/b.mjs (30.0 s)\n",
  "wt-ship-slice-3200.log": "ship: auto-merge on\nPR https://github.com/example/example/pull/1\n",
  "misc-slice-3200.log": "step took (10 s)\n",
};
for (const [name, text] of Object.entries(files)) writeFileSync(join(logDir, name), text);

const want = { validate: 1, look: 0.7, test: 2.5, session: 3, ship: 0 };
const got = slice.waitMinutes(Object.entries(files).map(([name, text]) => ({ name, text })));
check(JSON.stringify(got) === JSON.stringify(want), `waits per step: explicit waited values, the (N s) fallback for a log with none, and no step ignored (want ${JSON.stringify(want)}, got ${JSON.stringify(got)})`);
check(slice.workMinutes(12, want) === 4.8, `working is the minutes less the waits (want 4.8, got ${slice.workMinutes(12, want)})`);
check(slice.workMinutes(2, want) === 0, "working never goes below zero");

// 2. record writes the waitMin map; an old row with none is skipped by stats
const ledger = join(tmp, "ledger.jsonl");
const capture = async (args) => {
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
const T0 = "2026-11-18T10:00:00.000Z";
const T12 = "2026-11-18T10:12:00.000Z";
const rec = await capture(["record", "3200", "--model", "sonnet", "--outcome", "handoff", "--gate", "pass", "--started", T0, "--now", T12, "--ledger", ledger]);
const row = JSON.parse(readFileSync(ledger, "utf8").trim().split("\n").pop());
check(rec.code === 0 && JSON.stringify(row.waitMin) === JSON.stringify(want), `record writes the waitMin map from the worktree's logs (got ${rec.text} ${JSON.stringify(row.waitMin)})`);
appendFileSync(ledger, JSON.stringify({ at: T0, issue: 1, model: "sonnet", type: "capability", outcome: "merged", gate: "pass", minutes: 30 }) + "\n");

// 3. stats prints the split per card type
const st = await capture(["stats", "--ledger", ledger]);
check(st.text.includes("waiting 7.2m") && st.text.includes("working 4.8m"), `stats prints waiting and working minutes (got ${st.text})`);
check(st.text.includes("validate 1m") && st.text.includes("test 2.5m") && st.text.includes("session 3m"), `stats prints the waits per step (got ${st.text})`);
check(!st.text.includes("waiting 0m"), "a row with no waitMin does not count as zero wait");

if (failures.length) {
  console.log(`SUMMARY slice-wait-selftest fail ${failures.length}`);
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}
console.log("SUMMARY slice-wait-selftest ok");
