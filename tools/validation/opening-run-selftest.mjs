// Proves `tools\cli opening` (slice #1495, docs/playtest/m4-opening-day.md "After the run") on fixture beat scripts:
// inputs: tools/commands/opening.mjs, tools/validation/fixtures/opening-run/**
//   1. a pass run: two fixture beat scripts (B1 all pass, B9 two checks blocked by slices) give exit 0 and a report with one
//      line per beat: B1 PASS, B9 "BLOCKED by #1413, #1274" (blocked is not a failure), and the report file is written;
//   2. the scripts are found by glob (no list), the lines that are not BEAT lines are ignored, and --host reaches the script;
//   3. a sheet beat with no script is listed, not failed: a game beat with a proof verb gets its `harness.mjs proof <verb>` line
//      (and a note when the verb is not on main); a beat with neither is marked manual;
//   4. a fixture FAIL gives exit 1 and names the failing check; a script that dies with no BEAT line is a FAIL too;
//   5. --beats B1 runs only B1;
//   6. the parsing helpers: BEAT lines, the aggregate status, the sheet's beats and verbs.
// red-proof: written before tools/commands/opening.mjs existed (the import failed); then with the FAIL-to-exit-1 line removed,
// "a fixture FAIL exits 1" failed, and with blockedBy returning [], "B9 is blocked by #1413, #1274" failed. Restored.
import { existsSync, mkdtempSync, readFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { aggregate, blockedBy, main, parseBeatLines, sheetBeats } from "../commands/opening.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const FIX = join(HERE, "fixtures", "opening-run");
const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};

// 6. helpers first
const lines = parseBeatLines("noise\nBEAT B1 a PASS fine\nBEAT B9 b BLOCKED needs #1413 and #1274\n  BEAT B2 c FAIL broke\nBEAT B2 d MAYBE x\nBEAT\n");
check(lines.length === 3, `three BEAT lines parse, noise and malformed lines are dropped (got ${lines.length})`);
check(lines[1].beat === "B9" && lines[1].id === "b" && lines[1].status === "BLOCKED" && lines[1].reason === "needs #1413 and #1274", "a BEAT line reads back its fields");
check(JSON.stringify(blockedBy(lines)) === JSON.stringify([1413, 1274]), "blockedBy lists the slice numbers in order of first mention");
check(aggregate([{ status: "PASS" }, { status: "BLOCKED" }]).status === "BLOCKED", "a blocked check makes the beat blocked");
check(aggregate([{ status: "BLOCKED" }, { status: "FAIL" }]).status === "FAIL", "a fail beats blocked");
check(aggregate([{ status: "PASS" }]).status === "PASS", "all pass is pass");
const sheet = sheetBeats(readFileSync(join(FIX, "sheet.md"), "utf8"));
check(sheet.map((b) => b.beat).join(",") === "B1,B2,B3,B6,B8,B9,B11", `the sheet's beats, not the "Not in this run" heading (got ${sheet.map((b) => b.beat)})`);
check(sheet.find((b) => b.beat === "B6").verbs.join() === "day-rate", "B6 names the day-rate proof verb once");
check(sheet.find((b) => b.beat === "B8").verbs.join() === "restart-cycle", "B8 names restart-cycle");
check(sheet.find((b) => b.beat === "B1").title === "Desk: go or no-go before opening", "the title drops the minutes");
check(sheet.find((b) => b.beat === "B8").title === "The clean restart and the reload", "the title drops the markers");

const tmp = mkdtempSync(join(tmpdir(), "lf-opening-"));
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
const common = ["--sheet", join(FIX, "sheet.md"), "--proofs", join(FIX, "proofs"), "--date", "2026-11-15"];

// 1-3. the pass run
const ok = await capture(["--dir", join(FIX, "ok"), "--host", "scratch-host", ...common]);
const reportPath = join(tmp, "tmp", "playtest", "opening-2026-11-15.md");
check(ok.code === 0, `a pass run with a blocked beat exits 0 (got ${ok.code}: ${ok.text})`);
check(existsSync(reportPath), "the report is written to tmp/playtest/opening-<date>.md");
const report = existsSync(reportPath) ? readFileSync(reportPath, "utf8") : "";
check(/^- B1 Desk: go or no-go before opening: PASS/m.test(report), `B1 is a PASS line (report: ${report})`);
check(/^- B9 Wipe with notice: BLOCKED by #1413, #1274/m.test(report), "B9 is blocked by #1413, #1274");
check(!/^- B9 .*FAIL/m.test(report), "a blocked beat is not reported as a failure");
check(report.includes("Host folder: scratch-host") && /^- B1 .*PASS \(2 checks\)/m.test(report), "--host reaches the beat script, as the flag and as LF_HOST_DIR (the fixture fails otherwise)");
check(!/chatter/.test(report), "lines that are not BEAT lines stay out of the report");
check(/^- B6 The 2-hour day: NOT RUN/m.test(report) && report.includes("node agent-box/harness.mjs proof day-rate"), "B6, a game beat, lists its proof line");
check(!/proof day-rate.*not on main/.test(report), "day-rate is on main in the fixture, no note");
check(/proof restart-cycle.*not on main/.test(report), "restart-cycle is not on main in the fixture and says so");
check(/^- B3 The rules board at the plaza: NOT RUN.*no script/m.test(report), "a beat with neither script nor verb is marked manual");
check(/SUMMARY opening ok beats=7 pass=1 fail=0 blocked=1 notrun=5 report=/.test(ok.text), `a SUMMARY line (got ${ok.text})`);

// 4. fail
const bad = await capture(["--dir", join(FIX, "fail"), ...common]);
check(bad.code === 1, `a fixture FAIL exits 1 (got ${bad.code})`);
check(/B2 .*FAIL.*wrong-password-refused/.test(bad.text), `the failing check is named (got ${bad.text})`);
check(/B3 .*FAIL.*exited 3/.test(bad.text), `a script that dies with no BEAT line is a FAIL (got ${bad.text})`);
check(/SUMMARY opening fail/.test(bad.text), "the fail run says fail");

// 5. --beats
const only = await capture(["--dir", join(FIX, "ok"), "--beats", "b1", ...common]);
const onlyReport = readFileSync(reportPath, "utf8");
check(only.code === 0 && /^- B1 /m.test(onlyReport) && !/^- B9 /m.test(onlyReport) && !/^- B6 /m.test(onlyReport), "--beats B1 runs and reports only B1");

if (failures.length) {
  console.log(`SUMMARY opening-run-selftest fail ${failures.length}`);
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}
console.log("SUMMARY opening-run-selftest ok");
