// Proves every orientation-quiz question tests a published rule (#1421).
// inputs: tools/rules/quiz-map.json, addons/LF_Everon1989/Configs/Rules/LF_Quiz_Everon.conf, website/content/rules.json
//   The quiz is a separately written subset of the published rules (decision 2026-11-14): never generated from them,
//   and `tools\cli rules export` never reads quiz keys. tools/rules/quiz-map.json only LINKS them: question id ->
//   the opening words of the published rule it tests. This check is the only reader of the map.
//   1. the real quiz and the real published rules pass: every question id has a map entry, and each entry matches
//      exactly one published rule;
//   2. a quiz id with no map entry fails with one line naming the id;
//   3. a map entry naming a rule that is not published fails with one line naming the id and the rule;
//   4. a snippet that matches two published rules fails (a map entry must pin one rule);
//   5. a map entry for an id the quiz no longer has fails (the map cannot drift).
// red-proof: with tools/rules/quiz-map.json absent, "the real quiz has no problems" failed; with `safezone` missing
// from the map, "an unmapped question is named" failed on the real quiz (2026-11-15, #1421).
import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const QUIZ = join(ROOT, "addons", "LF_Everon1989", "Configs", "Rules", "LF_Quiz_Everon.conf");
const RULES = join(ROOT, "website", "content", "rules.json");
const MAP = join(ROOT, "tools", "rules", "quiz-map.json");

const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};

export const quizIds = (confText) => [...confText.matchAll(/m_sId\s+"([^"]+)"/g)].map((m) => m[1]);

// One line per problem; empty when every question tests exactly one published rule.
export function quizRuleProblems(ids, map, rules) {
  const problems = [];
  for (const id of ids) {
    const snippet = map[id];
    if (typeof snippet !== "string" || !snippet.trim()) {
      problems.push(`quiz question '${id}' has no entry in tools/rules/quiz-map.json: it tests no published rule`);
      continue;
    }
    const hits = rules.filter((r) => r.toLowerCase().startsWith(snippet.trim().toLowerCase()));
    if (hits.length === 0) problems.push(`quiz question '${id}' maps to '${snippet}', which is not a published rule`);
    else if (hits.length > 1) problems.push(`quiz question '${id}' maps to '${snippet}', which matches ${hits.length} published rules`);
  }
  for (const id of Object.keys(map)) {
    if (!ids.includes(id)) problems.push(`tools/rules/quiz-map.json maps '${id}', which is not a quiz question`);
  }
  return problems;
}

const fixtureRules = ["No random violence: hurt someone only for a reason.", "Fear for your life: comply.", "Fear of heights: look down."];
const fixtureOk = { rdm: "No random violence", fear: "Fear for your life" };

check(quizRuleProblems(["rdm", "fear"], fixtureOk, fixtureRules).length === 0, "a fully mapped fixture quiz passes");
const unmapped = quizRuleProblems(["rdm", "fear", "safezone"], fixtureOk, fixtureRules);
check(unmapped.length === 1 && /'safezone'.*no entry/.test(unmapped[0]), `an unmapped question is named (got ${JSON.stringify(unmapped)})`);
const unpublished = quizRuleProblems(["rdm", "fear"], { ...fixtureOk, fear: "Safe zone: no shooting" }, fixtureRules);
check(unpublished.length === 1 && /'fear'.*Safe zone.*not a published rule/.test(unpublished[0]), `a map entry naming an unpublished rule is named (got ${JSON.stringify(unpublished)})`);
const ambiguous = quizRuleProblems(["rdm", "fear"], { ...fixtureOk, fear: "Fear" }, fixtureRules);
check(ambiguous.length === 1 && /'fear'.*2 published rules/.test(ambiguous[0]), `a snippet that matches two rules is refused (got ${JSON.stringify(ambiguous)})`);
const stale = quizRuleProblems(["rdm"], fixtureOk, fixtureRules);
check(stale.length === 1 && /'fear'.*not a quiz question/.test(stale[0]), `a map entry for a removed question is named (got ${JSON.stringify(stale)})`);

check(existsSync(QUIZ), "the pack quiz exists");
check(existsSync(RULES), "website/content/rules.json is committed (run: tools\\cli rules export)");
check(existsSync(MAP), "tools/rules/quiz-map.json exists");
if (existsSync(QUIZ) && existsSync(RULES) && existsSync(MAP)) {
  const ids = quizIds(readFileSync(QUIZ, "utf8"));
  const rules = JSON.parse(readFileSync(RULES, "utf8")).rules || [];
  const map = JSON.parse(readFileSync(MAP, "utf8"));
  check(ids.length > 0, "the pack quiz has questions");
  const problems = quizRuleProblems(ids, map, rules);
  check(problems.length === 0, "the real quiz has no problems");
  for (const p of problems) failures.push(p);
} else {
  failures.push("the real quiz has no problems: a file above is missing");
}

if (failures.length) {
  for (const f of failures) console.error(`quiz-rules-selftest: ${f}`);
  console.error(`SUMMARY quiz-rules-selftest fail ${failures.length}`);
  process.exit(1);
}
console.log("SUMMARY quiz-rules-selftest ok");
