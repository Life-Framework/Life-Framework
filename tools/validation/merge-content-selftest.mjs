// Real git merges through the merge drivers, compared byte for byte with the
// expected union. Covers the 2026-11-14 damage: branches forked before #958
// merged main with the old string-table parser, which ended an item at the
// first `}` of a {term} placeholder, fused it with its neighbour and lost ids.
//
// 1. Two branches append string rows holding {term} placeholders, unescaped
//    quotes, `$200`, `%1`/`%n` format placeholders (also at line end), `|`
//    and long text; one also edits a Comment only.
// 2. Two branches append triage rows with the same characters; a row both
//    sides added at different spots stays once.
// 3. A branch whose own tools/merge still has the old parser merges a branch
//    with the fixed one: run-driver.mjs must run the newer copy.

import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { mergeLines } from "../merge/merge-markdown.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const MERGE_SRC = join(ROOT, "tools", "merge");
const failures = [];
const fail = (message) => failures.push(message);

function git(dir, args) {
  return spawnSync("git", args, { windowsHide: true, cwd: dir, encoding: "utf8" });
}

function initRepo(dir) {
  mkdirSync(dir, { recursive: true });
  git(dir, ["init", "-q", "-b", "master"]);
  for (const [key, value] of [
    ["user.email", "merge-content-selftest@example.com"],
    ["user.name", "merge-content-selftest"],
    ["core.autocrlf", "false"],
    ["commit.gpgsign", "false"],
  ]) git(dir, ["config", key, value]);
}

function commitAll(dir, message) {
  git(dir, ["add", "-A"]);
  const result = git(dir, ["commit", "-q", "-m", message]);
  if (result.status !== 0) fail(`commit "${message}" failed: ${result.stderr || result.stdout}`);
}

// Copies tools/merge into the repo and registers its drivers.gitconfig, so the
// merge runs the same command line a worktree runs.
function installDrivers(dir, transform) {
  const target = join(dir, "tools", "merge");
  mkdirSync(target, { recursive: true });
  for (const name of readdirSync(MERGE_SRC)) {
    writeFileSync(join(target, name), (transform || ((_, text) => text))(name, readFileSync(join(MERGE_SRC, name), "utf8")));
  }
  git(dir, ["config", "--add", "include.path", join(target, "drivers.gitconfig").replace(/\\/g, "/")]);
}

function item(guid, id, lines) {
  return `  CustomStringTableItem "{${guid}}" {\n   Id "${id}"\n${lines.map((line) => `   ${line}\n`).join("")}  }`;
}

function table(items) {
  return `StringTable {\n ItemClassName "CustomStringTableItem"\n Items {\n${items.join("\n")}\n }\n}\n`;
}

const LONG = `You owe {currency:sym}${"1234567890".repeat(30)} to the {Police}; "pay" at the till | or the "{town}" court takes it.`;
const basePolice = item("00000000000000A1", "LF-Base_Police", [
  'Target_en_us "Call {police} now | they owe $200."',
  'Target_de_de "Ruf die {police} | sie schulden 200 $."',
  'Comment "Term "police" (#958): core text writes {police} or {Police}."',
]);
const baseLong = item("00000000000000A2", "LF-Base_Long", [`Target_en_us "${LONG}"`, 'Comment "Long row {x}."']);
const baseLongTheirs = item("00000000000000A2", "LF-Base_Long", [`Target_en_us "${LONG}"`, 'Comment "Long row {x}, comment edited on main."']);
const oursFine = item("00000000000000B1", "LF-Ours_Fine", [
  'Target_en_us "Pay {currency:sym}200 to the {Police} | or go to {town} jail."',
  'Comment "Says "pay" once; {currency:sym} is the symbol, $200 the old literal."',
]);
const oursMoney = item("00000000000000B2", "LF-Ours_Money", [
  'Target_en_us "Item sold for $%1!"',
  'Target_de_de "Behandlung: %1"',
  'Target_fr_fr "%1 paie %2 a %3"',
  'Comment "}"',
]);
const oursFormat = item("00000000000000B3", "LF-Ours_Format", ['Target_en_us "needs %1"', 'Comment "Ends in a format placeholder: %1"']);
const theirsCost = item("00000000000000C1", "LF-Theirs_Cost", [
  'Target_en_us "It costs $%1 {currency:pl} | {medic:pl} are free."',
  'Comment "Term "currency" (#958): plural, {currency:pl}."',
]);
const theirsAfter = item("00000000000000C2", "LF-Theirs_After", ['Target_en_us "A row after the term row, %1 of %2"']);

const baseSt = table([basePolice, baseLong]);
const oursSt = table([basePolice, baseLong, oursFine, oursMoney, oursFormat]);
const theirsSt = table([basePolice, baseLongTheirs, theirsCost, theirsAfter]);
const expectedSt = table([basePolice, baseLongTheirs, oursFine, oursMoney, oursFormat, theirsCost, theirsAfter]);

const triageHead = "# Debug log triage\n\n| Message | Verdict | Notes |\n|---|---|---|\n| `[LFDebug:A] a` | INFO | base row |\n";
const triageTail = "\n## How to add a row\n\nAppend it.\n";
const oursRow = '| `[LFDebug:Terms] {police} -> "Sheriff"` | BY DESIGN | #958: costs $200 \\| a pipe, {currency:sym}. |\n';
const theirsRow = "| `[LFDebug:Shop] sold for $%1 {x}` | EXPECTED | #1026: `}` and `{` in a row, \"quoted\", ends in %1 |\n";
const triageBase = triageHead + triageTail;
const triageOurs = triageHead + oursRow + triageTail;
const triageTheirs = triageHead + theirsRow + triageTail;
const triageExpected = triageHead + oursRow + theirsRow + triageTail;

// Both sides add the same row at different spots (a branch that copied main's
// rows by hand, then merged main): one copy stays.
const sharedRow = "| `[LFDebug:Placement] refused: %1` | BY DESIGN | #1026: the shared row. |\n";
const secondRow = "| `[LFDebug:B] b` | INFO | second base row |\n";
const doubleBase = triageHead + secondRow + triageTail;
const doubleOurs = triageHead + sharedRow + secondRow + triageTail;
const doubleTheirs = triageHead + secondRow + sharedRow + triageTail;
const doubleMerge = mergeLines(doubleBase, doubleOurs, doubleTheirs);
if (!doubleMerge.ok || doubleMerge.text !== doubleOurs) {
  fail(`a row both sides added should stay once:\n${doubleMerge.text}`);
}

const ST = "everonlife_localization.st";
const TRIAGE = "debug-log-triage.md";
const ATTRIBUTES = `${ST} merge=lf-string-table\n${TRIAGE} merge=lf-markdown\n`;

function firstDifference(actual, expected) {
  let i = 0;
  while (i < actual.length && actual[i] === expected[i]) i++;
  return `first difference at byte ${i}: got ${JSON.stringify(actual.slice(i, i + 80))}, want ${JSON.stringify(expected.slice(i, i + 80))}`;
}

// baseDrivers rewrites the driver copy committed at the fork point; theirs
// then commits the current copy, so ours keeps the old one.
function mergeCase(name, baseDrivers) {
  const dir = join(temp, name);
  initRepo(dir);
  installDrivers(dir, baseDrivers);
  writeFileSync(join(dir, ".gitattributes"), ATTRIBUTES);
  writeFileSync(join(dir, ST), baseSt);
  writeFileSync(join(dir, TRIAGE), triageBase);
  commitAll(dir, "base");
  git(dir, ["checkout", "-q", "-b", "theirs"]);
  writeFileSync(join(dir, ST), theirsSt);
  writeFileSync(join(dir, TRIAGE), triageTheirs);
  commitAll(dir, "theirs: main's rows");
  if (baseDrivers) {
    installDrivers(dir);
    commitAll(dir, "theirs: fixed driver");
  }
  git(dir, ["checkout", "-q", "master"]);
  writeFileSync(join(dir, ST), oursSt);
  writeFileSync(join(dir, TRIAGE), triageOurs);
  commitAll(dir, "ours: branch rows");
  const merge = git(dir, ["merge", "--no-edit", "theirs"]);
  if (merge.status !== 0) {
    fail(`${name}: merge stopped (exit ${merge.status}): ${(merge.stdout + merge.stderr).trim().slice(0, 400)}`);
    return;
  }
  const st = readFileSync(join(dir, ST), "utf8");
  if (st !== expectedSt) fail(`${name}: string table is not the union; ${firstDifference(st, expectedSt)}`);
  const triage = readFileSync(join(dir, TRIAGE), "utf8");
  if (triage !== triageExpected) fail(`${name}: triage doc is not the union; ${firstDifference(triage, triageExpected)}`);
}

// The driver every branch forked before #958 carries: the `[^}]*` item
// parser and no coverage check.
const OLD_ITEM_RE = 'const ITEM_RE = /CustomStringTableItem\\s+"\\{[0-9A-Fa-f]+\\}"\\s*\\{[^}]*\\}/g;';
const COVERAGE_HEAD = "export function coverageError(parsed, label) {";
function oldParser(name, text) {
  if (name !== "union-string-table.mjs") return text;
  const lines = text.split("\n");
  const at = lines.findIndex((line) => line.startsWith("const ITEM_RE ="));
  if (at < 0) throw new Error("union-string-table.mjs has no ITEM_RE line; update oldParser");
  lines[at] = OLD_ITEM_RE;
  return lines.join("\n").replace(COVERAGE_HEAD, `${COVERAGE_HEAD}\n  return null;`);
}

const temp = join(tmpdir(), `lf-merge-content-${process.pid}`);
if (existsSync(temp)) rmSync(temp, { recursive: true, force: true });
try {
  mergeCase("same-driver");
  mergeCase("stale-ours", oldParser);
} finally {
  rmSync(temp, { recursive: true, force: true });
}

if (failures.length) {
  console.log("merge-content-selftest: FAIL");
  for (const failure of failures) console.log(`  - ${failure}`);
  process.exit(1);
}
console.log("merge-content-selftest: OK (git merges of string rows with {term}, %1, quotes, $ and | and of triage rows equal the union byte for byte; a branch with the old parser runs the newer driver)");
