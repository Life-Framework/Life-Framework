// tools/validation/host-remote-schedule-selftest.mjs
// inputs: tools/host/remote-schedule.mjs, tools/host/commands/remote.mjs, tools/host/autostart.mjs
//
// Proves `tools\cli host remote 0grind saves pull --schedule|--unschedule|--schedule-status` (#1725)
// without the Task Scheduler and without any 0grind call (the fake runner keeps the task in memory, the
// pull is an injected stub; this laptop's IP is not on 0grind's allowlist). --schedule builds the exact
// `/Create` arguments with the interval and a `/TR` under 261 characters, --unschedule and
// --schedule-status parse the fixture CSV, --dry-run prints the schtask without running it, and the
// prune keeps the newest N `-remote` pulls and every tagged one and never touches a local backup.
// red-proof: before tools/host/remote-schedule.mjs existed the import failed:
// "FAIL remote-schedule.mjs missing (Cannot find module ...)" (2026-11-16).

import { existsSync, mkdirSync, mkdtempSync, readdirSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { safeRemoveTree } from "../link-safety.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
let failed = 0;
function fail(msg) {
  console.log(`FAIL  ${msg}`);
  failed++;
}
function eq(label, got, want) {
  if (JSON.stringify(got) !== JSON.stringify(want)) fail(`${label}: got ${JSON.stringify(got)}, want ${JSON.stringify(want)}`);
}

function csv(fields) {
  return fields.map((f) => `"${String(f).replace(/"/g, '""')}"`).join(",");
}
function queryLine(task, { lastRun = "16/11/2026 6:00:00 AM", lastResult = "0" } = {}) {
  const f = new Array(20).fill("N/A");
  f[0] = "LAPTOP";
  f[1] = `\\${task.name}`;
  f[3] = "Ready";
  f[5] = lastRun;
  f[6] = lastResult;
  f[8] = task.command;
  f[14] = task.user;
  return csv(f);
}

// A fake Task Scheduler: /Create stores, /Query prints the stored task, /Delete removes it.
function fakeScheduler(opts = {}) {
  const s = { task: null, calls: [], lastRun: opts.lastRun, lastResult: opts.lastResult };
  s.run = (args, o) => {
    s.calls.push({ args, timeoutMs: o?.timeoutMs });
    const verb = args[0];
    const arg = (flag) => args[args.indexOf(flag) + 1];
    if (verb === "/Create") {
      s.task = { name: arg("/TN"), command: arg("/TR"), user: arg("/RU") };
      return { status: 0, stdout: "SUCCESS\r\n", stderr: "" };
    }
    if (verb === "/Query") {
      if (!s.task || s.task.name !== arg("/TN")) return { status: 1, stdout: "", stderr: "ERROR\r\n" };
      return { status: 0, stdout: `${queryLine(s.task, { lastRun: s.lastRun, lastResult: s.lastResult })}\r\n`, stderr: "" };
    }
    if (verb === "/Delete") {
      if (!s.task) return { status: 1, stdout: "", stderr: "ERROR\r\n" };
      s.task = null;
      return { status: 0, stdout: "SUCCESS\r\n", stderr: "" };
    }
    return { status: 2, stdout: "", stderr: `unexpected schtasks verb ${verb}` };
  };
  s.verbs = (v) => s.calls.filter((c) => c.args[0] === v).length;
  return s;
}

let sched, cmd;
try {
  sched = await import(pathToFileURL(join(ROOT, "tools", "host", "remote-schedule.mjs")).href);
  cmd = await import(pathToFileURL(join(ROOT, "tools", "host", "commands", "remote.mjs")).href);
} catch (e) {
  fail(`remote-schedule.mjs missing (${e.message})`);
  console.log("SUMMARY host-remote-schedule-selftest fail 1");
  process.exit(1);
}

const FAKE_ROOT = "C:\\lf\\Life-Framework-ws-host";
const NODE = "C:\\Program Files\\nodejs\\node.exe";
const USER = "LAPTOP\\jasper";
const base = { root: FAKE_ROOT, hostDir: FAKE_ROOT, isMain: false, nodePath: NODE, user: USER, env: {} };
const quiet = async (fn) => {
  const log = console.log;
  const lines = [];
  console.log = (...a) => lines.push(a.join(" "));
  try {
    return { code: await fn(), out: lines.join("\n") };
  } finally {
    console.log = log;
  }
};
const remote = (args, ctx) => cmd.main(["0grind", "saves", "pull", ...args], ctx);

// --- the exact /Create arguments, interval included, /TR under schtasks' 261-character cap
const wantTr = `"${NODE}" "C:\\lf\\Life-Framework-ws-host\\tools\\host\\remote-schedule.mjs" --run --every 6 --keep 20`;
eq("task name", sched.TASK_NAME, "LifeFramework 0grind Saves Pull");
eq("default interval", sched.DEFAULT_EVERY_HOURS, 6);
eq("default keep", sched.DEFAULT_KEEP, 20);
eq("create args (6 h)", sched.createArgs({ root: FAKE_ROOT, nodePath: NODE, user: USER, everyHours: 6, keep: 20 }), [
  "/Create", "/TN", sched.TASK_NAME, "/TR", wantTr,
  "/SC", "HOURLY", "/MO", "6", "/RU", USER, "/RL", "LIMITED", "/F",
]);
eq("create args honour --every 12", sched.createArgs({ root: FAKE_ROOT, nodePath: NODE, user: USER, everyHours: 12, keep: 5 }).slice(7, 9), ["/MO", "12"]);
if (sched.taskCommand({ root: "C:\\" + "x".repeat(60), nodePath: NODE, everyHours: 6, keep: 20 }).length >= 261) fail("/TR reaches 261 characters for a 60-character checkout path");

// --- --schedule registers once, a second identical schedule changes nothing
const s = fakeScheduler();
let r = await quiet(() => remote(["--schedule"], { ...base, runner: s.run }));
eq("schedule exit", r.code, 0);
eq("schedule creates once", s.verbs("/Create"), 1);
eq("registered /TR", s.task?.command, wantTr);
const create = s.calls.find((c) => c.args[0] === "/Create").args;
eq("scheduled every 6 h by default", create.slice(create.indexOf("/SC"), create.indexOf("/SC") + 4), ["/SC", "HOURLY", "/MO", "6"]);
r = await quiet(() => remote(["--schedule"], { ...base, runner: s.run }));
eq("second schedule exit", r.code, 0);
eq("second schedule creates nothing more", s.verbs("/Create"), 1);
if (!/already/i.test(r.out)) fail(`second schedule does not say already scheduled: ${r.out}`);
r = await quiet(() => remote(["--schedule", "--every", "12", "--keep", "5"], { ...base, runner: s.run }));
eq("changed interval exit", r.code, 0);
eq("changed interval re-creates", s.verbs("/Create"), 2);
if (!s.task?.command.endsWith("--run --every 12 --keep 5")) fail(`changed keep not in /TR: ${s.task?.command}`);
if (s.calls.some((c) => !(c.timeoutMs > 0 && c.timeoutMs <= 60000))) fail("a schtasks call has no bounded timeout");

// --- bad intervals refuse and touch nothing
const bad = fakeScheduler();
for (const v of ["0", "24", "abc", "-1", "1.5"]) {
  r = await quiet(() => remote(["--schedule", "--every", v], { ...base, runner: bad.run }));
  eq(`--every ${v} exit`, r.code, 1);
}
r = await quiet(() => remote(["--schedule", "--keep", "0"], { ...base, runner: bad.run }));
eq("--keep 0 exit", r.code, 1);
eq("bad usage touched schtasks", bad.calls.length, 0);

// --- --schedule-status parses the fixture CSV
const info = sched.parseQuery(`${queryLine({ name: sched.TASK_NAME, command: wantTr, user: USER })}\r\n`);
eq("parse registered", info.registered, true);
eq("parse command", info.command, wantTr);
eq("parse lastRun", info.lastRun, "16/11/2026 6:00:00 AM");
eq("parse ran", info.ran, true);
const s2 = fakeScheduler({ lastRun: "16/11/2026 6:00:00 AM" });
await quiet(() => remote(["--schedule"], { ...base, runner: s2.run }));
r = await quiet(() => remote(["--schedule-status"], { ...base, runner: s2.run }));
eq("status registered exit", r.code, 0);
if (!/^\[LFDebug:Host0grind\] scheduled: every 6 h/m.test(r.out) || !r.out.includes("6:00:00") || !r.out.includes(USER)) fail(`status lacks registered/last run/interval: ${r.out}`);
s2.lastRun = "30/11/1999 12:00:00 AM";
s2.lastResult = "267011";
r = await quiet(() => remote(["--schedule-status"], { ...base, runner: s2.run }));
if (!/never/i.test(r.out)) fail(`status of a task that never ran does not say so: ${r.out}`);

// --- --unschedule deletes, a second one is a no-op, status then says not scheduled
r = await quiet(() => remote(["--unschedule"], { ...base, runner: s2.run }));
eq("unschedule exit", r.code, 0);
eq("task gone", s2.task, null);
eq("unschedule deletes once", s2.verbs("/Delete"), 1);
r = await quiet(() => remote(["--unschedule"], { ...base, runner: s2.run }));
eq("second unschedule exit", r.code, 0);
eq("second unschedule deletes nothing more", s2.verbs("/Delete"), 1);
r = await quiet(() => remote(["--schedule-status"], { ...base, runner: s2.run }));
eq("status when absent exits 1", r.code, 1);
if (!/not scheduled/i.test(r.out)) fail(`status when absent: ${r.out}`);

// --- --dry-run prints the schtask and never reaches the runner
const trip = fakeScheduler();
r = await quiet(() => remote(["--schedule", "--dry-run"], { ...base, runner: trip.run }));
eq("dry-run exit", r.code, 0);
eq("dry-run reached the runner", trip.calls.length, 0);
if (!r.out.includes("/SC") || !r.out.includes("HOURLY") || !r.out.includes("remote-schedule.mjs")) fail(`dry-run does not print the task: ${r.out}`);

// --- the main checkout is the world-editor copy: refuse to schedule or unschedule, touch nothing
const guarded = fakeScheduler();
r = await quiet(() => remote(["--schedule"], { ...base, isMain: true, runner: guarded.run }));
eq("main checkout schedule exit", r.code, 1);
eq("main checkout touched schtasks", guarded.calls.length, 0);
if (!/main checkout/i.test(r.out)) fail(`main checkout refusal not explained: ${r.out}`);

// --- a hung schtasks fails the command
const hung = { run: () => ({ status: null, stdout: "", stderr: "", error: Object.assign(new Error("ETIMEDOUT"), { code: "ETIMEDOUT" }) }) };
r = await quiet(() => remote(["--schedule"], { ...base, runner: hung.run }));
eq("timeout exit", r.code, 1);
if (!/timed out/i.test(r.out)) fail(`timeout not reported: ${r.out}`);

// --- prune: the newest N plain -remote pulls stay; tagged and local backups are never touched
const work = mkdtempSync(join(tmpdir(), "lf-remote-sched-"));
try {
  const backups = join(work, "backups");
  const stampOf = (d) => `2026-11-${String(d).padStart(2, "0")}_06-00-00`;
  const plainRemote = [];
  for (let d = 1; d <= 8; d++) plainRemote.push(`${stampOf(d)}-remote`);
  plainRemote.push(`${stampOf(8)}-remote-2`); // a second pull in the same second, still plain
  const tagged = [`${stampOf(1)}-remote-keep`, `${stampOf(2)}-remote-pre-wipe`];
  const local = [`${stampOf(1)}-start`, `${stampOf(2)}-stop`, `${stampOf(1)}-crash`, `${stampOf(1)}-wipe`, `${stampOf(3)}-restart`];
  const partial = [`${stampOf(9)}-remote.partial`];
  for (const n of [...plainRemote, ...tagged, ...local, ...partial, "notes"]) mkdirSync(join(backups, n), { recursive: true });
  const all = () => readdirSync(backups).sort();

  const plan = sched.planRemotePrune(backups, 3);
  eq("plan keeps newest 3 plain remote pulls", plan.keep, [`${stampOf(7)}-remote`, `${stampOf(8)}-remote`, `${stampOf(8)}-remote-2`]);
  eq("plan deletes the older plain remote pulls", plan.delete, plainRemote.slice(0, 6));
  if (plan.delete.some((n) => tagged.includes(n) || local.includes(n) || partial.includes(n))) fail("plan would delete a tagged, local or partial folder");

  const before = all();
  const dry = sched.pruneRemotePulls(backups, 3, { dryRun: true });
  eq("dry prune reports", dry.deleted.length, 6);
  eq("dry prune deletes nothing", all(), before);

  const real = sched.pruneRemotePulls(backups, 3);
  eq("prune deleted", real.deleted, plainRemote.slice(0, 6));
  eq("after prune", all(), [...plainRemote.slice(6), ...tagged, ...local, ...partial, "notes"].sort());
  eq("prune of fewer than N deletes nothing", sched.pruneRemotePulls(backups, 20).deleted, []);
  eq("a missing backups folder is fine", sched.planRemotePrune(join(work, "none"), 3), { keep: [], delete: [] });

  // --- `saves pull --prune --keep N`: pull first (stubbed), then prune; a failed pull prunes nothing
  const calls = [];
  const okPull = async () => (calls.push("pull"), { ok: true, dest: join(backups, `${stampOf(10)}-remote`), files: 2, bytes: 9, warnings: [] });
  mkdirSync(join(backups, `${stampOf(10)}-remote`));
  const env = { LF_0GRIND_TOKEN_FILE: join(work, "token"), LF_0GRIND_DIR: work, LF_0GRIND_API: "http://127.0.0.1:9" };
  const { writeFileSync } = await import("node:fs");
  writeFileSync(env.LF_0GRIND_TOKEN_FILE, "tok_fake_for_selftest_only");
  r = await quiet(() => remote(["--prune", "--keep", "2"], { ...base, env, savesPull: okPull }));
  eq("pull --prune exit", r.code, 0);
  eq("pull ran once", calls, ["pull"]);
  const left = all().filter((n) => /-remote(-\d+)?$/.test(n));
  eq("pull --prune left the newest 2 plain pulls", left, [`${stampOf(8)}-remote-2`, `${stampOf(10)}-remote`]);
  if (!tagged.every((n) => existsSync(join(backups, n))) || !local.every((n) => existsSync(join(backups, n)))) fail("pull --prune touched a tagged or local backup");

  const afterFail = all();
  const badPull = async () => ({ ok: false, reason: "boom" });
  r = await quiet(() => remote(["--prune", "--keep", "1"], { ...base, env, savesPull: badPull }));
  eq("failed pull exit", r.code, 1);
  eq("failed pull prunes nothing", all(), afterFail);
  r = await quiet(() => remote([], { ...base, env, savesPull: okPull }));
  eq("a plain pull does not prune", all().filter((n) => /-remote(-\d+)?$/.test(n)).length, 2);
} finally {
  safeRemoveTree(work);
}

// --- the scheduled entry starts the pull hidden through quiet-launch, with the keep count
const spawned = [];
const pid = sched.runScheduled({ root: FAKE_ROOT, nodePath: NODE, keep: 7, spawn: (exe, args, o) => (spawned.push({ exe, args, o }), 4242) });
eq("scheduled pid", pid, 4242);
eq("scheduled spawn", spawned, [{ exe: NODE, args: [join(FAKE_ROOT, "tools", "cli.mjs"), "host", "remote", "0grind", "saves", "pull", "--prune", "--keep", "7"], o: { cwd: FAKE_ROOT } }]);

console.log(failed ? `SUMMARY host-remote-schedule-selftest fail ${failed}` : "SUMMARY host-remote-schedule-selftest ok");
process.exit(failed ? 1 : 0);
