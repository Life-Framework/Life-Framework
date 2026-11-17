// tools/lib/gpu-session.mjs - is there a desktop session on this machine that a GPU tool (Workbench, a game
// client) can render in, and where does a GPU launch go?
//
// ssh runs in session 0, which has no display output: Workbench started there dies with
//   RENDER (E): Couldn't build device list for any adapter
// (lf-testbox, 2026-11-17: after a reboot every Workbench and client the pool started failed that way until the
// signed-in desktop session was usable). A GPU tool therefore starts in the signed-in user's desktop session
// (interactive-launch.mjs for a detached process, tools/remote/desktop/in-session.mjs for a whole CLI run). This
// module is the one place that decides
//   gpuRoute()         "direct" (not over ssh, or already in the desktop session) | "desktop-session"
//   verdict(probe)     whether a desktop session exists and is Active (a disconnected one has no outputs), as ONE line
//   requireDesktopSession()   the fail-fast: throws NoDesktopSession with that line instead of hanging or failing deep
//                             inside a launch
// and holds the PowerShell that reads the state (PS_GPU_PROBE: the same text runs from the pool's state reads, from
// a launch, and from the logon helper). The helper (desktopHelperScript, a scheduled task "At log on" made by
// `tools\cli desktop-helper setup`) runs in the desktop session, writes the probe to a heartbeat file every few
// seconds and keeps the display awake. docs/setup.md "Desktop helper"; tools/validation/gpu-session-selftest.mjs.

import { spawnSync } from "node:child_process";
import { readFileSync } from "node:fs";
import { hostname } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const POOL_JSON = join(dirname(fileURLToPath(import.meta.url)), "..", "pool.json");

export const NO_SESSION_CODE = "LF_NO_DESKTOP_SESSION";
export const HEARTBEAT_FILE = "C:/LF/desktop/gpu-session.json";
export const HELPER_TASK = "LF-DesktopHelper";
export const HEARTBEAT_STALE_S = 45;

export class NoDesktopSession extends Error {
  constructor(message) {
    super(message);
    this.name = "NoDesktopSession";
    this.code = NO_SESSION_CODE;
  }
}

// "direct": the process may start a GPU tool itself (a desktop machine, a process already inside the desktop session
// -- LF_WB_INTERACTIVE=1 -- or LF_SHOW_WINDOWS=1 asked for). "desktop-session": it is over ssh, so the tool must go
// through the interactive-session path. Pure.
export function gpuRoute({ platform = process.platform, env = process.env } = {}) {
  if (platform !== "win32") return "direct";
  if (env.LF_WB_INTERACTIVE === "1" || env.LF_SHOW_WINDOWS === "1") return "direct";
  return env.SSH_CONNECTION ? "desktop-session" : "direct";
}

// Commands whose work starts a GPU tool in-process (no router, no interactive spawn of their own). Over ssh the
// whole CLI run is moved into the desktop session (cliInDesktopSession) before any of it starts.
// `call` only for the wb_* tools: the MCP starts a Workbench itself. Pure: args is the argv after "cli.mjs".
export function needsDesktopRerun(args) {
  const [cmd, a] = args;
  if (cmd === "ui-image") return true;
  if (cmd === "asset") return a === "run"; // new, status, doctor and selftest start no Workbench
  if (cmd === "pack") return !["new", "check", "explain"].includes(a);
  if (cmd === "call") return /^wb_/.test(String(a ?? ""));
  return false;
}

// The PowerShell that reads the desktop session: one line `lf-gpu: {json}` on stdout. Works from session 0 (ssh) and
// from the desktop session. state is the WTS connect state of the session explorer.exe runs in: 0 Active,
// 1 Connected, 4 Disconnected (no display outputs: Workbench cannot render), -1 unknown.
const PS_GPU_PROBE_BODY = [
  "if (-not ('LfWts' -as [type])) { Add-Type -TypeDefinition 'using System; using System.Runtime.InteropServices; public static class LfWts { [DllImport(\"wtsapi32.dll\", SetLastError=true)] static extern bool WTSQuerySessionInformation(IntPtr h, int sid, int cls, out IntPtr buf, out int len); [DllImport(\"wtsapi32.dll\")] static extern void WTSFreeMemory(IntPtr p); public static int State(int sid) { IntPtr b; int n; if (!WTSQuerySessionInformation(IntPtr.Zero, sid, 8, out b, out n)) return -1; int s = Marshal.ReadInt32(b); WTSFreeMemory(b); return s; } }' }",
  "$lfg = @{ user = ''; session = -1; state = -1; locked = $false; monitors = 0; error = '' }",
  "try {",
  "  $ex = @(Get-CimInstance Win32_Process -Filter \"Name='explorer.exe'\" -ErrorAction Stop | Where-Object { $_.SessionId -gt 0 }) | Select-Object -First 1",
  "  if ($ex) {",
  "    $o = Invoke-CimMethod -InputObject $ex -MethodName GetOwner",
  "    $lfg.user = \"$($o.Domain)\\$($o.User)\"; $lfg.session = [int]$ex.SessionId; $lfg.state = [LfWts]::State([int]$ex.SessionId)",
  "    $lfg.locked = [bool](@(Get-Process -Name LogonUI -ErrorAction SilentlyContinue | Where-Object { $_.SessionId -eq $ex.SessionId }).Count)",
  "    $lfg.monitors = @(Get-CimInstance -Namespace root\\wmi -ClassName WmiMonitorBasicDisplayParams -ErrorAction SilentlyContinue | Where-Object { $_.Active }).Count",
  "  }",
  "} catch { $lfg.error = $_.Exception.Message }",
].join("\n");
export const PS_GPU_PROBE = PS_GPU_PROBE_BODY + "\n[Console]::Out.WriteLine('lf-gpu: ' + ($lfg | ConvertTo-Json -Compress))";

// The heartbeat the logon helper writes (one line `lf-gpu-hb: {json}`; nothing when there is no file).
export const PS_READ_HEARTBEAT = [
  `$lfhb = '${HEARTBEAT_FILE.replace(/\//g, "\\")}'`,
  "if (Test-Path -LiteralPath $lfhb) { try { [Console]::Out.WriteLine('lf-gpu-hb: ' + ((Get-Content -LiteralPath $lfhb -Raw).Trim() -replace '\\s+', ' ')) } catch {} }",
].join("\n");

// Parses `lf-gpu: {json}` out of a script's output: { user, session, state, locked, monitors, error } or null.
export function parseProbe(text) {
  const m = /^lf-gpu: (\{.*\})\s*$/m.exec(String(text ?? ""));
  if (!m) return null;
  try {
    const p = JSON.parse(m[1]);
    return { user: String(p.user ?? ""), session: Number(p.session ?? -1), state: Number(p.state ?? -1), locked: !!p.locked, monitors: Number(p.monitors ?? 0), error: String(p.error ?? "") };
  } catch {
    return null;
  }
}

const STATE_NAMES = { 0: "Active", 1: "Connected", 2: "ConnectQuery", 3: "Shadow", 4: "Disconnected", 5: "Idle", 6: "Listen", 7: "Reset", 8: "Down", 9: "Init" };
export const stateName = (s) => STATE_NAMES[s] ?? "unknown";

// Pure: { ok, line, note } for a probe read on machine `name`. `line` is the one line a caller prints when !ok.
// Nobody signed in, or the session disconnected (state 4 and the like), cannot render. A locked session still can
// (Workbench draws on the default desktop), so it is ok with a note, never a refusal. A probe that did not answer
// at all (probe === null) is unknown, not a refusal: the machine may not be reachable, which the caller decides.
export function verdict(probe, name = "this machine") {
  if (!probe) return { ok: null, line: `${name}: desktop session unknown (no answer)`, note: "unknown" };
  if (probe.error && !probe.user) return { ok: null, line: `${name}: desktop session unknown (${probe.error.split("\n")[0]})`, note: "unknown" };
  if (!probe.user || probe.session < 1) return { ok: false, line: `${name} has no signed-in desktop session: log in once`, note: "nobody signed in" };
  if (probe.state !== 0 && probe.state !== 1 && probe.state !== -1) {
    return { ok: false, line: `${name} has no usable desktop session (${probe.user} in session ${probe.session} is ${stateName(probe.state)}): sit at the machine or reconnect once`, note: `session ${stateName(probe.state)}` };
  }
  const bits = [probe.user, `session ${probe.session}`, stateName(probe.state), `${probe.monitors} monitor(s)`];
  if (probe.locked) bits.push("locked");
  return { ok: true, line: `${name}: desktop session ${bits.join(", ")}`, note: probe.locked ? "locked (still renders)" : "ok" };
}

// This machine's pool name ("lf-testbox" on the laptop): the entry of tools/pool.json whose "hostname" is this host's,
// else the hostname itself. Read straight from the file (no pool.mjs import: a launch helper stays light).
export function machineLabel(host = hostname(), poolFile = POOL_JSON) {
  try {
    const machines = JSON.parse(readFileSync(poolFile, "utf8")).machines ?? [];
    const mine = machines.find((m) => m.hostname && String(m.hostname).toLowerCase() === String(host).toLowerCase());
    if (mine?.name) return mine.name;
  } catch {}
  return host;
}

const PWSH = join(process.env.SystemRoot || "C:\\Windows", "System32", "WindowsPowerShell", "v1.0", "powershell.exe");

// Reads this machine's desktop session (a ~1 s powershell). null when it cannot run.
export function readLocalProbe({ spawn = spawnSync } = {}) {
  if (process.platform !== "win32") return null;
  const encoded = Buffer.from(PS_GPU_PROBE, "utf16le").toString("base64");
  const r = spawn(PWSH, ["-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-EncodedCommand", encoded], { encoding: "utf8", windowsHide: true, timeout: 30000 });
  return parseProbe(r?.stdout);
}

let cached = null;

// The fail-fast. Over ssh on Windows (gpuRoute "desktop-session") a GPU tool needs a desktop session; without one this
// throws NoDesktopSession with the one clear line. A direct route (a desktop machine, or already inside the desktop
// session) needs no check. An unreadable probe never blocks a launch: the launch itself reports its own failure.
// opts: env/platform (gpuRoute), probe() (a test seam), label (the machine's pool name), cacheMs.
export function requireDesktopSession({ env = process.env, platform = process.platform, probe = readLocalProbe, label, cacheMs = 20000, now = Date.now } = {}) {
  if (gpuRoute({ env, platform }) !== "desktop-session") return { ok: true, skipped: true };
  const name = label ?? machineLabel();
  let p;
  if (cached && probe === readLocalProbe && now() - cached.at < cacheMs) p = cached.p;
  else {
    p = probe();
    if (probe === readLocalProbe) cached = { at: now(), p };
  }
  const v = verdict(p, name);
  if (v.ok === false) throw new NoDesktopSession(v.line);
  return { ...v, ok: true };
}

export function resetCache() {
  cached = null;
}

// One status line for `pool status`: the session verdict and whether the logon helper is alive (heartbeat text or null).
export function desktopLine(name, probe, heartbeatText, { now = Date.now() } = {}) {
  const v = verdict(probe, name);
  const hb = heartbeatText == null ? null : heartbeatState(heartbeatText, { now });
  const helper = hb === null ? "no logon helper heartbeat (tools\\cli desktop-helper setup)" : hb.helper ? `logon helper alive (${hb.ageS} s ago)` : `logon helper stale (${hb.why})`;
  if (v.ok === true) return `GPU session ok: ${v.line.replace(/^[^:]*: desktop session /, "")}; ${helper}`;
  if (v.ok === false) return `NO GPU SESSION: ${v.line}`;
  return `GPU session unknown ${v.line.replace(/^[^:]*: desktop session unknown /, "")}; ${helper}`;
}

// Heartbeat freshness: the helper writes { at: <ms>, ...probe } every few seconds. Pure.
export function heartbeatState(text, { now = Date.now(), staleS = HEARTBEAT_STALE_S } = {}) {
  let j;
  try {
    j = JSON.parse(String(text ?? "").replace(/^\uFEFF/, ""));
  } catch {
    return { helper: false, why: "no heartbeat file" };
  }
  const at = Number(j?.at);
  if (!Number.isFinite(at)) return { helper: false, why: "heartbeat unreadable" };
  const ageS = Math.round((now - at) / 1000);
  return ageS <= staleS ? { helper: true, ageS } : { helper: false, why: `last heartbeat ${ageS} s ago`, ageS };
}

// The logon helper, as the PowerShell text setup writes to C:\LF\desktop\desktop-helper.ps1. It runs hidden in the
// desktop session: every 5 s it asks the display and the system to stay awake (ES_CONTINUOUS | ES_DISPLAY_REQUIRED |
// ES_SYSTEM_REQUIRED) and writes the probe + a timestamp to the heartbeat file. One instance only (a named mutex).
export function desktopHelperScript() {
  return [
    "# LF desktop helper (tools\\cli desktop-helper setup). Runs in the signed-in desktop session from a task 'At log on'.",
    "$ErrorActionPreference = 'Continue'",
    "$created = $false",
    "$mutex = New-Object System.Threading.Mutex($true, 'Local\\LF-DesktopHelper', [ref]$created)",
    "if (-not $created) { exit 0 }",
    "Add-Type -TypeDefinition 'using System; using System.Runtime.InteropServices; public static class LfAwake { [DllImport(\"kernel32.dll\")] public static extern uint SetThreadExecutionState(uint f); }'",
    `$file = '${HEARTBEAT_FILE.replace(/\//g, "\\")}'`,
    "New-Item -ItemType Directory -Force (Split-Path $file) | Out-Null",
    "while ($true) {",
    "  [void][LfAwake]::SetThreadExecutionState(0x80000003)",
    "  $out = & { " + PS_GPU_PROBE_BODY + "\n$lfg | ConvertTo-Json -Compress }",
    "  try { $j = $out | ConvertFrom-Json; $j | Add-Member -NotePropertyName at -NotePropertyValue ([DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()) -Force; $j | Add-Member -NotePropertyName pid -NotePropertyValue $PID -Force; ($j | ConvertTo-Json -Compress) | Set-Content -LiteralPath ($file + '.tmp') -Encoding ASCII; Move-Item -LiteralPath ($file + '.tmp') -Destination $file -Force } catch {}",
    "  Start-Sleep -Seconds 5",
    "}",
    "",
  ].join("\n");
}

// The PowerShell that registers (or replaces) the "At log on" task for the user who runs it and starts it now when
// the desktop session is up. Idempotent: -Force replaces the task; a helper already running stays (its mutex).
export function setupScript({ user, helperPath = "C:\\LF\\desktop\\desktop-helper.ps1" } = {}) {
  const q = (s) => `'${String(s).replace(/'/g, "''")}'`;
  return [
    "$ErrorActionPreference = 'Stop'",
    `$user = ${user ? q(user) : "(whoami)"}`,
    `$a = New-ScheduledTaskAction -Execute 'powershell.exe' -Argument ${q(`-NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File "${helperPath}"`)}`,
    "$t = New-ScheduledTaskTrigger -AtLogOn -User $user",
    "$p = New-ScheduledTaskPrincipal -UserId $user -LogonType Interactive -RunLevel Limited",
    "$s = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit ([TimeSpan]::Zero) -MultipleInstances IgnoreNew -RestartCount 3 -RestartInterval (New-TimeSpan -Minutes 1)",
    `Register-ScheduledTask -TaskName ${q(HELPER_TASK)} -Action $a -Trigger $t -Principal $p -Settings $s -Force | Out-Null`,
    `Write-Output ('registered ' + ${q(HELPER_TASK)} + ' for ' + $user)`,
  ].join("\n");
}
