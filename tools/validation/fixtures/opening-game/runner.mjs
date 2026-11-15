// Fixture proof runner: `node runner.mjs <verb>` stands in for the real proof run on the session.
// quiz-proof and police-application-proof pass; quiz-rejoin-proof fails with a reason on its last line.
// LF_FIXTURE_CALLS (a file) gets one line per call, so the selftest can see what ran.
import { appendFileSync } from "node:fs";
const verb = process.argv[2];
if (process.env.LF_FIXTURE_CALLS) appendFileSync(process.env.LF_FIXTURE_CALLS, `${verb}\n`);
console.log(`running ${verb}`);
if (verb === "quiz-rejoin-proof") {
  console.log("FAIL second quiz: the quiz ran again for a known account");
  process.exit(1);
}
console.log(`${verb} ok`);
