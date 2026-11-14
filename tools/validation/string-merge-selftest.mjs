// inputs: tools/merge/**, tools/validation/string-merge-selftest.mjs
// Two PRs that add strings merge on plain git, the way GitHub merges them: no merge drivers.
//
// GitHub cannot run tools/merge/*. When every tool appended keys before the closing braces, two
// string PRs inserted at the same line of the .st and of every generated .conf and conflicted.
// The .st is now kept sorted by Id (tools/merge/sort-string-table.mjs) and the confs follow it, so
// keys of different features land in different gaps. This runs that merge in a scratch repo with
// no driver config and checks:
//   1. the merge is clean for the .st and the derived conf (Ids and Texts arrays);
//   2. the merged .st is still canonical and holds both sides' keys;
//   3. the merged conf equals the conf derived from the merged .st (Ids and Texts stay paired);
//   4. canonicalStringTable is idempotent and refuses a table that does not parse whole.

import { spawnSync } from "node:child_process";
import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { canonicalStringTable } from "../merge/sort-string-table.mjs";
import { parseStringTable } from "../merge/union-string-table.mjs";

const failures = [];
const fail = (m) => failures.push(m);

function git(dir, args) {
  return spawnSync("git", args, { windowsHide: true, cwd: dir, encoding: "utf8" });
}

function item(guid, id, text, indent = "  ") {
  return `${indent}CustomStringTableItem "{${guid}}" {\n${indent} Id "${id}"\n${indent} Target_en_us "${text}"\n${indent}}`;
}

function table(items) {
  return `StringTable {\n ItemClassName "CustomStringTableItem"\n Items {\n${items.join("\n")}\n  }\n }\n}\n`;
}

// What a tool does: append before the closing braces, then regen-localization sorts.
function appendAndSort(st, items) {
  const tail = st.lastIndexOf("\n  }\n }\n}");
  const appended = `${st.slice(0, tail)}\n${items.join("\n")}${st.slice(tail)}`;
  const res = canonicalStringTable(appended);
  if (!res.ok) throw new Error(res.error);
  return res.text;
}

// The shape regen-localization.ps1 writes: Ids then position-parallel Texts, in .st order.
function conf(st) {
  const items = parseStringTable(st).items.map((it) => ({ id: it.id, text: /Target_en_us "([^"]*)"/.exec(it.block)[1] }));
  return `StringTableRuntime {\r\n Ids {${items.map((i) => `\r\n  "${i.id}"`).join("")}\r\n }\r\n Texts {${items.map((i) => `\r\n  "${i.text}"`).join("")}\r\n }\r\n}\r\n`;
}

const base = [];
for (const [n, feature] of ["Atm", "Bank", "Fishing", "Garage", "Jail", "Medic", "Police", "Shop", "Taxi", "Zone"].entries()) {
  base.push(item(`00000000000000${String(n).padStart(2, "0")}`, `LF-${feature}_Title`, `${feature} title`));
  base.push(item(`00000000000001${String(n).padStart(2, "0")}`, `LF-${feature}_Close`, "Close"));
}
const baseSt = canonicalStringTable(table(base)).text;
// Branch A adds a feature between Bank and Fishing; branch B adds one between Medic and Police.
// Both texts repeat a neighbour's ("Close") to stress the Texts-array diff.
const oursSt = appendAndSort(baseSt, [
  item("000000000000A001", "LF-Bakery_Open", "Open the bakery", "    "),
  item("000000000000A002", "LF-Bakery_Close", "Close"),
]);
const theirsSt = appendAndSort(baseSt, [
  item("000000000000B001", "LF-Mine_Dig", "Dig"),
  item("000000000000B002", "LF-Mine_Close", "Close"),
]);

const dir = join(tmpdir(), `lf-string-merge-${process.pid}-${Date.now()}`);
try {
  mkdirSync(dir, { recursive: true });
  git(dir, ["init", "-q", "-b", "master"]);
  for (const [k, v] of [["user.email", "string-merge-selftest@example.com"], ["user.name", "string-merge-selftest"], ["core.autocrlf", "false"], ["commit.gpgsign", "false"]]) git(dir, ["config", k, v]);
  // The repo's own attributes for these files, so -text and the (undefined here) drivers apply as on GitHub.
  writeFileSync(join(dir, ".gitattributes"), "* text=auto eol=lf\n*.conf -text\n*.st merge=lf-string-table\n*.conf merge=lf-regen-localization\n");
  const write = (st) => {
    writeFileSync(join(dir, "t.st"), st);
    writeFileSync(join(dir, "t.en_us.conf"), conf(st));
  };
  write(baseSt);
  git(dir, ["add", "-A"]);
  git(dir, ["commit", "-q", "-m", "base"]);
  git(dir, ["checkout", "-q", "-b", "theirs"]);
  write(theirsSt);
  git(dir, ["commit", "-q", "-am", "theirs"]);
  git(dir, ["checkout", "-q", "master"]);
  write(oursSt);
  git(dir, ["commit", "-q", "-am", "ours"]);
  const merge = git(dir, ["merge", "--no-edit", "-q", "theirs"]);
  if (merge.status !== 0) {
    fail(`plain git merge of two string PRs conflicted: ${(merge.stdout + merge.stderr).trim().split("\n").slice(0, 3).join(" | ")}`);
  } else {
    const mergedSt = readFileSync(join(dir, "t.st"), "utf8");
    const canon = canonicalStringTable(mergedSt);
    if (!canon.ok || canon.text !== mergedSt) fail("the merged .st is not sorted by Id");
    const ids = parseStringTable(mergedSt).items.map((i) => i.id);
    for (const id of ["LF-Bakery_Open", "LF-Bakery_Close", "LF-Mine_Dig", "LF-Mine_Close"]) if (!ids.includes(id)) fail(`merged .st lost ${id}`);
    if (readFileSync(join(dir, "t.en_us.conf"), "utf8") !== conf(mergedSt)) fail("the merged conf is not the conf of the merged .st (Ids and Texts out of step)");
  }
} finally {
  rmSync(dir, { recursive: true, force: true });
}

const again = canonicalStringTable(oursSt);
if (!again.ok || again.text !== oursSt) fail("canonicalStringTable is not idempotent");
const broken = table([item("00000000000000F1", "LF-A_A", "a").replace(/\n  \}$/, ""), item("00000000000000F2", "LF-B_B", "b")]);
if (canonicalStringTable(broken).ok) fail("canonicalStringTable accepted an item without its closing brace");

if (failures.length) {
  console.log("string-merge-selftest: FAIL");
  for (const f of failures) console.log(`  - ${f}`);
  process.exit(1);
}
console.log("string-merge-selftest: OK (two string PRs merge on plain git with no drivers; the merged .st stays sorted and its conf stays paired)");
