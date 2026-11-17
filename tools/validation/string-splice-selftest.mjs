// tools/validation/string-splice-selftest.mjs - validate-string-splice (#2673).
// inputs: tools/validation/validate-string-splice.mjs, tools/lib/string-splice.mjs, tools/validation/string-splice-selftest.mjs, tools/validation/fixtures/string-splice/**
//
// Scans tools/validation/fixtures/string-splice/tree (a bad script, a good script, a Tests/ script) and checks:
//   - every planted shape is found (Substring + splice, += tail, payload tail, payload Format);
//   - bounded lengths, row-name tails, comments, strings, log-line Formats, `// lf-bounded:` marks and Tests/ pass;
//   - the baseline ratchet: a listed site passes, an unlisted one fails, a fixed one is stale, no reason fails;
//   - the real baseline holds only entries with a reason, and the real tree is clean against it.
//
// red-proof: 2026-11-17 with the `splice` branch of scanSource disabled, "bad: Stamp is a splice" printed FAIL and
// the run exited 1; restored, it prints "string-splice-selftest: OK".
import { spawnSync } from "node:child_process";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { boundedLength, compare, keyHits, scanSource, strip } from "../lib/string-splice.mjs";
import { scanTree } from "./validate-string-splice.mjs";

let failed = 0;
const check = (ok, msg) => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${msg}`);
  if (!ok) failed++;
};

const here = dirname(fileURLToPath(import.meta.url));
const tree = join(here, "fixtures", "string-splice", "tree");
const bad = "LifeFramework/Scripts/Game/Core/LF_SpliceBad.c";
const good = "LifeFramework/Scripts/Game/Core/LF_SpliceGood.c";

const { found, sites } = scanTree(tree);
const at = (file, method, kind) => sites.filter((s) => s.file === file && s.method === method && s.kind === kind).length;

check(at(bad, "Stamp", "splice") === 2, "bad: Stamp is a splice (two Substring calls joined with +)");
check(at(bad, "Append", "splice") === 1, "bad: Append is a splice (`result += text.Substring(i, length - i)`)");
check(at(bad, "Rest", "tail") === 1, "bad: Rest is a tail of a payload-named string");
check(at(bad, "Wrap", "format") === 1, "bad: Wrap is a string.Format with a payload argument");
check(sites.filter((s) => s.file === bad).length === 5, `bad: exactly 5 sites, got ${sites.filter((s) => s.file === bad).length}`);
check(sites.filter((s) => s.file === good).length === 0, `good: no site, got ${sites.filter((s) => s.file === good).map((s) => `${s.method}:${s.line}`)}`);
check(sites.every((s) => !s.file.includes("/Tests/")), "Tests/ is not scanned");

// The scanner's pieces.
check(boundedLength("3") && boundedLength(" MAX_TEXT ") && boundedLength("LF_Foo.MAX_TEXT") && boundedLength("Math.Min(n, CAP)"), "boundedLength: number, CONSTANT, Math.Min with a cap");
check(!boundedLength("len - i") && !boundedLength("name.Length() - 1") && !boundedLength("Math.Min(a, b.Length())"), "boundedLength: a computed length is not a cap");
check(strip('a // b.Substring(0, i) + c\nd "x + y" /* z */').split("\n").every((l) => !l.includes("Substring") && !l.includes("x + y")), "strip blanks comments and strings");
check(scanSource("class A { void M(string s, int i) { string t = s.Substring(i, s.Length() - i) + s; } }").length === 1, "a one-line splice is found");
check(scanSource("class A {\n void M(string s, int i)\n {\n  // lf-bounded: s is an id\n  string t = s.Substring(0, i) + s;\n }\n}").length === 0, "lf-bounded on the line above silences a site");

// The ratchet.
const k = `${bad}::Stamp::splice`;
const all = Object.fromEntries([...found].map(([key, count]) => [key, { count, reason: "a fixture site kept on purpose" }]));
check(compare(found, all).fresh.length === 0 && compare(found, all).stale.length === 0, "ratchet: a fully listed baseline passes");
const missing = { ...all };
delete missing[k];
check(compare(found, missing).fresh.map((f) => f.key).join() === k, "ratchet: a site missing from the baseline fails");
check(compare(found, { ...all, [k]: { count: 1, reason: "one of two" } }).fresh.length === 1, "ratchet: more sites than the baseline allows fails");
check(compare(found, { ...all, [k]: { count: 3, reason: "one fixed since" } }).stale.length === 1, "ratchet: a fixed site is stale until the entry shrinks");
check(compare(found, { ...all, "Core/Gone.c::Old::splice": { count: 1, reason: "file deleted since" } }).stale.length === 1, "ratchet: an entry for a vanished site is stale");
check(compare(found, { ...all, [k]: { count: 2, reason: "" } }).noReason.length === 1, "ratchet: an entry with no reason fails");

// The command line: the fixture tree fails with an empty baseline, passes with a full one.
const run = (baseline) => spawnSync(process.execPath, [join(here, "validate-string-splice.mjs")], {
  env: { ...process.env, LF_SPLICE_ROOT: tree, LF_SPLICE_BASELINE: baseline },
  encoding: "utf8",
});
const empty = run(join(here, "fixtures", "string-splice", "empty.json"));
check(empty.status === 1 && /FAIL addons\/LifeFramework\/Scripts\/Game\/Core\/LF_SpliceBad\.c:\d+ Stamp \[splice\]/.test(empty.stdout), "cli: the fixture tree fails an empty baseline, naming file:line");
const full = run(join(here, "fixtures", "string-splice", "full.json"));
check(full.status === 0, `cli: the fixture tree passes its full baseline (${full.status}: ${full.stdout.trim().split("\n").pop()})`);

// The real baseline and tree.
const real = JSON.parse(readFileSync(join(here, "string-splice-baseline.json"), "utf8"));
const bare = Object.entries(real).filter(([, b]) => typeof b.reason !== "string" || b.reason.trim().length < 12).map(([key]) => key);
check(bare.length === 0, `baseline: every entry has a reason (${bare})`);
const repo = scanTree(join(here, "..", ".."));
const verdict = compare(repo.found, real);
check(verdict.fresh.length === 0, `repo: no site outside the baseline (${verdict.fresh.map((f) => f.key)})`);
check(verdict.stale.length === 0, `repo: no stale baseline entry (${verdict.stale.map((f) => f.key)})`);
check(keyHits("x", []).size === 0, "keyHits of nothing is empty");

if (failed) {
  console.log(`string-splice-selftest: ${failed} check(s) failed`);
  process.exit(1);
}
console.log("string-splice-selftest: OK (planted splices red, bounded forms green, ratchet holds)");
