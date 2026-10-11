// Proves the merge drivers on fixture files and a temporary git repo.
// Does not boot the game and does not merge this worktree.
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { mergeStringTables, parseStringTable } from "../merge/union-string-table.mjs";
import { mergeFeatures } from "../merge/union-features.mjs";
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

const featureBase = "## Money\r\n\r\n### `EL_A` — `A.c`\r\n- row a\r\n";
const featureOurs = "## Money\r\n\r\n### `EL_A` — `A.c`\r\n- row a\r\n\r\n### `EL_B` — `B.c`\r\n- row b\r\n";
const featureTheirs = "## Money\r\n\r\n### `EL_A` — `A.c`\r\n- row a\r\n\r\n### `EL_C` — `C.c`\r\n- row c\r\n";
const features = mergeFeatures(featureBase, featureOurs, featureTheirs);
if (!features.text.includes("### `EL_B`") || !features.text.includes("### `EL_C`")) {
  fail("feature merge dropped a row added on one side");
}
if (!features.text.includes("### `EL_A`")) fail("feature merge dropped the shared row");

const stPath = join(ROOT, "addons", "LifeFramework", "Language", "everonlife_localization.st");
const stText = readFileSync(stPath, "utf8");
const stRound = mergeStringTables(stText, stText, stText);
if (!stRound.ok || stRound.text !== stText) fail("string table does not round-trip");

const featuresPath = join(ROOT, "docs", "features.md");
const featuresText = readFileSync(featuresPath, "utf8");
const featuresRound = mergeFeatures(featuresText, featuresText, featuresText);
if (featuresRound.text !== featuresText) fail("features.md does not round-trip");

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
  ["docs/features.md", "lf-features"],
]) {
  if (!attributes.includes(`${path} merge=${driver}`)) fail(`.gitattributes missing ${path} merge=${driver}`);
  if (!driverConfig.includes(`[merge "${driver}"]`)) fail(`drivers.gitconfig missing ${driver}`);
}

function git(dir, args, env) {
  return spawnSync("git", args, { cwd: dir, encoding: "utf8", env: env ?? process.env });
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

  const featureRepo = join(temp, "features");
  initRepo(featureRepo);
  configureDriver(featureRepo, "lf-features", "union-features.mjs");
  writeFileSync(join(featureRepo, ".gitattributes"), "docs/features.md merge=lf-features\n");
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
console.log("merge-drivers-selftest: OK (string ids union, text clash conflicts, both feature rows kept, generated files call regen)");
