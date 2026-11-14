// Proves the merge drivers on fixture files and a temporary git repo.
// inputs: tools/**, agent-box/**, .githooks/**
// Does not boot the game and does not merge this worktree.
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { mergeStringTables, parseStringTable } from "../merge/union-string-table.mjs";
import { mergeLines } from "../merge/merge-markdown.mjs";
import { mergeJsonKeys } from "../merge/merge-json-keys.mjs";
import { regenPlan } from "../merge/regen-generated.mjs";
import { mergeLintBaselines } from "../merge/union-lint-baseline.mjs";

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
const featureBase = md("## Money", "", "### `LF_A` — `A.c`", "- row a", "");
const featureOurs = featureBase + md("### `LF_B` — `B.c`", "- row b", "");
const featureTheirs = featureBase + md("### `LF_C` — `C.c`", "- row c", "");

// Both-append: each side adds a row at the end of the same section.
const both = mergeLines(featureBase, featureOurs, featureTheirs);
if (!both.ok) fail("both-append should merge clean");
for (const heading of ["### `LF_A`", "### `LF_B`", "### `LF_C`"]) {
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
  "### `LF_PlayerJobComponent`", "- copy one: job tool grant", "",
  "### `LF_PlayerJobComponent`", "- copy two: paycheck", "",
);
const loss = mergeLines(lossBase, lossBase, lossBase + md("### `LF_C` — `C.c`", "- row c", ""));
if (!loss.text.includes("job tool grant") || !loss.text.includes("paycheck")) fail("a row under a repeated heading was dropped");
if (!loss.text.includes("### `LF_C`")) fail("the row theirs appended was dropped");

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

const lintText = readFileSync(join(ROOT, "tools", "validation", "enforce-lint-baseline.json"), "utf8");
const lintRound = mergeLintBaselines(lintText, lintText, lintText);
if (!lintRound.ok || lintRound.text !== lintText) fail("enforce-lint-baseline.json does not round-trip");

const locPlan = regenPlan("addons/LifeFramework/Language/everonlife_localization.en_us.conf");
const testPlan = regenPlan("addons/LifeFramework/Scripts/Game/Tests/LF_TestRegistrations.generated.c");
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
  ["addons/LifeFramework/Scripts/Game/Tests/LF_TestRegistrations.generated.c", "lf-regen-tests"],
  ["docs/features.md", "lf-markdown"],
  ["docs/debug-log-triage.md", "lf-markdown"],
  ["tools/validation/enforce-lint-baseline.json", "lf-lint-baseline"],
  ["docs/asset-inventory.md", "lf-table-rows"],
  ["docs/playtest.md", "lf-markdown"],
  ["website/content/gallery.json", "lf-json-keys"],
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
    if (!text.includes("### `LF_B`") || !text.includes("### `LF_C`")) fail("git merge did not keep both feature rows");
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

  // Lint baseline: two branches each fix different debt, so each deletes a
  // different adjacent entry. A line merge conflicts on the shared hunk; the
  // driver must keep both removals and the entry theirs accepted.
  const lintRepo = join(temp, "lint");
  initRepo(lintRepo);
  configureDriver(lintRepo, "lf-lint-baseline", "union-lint-baseline.mjs");
  writeFileSync(join(lintRepo, ".gitattributes"), "enforce-lint-baseline.json merge=lf-lint-baseline\n");
  const entry = (rule, file, text) => ({ rule, file, text });
  const lintA = entry("L1", "A.c", "a();");
  const lintB = entry("L1", "B.c", "b();");
  const lintC = entry("L1", "C.c", "c();");
  const lintD = entry("L2", "D.c", "d();");
  const lintE = entry("L1", "E.c", "e();");
  const lintJson = (entries) => JSON.stringify(entries, null, 1) + "\n";
  const lintPath = join(lintRepo, "enforce-lint-baseline.json");
  writeFileSync(lintPath, lintJson([lintA, lintB, lintC, lintD]));
  commitAll(lintRepo, "base");
  git(lintRepo, ["checkout", "-q", "-b", "theirs"]);
  writeFileSync(lintPath, lintJson([lintA, lintB, lintD, lintE]));
  commitAll(lintRepo, "theirs fixes C, accepts E");
  git(lintRepo, ["checkout", "-q", "master"]);
  writeFileSync(lintPath, lintJson([lintA, lintC, lintD]));
  commitAll(lintRepo, "ours fixes B");
  const lintMerge = git(lintRepo, ["merge", "theirs", "--no-edit"]);
  if (lintMerge.status !== 0) fail(`lint-baseline merge should be clean:\n${lintMerge.stdout}\n${lintMerge.stderr}`);
  else {
    const merged = readFileSync(lintPath, "utf8");
    if (merged !== lintJson([lintA, lintE, lintD])) fail(`lint-baseline merge wrote ${JSON.stringify(merged)}`);
  }

  // Keyed table and JSON drivers: each proves a clean double-append and a
  // same-key clash in a scratch repo with the real git merge.
  function proveDriver({ name, script, file, base, sides, clash }) {
    const repo = join(temp, name);
    initRepo(repo);
    configureDriver(repo, name, script);
    writeFileSync(join(repo, ".gitattributes"), `${file} merge=${name}
`);
    writeFileSync(join(repo, file), base);
    commitAll(repo, "base");
    git(repo, ["checkout", "-q", "-b", "theirs"]);
    writeFileSync(join(repo, file), sides.theirs);
    commitAll(repo, "theirs");
    git(repo, ["checkout", "-q", "master"]);
    writeFileSync(join(repo, file), sides.ours);
    commitAll(repo, "ours");
    const clean = git(repo, ["merge", "theirs", "--no-edit"]);
    if (clean.status !== 0) fail(`${name}: double append should merge clean:
${clean.stdout}
${clean.stderr}`);
    else {
      const text = readFileSync(join(repo, file), "utf8");
      for (const needle of sides.expect) if (!text.includes(needle)) fail(`${name}: merged file lost ${needle}`);
    }
    git(repo, ["checkout", "-q", "-b", "left", "master~1"]);
    writeFileSync(join(repo, file), clash.left);
    commitAll(repo, "left");
    git(repo, ["checkout", "-q", "-b", "right", "master~1"]);
    writeFileSync(join(repo, file), clash.right);
    commitAll(repo, "right");
    const stopped = git(repo, ["merge", "left", "--no-edit"]);
    const text = readFileSync(join(repo, file), "utf8");
    if (stopped.status === 0) fail(`${name}: same row/key changed on both sides should conflict`);
    else if (!text.includes("<<<<<<<") || !text.includes(clash.markLeft) || !text.includes(clash.markRight)) {
      fail(`${name}: conflict should hold both sides between markers, got ${JSON.stringify(text)}`);
    } else if (clash.keep && !text.includes(clash.keep)) fail(`${name}: unrelated entry ${clash.keep} was lost`);
    git(repo, ["merge", "--abort"]);
  }

  const inv = (...rows) => md("# Inventory", "", "| Item | Prefab | Status |", "|---|---|---|", ...rows, "");
  const invBase = inv("| Pager | `LF_Pager.et` | placeholder |");
  proveDriver({
    name: "lf-table-rows",
    script: "merge-table-rows.mjs",
    file: "asset-inventory.md",
    base: invBase,
    sides: {
      ours: inv("| Pager | `LF_Pager.et` | placeholder |", "| Still | `LF_Still.et` | placeholder |"),
      theirs: inv("| Pager | `LF_Pager.et` | placeholder |", "| Drill | `LF_Drill.et` | placeholder |"),
      expect: ["LF_Still.et", "LF_Drill.et", "LF_Pager.et"],
    },
    clash: {
      // Both sides add a row for the same prefab with different text, and one
      // side also adds an unrelated row that must survive.
      left: inv("| Pager | `LF_Pager.et` | placeholder |", "| Van | `LF_Van.et` | placeholder |", "| Tray | `LF_Tray.et` | final |"),
      right: inv("| Pager | `LF_Pager.et` | placeholder |", "| Van | `LF_Van.et` | final |"),
      markLeft: "| Van | `LF_Van.et` | placeholder |",
      markRight: "| Van | `LF_Van.et` | final |",
      keep: "LF_Tray.et",
    },
  });
  const gal = (captions, skip = []) => JSON.stringify({ sections: [{ id: "town", title: "Town" }], captions, skip }, null, 2) + "\n";
  proveDriver({
    name: "lf-json-keys",
    script: "merge-json-keys.mjs",
    file: "gallery.json",
    base: gal({ "1/a": "A" }),
    sides: {
      ours: gal({ "1/a": "A", "2/b": "B" }, ["x.png"]),
      theirs: gal({ "1/a": "A", "3/c": "C" }, ["y.png"]),
      expect: ['"2/b": "B"', '"3/c": "C"', "x.png", "y.png"],
    },
    clash: {
      left: gal({ "1/a": "A left", "9/z": "Z" }),
      right: gal({ "1/a": "A right" }),
      markLeft: '"1/a": "A left"',
      markRight: '"1/a": "A right"',
      keep: '"9/z": "Z"',
    },
  });
  const galleryText = readFileSync(join(ROOT, "website", "content", "gallery.json"), "utf8");
  const galleryRound = mergeJsonKeys(galleryText, galleryText, galleryText);
  if (!galleryRound.ok || galleryRound.text !== galleryText) fail("gallery.json is not in the json-keys house layout");
  const playtestText = readFileSync(join(ROOT, "docs", "playtest.md"), "utf8");
  const playtestAppend = mergeLines(playtestText, playtestText + "- ours line\n", playtestText + "- theirs line\n");
  if (!playtestAppend.ok || !playtestAppend.text.includes("ours line") || !playtestAppend.text.includes("theirs line")) {
    fail("playtest.md both-append should keep both lines under lf-markdown");
  }

  const regenRepo = join(temp, "regen");
  initRepo(regenRepo);
  configureDriver(regenRepo, "lf-regen-localization", "regen-generated.mjs");
  configureDriver(regenRepo, "lf-regen-tests", "regen-generated.mjs");
  writeFileSync(
    join(regenRepo, ".gitattributes"),
    "everonlife_localization.en_us.conf merge=lf-regen-localization\nLF_TestRegistrations.generated.c merge=lf-regen-tests\n",
  );
  writeFileSync(join(regenRepo, "everonlife_localization.en_us.conf"), "ours-conf\n");
  writeFileSync(join(regenRepo, "LF_TestRegistrations.generated.c"), "ours-registry\n");
  commitAll(regenRepo, "base");
  git(regenRepo, ["checkout", "-q", "-b", "theirs"]);
  writeFileSync(join(regenRepo, "everonlife_localization.en_us.conf"), "theirs-conf\n");
  writeFileSync(join(regenRepo, "LF_TestRegistrations.generated.c"), "theirs-registry\n");
  commitAll(regenRepo, "theirs");
  git(regenRepo, ["checkout", "-q", "master"]);
  writeFileSync(join(regenRepo, "everonlife_localization.en_us.conf"), "master-conf\n");
  writeFileSync(join(regenRepo, "LF_TestRegistrations.generated.c"), "master-registry\n");
  commitAll(regenRepo, "ours");
  const regenEnv = { ...process.env, LF_MERGE_REGEN_MODE: "plan" };
  const regenMerge = git(regenRepo, ["merge", "theirs", "--no-edit"], regenEnv);
  if (regenMerge.status !== 0) fail(`regen driver merge should be clean:\n${regenMerge.stdout}\n${regenMerge.stderr}`);
  else {
    const conf = readFileSync(join(regenRepo, "everonlife_localization.en_us.conf"), "utf8");
    const registry = readFileSync(join(regenRepo, "LF_TestRegistrations.generated.c"), "utf8");
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
console.log("merge-drivers-selftest: OK (string ids union, clashes keep both sides in markers, markdown both-append/no-op/duplication/loss/conflict fixtures, generated files call regen, lint baseline keeps both sides' fixes, keyed table rows and gallery JSON keys union with per-key conflicts)");
