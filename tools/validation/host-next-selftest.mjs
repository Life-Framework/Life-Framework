// tools/validation/host-next-selftest.mjs
// inputs: tools/host/next.mjs, tools/host/commands/next.mjs
//
// Proves `tools\cli host next` (#1416): from the restarts section (times 06:00 and 18:00, warnings 15, 5, 1)
// and a clock, it names the next restart and its warning times (the game's rule, LF_RestartSchedule): at 05:50
// the restart is 06:00 today with the 05:45 warning passed and 05:55 and 05:59 still ahead; at 19:00 it is
// 06:00 tomorrow; no `restarts` (or an empty list) gives "no scheduled restarts". Only a posted session
// (one with an `announced` time) is the next session; a draft, a null next and a past session are not.
// Local time throughout (the clock is built from local fields), so the run does not depend on the zone.
// red-proof: before tools/host/next.mjs existed the import failed:
// "FAIL next.mjs missing (Cannot find module ...)" (2026-11-15).

import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const tmp = mkdtempSync(join(tmpdir(), "lf-host-next-"));
let failed = 0;
function fail(msg) {
  console.log(`FAIL  ${msg}`);
  failed++;
}
const at = (y, mo, d, h, mi) => new Date(y, mo - 1, d, h, mi, 0);
const hhmm = (d) => `${String(d.getHours()).padStart(2, "0")}:${String(d.getMinutes()).padStart(2, "0")}`;

async function runCmd(cmd, args, ctx) {
  const lines = [];
  const orig = console.log;
  console.log = (...a) => lines.push(a.join(" "));
  let code;
  try {
    code = await cmd.main(args, ctx);
  } finally {
    console.log = orig;
  }
  return { code, out: lines.join("\n") };
}

try {
  let next, cmd;
  try {
    next = await import(pathToFileURL(join(ROOT, "tools", "host", "next.mjs")).href);
    cmd = await import(pathToFileURL(join(ROOT, "tools", "host", "commands", "next.mjs")).href);
  } catch (e) {
    fail(`next.mjs missing (${e.message})`);
    console.log("SUMMARY host-next-selftest fail 1");
    process.exit(1);
  }

  const restarts = { times: ["06:00", "18:00"], warningMinutes: [15, 5, 1] };

  // 05:50: restart 06:00 today; 05:45 passed, 05:55 and 05:59 ahead
  const a = next.nextRestart(restarts, at(2026, 11, 16, 5, 50));
  if (!a) fail("05:50 gave no restart");
  else {
    if (hhmm(a.at) !== "06:00" || a.at.getDate() !== 16) fail(`05:50 restart ${a.at}, want 06:00 on the 16th`);
    if (a.untilMs !== 10 * 60000) fail(`05:50 untilMs ${a.untilMs}, want 600000`);
    const w = Object.fromEntries(a.warnings.map((x) => [hhmm(x.at), x.state]));
    if (w["05:45"] !== "passed" || w["05:55"] !== "ahead" || w["05:59"] !== "ahead" || a.warnings.length !== 3) fail(`05:50 warnings ${JSON.stringify(w)}`);
    if (a.warnings.map((x) => x.lead).join() !== "15,5,1") fail("warnings not listed longest lead first");
  }

  // 19:00: 06:00 tomorrow, every warning still ahead
  const b = next.nextRestart(restarts, at(2026, 11, 16, 19, 0));
  if (!b || hhmm(b.at) !== "06:00" || b.at.getDate() !== 17) fail(`19:00 restart ${b && b.at}, want 06:00 on the 17th`);
  else if (!b.warnings.every((x) => x.state === "ahead")) fail("19:00 has a warning that is not ahead");

  // 05:59:30: the 05:59 warning has passed too; exactly on a restart it is that restart (0 s)
  const c = next.nextRestart(restarts, at(2026, 11, 16, 6, 0));
  if (!c || c.untilMs !== 0 || c.warnings.some((x) => x.state !== "passed")) fail("on the restart minute: want 0 s and every warning passed");

  // a month end rolls over: 18:00 gone on the 30th gives 06:00 on 1 December
  const d = next.nextRestart(restarts, at(2026, 11, 30, 19, 0));
  if (!d || d.at.getMonth() !== 11 || d.at.getDate() !== 1) fail(`30 Nov 19:00 restart ${d && d.at}, want 1 Dec`);

  // no restarts: missing section, empty list, only bad times
  for (const [label, r] of [["undefined", undefined], ["empty times", { times: [] }], ["bad times", { times: ["25:00", "6:00", "ab:cd"] }]]) {
    if (next.nextRestart(r, at(2026, 11, 16, 5, 50)) !== null) fail(`${label}: want null (no scheduled restarts)`);
  }
  // a bad time is skipped, a good one still counts; no warningMinutes keeps the shipped 15, 5, 1
  const e = next.nextRestart({ times: ["xx:yy", "07:30"] }, at(2026, 11, 16, 5, 50));
  if (!e || hhmm(e.at) !== "07:30" || e.warnings.map((x) => x.lead).join() !== "15,5,1") fail("bad time skipped / default leads");
  // a lead outside 1 to 120 or a repeat is dropped
  const f = next.nextRestart({ times: ["06:00"], warningMinutes: [5, 5, 0, 121, 10] }, at(2026, 11, 16, 5, 50));
  if (!f || f.warnings.map((x) => x.lead).join() !== "10,5") fail(`lead filter ${f && f.warnings.map((x) => x.lead)}`);

  // sessions: posted only
  const posted = { next: { at: "2026-11-20T20:00:00Z", build: "alpha-3", wipe: false, announced: "2026-11-15T09:00:00Z" } };
  const now = at(2026, 11, 16, 5, 50);
  const s1 = next.nextSession(posted, now);
  if (!s1 || s1.build !== "alpha-3" || s1.wipe !== false || s1.at.toISOString() !== "2026-11-20T20:00:00.000Z") fail(`posted session ${JSON.stringify(s1)}`);
  const draft = { next: { at: "2026-11-20T20:00:00Z", build: "alpha-3", wipe: false } };
  if (next.nextSession(draft, now) !== null) fail("a draft (no announced) counted as the next session");
  if (next.nextSession({ next: null }, now) !== null || next.nextSession({}, now) !== null || next.nextSession(null, now) !== null) fail("null next counted");
  if (next.nextSession({ next: { ...posted.next, announced: "" } }, now) !== null) fail("empty announced counted");
  if (next.nextSession({ next: { ...posted.next, announced: "not a date" } }, now) !== null) fail("unparseable announced counted");
  if (next.nextSession({ next: { ...posted.next, at: "2026-11-10T20:00:00Z" } }, now) !== null) fail("a session in the past counted as next");
  if (next.nextSession({ next: { ...posted.next, at: "soon" } }, now) !== null) fail("an unparseable at counted");

  // command: files in, lines out
  const cfg = join(tmp, "config.json");
  const sess = join(tmp, "sessions.json");
  writeFileSync(cfg, JSON.stringify({ restarts }));
  writeFileSync(sess, JSON.stringify(posted));
  if (!cmd.help || typeof cmd.main !== "function") fail("commands/next.mjs lacks help/main");
  const r1 = await runCmd(cmd, [], { root: ROOT, hostDir: tmp, now, configFile: cfg, sessionsFile: sess });
  if (r1.code !== 0) fail(`command exit ${r1.code}, want 0`);
  if (!/restart.*06:00/.test(r1.out) || !/05:55/.test(r1.out) || !/05:45/.test(r1.out)) fail(`command output missing the restart or its warnings:\n${r1.out}`);
  if (!/session.*alpha-3/.test(r1.out)) fail(`command output missing the session:\n${r1.out}`);

  writeFileSync(cfg, JSON.stringify({ jobs: {} }));
  writeFileSync(sess, JSON.stringify(draft));
  const r2 = await runCmd(cmd, [], { root: ROOT, hostDir: tmp, now, configFile: cfg, sessionsFile: sess });
  if (!/no scheduled restarts/.test(r2.out)) fail(`no restarts: missing "no scheduled restarts":\n${r2.out}`);
  if (!/no announced session/.test(r2.out)) fail(`draft: missing "no announced session":\n${r2.out}`);
  if (!/draft/.test(r2.out)) fail(`draft: the line should say a draft exists:\n${r2.out}`);

  // a missing config file is a plain line, not a crash
  const r3 = await runCmd(cmd, [], { root: ROOT, hostDir: join(tmp, "nohost"), now, configFile: join(tmp, "nope.json"), sessionsFile: join(tmp, "nope2.json") });
  if (!/no scheduled restarts/.test(r3.out) || !/no announced session/.test(r3.out)) fail(`missing files:\n${r3.out}`);
} finally {
  rmSync(tmp, { recursive: true, force: true });
}

console.log(failed ? `SUMMARY host-next-selftest fail ${failed}` : "SUMMARY host-next-selftest ok");
process.exit(failed ? 1 : 0);
