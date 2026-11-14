import { clientBox, quizAnswers, sessionDir } from "../session.mjs";

// The rules orientation quiz (#915) on a running session, driven the way a player does it.
//   node agent-box/session.mjs up --profile full     (one client)
//   node agent-box/session.mjs quiz-proof [--name quiz]
//   node agent-box/session.mjs down
// A new player lands on the quiz, not the registration form. The proof answers the first question
// wrong on purpose to see the rule explained, retries and answers right, continues to the
// registration form, and keeps three shots: a question, the fail page with the rules, the pass page.
async function runQuizProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 200)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const at = process.argv.indexOf("--name");
  const prefix = at >= 0 && process.argv[at + 1] ? process.argv[at + 1] : "quiz";
  const shot = (name) => on(1, { verb: "capture", mode: "ui", name: `${prefix}_${name}` }, 25000);
  const fails = [];
  const expect = (cond, what) => { if (!cond) fails.push(what); };
  const answers = quizAnswers();
  expect(answers.length > 0, "no questions in the pack's quiz file");

  wait(6000);
  let ui = on(1, { verb: "ui" });
  expect(ui.detail.includes("QuizOption0Button"), "the new player does not see the quiz first");
  expect(!ui.detail.includes("SignButton"), "the registration form is open before the quiz is passed");
  shot("1_question");

  // Every question wrong: the fail page explains each rule and offers another go.
  for (const answer of answers) {
    on(1, { verb: "click", widget: `QuizOption${(answer + 1) % 3}Button` });
    wait(500);
  }
  wait(2500);
  ui = on(1, { verb: "ui" });
  expect(ui.detail.includes("QuizRetryButton"), "the fail page has no Try again button");
  shot("2_rules_after_wrong");

  // Right answers pass; the form is still shut until Continue.
  on(1, { verb: "click", widget: "QuizRetryButton" });
  wait(2000);
  for (const answer of answers) {
    on(1, { verb: "click", widget: `QuizOption${answer}Button` });
    wait(500);
  }
  wait(2500);
  ui = on(1, { verb: "ui" });
  expect(ui.detail.includes("QuizContinueButton"), "the pass page has no Continue button");
  expect(!ui.detail.includes("SignButton"), "the registration form opened before Continue");
  shot("3_passed");
  on(1, { verb: "click", widget: "QuizContinueButton" });
  wait(3000);
  ui = on(1, { verb: "ui" });
  expect(ui.detail.includes("SignButton"), "Continue did not open the registration form");

  if (fails.length) {
    console.log("quiz-proof: FAIL");
    for (const f of fails) console.log(`  ${f}`);
    return 1;
  }
  console.log("quiz-proof: OK");
  return 0;
}

export default { name: "quiz-proof", usage: "quiz-proof [--name quiz]", run: () => runQuizProof() };
