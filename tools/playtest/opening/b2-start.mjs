// opening-run: host=outside timeout=1200
// tools/playtest/opening/b2-start.mjs
// Beat B2 of docs/playtest/m4-opening-day.md: the flagship start and the password gate, the desk half, on a scratch host.
//
//   node tools/playtest/opening/b2-start.mjs --host <scratch dir> [--timeout <s>] [--watch-wait <s>] [--cli <file>]
//
//   --host        the scratch host folder (required). It is passed to every command as LF_HOST_DIR. The real host folder is refused.
//   --timeout     seconds `host start --flagship --wait` may run (default 420; the start itself waits up to 300).
//   --watch-wait  seconds to wait for `host watch` to say it adopted the host (default 30).
//   --cli         the cli to call as a child process (default tools/cli.mjs); the selftest points it at a stand-in.
//
// One line per checkbox of the beat, in the beat's order: `BEAT B2 <check-id> PASS|FAIL|BLOCKED <reason>`. Exit 1 only when a
// line is FAIL; exit 2 for a bad call (no BEAT line then). `tools\cli opening` collects the beat scripts by glob.
//
//   start                 `host start --flagship --wait` exits 0 and prints `host: up (pid N)`.
//   save-loaded           the host's newest console.log has `save loaded:` and `Entered online game state.`.
//   server-name           the live server.json names the server exactly `Life Framework | Everon 1989 (Alpha)`.
//   password              the live server.json has a join password (only tested for being set; never printed).
//   join-wrong-password   BLOCKED by #1520: a client join is manual.
//   join-right-password   BLOCKED by #1520: a client join is manual.
//   watch                 `host watch` is started and says it adopted the running host, and its process is still up.
//
// The beat stops what it started before it returns, by pid, on every path (#1692): the watcher it recorded, then the host
// through `host stop`, then a forced kill of what is left (tools/playtest/opening/_scratch.mjs reapHost). A host with no
// save point before the start is "no save yet" (#1693): a start that never reaches its ready line, no console.log or no live
// server.json is BLOCKED there, not FAIL; on a host that has saved before the same findings are FAIL.
// The name and password checks are `configCheck` of tools/host/ready.mjs, split by what it reports.
// Proof: tools/validation/opening-b2-selftest.mjs.

import { closeSync, existsSync, mkdirSync, openSync, readFileSync } from "node:fs";
import { spawn, spawnSync } from "node:child_process";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { READY_LINE, findSavePoints, hostDir, hostPaths, newestConsoleLog, pidAlive } from "../../host/host.mjs";
import { configCheck } from "../../host/ready.mjs";
import { reapHost, recordPid } from "./_scratch.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");

const val = (args, name) => {
  const i = args.indexOf(name);
  return i === -1 || i + 1 >= args.length ? null : args[i + 1];
};

function usage(msg) {
  console.error(`b2-start: ${msg}\nusage: node tools/playtest/opening/b2-start.mjs --host <scratch dir> [--timeout <s>] [--watch-wait <s>] [--cli <file>]`);
  process.exit(2);
}

const sleepMs = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
const firstLine = (t) => (String(t).split(/\r?\n/).find((l) => l.trim()) || "no output").trim();

export async function runB2(opts) {
  const { host, cli } = opts;
  const out = [];
  try {
    await runChecks(opts, out);
  } finally {
    // stops the watcher, then the host, by pid, whatever happened above (a timed-out start leaves its server up too)
    const done = reapHost({ host, cli, say: (m) => console.error(`[b2] ${m}`) });
    if (done.leftover.length) console.error(`[b2] still running: ${done.leftover.map((p) => `${p.role} pid ${p.pid}`).join(", ")}`);
  }
  return out;
}

async function runChecks(opts, out) {
  const { host, cli, timeoutMs, watchWaitMs } = opts;
  const env = { ...process.env, LF_HOST_DIR: host };
  const line = (id, verdict, reason) => out.push({ id, verdict, reason: String(reason).replace(/\s+/g, " ").trim() });
  // "no save yet": no save point under the host's profile before the start (a fresh scratch host).
  const freshHost = findSavePoints(hostPaths(host).profile).length === 0;
  const NO_SAVE = "no save yet on this host";

  // start
  const r = spawnSync(process.execPath, [cli, "host", "start", "--flagship", "--wait"], { cwd: ROOT, env, encoding: "utf8", timeout: timeoutMs, maxBuffer: 16 * 1024 * 1024, windowsHide: true });
  const text = `${r.stdout || ""}${r.stderr || ""}`;
  const up = /^host: up \(pid (\d+)\)/m.exec(text);
  let started = false;
  if (r.error && r.error.code === "ETIMEDOUT") {
    const why = `host start --flagship --wait timed out after ${timeoutMs / 1000}s`;
    line("start", freshHost ? "BLOCKED" : "FAIL", freshHost ? `${NO_SAVE}: ${why}; the server never printed its ready line on a first boot` : why);
  }
  else if (r.status !== 0) line("start", "FAIL", `host start --flagship --wait exited ${r.status}: ${firstLine(text)}`);
  else if (!up) line("start", "FAIL", `host start --flagship --wait printed no \`host: up (pid N)\` line: ${firstLine(text)}`);
  else {
    started = true;
    line("start", "PASS", `host: up (pid ${up[1]})`);
  }

  // save-loaded
  const log = newestConsoleLog(hostPaths(host).profile);
  if (!log) line("save-loaded", freshHost ? "BLOCKED" : "FAIL", freshHost ? `${NO_SAVE}: the host has no console.log yet` : "the host has no console.log yet");
  else {
    const body = readFileSync(log, "utf8");
    const missing = [];
    if (!/save loaded:/.test(body)) missing.push("`save loaded:`");
    if (!body.includes(READY_LINE)) missing.push(`\`${READY_LINE}\``);
    const fresh = missing.length === 1 && missing[0].startsWith("`save") ? " (a host with no save yet loads none: stop it once, or use the maintainer's host, so a save exists)" : "";
    if (missing.length === 1 && missing[0].startsWith("`save") && freshHost) line("save-loaded", "BLOCKED", `${NO_SAVE}: the console log has no \`save loaded:\`, and a host with no save loads none${fresh}`);
    else if (missing.length) line("save-loaded", "FAIL", `the console log has no ${missing.join(" and no ")}${fresh}`);
    else line("save-loaded", "PASS", "the console log has `save loaded:` and `Entered online game state.`");
  }

  // server-name and password: ready.mjs reports them together, so split what it says.
  const cfg = await configCheck({ hostDir: host });
  if (cfg.ok) {
    line("server-name", "PASS", "the live server.json names the server exactly");
    line("password", "PASS", "the live server.json has a join password set");
  } else {
    const problems = cfg.detail.split("; ");
    const nameProblem = problems.find((p) => /server name/.test(p));
    const passProblem = problems.find((p) => /password/.test(p));
    const whole = !nameProblem && !passProblem; // no server.json, or not JSON: both are unreadable
    const noJson = whole && /^no live server\.json/.test(cfg.detail);
    const wholeVerdict = noJson && freshHost ? "BLOCKED" : "FAIL";
    const wholeWhy = wholeVerdict === "BLOCKED" ? `${NO_SAVE}: ${cfg.detail}` : cfg.detail;
    if (nameProblem || whole) line("server-name", nameProblem ? "FAIL" : wholeVerdict, nameProblem || wholeWhy);
    else line("server-name", "PASS", "the live server.json names the server exactly");
    if (passProblem || whole) line("password", passProblem ? "FAIL" : wholeVerdict, passProblem || wholeWhy);
    else line("password", "PASS", "the live server.json has a join password set");
  }

  // the join checks need a game client
  line("join-wrong-password", "BLOCKED", "blocked by #1520 (manual): a join with a wrong password must be refused with the engine's message");
  line("join-right-password", "BLOCKED", "blocked by #1520 (manual): a join with the right password must reach the world and spawn the character in town");

  // watch
  if (!started) {
    const blocked = out.find((o) => o.id === "start")?.verdict === "BLOCKED";
    line("watch", blocked ? "BLOCKED" : "FAIL", "no host is up, so there is nothing for `host watch` to adopt");
  }
  else {
    const logDir = join(host, "logs");
    mkdirSync(logDir, { recursive: true });
    const logFile = join(logDir, "b2-watch.log");
    const fd = openSync(logFile, "w");
    let child;
    try {
      child = spawn(process.execPath, [cli, "host", "watch", "--flagship"], { cwd: ROOT, env, detached: true, stdio: ["ignore", fd, fd], windowsHide: true });
    } finally {
      closeSync(fd);
    }
    child.unref();
    const pid = child.pid;
    recordPid(host, "host watch", pid); // reapHost stops it before this script returns
    let exited = false;
    child.on("exit", () => {
      exited = true;
    });
    const deadline = Date.now() + watchWaitMs;
    let adopted = false;
    while (Date.now() < deadline) {
      sleepMs(500);
      if (existsSync(logFile) && /watching the running host \(pid \d+\)|host up \(pid \d+\)/.test(readFileSync(logFile, "utf8"))) {
        adopted = true;
        break;
      }
      if (!pid || (exited && !pidAlive(pid))) break;
    }
    if (!adopted) {
      const tail = existsSync(logFile) ? firstLine(readFileSync(logFile, "utf8")) : "no output";
      line("watch", "FAIL", `host watch did not say it adopted the host within ${watchWaitMs / 1000}s (${tail}); log ${logFile}`);
    } else if (!pidAlive(pid)) line("watch", "FAIL", `host watch (pid ${pid}) exited right after it adopted the host; log ${logFile}`);
    else line("watch", "PASS", `host watch is running (pid ${pid}); log ${logFile}`);
  }
}

async function main(args) {
  const hostArg = val(args, "--host");
  if (!hostArg) usage("--host <scratch dir> is required");
  const host = resolve(hostArg);
  const real = resolve(hostDir({ ...process.env, LF_HOST_DIR: "" }));
  const same = process.platform === "win32" ? host.toLowerCase() === real.toLowerCase() : host === real;
  if (same) usage(`${host} is the real host folder; give a scratch folder`);
  if (!existsSync(host)) usage(`${host} does not exist`);
  const timeout = Number(val(args, "--timeout") || 420);
  if (!(timeout > 0)) usage("--timeout must be a number of seconds above 0");
  const watchWait = Number(val(args, "--watch-wait") || 30);
  if (!(watchWait > 0)) usage("--watch-wait must be a number of seconds above 0");
  const cli = resolve(val(args, "--cli") || join(ROOT, "tools", "cli.mjs"));
  const lines = await runB2({ host, cli, timeoutMs: timeout * 1000, watchWaitMs: watchWait * 1000 });
  for (const l of lines) console.log(`BEAT B2 ${l.id} ${l.verdict} ${l.reason}`);
  process.exit(lines.some((l) => l.verdict === "FAIL") ? 1 : 0);
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) await main(process.argv.slice(2));
