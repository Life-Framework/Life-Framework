// tools/validation/host-feedback-link-selftest.mjs
// inputs: tools/host/feedback.mjs, tools/host/commands/feedback.mjs, tools/validation/fixtures/host-feedback/**
//
// Proves #1611: the issue drafts of `host feedback` link back to their session. On the feedback
// fixture plus a fixture `host summary` report, every draft names the session date and the report,
// and lists the other drafts of the same session, so the filed issues point at the session and at
// one another. Still no author name or Discord id in a draft. No game, no network.
// red-proof: before the session line existed this failed at its first check:
// "FAIL  draft 1 has no session line" (2026-11-15, #1611).

import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
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

const lib = await import(pathToFileURL(join(ROOT, "tools", "host", "feedback.mjs")).href);
const messages = lib.parseExport(readFileSync(join(FIX, "export.txt"), "utf8"));
const session = { date: "2026-11-15", report: "sessions/2026-11-15_19-00-00.md" };

function check(label, drafts) {
  if (drafts.length !== 3) {
    fail(`${label}: expected 3 drafts, got ${drafts.length}`);
    return;
  }
  drafts.forEach((d, i) => {
    const n = i + 1;
    if (!/^Session: 2026-11-15\b/m.test(d.body)) fail(`${label}: draft ${n} has no session line`);
    if (!d.body.includes("sessions/2026-11-15_19-00-00.md")) fail(`${label}: draft ${n} does not name the host summary report`);
    const m = /^Other drafts from this session:\n((?:- .*\n?)+)/m.exec(d.body);
    if (!m) {
      fail(`${label}: draft ${n} has no sibling list`);
      return;
    }
    const lines = m[1].trim().split("\n");
    if (lines.length !== 2) fail(`${label}: draft ${n} lists ${lines.length} siblings, want 2`);
    drafts.forEach((o, j) => {
      const has = m[1].includes(o.title);
      if (j === i && has) fail(`${label}: draft ${n} lists itself`);
      if (j !== i && !has) fail(`${label}: draft ${n} does not list "${o.title}"`);
    });
    if (/\d{17,20}|<@!?&?\d+>/.test(d.body)) fail(`${label}: draft ${n} carries a Discord id`);
    if (/\b(Rook|Mara|Dex|Ivy|Tomas)\b/.test(d.body)) fail(`${label}: draft ${n} carries an author name`);
  });
}

// 1. the library with an explicit session
check("lib", lib.draftFeedback(messages, { session }).drafts);

// 2. a lone draft says so instead of an empty list
const lone = lib.draftFeedback(messages.slice(0, 1), { session }).drafts;
if (lone.length !== 1 || !/Other drafts from this session: none/.test(lone[0]?.body ?? "")) fail("a lone draft must say 'Other drafts from this session: none'");

// 3. no session at all: no promise of a report (old callers keep working)
const bare = lib.draftFeedback(messages).drafts;
if (bare.some((d) => /^Session:/m.test(d.body))) fail("a draft made without a session must not claim one");

// 4. findSessionReport picks the newest report of that date
const tmp = mkdtempSync(join(tmpdir(), "lf-host-feedback-link-"));
try {
  const sessions = join(tmp, "sessions");
  mkdirSync(sessions);
  for (const f of ["2026-11-14_20-00-00.md", "2026-11-15_17-00-00.md", "2026-11-15_19-00-00.md", "notes.txt"]) writeFileSync(join(sessions, f), "# Session report\n");
  const found = typeof lib.findSessionReport === "function" ? lib.findSessionReport(sessions, "2026-11-15") : null;
  if (found !== "sessions/2026-11-15_19-00-00.md") fail(`findSessionReport: ${JSON.stringify(found)}`);
  const none = typeof lib.findSessionReport === "function" ? lib.findSessionReport(sessions, "2026-12-01") : "x";
  if (none !== null) fail(`findSessionReport for a date with no report: ${JSON.stringify(none)}`);

  // 5. end to end: the command, run with a host dir that holds the report
  const out = join(tmp, "out");
  const hostDir = join(tmp, "host");
  mkdirSync(join(hostDir, "sessions"), { recursive: true });
  const day = new Date();
  const p = (n) => String(n).padStart(2, "0");
  const date = `${day.getFullYear()}-${p(day.getMonth() + 1)}-${p(day.getDate())}`;
  writeFileSync(join(hostDir, "sessions", `${date}_21-30-00.md`), "# Session report\n");
  const r = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "host", "feedback", join(FIX, "export.txt"), "--out", out], {
    cwd: ROOT,
    encoding: "utf8",
    timeout: 60000,
    windowsHide: true,
    env: { ...process.env, LF_HOST_DIR: hostDir },
  });
  if (r.status !== 0) fail(`host feedback exited ${r.status}: ${r.stdout}${r.stderr}`);
  else {
    const first = readFileSync(join(out, "1.md"), "utf8");
    if (!new RegExp(`^Session: ${date}\\b`, "m").test(first)) fail("the command's draft 1 has no session line");
    if (!/^Other drafts from this session:/m.test(first)) fail("the command's draft 1 has no sibling list");
  }
} finally {
  rmSync(tmp, { recursive: true, force: true });
}

if (failed) {
  console.log(`host-feedback-link-selftest: ${failed} case(s) failed`);
  process.exit(1);
}
console.log("host-feedback-link-selftest: OK (every draft names its session and report and lists its siblings)");
