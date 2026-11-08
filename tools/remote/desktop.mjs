// tools/remote/desktop.mjs - the test box's desktop from ssh: screenshots, screen + audio
// recordings, and the virtual display driver. ssh runs in session 0 with no desktop, so every
// capture runs in the signed-in session through a one-shot scheduled task
// (tools/remote/desktop/run-interactive.ps1). docs/worktrees.md "Desktop on the box".

import { spawnSync } from "node:child_process";
import { mkdirSync, readdirSync } from "node:fs";
import { join } from "node:path";
import { psQuote, scp, sshPs } from "./testbox.mjs";

const REMOTE_DIR = "C:/LF/desktop";
const RUN = `powershell -NoProfile -ExecutionPolicy Bypass -File ${REMOTE_DIR}/run-interactive.ps1`;

// Copies tools/remote/desktop/* to the box (small text files; every call, so the box never runs stale scripts).
function pushScripts(root, host) {
  const dir = join(root, "tools", "remote", "desktop");
  const mk = sshPs(host, `New-Item -ItemType Directory -Force ${psQuote(REMOTE_DIR)} | Out-Null`, { timeout: 30000 });
  if (mk.status !== 0) return false;
  const up = scp([...readdirSync(dir).map((f) => join(dir, f)), `${host}:${REMOTE_DIR}/`]);
  if (up.status !== 0) process.stderr.write(up.stderr);
  return up.status === 0;
}

function inSession(host, psFile, args, waitSec) {
  const inner = `-NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File ${REMOTE_DIR}/${psFile} ${args}`;
  return sshPs(host, `${RUN} -WorkDir ${psQuote(REMOTE_DIR)} -WaitSec ${waitSec} -Exe powershell.exe -Arguments ${psQuote(inner)}`, { timeout: (waitSec + 60) * 1000 });
}

function fetchBack(root, host, remoteName, ext) {
  const outDir = join(root, "tmp", "testbox");
  mkdirSync(outDir, { recursive: true });
  const local = join(outDir, `${remoteName.replace(/\.\w+$/, "")}-${new Date().toISOString().replace(/[:.]/g, "-")}${ext}`);
  const r = scp([`${host}:${REMOTE_DIR}/${remoteName}`, local]);
  return r.status === 0 ? local : null;
}

export function cmdScreenshot(root, host) {
  if (!pushScripts(root, host)) return 1;
  const r = inSession(host, "shot.ps1", `-Out ${REMOTE_DIR}/shot.png`, 30);
  process.stdout.write(r.stdout);
  const screens = sshPs(host, `Get-Content ${psQuote(`${REMOTE_DIR}/shot.png.txt`)}`, { timeout: 30000 });
  process.stdout.write(screens.stdout);
  const file = fetchBack(root, host, "shot.png", ".png");
  console.log(file ? `testbox: screenshot ${file}` : "FAIL  no screenshot came back (is a user signed in on the box?)");
  return file ? 0 : 1;
}

export function cmdRecord(root, host, flags) {
  const secs = Math.max(1, Number(flags.find((f) => /^\d+$/.test(f)) ?? 10));
  const gdi = flags.includes("--gdigrab") ? "-Grabber gdigrab" : "";
  if (!pushScripts(root, host)) return 1;
  const r = inSession(host, "record.ps1", `-Seconds ${secs} -Out ${REMOTE_DIR}/clip.mp4 ${gdi}`, secs + 60);
  process.stdout.write(r.stdout);
  const check = sshPs(host, `Get-Content ${psQuote(`${REMOTE_DIR}/clip.mp4.log`)}, ${psQuote(`${REMOTE_DIR}/clip.mp4.txt`)} -ErrorAction SilentlyContinue`, { timeout: 30000 });
  process.stdout.write(check.stdout);
  const file = fetchBack(root, host, "clip.mp4", ".mp4");
  console.log(file ? `testbox: clip ${file}` : "FAIL  no clip came back");
  return file ? 0 : 1;
}

// Starts a program in the box's desktop session and returns at once (a game client, say).
export function cmdStart(root, host, flags) {
  const [exe, ...args] = flags;
  if (!exe) {
    console.log("usage: tools\cli testbox start <exe on the box> [args...]");
    return 2;
  }
  if (!pushScripts(root, host)) return 1;
  const r = sshPs(host, `${RUN} -NoWait -Exe ${psQuote(exe)} -Arguments ${psQuote(args.join(" "))}`, { timeout: 60000 });
  process.stdout.write(r.stdout + r.stderr);
  return r.status ?? 1;
}

export function cmdVdd(root, host) {
  if (!pushScripts(root, host)) return 1;
  const res = spawnSync("ssh", ["-o", "BatchMode=yes", host, `powershell -NoProfile -ExecutionPolicy Bypass -File ${REMOTE_DIR}/install-vdd.ps1`], { stdio: "inherit", windowsHide: true });
  return res.status ?? 1;
}
