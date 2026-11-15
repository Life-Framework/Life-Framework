// tools/validation/opening-b9-selftest.mjs
// inputs: tools/playtest/opening/**, tools/host/**, tools/validation/fixtures/opening-b9/**, docs/community/announce-template.md
//
// Proves the opening-day beat script B9 (#1492, docs/playtest/m4-opening-day.md) on the fixture
// host: `node tools/playtest/opening/b9-wipe.mjs` copies the fixture into tmp/, runs the real
// `host wipe` and `host announce --wipe` against the copy, and prints one `CHECK <id> <status>`
// line per step. Every check must be PASS, or BLOCKED naming #1413 while the wipe notice gate is
// not on main. The script refuses any --host outside tmp/, never touches the real host folder or
// website/content/sessions.json, and removes its scratch copy.
// red-proof: before tools/playtest/opening/b9-wipe.mjs existed this file failed on its first run:
// "tools/playtest/opening/b9-wipe.mjs is missing" (2026-11-15).

import { existsSync, mkdirSync, mkdtempSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { spawnSync } from "node:child_process";
import { safeRemoveTree } from "../link-safety.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const SCRIPT = join(ROOT, "tools", "playtest", "opening", "b9-wipe.mjs");
const SESSIONS = join(ROOT, "website", "content", "sessions.json");
const EXPECTED = ["refuses-while-running", "refuses-without-notice", "announce-draft", "refuses-fresh-notice", "aged-notice", "wipe-backup", "wipe-moved-aside", "wipe-logged"];

let failed = 0;
const fail = (m) => {
  console.log(`FAIL  ${m}`);
  failed++;
};

if (!existsSync(SCRIPT)) {
  fail("tools/playtest/opening/b9-wipe.mjs is missing");
  console.log(`opening-b9-selftest: ${failed} case(s) failed`);
  process.exit(1);
}

const run = (args, env = {}) =>
  spawnSync(process.execPath, [SCRIPT, ...args], { cwd: ROOT, encoding: "utf8", timeout: 240000, windowsHide: true, env: { ...process.env, ...env } });

const sessionsBefore = existsSync(SESSIONS) ? readFileSync(SESSIONS, "utf8") : null;
const outside = mkdtempSync(join(tmpdir(), "lf-b9-outside-"));
const scratchRoot = join(ROOT, "tmp", "opening-b9");

try {
  // 1. A --host outside tmp/ is refused before anything is copied or run, the repo root included.
  for (const bad of [outside, ROOT, join(ROOT, "tools"), join(ROOT, "tmp", "..", "tools")]) {
    const r = run(["--host", bad]);
    if (r.status === 0 || !/outside tmp/i.test(`${r.stdout}${r.stderr}`)) fail(`--host ${bad} was not refused with "outside tmp": exit ${r.status}`);
    if (/CHECK /.test(r.stdout)) fail(`--host ${bad} ran checks`);
  }

  // 2. The fixture run: every check present, PASS or BLOCKED naming #1413.
  const before = existsSync(scratchRoot) ? readdirSync(scratchRoot).length : 0;
  const r = run([]);
  const out = `${r.stdout}${r.stderr}`;
  if (r.status !== 0) fail(`the fixture run exited ${r.status}:\n${out}`);
  const checks = new Map();
  for (const m of out.matchAll(/^CHECK (\S+) (PASS|BLOCKED|FAIL)\b(.*)$/gm)) checks.set(m[1], { status: m[2], detail: m[3] });
  for (const id of EXPECTED) {
    const c = checks.get(id);
    if (!c) fail(`no CHECK line for ${id}`);
    else if (c.status === "FAIL") fail(`${id} failed:${c.detail}`);
    else if (c.status === "BLOCKED" && !/#1413/.test(c.detail)) fail(`${id} is BLOCKED without naming #1413:${c.detail}`);
  }
  if (!/^SUMMARY b9-wipe (ok|blocked)\b/m.test(out)) fail(`no ok/blocked SUMMARY line:\n${out}`);
  // The refusal while the host runs and the backup/move/log steps work on main today.
  for (const id of ["refuses-while-running", "announce-draft", "wipe-backup", "wipe-moved-aside", "wipe-logged"]) {
    if (checks.get(id)?.status !== "PASS") fail(`${id} must be PASS, got ${checks.get(id)?.status}`);
  }

  // 3. Nothing leaked: the scratch copy is gone and the real sessions.json is untouched.
  const after = existsSync(scratchRoot) ? readdirSync(scratchRoot).length : 0;
  if (after > before) fail(`the scratch copy was left in tmp/opening-b9 (${after - before} run folder(s))`);
  const sessionsAfter = existsSync(SESSIONS) ? readFileSync(SESSIONS, "utf8") : null;
  if (sessionsAfter !== sessionsBefore) fail("website/content/sessions.json changed: the announce step wrote outside the scratch copy");

  // 4. Ageing the notice rewrites the timestamp in the scratch file only, by 25 hours.
  const mod = await import(pathToFileURL(SCRIPT).href);
  if (typeof mod.ageNotice !== "function") fail("b9-wipe.mjs does not export ageNotice");
  else {
    const f = join(outside, "sessions.json");
    const t = Date.parse("2026-11-20T12:00:00Z");
    writeFileSync(f, JSON.stringify({ next: { at: "2026-11-22T19:00:00.000Z", build: "1.3.0.1", wipe: true, announced: true, announcedAt: new Date(t).toISOString() } }));
    const a = mod.ageNotice(f, 25);
    const row = JSON.parse(readFileSync(f, "utf8")).next;
    if (!a.aged || Date.parse(row.announcedAt) !== t - 25 * 3600000) fail(`ageNotice did not move announcedAt back 25 h: ${JSON.stringify({ a, row })}`);
    if (row.at !== "2026-11-22T19:00:00.000Z") fail("ageNotice moved the session time");
    writeFileSync(f, JSON.stringify({ next: { at: "2026-11-22T19:00:00.000Z", wipe: true, announced: false } }));
    if (mod.ageNotice(f, 25).aged) fail("ageNotice reported ageing a file with no notice timestamp");
  }
} finally {
  safeRemoveTree(outside);
}

if (failed) {
  console.log(`opening-b9-selftest: ${failed} case(s) failed`);
  process.exit(1);
}
console.log("opening-b9-selftest ok");
