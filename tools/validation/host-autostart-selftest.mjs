// tools/validation/host-autostart-selftest.mjs
// inputs: tools/host/autostart.mjs, tools/host/commands/autostart.mjs
//
// Proves `tools\cli host autostart install|remove|status` without ever touching the real
// Task Scheduler: every `schtasks` call goes through an injected fake that keeps the
// "registered" task in memory. Install builds the exact task arguments, a second install
// changes nothing, a moved checkout re-registers, remove deletes, status parses a fixture
// query (registered, never run, not registered), a timed-out call fails, the main checkout
// is refused, and --dry-run never reaches the runner. The logon entry starts
// `host watch --flagship` hidden (quiet-launch).
// red-proof: before tools/host/autostart.mjs existed the import failed:
// "FAIL autostart.mjs missing (Cannot find module ...)" (2026-11-15).

import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
let failed = 0;
function fail(msg) {
  console.log(`FAIL  ${msg}`);
  failed++;
}
function eq(label, got, want) {
  if (JSON.stringify(got) !== JSON.stringify(want)) fail(`${label}: got ${JSON.stringify(got)}, want ${JSON.stringify(want)}`);
}

// `schtasks /Query /FO CSV /V /NH` is one CSV line; these are the columns the code reads.
function csv(fields) {
  return fields.map((f) => `"${String(f).replace(/"/g, '""')}"`).join(",");
}
function queryLine(task, { lastRun = "15/11/2026 9:05:00 AM", lastResult = "0" } = {}) {
  const f = new Array(20).fill("N/A");
  f[0] = "HOSTPC";
  f[1] = `\\${task.name}`;
  f[3] = "Ready";
  f[4] = "Interactive only";
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
    if (opts.timeout) return { status: null, stdout: "", stderr: "", error: Object.assign(new Error("spawnSync schtasks.exe ETIMEDOUT"), { code: "ETIMEDOUT" }) };
    const verb = args[0];
    const arg = (flag) => args[args.indexOf(flag) + 1];
    if (verb === "/Create") {
      s.task = { name: arg("/TN"), command: arg("/TR"), user: arg("/RU") };
      return { status: 0, stdout: "SUCCESS: The scheduled task has been created.\r\n", stderr: "" };
    }
    if (verb === "/Query") {
      if (!s.task || s.task.name !== arg("/TN")) return { status: 1, stdout: "", stderr: "ERROR: The system cannot find the file specified.\r\n" };
      return { status: 0, stdout: `${queryLine(s.task, { lastRun: s.lastRun, lastResult: s.lastResult })}\r\n`, stderr: "" };
    }
    if (verb === "/Delete") {
      if (!s.task) return { status: 1, stdout: "", stderr: "ERROR: The system cannot find the file specified.\r\n" };
      s.task = null;
      return { status: 0, stdout: "SUCCESS: The scheduled task was successfully deleted.\r\n", stderr: "" };
    }
    return { status: 2, stdout: "", stderr: `unexpected schtasks verb ${verb}` };
  };
  s.verbs = (v) => s.calls.filter((c) => c.args[0] === v).length;
  return s;
}

let auto, cmd;
try {
  auto = await import(pathToFileURL(join(ROOT, "tools", "host", "autostart.mjs")).href);
  cmd = await import(pathToFileURL(join(ROOT, "tools", "host", "commands", "autostart.mjs")).href);
} catch (e) {
  fail(`autostart.mjs missing (${e.message})`);
  console.log("SUMMARY host-autostart-selftest fail 1");
  process.exit(1);
}

if (!cmd.help || typeof cmd.main !== "function") fail("commands/autostart.mjs lacks help/main");

const FAKE_ROOT = "C:\\lf\\Life-Framework-ws-host";
const NODE = "C:\\Program Files\\nodejs\\node.exe";
const USER = "HOSTPC\\jasper";
const base = { root: FAKE_ROOT, hostDir: FAKE_ROOT, isMain: false, nodePath: NODE, user: USER };
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

// --- the exact task arguments
const wantCommand = `"${NODE}" "C:\\lf\\Life-Framework-ws-host\\tools\\host\\autostart.mjs" --run`;
eq("task name", auto.TASK_NAME, "LifeFramework Host Autostart");
eq("create args", auto.createArgs({ root: FAKE_ROOT, nodePath: NODE, user: USER }), [
  "/Create", "/TN", auto.TASK_NAME, "/TR", wantCommand,
  "/SC", "ONLOGON", "/RU", USER, "/RL", "LIMITED", "/DELAY", "0000:30", "/F",
]);

// --- install, then a second install is a no-op
const s = fakeScheduler();
let r = await quiet(() => cmd.main(["install"], { ...base, runner: s.run }));
eq("install exit", r.code, 0);
eq("install creates once", s.verbs("/Create"), 1);
eq("registered task command", s.task?.command, wantCommand);
eq("registered task user", s.task?.user, USER);
r = await quiet(() => cmd.main(["install"], { ...base, runner: s.run }));
eq("second install exit", r.code, 0);
eq("second install creates nothing more", s.verbs("/Create"), 1);
if (!/already/i.test(r.out)) fail(`second install says nothing about already installed: ${r.out}`);

// --- the checkout moved: the stale task is replaced, once
const moved = { ...base, root: "C:\\lf\\Life-Framework-ws-other" };
r = await quiet(() => cmd.main(["install"], { ...moved, runner: s.run }));
eq("moved install exit", r.code, 0);
eq("moved install re-creates", s.verbs("/Create"), 2);
if (!s.task?.command.includes("Life-Framework-ws-other")) fail("moved install kept the old path");

// --- every schtasks call has a timeout
if (!s.calls.length || s.calls.some((c) => !(c.timeoutMs > 0 && c.timeoutMs <= 60000))) fail("a schtasks call has no bounded timeout");

// --- status parses a fixture query
const info = auto.parseQuery(`${queryLine({ name: auto.TASK_NAME, command: wantCommand, user: USER })}\r\n`);
eq("parse registered", info.registered, true);
eq("parse command", info.command, wantCommand);
eq("parse user", info.user, USER);
eq("parse status", info.status, "Ready");
eq("parse lastRun", info.lastRun, "15/11/2026 9:05:00 AM");
eq("parse lastResult", info.lastResult, 0);
eq("parse ran", info.ran, true);
const never = auto.parseQuery(`${queryLine({ name: auto.TASK_NAME, command: wantCommand, user: USER }, { lastRun: "30/11/1999 12:00:00 AM", lastResult: "267011" })}\n`);
eq("never-run task", never.ran, false);

s.task = null;
s.calls.length = 0;
await quiet(() => cmd.main(["install"], { ...base, runner: s.run }));
s.lastRun = "15/11/2026 9:05:00 AM";
r = await quiet(() => cmd.main(["status"], { ...base, runner: s.run }));
eq("status registered exit", r.code, 0);
if (!/registered/i.test(r.out) || !r.out.includes("9:05:00") || !r.out.includes(USER)) fail(`status output lacks registered/last run/user: ${r.out}`);
s.lastRun = "30/11/1999 12:00:00 AM";
s.lastResult = "267011";
r = await quiet(() => cmd.main(["status"], { ...base, runner: s.run }));
if (!/never/i.test(r.out)) fail(`status of a task that never ran does not say so: ${r.out}`);

// --- remove deletes, a second remove is a no-op
r = await quiet(() => cmd.main(["remove"], { ...base, runner: s.run }));
eq("remove exit", r.code, 0);
eq("task gone after remove", s.task, null);
eq("remove deletes once", s.verbs("/Delete"), 1);
r = await quiet(() => cmd.main(["remove"], { ...base, runner: s.run }));
eq("second remove exit", r.code, 0);
eq("second remove deletes nothing more", s.verbs("/Delete"), 1);
r = await quiet(() => cmd.main(["status"], { ...base, runner: s.run }));
eq("status when not registered exits 1", r.code, 1);
if (!/not registered/i.test(r.out)) fail(`status when absent: ${r.out}`);

// --- a hung schtasks fails the command
const hung = fakeScheduler({ timeout: true });
r = await quiet(() => cmd.main(["install"], { ...base, runner: hung.run }));
eq("timeout install exit", r.code, 1);
if (!/timed out/i.test(r.out)) fail(`timeout not reported: ${r.out}`);

// --- the main checkout is the world-editor copy: refuse, touch nothing
const guarded = fakeScheduler();
r = await quiet(() => cmd.main(["install"], { ...base, isMain: true, runner: guarded.run }));
eq("main checkout install exit", r.code, 1);
eq("main checkout touched schtasks", guarded.calls.length, 0);
if (!/main checkout/i.test(r.out)) fail(`main checkout refusal not explained: ${r.out}`);

// --- --dry-run prints the arguments and never reaches the runner
const trip = fakeScheduler();
r = await quiet(() => cmd.main(["install", "--dry-run"], { ...base, runner: trip.run }));
eq("dry-run exit", r.code, 0);
eq("dry-run reached the runner", trip.calls.length, 0);
if (!r.out.includes("/SC") || !r.out.includes("ONLOGON")) fail(`dry-run does not print the task arguments: ${r.out}`);

// --- bad usage
r = await quiet(() => cmd.main(["bogus"], { ...base, runner: guarded.run }));
eq("unknown sub exit", r.code, 1);

// --- the logon entry starts `host watch --flagship` hidden through quiet-launch
const spawned = [];
const pid = auto.runAtLogon({ root: FAKE_ROOT, nodePath: NODE, spawn: (exe, args, o) => (spawned.push({ exe, args, o }), 4242) });
eq("logon pid", pid, 4242);
eq("logon spawn", spawned, [{ exe: NODE, args: [join(FAKE_ROOT, "tools", "cli.mjs"), "host", "watch", "--flagship"], o: { cwd: FAKE_ROOT } }]);

console.log(failed ? `SUMMARY host-autostart-selftest fail ${failed}` : "SUMMARY host-autostart-selftest ok");
process.exit(failed ? 1 : 0);
