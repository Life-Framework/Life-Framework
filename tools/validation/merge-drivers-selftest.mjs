// Proves the merge drivers on fixture files and a temporary git repo.
// Does not boot the game and does not merge this worktree.
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { mergeStringTables, parseStringTable } from "../merge/union-string-table.mjs";
import { mergeLines } from "../merge/merge-markdown.mjs";
import { regenPlan } from "../merge/regen-generated.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const failures = [];

function fail(message) {
  failures.push(message);
}

function item(id, text, guid = "0000000000000001") {
  return `    CustomStringTableItem "{${guid}}" {\r\n     Id "${id}"\r\n     Target_en_us "${text}"\r\n    }`;
}

function table(blocks) {
  return `StringTable {\r\n Items {\r\n${blocks.join("\r\n")}\r\n }\r\n}\r\n`;
}

const baseTable = table([item("EL-A", "alpha", "0000000000000001")]);
const oursTable = table([
  item("EL-A", "alpha", "0000000000000001"),
  item("EL-B", "bravo", "0000000000000002"),
]);
const theirsTable = table([
  item("EL-A", "alpha", "0000000000000001"),
  item("EL-C", "charlie", "0000000000000003"),
]);
const union = mergeStringTables(baseTable, oursTable, theirsTable);
if (!union.ok) fail(`string union should succeed: ${union.error}`);
else {
  const ids = parseStringTable(union.text).items.map((entry) => entry.id);
  if (ids.join(",") !== "EL-A,EL-B,EL-C") fail(`string union ids were ${ids.join(",")}`);
}
const clash = mergeStringTables(
  baseTable,
  table([item("EL-A", "ours", "0000000000000001")]),
  table([item("EL-A", "theirs", "0000000000000001")]),
);
if (clash.ok) fail("same id with different text should fail");

// Markdown fixtures, CRLF like the checked-out docs.
const md = (...lines) => lines.map((line) => `${line}\r\n`).join("");
const count = (text, needle) => text.split(needle).length - 1;
const jail = (first, third) => md("### `LF_Jail` — `Jail.c`", `- ${first}`, "- second bullet", `- ${third}`, "");
const featureBase = md("## Money", "", "### `EL_A` — `A.c`", "- row a", "");
const featureOurs = featureBase + md("### `EL_B` — `B.c`", "- row b", "");
const featureTheirs = featureBase + md("### `EL_C` — `C.c`", "- row c", "");

// Both-append: each side adds a row at the end of the same section.
const both = mergeLines(featureBase, featureOurs, featureTheirs);
if (!both.ok) fail("both-append should merge clean");
for (const heading of ["### `EL_A`", "### `EL_B`", "### `EL_C`"]) {
  if (count(both.text, heading) !== 1) fail(`both-append should keep ${heading} once:\n${both.text}`);
}

// No-op: only one side changed, or both made the same change.
const oneSide = mergeLines(featureBase, featureBase, featureTheirs);
if (!oneSide.ok || oneSide.text !== featureTheirs) fail("a one-sided change should take that side verbatim");
const same = mergeLines(featureBase, featureOurs, featureOurs);
if (!same.ok || same.text !== featureOurs) fail("identical changes should merge to that text once");

// Duplication: both sides edit different bullets of one row and theirs adds
// a section. The row must appear once with both edits. The old heading-keyed
// driver emitted the row twice here (2026-11-08).
const dupBase = md("## Police", "") + jail("first bullet", "third bullet");
const dupOurs = md("## Police", "") + jail("first bullet, ours edit", "third bullet");
const dupTheirs =
  md("## Police", "") + jail("first bullet", "third bullet, theirs edit") + md("## Hospital", "", "### `LF_Hospital` — `H.c`", "- row h", "");
const dup = mergeLines(dupBase, dupOurs, dupTheirs);
if (!dup.ok) fail(`edits to different lines of one row should merge clean:\n${dup.text}`);
if (count(dup.text, "### `LF_Jail`") !== 1) fail(`a row edited on both sides appears ${count(dup.text, "### `LF_Jail`")} times`);
if (!dup.text.includes("ours edit") || !dup.text.includes("theirs edit")) fail("a side's edit to a shared row was lost");
if (count(dup.text, "## Hospital") !== 1) fail("the section theirs added was not kept once");

// Loss: a heading repeated inside a section (left by an earlier merge, as in
// c96fcfa) must not collapse. Theirs only appends a row. The old driver kept
// one copy per heading and dropped the other copy's lines.
const lossBase = md(
  "## Jobs", "",
  "### `EL_PlayerJobComponent`", "- copy one: job tool grant", "",
  "### `EL_PlayerJobComponent`", "- copy two: paycheck", "",
);
const loss = mergeLines(lossBase, lossBase, lossBase + md("### `EL_C` — `C.c`", "- row c", ""));
if (!loss.text.includes("job tool grant") || !loss.text.includes("paycheck")) fail("a row under a repeated heading was dropped");
if (!loss.text.includes("### `EL_C`")) fail("the row theirs appended was dropped");

// Genuine conflict: both sides rewrite the same line. Markers, never a pick.
const clashMd = mergeLines(featureBase, featureBase.replace("row a", "row a, ours"), featureBase.replace("row a", "row a, theirs"));
if (clashMd.ok) fail("same-line edits should report a conflict");
if (!clashMd.text.includes("<<<<<<< ours") || !clashMd.text.includes("row a, ours") || !clashMd.text.includes("row a, theirs")) {
  fail("a same-line conflict should keep both sides between markers");
}

// Triage table: both sides append a table row.
const rowA = md("| `a` | INFO |");
const tableBase = md("## Triage table", "", "| Message | Verdict |", "|---|---|") + rowA + md("", "## How to add a row");
const tableOurs = tableBase.replace(rowA, rowA + md("| `b` | FIXED |"));
const tableTheirs = tableBase.replace(rowA, rowA + md("| `c` | OPEN |"));
const tableMerge = mergeLines(tableBase, tableOurs, tableTheirs);
if (!tableMerge.ok || !tableMerge.text.includes(rowA + md("| `b` | FIXED |", "| `c` | OPEN |"))) {
  fail(`triage rows appended on both sides should both stay:\n${tableMerge.text}`);
}

const stPath = join(ROOT, "addons", "LifeFramework", "Language", "everonlife_localization.st");
const stText = readFileSync(stPath, "utf8");
const stRound = mergeStringTables(stText, stText, stText);
if (!stRound.ok || stRound.text !== stText) fail("string table does not round-trip");

for (const doc of ["features.md", "debug-log-triage.md"]) {
  const docText = readFileSync(join(ROOT, "docs", doc), "utf8");
  const round = mergeLines(docText, docText, docText);
  if (!round.ok || round.text !== docText) fail(`${doc} does not round-trip`);
}

const locPlan = regenPlan("addons/LifeFramework/Language/everonlife_localization.en_us.conf");
const testPlan = regenPlan("addons/LifeFramework/Scripts/Game/Tests/EL_TestRegistrations.generated.c");
if (!locPlan || !locPlan.script.endsWith("regen-localization.ps1") || !locPlan.args.includes("-Write")) {
  fail("language confs should resolve by regen-localization -Write");
}
if (!testPlan || !testPlan.script.endsWith("gen-test-registry.ps1") || !testPlan.args.includes("-Write")) {
  fail("the test registry should resolve by regen-tests -Write");
}

const attributes = readFileSync(join(ROOT, ".gitattributes"), "utf8");
const driverConfig = readFileSync(join(ROOT, "tools", "merge", "drivers.gitconfig"), "utf8");
for (const [path, driver] of [
  ["addons/LifeFramework/Language/everonlife_localization.st", "lf-string-table"],
  ["addons/LifeFramework/Language/everonlife_localization.*.conf", "lf-regen-localization"],
  ["addons/LifeFramework/Scripts/Game/Tests/EL_TestRegistrations.generated.c", "lf-regen-tests"],
  ["docs/features.md", "lf-markdown"],
  ["docs/debug-log-triage.md", "lf-markdown"],
]) {
  if (!attributes.includes(`${path} merge=${driver}`)) fail(`.gitattributes missing ${path} merge=${driver}`);
  if (!driverConfig.includes(`[merge "${driver}"]`)) fail(`drivers.gitconfig missing ${driver}`);
}

function git(dir, args, env) {
  return spawnSync("git", args, { windowsHide: true, cwd: dir, encoding: "utf8", env: env ?? process.env });
}

function initRepo(dir) {
  mkdirSync(dir, { recursive: true });
  git(dir, ["init", "-q"]);
  git(dir, ["config", "user.email", "merge-selftest@example.com"]);
  git(dir, ["config", "user.name", "merge-selftest"]);
  git(dir, ["config", "core.autocrlf", "false"]);
  git(dir, ["config", "commit.gpgsign", "false"]);
}

function commitAll(dir, message) {
  git(dir, ["add", "-A"]);
  const result = git(dir, ["commit", "-q", "-m", message]);
  if (result.status !== 0) fail(`commit failed in ${dir}: ${result.stderr || result.stdout}`);
}

function configureDriver(dir, name, script) {
  const absolute = join(ROOT, "tools", "merge", script).replace(/\\/g, "/");
  git(dir, ["config", `merge.${name}.name`, name]);
  git(dir, ["config", `merge.${name}.driver`, `node "${absolute}" %O %A %B %P`]);
}

const temp = join(tmpdir(), `lf-merge-drivers-${process.pid}`);
if (existsSync(temp)) rmSync(temp, { recursive: true, force: true });

try {
  const stringRepo = join(temp, "strings");
  initRepo(stringRepo);
  configureDriver(stringRepo, "lf-string-table", "union-string-table.mjs");
  writeFileSync(join(stringRepo, ".gitattributes"), "strings.st merge=lf-string-table\n");
  writeFileSync(join(stringRepo, "strings.st"), baseTable);
  commitAll(stringRepo, "base");
  git(stringRepo, ["checkout", "-q", "-b", "theirs"]);
  writeFileSync(join(stringRepo, "strings.st"), theirsTable);
  commitAll(stringRepo, "theirs");
  git(stringRepo, ["checkout", "-q", "master"]);
  writeFileSync(join(stringRepo, "strings.st"), oursTable);
  commitAll(stringRepo, "ours");
  const merged = git(stringRepo, ["merge", "theirs", "--no-edit"]);
  if (merged.status !== 0) fail(`string-id merge should be clean:\n${merged.stdout}\n${merged.stderr}`);
  else {
    const text = readFileSync(join(stringRepo, "strings.st"), "utf8");
    if (!text.includes('Id "EL-B"') || !text.includes('Id "EL-C"')) fail("git merge did not union string ids");
  }
  git(stringRepo, ["checkout", "-q", "-b", "left", "master"]);
  writeFileSync(join(stringRepo, "strings.st"), table([item("EL-A", "left", "0000000000000001")]));
  commitAll(stringRepo, "left");
  git(stringRepo, ["checkout", "-q", "theirs"]);
  writeFileSync(join(stringRepo, "strings.st"), table([item("EL-A", "right", "0000000000000001"), item("EL-C", "charlie", "0000000000000003")]));
  commitAll(stringRepo, "right");
  const conflicted = git(stringRepo, ["merge", "left", "--no-edit"]);
  if (conflicted.status === 0) fail("different text for one id should conflict");
  else {
    const text = readFileSync(join(stringRepo, "strings.st"), "utf8");
    if (!text.includes("<<<<<<<") || !text.includes('"left"') || !text.includes('"right"')) {
      fail("a string clash should leave both texts between markers");
    }
  }
  git(stringRepo, ["merge", "--abort"]);

  const featureRepo = join(temp, "features");
  initRepo(featureRepo);
  configureDriver(featureRepo, "lf-markdown", "merge-markdown.mjs");
  writeFileSync(join(featureRepo, ".gitattributes"), "docs/features.md merge=lf-markdown\n");
  mkdirSync(join(featureRepo, "docs"), { recursive: true });
  writeFileSync(join(featureRepo, "docs", "features.md"), featureBase);
  commitAll(featureRepo, "base");
  git(featureRepo, ["checkout", "-q", "-b", "theirs"]);
  writeFileSync(join(featureRepo, "docs", "features.md"), featureTheirs);
  commitAll(featureRepo, "theirs");
  git(featureRepo, ["checkout", "-q", "master"]);
  writeFileSync(join(featureRepo, "docs", "features.md"), featureOurs);
  commitAll(featureRepo, "ours");
  const featureMerge = git(featureRepo, ["merge", "theirs", "--no-edit"]);
  if (featureMerge.status !== 0) fail(`feature-row merge should be clean:\n${featureMerge.stdout}\n${featureMerge.stderr}`);
  else {
    const text = readFileSync(join(featureRepo, "docs", "features.md"), "utf8");
    if (!text.includes("### `EL_B`") || !text.includes("### `EL_C`")) fail("git merge did not keep both feature rows");
  }
  git(featureRepo, ["checkout", "-q", "-b", "left", "master"]);
  writeFileSync(join(featureRepo, "docs", "features.md"), featureOurs.replace("row b", "row b, left"));
  commitAll(featureRepo, "left");
  git(featureRepo, ["checkout", "-q", "-b", "right", "master"]);
  writeFileSync(join(featureRepo, "docs", "features.md"), featureOurs.replace("row b", "row b, right"));
  commitAll(featureRepo, "right");
  const featureClash = git(featureRepo, ["merge", "left", "--no-edit"]);
  const clashText = readFileSync(join(featureRepo, "docs", "features.md"), "utf8");
  if (featureClash.status === 0) fail("git should stop on a same-line feature edit");
  else if (!clashText.includes("<<<<<<<") || !clashText.includes("row b, left") || !clashText.includes("row b, right")) {
    fail("the conflicted features.md should hold both sides between markers");
  }
  git(featureRepo, ["merge", "--abort"]);

  const regenRepo = join(temp, "regen");
  initRepo(regenRepo);
  configureDriver(regenRepo, "lf-regen-localization", "regen-generated.mjs");
  configureDriver(regenRepo, "lf-regen-tests", "regen-generated.mjs");
  writeFileSync(
    join(regenRepo, ".gitattributes"),
    "everonlife_localization.en_us.conf merge=lf-regen-localization\nEL_TestRegistrations.generated.c merge=lf-regen-tests\n",
  );
  writeFileSync(join(regenRepo, "everonlife_localization.en_us.conf"), "ours-conf\n");
  writeFileSync(join(regenRepo, "EL_TestRegistrations.generated.c"), "ours-registry\n");
  commitAll(regenRepo, "base");
  git(regenRepo, ["checkout", "-q", "-b", "theirs"]);
  writeFileSync(join(regenRepo, "everonlife_localization.en_us.conf"), "theirs-conf\n");
  writeFileSync(join(regenRepo, "EL_TestRegistrations.generated.c"), "theirs-registry\n");
  commitAll(regenRepo, "theirs");
  git(regenRepo, ["checkout", "-q", "master"]);
  writeFileSync(join(regenRepo, "everonlife_localization.en_us.conf"), "master-conf\n");
  writeFileSync(join(regenRepo, "EL_TestRegistrations.generated.c"), "master-registry\n");
  commitAll(regenRepo, "ours");
  const regenEnv = { ...process.env, LF_MERGE_REGEN_MODE: "plan" };
  const regenMerge = git(regenRepo, ["merge", "theirs", "--no-edit"], regenEnv);
  if (regenMerge.status !== 0) fail(`regen driver merge should be clean:\n${regenMerge.stdout}\n${regenMerge.stderr}`);
  else {
    const conf = readFileSync(join(regenRepo, "everonlife_localization.en_us.conf"), "utf8");
    const registry = readFileSync(join(regenRepo, "EL_TestRegistrations.generated.c"), "utf8");
    if (!conf.startsWith("regen:localization")) fail(`conf driver wrote ${JSON.stringify(conf)}`);
    if (!registry.startsWith("regen:tests")) fail(`registry driver wrote ${JSON.stringify(registry)}`);
  }
} finally {
  rmSync(temp, { recursive: true, force: true });
}

if (failures.length) {
  console.log("merge-drivers-selftest: FAIL");
  for (const failure of failures) console.log(`  - ${failure}`);
  process.exit(1);
}
console.log("merge-drivers-selftest: OK (string ids union, clashes keep both sides in markers, markdown both-append/no-op/duplication/loss/conflict fixtures, generated files call regen)");
