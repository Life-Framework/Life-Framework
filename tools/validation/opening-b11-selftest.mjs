// tools/validation/opening-b11-selftest.mjs
// inputs: tools/playtest/opening/b11-after.mjs, tools/host/commands/triage.mjs, tools/host/triage-draft.mjs, tools/lib/log-triage.mjs, tools/validation/fixtures/opening-b11/**
//
// Proves opening-day beat B11 (docs/playtest/m4-opening-day.md "B11 After") on a fixture session log (a real
// server console.log tail, secrets stripped): `host triage` groups the log into known and new lines and drafts
// an issue per new group without filing one (a `gh` on PATH that records any call never gets called); the
// commands not on main are BLOCKED, not FAIL (`host history` while #1409 is open, `server-doctor`); and when a
// command is present its output is judged (injected runner), so a wrong answer is a FAIL. No game, no network.
// red-proof: before tools/playtest/opening/b11-after.mjs existed this stopped at import time:
// "Cannot find module '.../tools/playtest/opening/b11-after.mjs'" (2026-11-15, #1494).

import { dirname, join, resolve } from "node:path";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FIX = join(ROOT, "tools", "validation", "fixtures", "opening-b11");
const LOG = join(FIX, "console.log");
const TRIAGE = join(FIX, "triage.md");
const BEAT = join(ROOT, "tools", "playtest", "opening", "b11-after.mjs");
const { runBeat, formatLine } = await import("../playtest/opening/b11-after.mjs");

let failed = 0;
function fail(msg) {
  console.log(`FAIL  ${msg}`);
  failed++;
}
const by = (results) => Object.fromEntries(results.map((r) => [r.id, r]));
function expectStatus(results, id, want) {
  const r = by(results)[id];
  if (!r) fail(`no result for check ${id}`);
  else if (r.status !== want) fail(`${id} is ${r.status} (${r.reason}), want ${want}`);
  if (r && !r.reason) fail(`${id} has no reason`);
}

// A `gh` that records any call: the beat must never reach it.
const tmp = mkdtempSync(join(tmpdir(), "lf-b11-"));
const bin = join(tmp, "bin");
mkdirSync(bin);
const marker = join(tmp, "gh-was-called");
writeFileSync(join(bin, "gh.cmd"), `@echo called>"${marker}"\r\n`);
writeFileSync(join(bin, "gh"), `#!/bin/sh\necho called > "${marker}"\n`, { mode: 0o755 });
const sep = process.platform === "win32" ? ";" : ":";
const oldPath = process.env.PATH;
process.env.PATH = `${bin}${sep}${oldPath}`;

// 1. The real triage command on the fixture; the doctor and history commands reported as not on main.
const notOnMain = (rel) => !/server-doctor|history/.test(rel) && existsSync(join(ROOT, rel));
const real = await runBeat({ root: ROOT, log: LOG, triage: TRIAGE, has: notOnMain });
expectStatus(real, "triage-groups", "PASS");
expectStatus(real, "triage-drafts", "PASS");
for (const id of ["doctor-mods", "doctor-boot", "doctor-config", "history-session"]) expectStatus(real, id, "BLOCKED");
if (!/known=4 new=1/.test(by(real)["triage-groups"]?.reason ?? "")) fail(`triage-groups reason lacks known=4 new=1: ${by(real)["triage-groups"]?.reason}`);
if (!/drafts=1/.test(by(real)["triage-drafts"]?.reason ?? "")) fail(`triage-drafts reason lacks drafts=1: ${by(real)["triage-drafts"]?.reason}`);
if (existsSync(marker)) fail("the beat ran `gh`; drafts must only be printed");

// 2. With no log at all everything is BLOCKED (a host that has not run), nothing FAILs.
const none = await runBeat({ root: ROOT, log: null, triage: TRIAGE, has: notOnMain });
if (none.some((r) => r.status !== "BLOCKED")) fail(`no log must BLOCK every check, got ${none.map((r) => `${r.id}=${r.status}`).join(" ")}`);

// 3. Present commands are judged. Injected runner: triage as the real one, doctor and history scripted.
function scripted({ doctor, history, triageOut }) {
  return (args) => {
    const key = args.slice(0, 2).join(" ");
    if (key === "host triage") return triageOut ?? { status: 0, out: "SUMMARY host-triage ok groups=5 known=4 new=1 drafts=1 lines=5 log=x\ngh issue create --title \"t\" --body-file \"1.md\"\ndrafts in d; nothing is filed.\n" };
    if (args[0] === "server-doctor" || key === "host server-doctor") return doctor;
    if (key === "host history") return history;
    return { status: 1, out: `unexpected ${args.join(" ")}` };
  };
}
const present = () => true;
const goodDoctor = { status: 0, out: "mods loaded: LF_Everon1989, LifeFramework\nboot: the server booted (Entered online game state)\nconfig: no owner config; every section uses the default\n" };
const goodHistory = { status: 0, out: "2026-11-15 19:33 session 1: start 19:33:52, stop 19:34:10, restarts 0\n" };
const ok = await runBeat({ root: ROOT, log: LOG, triage: TRIAGE, has: present, run: scripted({ doctor: goodDoctor, history: goodHistory }) });
for (const id of ["doctor-mods", "doctor-boot", "doctor-config", "history-session"]) expectStatus(ok, id, "PASS");

const badDoctor = await runBeat({ root: ROOT, log: LOG, triage: TRIAGE, has: present, run: scripted({ doctor: { status: 0, out: "mods loaded: LifeFramework\nthe server did not boot\nconfig ok\n" }, history: goodHistory }) });
expectStatus(badDoctor, "doctor-mods", "FAIL"); // LF_Everon1989 missing
expectStatus(badDoctor, "doctor-boot", "FAIL");
expectStatus(badDoctor, "doctor-config", "FAIL"); // no fallback to defaults reported

const crashDoctor = await runBeat({ root: ROOT, log: LOG, triage: TRIAGE, has: present, run: scripted({ doctor: { status: 1, out: "boom" }, history: { status: 1, out: "no sessions" } }) });
for (const id of ["doctor-mods", "doctor-boot", "doctor-config", "history-session"]) expectStatus(crashDoctor, id, "FAIL");

const emptyHistory = await runBeat({ root: ROOT, log: LOG, triage: TRIAGE, has: present, run: scripted({ doctor: goodDoctor, history: { status: 0, out: "no sessions recorded\n" } }) });
expectStatus(emptyHistory, "history-session", "FAIL");

// 4. Triage that drafts the wrong number, or fails, is a FAIL; a draft gh line count must match.
const mismatch = await runBeat({ root: ROOT, log: LOG, triage: TRIAGE, has: present, run: scripted({ doctor: goodDoctor, history: goodHistory, triageOut: { status: 0, out: "SUMMARY host-triage ok groups=5 known=3 new=2 drafts=1 lines=5 log=x\ngh issue create --title \"t\"\ndrafts in d; nothing is filed.\n" } }) });
expectStatus(mismatch, "triage-drafts", "FAIL");
const dead = await runBeat({ root: ROOT, log: LOG, triage: TRIAGE, has: present, run: scripted({ doctor: goodDoctor, history: goodHistory, triageOut: { status: 1, out: "host triage: triage table not found" } }) });
expectStatus(dead, "triage-groups", "FAIL");

// 5. The standalone script: one `BEAT B11 <id> PASS|FAIL|BLOCKED <reason>` line per check, exit 1 only on a FAIL.
// The real doctor and history run here, so the host dir is a seeded temp one (a restart, then a stop), never this machine's.
const hostDir = join(tmp, "host");
for (const name of ["2026-11-15_19-33-52-start", "2026-11-15_19-34-10-start", "2026-11-15_19-35-00-stop"]) mkdirSync(join(hostDir, "backups", name), { recursive: true });
const run = spawnSync(process.execPath, [BEAT, "--log", LOG, "--triage", TRIAGE], { cwd: ROOT, encoding: "utf8", timeout: 120000, windowsHide: true, env: { ...process.env, LF_HOST_DIR: hostDir } });
const lines = `${run.stdout ?? ""}`.split(/\r?\n/).filter((l) => l.startsWith("BEAT "));
if (lines.length !== 6 || lines.some((l) => !/^BEAT B11 [a-z-]+ (PASS|FAIL|BLOCKED) \S/.test(l))) fail(`standalone output is not 6 well-formed BEAT lines: ${run.stdout}${run.stderr}`);
if (!/^BEAT B11 triage-groups PASS /m.test(run.stdout ?? "")) fail(`standalone triage-groups is not PASS: ${run.stdout}${run.stderr}`);
if (run.status !== 0) fail(`standalone exit ${run.status} with no FAIL expected: ${run.stdout}${run.stderr}`);
if (formatLine({ id: "x", status: "PASS", reason: "r" }) !== "BEAT B11 x PASS r") fail("formatLine shape");
if (existsSync(marker)) fail("the standalone beat ran `gh`");

// The beat starts no `gh` of its own: the only process it spawns is the repo CLI.
if (/["'`]gh["'`]|gh issue/.test(readFileSync(BEAT, "utf8").replace(/\/\/.*$/gm, "").replace(/\/\*[^]*?\*\//g, ""))) fail("b11-after.mjs names `gh` outside comments; drafts must only be printed");

process.env.PATH = oldPath;
if (failed) {
  console.log(`opening-b11-selftest: ${failed} case(s) failed`);
  process.exit(1);
}
console.log("opening-b11-selftest: OK (triage PASS and drafts without gh on the fixture log; BLOCKED for doctor and history while not on main; present commands judged)");
