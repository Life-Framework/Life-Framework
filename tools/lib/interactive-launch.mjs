// tools/lib/interactive-launch.mjs - start a GUI process on the desktop of the
// user logged on at the console, from a process that has no desktop (an ssh
// session runs in session 0).
//
// A game client started over ssh dies before it logs a line: there is no
// window station to render to. The test box keeps a user signed in, so the
// client goes through a one-shot scheduled task with an Interactive principal
// for that user (the way the box starts Steam). The task is removed again once
// the process is found; the process keeps running.
//
// Only used when interactiveNeeded() is true: Windows, over ssh.

import { spawnSync } from "node:child_process";
import { randomBytes } from "node:crypto";
import { join } from "node:path";

const PWSH = join(process.env.SystemRoot || "C:\\Windows", "System32", "WindowsPowerShell", "v1.0", "powershell.exe");

export function interactiveNeeded() {
  return process.platform === "win32" && !!process.env.SSH_CONNECTION && process.env.LF_SHOW_WINDOWS !== "1";
}

// One Windows command-line argument, quoted the way CommandLineToArgvW reads it.
export function winQuote(arg) {
  const s = String(arg);
  if (s !== "" && !/[\s"]/.test(s)) return s;
  return `"${s.replace(/(\\*)"/g, '$1$1\\"').replace(/(\\+)$/, "$1$1")}"`;
}

function psLiteral(s) {
  return `'${String(s).replace(/'/g, "''")}'`;
}

// The PowerShell that registers, starts, finds, and removes the task. Exported
// for the selftest; `marker` is a substring only this process's command line has.
export function launchScript(exe, args, cwd, marker, taskName, timeoutSec = 60) {
  const argLine = args.map(winQuote).join(" ");
  const exeName = exe.split(/[\\/]/).pop();
  return [
    "$ErrorActionPreference = 'Stop'",
    "$owner = Get-CimInstance Win32_Process -Filter \"Name='explorer.exe'\" | Select-Object -First 1 | Invoke-CimMethod -MethodName GetOwner",
    "if (-not $owner -or -not $owner.User) { Write-Output 'error no_desktop_user'; exit 3 }",
    "$user = \"$($owner.Domain)\\$($owner.User)\"",
    `$action = New-ScheduledTaskAction -Execute ${psLiteral(exe)} -Argument ${psLiteral(argLine)} -WorkingDirectory ${psLiteral(cwd)}`,
    "$principal = New-ScheduledTaskPrincipal -UserId $user -LogonType Interactive -RunLevel Limited",
    "$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit ([TimeSpan]::Zero) -MultipleInstances Parallel",
    `Register-ScheduledTask -TaskName ${psLiteral(taskName)} -Action $action -Principal $principal -Settings $settings -Force | Out-Null`,
    "try {",
    `  Start-ScheduledTask -TaskName ${psLiteral(taskName)}`,
    `  $deadline = (Get-Date).AddSeconds(${timeoutSec})`,
    "  $found = $null",
    "  while (-not $found -and (Get-Date) -lt $deadline) {",
    "    Start-Sleep -Milliseconds 500",
    `    $found = Get-CimInstance Win32_Process -Filter "Name='${exeName}'" | Where-Object { $_.CommandLine -and $_.CommandLine.Contains(${psLiteral(marker)}) } | Select-Object -First 1`,
    "  }",
    "  if (-not $found) { Write-Output 'error not_started'; exit 4 }",
    "  Write-Output $found.ProcessId",
    "} finally {",
    `  Unregister-ScheduledTask -TaskName ${psLiteral(taskName)} -Confirm:$false -ErrorAction SilentlyContinue`,
    "}",
  ].join("\n");
}

// Starts exe on the console user's desktop and returns its pid. Throws with the
// helper's reason when it cannot.
export function interactiveSpawnDetached(exe, args, { cwd = "", marker, timeoutSec = 60 } = {}) {
  if (!marker) throw new Error("interactiveSpawnDetached needs a marker (a command-line substring unique to this process)");
  const taskName = `LF-Interactive-${randomBytes(4).toString("hex")}`;
  const script = launchScript(exe, args, cwd, marker, taskName, timeoutSec);
  const encoded = Buffer.from(script, "utf16le").toString("base64");
  const res = spawnSync(PWSH, ["-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-EncodedCommand", encoded], {
    encoding: "utf8",
    windowsHide: true,
    timeout: (timeoutSec + 30) * 1000,
  });
  const out = (res.stdout || "").trim().split(/\r?\n/).pop() || "";
  const pid = Number.parseInt(out, 10);
  if (res.status !== 0 || !pid) throw new Error(`interactive launch failed (${taskName}): ${out || (res.stderr || "").trim() || `exit ${res.status}`}`);
  return pid;
}

export function selftest() {
  const fails = [];
  if (winQuote("plain") !== "plain") fails.push("plain arg was quoted");
  if (winQuote("C:\\Program Files\\x") !== '"C:\\Program Files\\x"') fails.push(`space arg: ${winQuote("C:\\Program Files\\x")}`);
  if (winQuote('a"b') !== '"a\\"b"') fails.push(`quote arg: ${winQuote('a"b')}`);
  if (winQuote("C:\\dir with space\\") !== '"C:\\dir with space\\\\"') fails.push(`trailing backslash: ${winQuote("C:\\dir with space\\")}`);
  const s = launchScript("C:\\G\\Game.exe", ["-profile", "C:\\p q\\c1"], "C:\\G", "-lfSession abc", "LF-Interactive-t");
  if (!s.includes("-LogonType Interactive")) fails.push("task is not interactive");
  if (!s.includes("Name='Game.exe'")) fails.push("process lookup does not use the exe name");
  if (!s.includes("Unregister-ScheduledTask")) fails.push("task is not removed");
  if (!s.includes("'-profile \"C:\\p q\\c1\"'")) fails.push("argument line is not quoted for the task");
  return fails;
}
