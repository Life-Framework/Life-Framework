// tools/discord-mcp/relay-ctl.mjs - `tools\cli discord relay start|stop|status` and `discord selftest`.
//
// The relay runs as a detached background process. State lives in the checkout's tmp/
// (gitignored): discord-relay.pid (JSON: pid + start time) and discord-relay.log. A stale pid
// file (process gone, or the pid reused by something that is not the relay) counts as stopped.

import { spawn, spawnSync } from "node:child_process";
import { closeSync, existsSync, mkdirSync, openSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { hookFile, tokenFile } from "./secrets.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const RELAY = join(HERE, "welcome-relay.mjs");
const MARK = "welcome-relay.mjs";

const files = (root) => ({ dir: join(root, "tmp"), pid: join(root, "tmp", "discord-relay.pid"), log: join(root, "tmp", "discord-relay.log") });

function readPidFile(path) {
  try {
    const j = JSON.parse(readFileSync(path, "utf8"));
    return Number.isInteger(j.pid) ? j : null;
  } catch {
    return null;
  }
}

function commandLine(pid) {
  if (process.platform === "win32") {
    const r = spawnSync(
      "powershell.exe",
      ["-NoProfile", "-NonInteractive", "-Command", `(Get-CimInstance Win32_Process -Filter 'ProcessId=${pid}').CommandLine`],
      { encoding: "utf8", windowsHide: true, timeout: 15000 },
    );
    return r.status === 0 ? r.stdout.trim() : "";
  }
  const r = spawnSync("ps", ["-p", String(pid), "-o", "args="], { encoding: "utf8", timeout: 15000 });
  return r.status === 0 ? r.stdout.trim() : "";
}

// True only when pid is alive AND is the relay (guards against a reused pid).
function isRelay(pid) {
  try {
    process.kill(pid, 0);
  } catch {
    return false;
  }
  return commandLine(pid).includes(MARK);
}

function tail(path, n) {
  try {
    return readFileSync(path, "utf8").trimEnd().split("\n").slice(-n);
  } catch {
    return [];
  }
}

function missingSecrets() {
  const out = [];
  if (!existsSync(tokenFile())) out.push(`bot token: ${tokenFile()}`);
  if (!existsSync(hookFile())) out.push(`webhook URL: ${hookFile()}`);
  return out;
}

function start(root) {
  const f = files(root);
  const cur = readPidFile(f.pid);
  if (cur && isRelay(cur.pid)) {
    console.log(`relay already running (pid ${cur.pid}); log ${f.log}`);
    return 0;
  }
  const missing = missingSecrets();
  if (missing.length) {
    console.log("FAIL  missing secret file(s), see tools/discord-mcp/README.md:");
    for (const m of missing) console.log(`      ${m}`);
    return 1;
  }
  mkdirSync(f.dir, { recursive: true });
  const out = openSync(f.log, "a");
  const child = spawn(process.execPath, [RELAY], { detached: true, windowsHide: true, stdio: ["ignore", out, out] });
  child.unref();
  closeSync(out);
  writeFileSync(f.pid, JSON.stringify({ pid: child.pid, started: new Date().toISOString() }) + "\n");
  console.log(`relay started (pid ${child.pid}); log ${f.log}`);
  console.log("check it with: tools\\cli discord relay status");
  return 0;
}

function stop(root) {
  const f = files(root);
  const cur = readPidFile(f.pid);
  if (!cur) {
    console.log("relay not running (no pid file)");
    return 0;
  }
  if (!isRelay(cur.pid)) {
    rmSync(f.pid, { force: true });
    console.log(`relay not running (pid ${cur.pid} is gone or not the relay); removed the stale pid file`);
    return 0;
  }
  try {
    process.kill(cur.pid);
  } catch (e) {
    console.log(`FAIL  could not stop pid ${cur.pid}: ${e.message}`);
    return 1;
  }
  rmSync(f.pid, { force: true });
  console.log(`relay stopped (pid ${cur.pid})`);
  return 0;
}

function status(root) {
  const f = files(root);
  const cur = readPidFile(f.pid);
  if (!cur || !isRelay(cur.pid)) {
    console.log(cur ? `relay: stopped (stale pid file, pid ${cur.pid})` : "relay: stopped");
    const t = tail(f.log, 3);
    if (t.length) console.log("last log lines:\n  " + t.join("\n  "));
    return 1;
  }
  console.log(`relay: running (pid ${cur.pid}, started ${cur.started})`);
  console.log("last log lines:\n  " + (tail(f.log, 5).join("\n  ") || "(empty)"));
  return 0;
}

const USAGE = `usage:
  tools\\cli discord relay start|stop|status   background welcome relay (pid file + log in tmp/)
  tools\\cli discord selftest                  check the event filter (joins, #welcome, DMs; no bots, no self)`;

export async function runDiscordCli(root, args) {
  const [sub, action] = args;
  if (sub === "selftest") {
    const r = spawnSync(process.execPath, [join(root, "tools", "validation", "discord-relay-selftest.mjs")], { stdio: "inherit", windowsHide: true });
    return r.status ?? 1;
  }
  if (sub === "relay" && action === "start") return start(root);
  if (sub === "relay" && action === "stop") return stop(root);
  if (sub === "relay" && action === "status") return status(root);
  console.log(USAGE);
  return sub ? 1 : 0;
}
