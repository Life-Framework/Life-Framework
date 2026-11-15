// tools/validation/triage-draft-selftest.mjs
// inputs: tools/host/triage-draft.mjs, tools/host/commands/triage.mjs, tools/lib/log-triage.mjs, tools/validation/fixtures/log-triage/**
//
// Proves `host triage --draft`: the log-triage fixture (2 new groups, 3 known) yields exactly two
// drafts with the expected titles, counts, first times and suspected features; the CLI writes
// `<n>.md` per new group, prints one `gh issue create` command each, and never files anything
// (the module imports no child_process; the selftest never puts `gh` on PATH). No game, no network.
// red-proof: before tools/host/triage-draft.mjs existed this stopped at import time:
// "Cannot find module '.../tools/host/triage-draft.mjs'" (2026-11-15, #1270).

import { dirname, join, resolve } from "node:path";
import { mkdtempSync, readFileSync, readdirSync, existsSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FIX = join(ROOT, "tools", "validation", "fixtures", "log-triage");
const { parseTriageTable, triageLog } = await import("../lib/log-triage.mjs");
const { draftGroups, ghCreateCommand } = await import("../host/triage-draft.mjs");

let failed = 0;
function fail(msg) {
  console.log(`FAIL  ${msg}`);
  failed++;
}

const logText = readFileSync(join(FIX, "console.log"), "utf8");
const rows = parseTriageTable(readFileSync(join(FIX, "triage.md"), "utf8"));
const { groups } = triageLog(logText, rows);

// Pure layer: two drafts, most frequent first, with the expected titles and fields.
const drafts = draftGroups(logText, groups);
const expect = [
  {
    title: "Log triage: Foo: Can't find variable 'm_Frobnicator' in class 'LF_FooComponent' (Scripts/Game/Feature/Foo/LF_FooComponent.c:N)",
    feature: "Foo",
    count: 2,
    firstTime: "12:00:11.100",
  },
  { title: "Log triage: Siren: mode switch exploded at 0xADDR", feature: "Siren", count: 1, firstTime: "12:00:10.000" },
];
if (drafts.length !== 2) fail(`expected 2 drafts, got ${drafts.length}: ${drafts.map((d) => d.title).join(" | ")}`);
else {
  expect.forEach((e, i) => {
    const d = drafts[i];
    if (d.title !== e.title) fail(`draft ${i + 1} title "${d.title}", want "${e.title}"`);
    if (d.feature !== e.feature) fail(`draft ${i + 1} feature "${d.feature}", want "${e.feature}"`);
    if (d.count !== e.count) fail(`draft ${i + 1} count ${d.count}, want ${e.count}`);
    if (d.firstTime !== e.firstTime) fail(`draft ${i + 1} first time "${d.firstTime}", want "${e.firstTime}"`);
    if (!d.body.includes(`Count: ${e.count}`) || !d.body.includes(e.firstTime) || !d.body.includes(e.feature)) fail(`draft ${i + 1} body misses count, first time or feature`);
    if (!d.body.includes("```") || !d.body.includes(d.excerpt.split("\n")[0])) fail(`draft ${i + 1} body has no fenced log excerpt`);
  });
  if (!drafts[1].excerpt.includes("mode switch exploded")) fail("excerpt of draft 2 lacks the offending line");
  if (drafts.some((d) => /known|FIXED|BY DESIGN/.test(d.title))) fail("a known group was drafted");
}

// A title with shell-active characters is quoted safely and the command names the body file.
const cmd = ghCreateCommand({ title: 'a "q" $x `b` it\'s', feature: "Foo" }, "tmp/triage/2026-11-15/1.md");
const quoted = /^gh issue create --title "([^]*)" --body-file "tmp\/triage\/2026-11-15\/1\.md"/.exec(cmd);
if (!quoted || /["$`\\]/.test(quoted[1]) || !quoted[1].includes("it's")) fail(`gh command not safely quoted: ${cmd}`);

// A log with only known lines drafts nothing.
const onlyKnown = logText.split("\n").filter((l) => !/Siren\] mode switch|Can't find variable/.test(l)).join("\n");
if (draftGroups(onlyKnown, triageLog(onlyKnown, rows).groups).length !== 0) fail("a log with no new groups produced drafts");

// The command, end to end through the CLI.
function host(...args) {
  const run = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "host", "triage", ...args], { cwd: ROOT, encoding: "utf8", timeout: 60000, windowsHide: true });
  return { status: run.status, out: `${run.stdout ?? ""}${run.stderr ?? ""}` };
}
const outDir = mkdtempSync(join(tmpdir(), "lf-triage-draft-"));
const log = join(FIX, "console.log");
const table = join(FIX, "triage.md");

const plain = host(log, "--triage", table);
if (plain.status !== 0 || /gh issue create/.test(plain.out)) fail(`without --draft it must only group, got: ${plain.out.trim()}`);

writeFileSync(join(outDir, "9.md"), "stale draft from an earlier run");
const run = host(log, "--triage", table, "--draft", "--out", outDir);
if (run.status !== 0) fail(`host triage --draft exited ${run.status}: ${run.out.trim()}`);
else {
  const files = readdirSync(outDir).sort();
  if (files.join(",") !== "1.md,2.md") fail(`expected drafts 1.md,2.md (stale 9.md removed), got ${files.join(",")}`);
  const ghLines = run.out.split(/\r?\n/).filter((l) => /^gh issue create /.test(l));
  if (ghLines.length !== 2) fail(`expected 2 gh issue create lines, got ${ghLines.length}: ${run.out.trim()}`);
  else if (!ghLines[0].includes("Log triage: Foo:") || !ghLines[1].includes("Log triage: Siren:")) fail(`gh lines out of order or wrong titles: ${ghLines.join(" // ")}`);
  if (!/SUMMARY host-triage ok groups=5 known=3 new=2 drafts=2/.test(run.out)) fail(`summary line missing drafts=2: ${run.out.trim()}`);
  if (existsSync(join(outDir, "1.md")) && !/Count: 2/.test(readFileSync(join(outDir, "1.md"), "utf8"))) fail("1.md does not carry its count");
}

// It only prints: neither module can start a process.
for (const f of ["tools/host/triage-draft.mjs", "tools/host/commands/triage.mjs"]) {
  if (/child_process|(?<![.\w])(?:spawn|exec|execSync|execFile)\w*\(/.test(readFileSync(join(ROOT, f), "utf8"))) fail(`${f} can start a process; drafts must only print gh commands`);
}

if (failed) {
  console.log(`triage-draft-selftest: ${failed} case(s) failed`);
  process.exit(1);
}
console.log("triage-draft-selftest: OK (2 drafts with titles, counts, first times, features; gh commands printed, not run)");
