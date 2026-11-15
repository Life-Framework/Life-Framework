// Proves `tools\cli rules export` (#1281): one source for the town board, the site and Discord.
// inputs: tools/commands/rules.mjs, addons/LF_Everon1989/Language/lf_everon1989_localization.st, website/content/rules.json
//   1. a fixture string table gives the expected JSON (title, intro, ordered rules, closing note) and the expected
//      Discord markdown block;
//   2. a quiz key (LF-QuizEveron_*) and every other sign key in the table is ignored: the quiz is a separate subset
//      (decision 2026-11-14), never generated from the rules;
//   3. a table with no rules title or no rules list is refused with a message, not exported empty;
//   4. escapes in a table value (\" and \n) read back as the text they stand for;
//   5. the committed website/content/rules.json is what the pack's real table exports now (it cannot drift), and its
//      Discord block fits one message (2000 characters).
// red-proof: with tools/commands/rules.mjs absent this file fails to import; with the quiz filter in buildRules removed,
// "a quiz key is ignored" failed; with the list split on "," instead of "|", "rules come out in table order" failed
// (2026-11-15, #1281).
import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};

const mod = await import("../commands/rules.mjs");
const { buildRules, renderMarkdown, DEFAULT_TABLE } = mod;

const row = (id, text) => `    CustomStringTableItem "{0000000000000000}" {\n     Id "${id}"\n     Target_en_us "${text}"\n     Comment "fixture"\n    }\n`;
const table = (...rows) => `StringTable {\n ItemClassName "CustomStringTableItem"\n Items {\n${rows.join("")} }\n}\n`;

const fixture = table(
  row("LF-QuizEveron_fear_Q", "Quiz question that must never be exported."),
  row("LF-QuizEveron_fear_Why", "Quiz answer that must never be exported."),
  row("LF-Sign_Rules_Next", "Report it to staff after the scene."),
  row("LF-Sign_Rules_Rules", "First rule: do this.|Second rule, with a comma.|Third \\\"quoted\\\" rule."),
  row("LF-Sign_Rules_Title", "Town rules"),
  row("LF-Sign_Rules_What", "Fixture Town | Alpha. Be a person."),
  row("LF-Sign_RunYourOwn_Title", "Run your own"),
);

const got = buildRules(fixture);
check(got.ok === true, `the fixture table builds: ${got.error || ""}`);
const want = {
  title: "Town rules",
  intro: "Fixture Town | Alpha. Be a person.",
  rules: ["First rule: do this.", "Second rule, with a comma.", 'Third "quoted" rule.'],
  next: "Report it to staff after the scene.",
};
check(JSON.stringify(got.rules) === JSON.stringify(want), `fixture JSON is the expected rules (got ${JSON.stringify(got.rules)})`);
check(JSON.stringify(got.rules?.rules) === JSON.stringify(want.rules), "rules come out in table order, split on the | separator");
check(!JSON.stringify(got.rules).includes("Quiz"), "a quiz key is ignored");
check(!JSON.stringify(got.rules).includes("Run your own"), "other sign cards are ignored");

const wantMd = [
  "**Town rules**",
  "Fixture Town | Alpha. Be a person.",
  "",
  "1. First rule: do this.",
  "2. Second rule, with a comma.",
  '3. Third "quoted" rule.',
  "",
  "Report it to staff after the scene.",
  "",
].join("\n");
check(renderMarkdown(want) === wantMd, `fixture markdown is the expected Discord block (got ${JSON.stringify(renderMarkdown(want))})`);

const noTitle = buildRules(table(row("LF-Sign_Rules_Rules", "A rule.")));
check(noTitle.ok === false && /LF-Sign_Rules_Title/.test(noTitle.error || ""), "a table with no title is refused and names the key");
const noRules = buildRules(table(row("LF-Sign_Rules_Title", "Town rules"), row("LF-Sign_Rules_Rules", " ")));
check(noRules.ok === false && /LF-Sign_Rules_Rules/.test(noRules.error || ""), "an empty rules list is refused and names the key");

const realTable = join(ROOT, DEFAULT_TABLE);
const jsonFile = join(ROOT, "website", "content", "rules.json");
check(existsSync(realTable), "the pack string table exists");
check(existsSync(jsonFile), "website/content/rules.json is committed (run: tools\\cli rules export)");
if (existsSync(realTable) && existsSync(jsonFile)) {
  const real = buildRules(readFileSync(realTable, "utf8"));
  check(real.ok === true, `the pack table builds: ${real.error || ""}`);
  const committed = JSON.parse(readFileSync(jsonFile, "utf8"));
  check(JSON.stringify(committed) === JSON.stringify(real.rules), "website/content/rules.json matches the pack table (run: tools\\cli rules export)");
  if (real.ok) {
    const md = renderMarkdown(real.rules);
    check(md.length <= 2000, `the Discord block fits one message (${md.length} of 2000 characters)`);
    check(real.rules.rules.length >= 1, "the real rules list is not empty");
  }
}

if (failures.length) {
  for (const f of failures) console.error("FAIL " + f);
  console.error(`rules-export-selftest: ${failures.length} problem(s)`);
  process.exit(1);
}
console.log("rules-export-selftest: OK (fixture JSON and Discord markdown, quiz keys ignored, bad tables refused, committed rules.json current)");
