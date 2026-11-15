// Proves `tools\cli opening --game` (slice #1599, docs/playtest/m4-opening-day.md B4, B5, B6, B8) on a fixture proof runner:
// inputs: tools/commands/opening.mjs, tools/validation/fixtures/opening-game/**
//   1. the verbs come from the sheet's proof-verb lines, not a list in code (B4 names two, B5 one, B6 and B8 one each);
//   2. a verb that passes gives a PASS line (B5), one that fails gives a FAIL with the proof's own reason (B4);
//   3. a verb that is not on main is BLOCKED by its card (B6 #1303, B8 #1268), never a FAIL, and is never run;
//   4. the desk beat's line (B9, from a beat script) stays in the same report;
//   5. exit is 1 only for the FAIL; a run limited to the passing and blocked beats exits 0;
//   6. with no session up nothing is run: each verb is BLOCKED and the `session up` line to run is printed;
//   7. without --game the verbs are listed, not run.
// red-proof: written before `--game` existed (the B5 PASS and B4 FAIL checks failed); then with the missing-verb branch
// returning FAIL, "B6 is BLOCKED by #1303" failed, and with the exit code ignoring FAIL, "the FAIL exits 1" failed. Restored.
import { existsSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { main, sheetBeats } from "../commands/opening.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const FIX = join(HERE, "fixtures", "opening-game");
const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};

const tmp = mkdtempSync(join(tmpdir(), "lf-opening-game-"));
const calls = join(tmp, "calls.txt");
process.env.LF_FIXTURE_CALLS = calls;
const capture = async (args) => {
  const out = [];
  const [log, err] = [console.log, console.error];
  console.log = (...a) => out.push(a.join(" "));
  console.error = (...a) => out.push(a.join(" "));
  try {
    return { code: await main(args, { root: tmp }), text: out.join("\n") };
  } finally {
    console.log = log;
    console.error = err;
  }
};
const common = ["--sheet", join(FIX, "sheet.md"), "--proofs", join(FIX, "proofs"), "--dir", join(FIX, "desk"), "--date", "2026-11-15", "--runner", join(FIX, "runner.mjs")];
const reportPath = join(tmp, "tmp", "playtest", "opening-2026-11-15.md");
const ran = () => (existsSync(calls) ? readFileSync(calls, "utf8").split(/\r?\n/).filter(Boolean) : []);

// 1. the verbs the sheet names
const sheet = sheetBeats(readFileSync(join(FIX, "sheet.md"), "utf8"));
check(sheet.find((b) => b.beat === "B4").verbs.join() === "quiz-proof,quiz-rejoin-proof", `B4 names two verbs (got ${sheet.find((b) => b.beat === "B4").verbs})`);
check(sheet.find((b) => b.beat === "B5").verbs.join() === "police-application-proof", "B5 names police-application-proof");
check(sheet.find((b) => b.beat === "B6").cards["day-rate"] === 1303, "B6's day-rate verb belongs to #1303");
check(sheet.find((b) => b.beat === "B8").cards["restart-cycle"] === 1268, "B8's restart-cycle verb belongs to #1268");

// 2-5. a run with a session up
const up = await capture(["--game", "--session-file", join(FIX, "session-up.json"), ...common]);
const report = existsSync(reportPath) ? readFileSync(reportPath, "utf8") : "";
check(/BEAT B5 police-application-proof PASS/.test(report), `B5's verb passed (report: ${report})`);
check(/^- B5 .*: PASS/m.test(report), "B5 is a PASS row");
check(/BEAT B4 quiz-rejoin-proof FAIL .*the quiz ran again for a known account/.test(report), "B4's failing verb is a FAIL with the proof's reason");
check(/^- B4 .*FAIL quiz-rejoin-proof: .*the quiz ran again/m.test(report), "B4 is a FAIL row that names the verb and the reason");
check(/BEAT B4 quiz-proof PASS/.test(report), "B4's passing verb is a PASS line beside its failing one");
check(/BEAT B6 day-rate BLOCKED .*#1303/.test(report) && /^- B6 .*BLOCKED by #1303/m.test(report), "B6 is BLOCKED by #1303");
check(/BEAT B8 restart-cycle BLOCKED .*#1268/.test(report) && /^- B8 .*BLOCKED by #1268/m.test(report), "B8 is BLOCKED by #1268");
check(!/^- B(6|8) .*FAIL/m.test(report), "a missing verb is never a FAIL");
check(ran().join() === "quiz-proof,quiz-rejoin-proof,police-application-proof", `only the verbs on main ran, in beat order (ran ${ran()})`);
check(/^- B9 .*PASS/m.test(report), "the desk beat's line stays in the report");
check(up.code === 1, `the FAIL exits 1 (got ${up.code})`);
check(/SUMMARY opening fail beats=5 pass=2 fail=1 blocked=2 notrun=0 report=/.test(up.text), `a SUMMARY line (got ${up.text})`);

// 5. only the passing and blocked beats exit 0
const good = await capture(["--game", "--beats", "B5,B6,B9", "--session-file", join(FIX, "session-up.json"), ...common]);
check(good.code === 0, `a run with no FAIL exits 0 (got ${good.code}: ${good.text})`);

// 6. no session: nothing runs, the line to run is printed
writeFileSync(calls, "");
for (const file of [join(FIX, "session-down.json"), join(tmp, "missing.json")]) {
  const none = await capture(["--game", "--session-file", file, ...common]);
  const text = readFileSync(reportPath, "utf8");
  check(ran().length === 0, "no verb runs when no session is up");
  check(/BEAT B5 police-application-proof BLOCKED no session up/.test(text), "a verb on main is BLOCKED when no session is up");
  check(/^- B5 .*BLOCKED/m.test(text) && !/^- B5 .*PASS/m.test(text), "B5 is not a PASS without a session");
  check(/session up --clients 2/.test(none.text), `the session up line is printed (got ${none.text})`);
  check(none.code === 0, `no session is not a failure (got ${none.code})`);
}

// 7. without --game nothing runs
writeFileSync(calls, "");
const plain = await capture(common);
check(ran().length === 0, "without --game no verb runs");
check(/^- B4 .*NOT RUN/m.test(readFileSync(reportPath, "utf8")), "without --game the game beats are listed as NOT RUN");
check(plain.code === 0, "without --game exit 0");

if (failures.length) {
  console.log(`SUMMARY opening-game-selftest fail ${failures.length}`);
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}
console.log("SUMMARY opening-game-selftest ok");
