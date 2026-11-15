// tools/validation/host-feedback-selftest.mjs
// inputs: tools/host/feedback.mjs, tools/host/commands/feedback.mjs, tools/validation/fixtures/host-feedback/**, .github/ISSUE_TEMPLATE/**
//
// Proves `host feedback <export.txt>`: a fixture export of five Discord messages (two the same bug,
// one thanks, one feature idea, one other bug) gives exactly three drafts in the repo's bug or
// feature template layout, no Discord user id in any draft, one `gh issue create` line per draft,
// and nothing filed (neither module can start a process). No game, no network.
// red-proof: before tools/host/feedback.mjs existed this failed at its first check:
// "FAIL  tools/host/feedback.mjs is missing" (2026-11-15, #1511).

import { existsSync, mkdtempSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FIX = join(ROOT, "tools", "validation", "fixtures", "host-feedback");
let failed = 0;
const fail = (msg) => {
  console.log(`FAIL  ${msg}`);
  failed++;
};
const eq = (name, got, want) => {
  if (got !== want) fail(`${name}\n   got:  ${JSON.stringify(got)}\n   want: ${JSON.stringify(want)}`);
};

const libPath = join(ROOT, "tools", "host", "feedback.mjs");
if (!existsSync(libPath)) {
  fail("tools/host/feedback.mjs is missing");
  console.log(`host-feedback-selftest: ${failed} case(s) failed`);
  process.exit(1);
}
const lib = await import(pathToFileURL(libPath).href);
const text = readFileSync(join(FIX, "export.txt"), "utf8");

// 1. parse: five messages, the authors and bodies come apart
const messages = lib.parseExport(text);
eq("message count", messages.length, 5);
eq("first author", messages[0]?.author, "Rook");
if (!/bank door is stuck/.test(messages[0]?.text ?? "")) fail(`first message text: ${messages[0]?.text}`);
const bracket = lib.parseExport("[2026-11-14 20:02] Kit: the siren never stops\n[2026-11-14 20:03] Ana: thx!!");
eq("bracket format count", bracket.length, 2);
eq("bracket format author", bracket[1]?.author, "Ana");

// 2. thanks are recognised, reports are not
eq("thanks", lib.isThanks("thanks for hosting, had a great time!"), true);
eq("gg", lib.isThanks("gg"), true);
eq("bug is not thanks", lib.isThanks("thanks but the door is stuck"), false);
eq("feature is not thanks", lib.isThanks("it would be nice to rename my sign"), false);

// 3. kind
eq("kind bug", lib.classify("the ATM shows success but my balance did not change"), "bug");
eq("kind feature", lib.classify("It would be nice to have a way to rename my shop sign"), "feature");

// 4. drafts: three, in order, with the right shape
const result = lib.draftFeedback(messages);
eq("thanks dropped", result.thanks, 1);
eq("duplicates merged", result.duplicates, 1);
const drafts = result.drafts;
eq("draft count", drafts.length, 3);
eq("draft kinds", drafts.map((d) => d.kind).join(","), "bug,feature,bug");
eq("draft reporters", drafts.map((d) => d.reporters).join(","), "2,1,1");
eq("draft labels", drafts.map((d) => d.label).join(","), "bug,enhancement,bug");
if (drafts.length === 3) {
  if (!drafts[0].title.startsWith("[Bug]: ") || !/bank door/i.test(drafts[0].title)) fail(`draft 1 title: ${drafts[0].title}`);
  if (!drafts[1].title.startsWith("[Feature]: ") || !/shop sign/i.test(drafts[1].title)) fail(`draft 2 title: ${drafts[1].title}`);
  for (const h of ["### What happened?", "### Steps to reproduce", "### Expected behavior", "### Logs or screenshots", "### Environment"]) {
    if (!drafts[0].body.includes(h)) fail(`bug draft lacks the template heading ${h}`);
  }
  for (const h of ["### Problem or opportunity", "### Proposed solution", "### Alternatives considered"]) {
    if (!drafts[1].body.includes(h)) fail(`feature draft lacks the template heading ${h}`);
  }
  if (!/2 players/.test(drafts[0].body)) fail("draft 1 body does not say two players reported it");
  if (!drafts[0].body.includes("stays stuck closed")) fail("draft 1 body lost the second report's words");
}

// 5. no Discord id anywhere: no snowflake, no <@id>, no author name
for (const d of drafts) {
  if (/\d{17,20}|<@!?&?\d+>/.test(d.title + d.body)) fail(`a Discord id is left in "${d.title}"`);
  if (/\b(Rook|Mara|Dex|Ivy|Tomas)\b/.test(d.title + d.body)) fail(`an author name is left in "${d.title}"`);
}

// 6. gh line quoting
const cmd = lib.ghCreateCommand({ title: 'a "q" $x `b` it\'s', label: "bug" }, "tmp/feedback/d/1.md");
const quoted = /^gh issue create --title "([^]*)" --body-file "tmp\/feedback\/d\/1\.md" --label bug$/.exec(cmd);
if (!quoted || /["$`\\]/.test(quoted[1]) || !quoted[1].includes("it's")) fail(`gh command not safely quoted: ${cmd}`);

// 7. the command, end to end through the CLI
const out = mkdtempSync(join(tmpdir(), "lf-host-feedback-"));
try {
  const host = (...args) => {
    const r = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "host", "feedback", ...args], { cwd: ROOT, encoding: "utf8", timeout: 60000, windowsHide: true });
    return { status: r.status, out: `${r.stdout ?? ""}${r.stderr ?? ""}` };
  };
  writeFileSync(join(out, "9.md"), "stale draft from an earlier run");
  const run = host(join(FIX, "export.txt"), "--out", out);
  if (run.status !== 0) fail(`host feedback exited ${run.status}: ${run.out.trim()}`);
  else {
    eq("files written", readdirSync(out).sort().join(","), "1.md,2.md,3.md");
    const gh = run.out.split(/\r?\n/).filter((l) => /^gh issue create /.test(l));
    eq("gh line count", gh.length, 3);
    if (gh.length === 3 && (!gh[0].includes("[Bug]: ") || !gh[1].includes("--label enhancement"))) fail(`gh lines wrong: ${gh.join(" // ")}`);
    if (!/SUMMARY host-feedback ok messages=5 thanks=1 duplicates=1 drafts=3/.test(run.out)) fail(`summary line wrong: ${run.out.trim()}`);
    if (existsSync(join(out, "1.md")) && /\d{17,20}/.test(readFileSync(join(out, "1.md"), "utf8"))) fail("1.md carries a Discord id");
  }
  const missing = host(join(out, "nope.txt"), "--out", out);
  if (missing.status === 0) fail("a missing export must exit non-zero");
  const empty = join(out, "empty.txt");
  writeFileSync(empty, "just words with no message header\n");
  const none = host(empty, "--out", out);
  if (none.status === 0) fail("an export with no messages must exit non-zero");
} finally {
  rmSync(out, { recursive: true, force: true });
}

// 8. it only prints: neither module can start a process
for (const f of ["tools/host/feedback.mjs", "tools/host/commands/feedback.mjs"]) {
  if (existsSync(join(ROOT, f)) && /child_process|(?<![.\w])(?:spawn|exec|execSync|execFile)\w*\(/.test(readFileSync(join(ROOT, f), "utf8"))) fail(`${f} can start a process; drafts must only print gh commands`);
}

if (failed) {
  console.log(`host-feedback-selftest: ${failed} case(s) failed`);
  process.exit(1);
}
console.log("host-feedback-selftest: OK (5 messages -> 3 drafts, thanks and duplicate dropped, ids stripped, gh lines printed, nothing filed)");
