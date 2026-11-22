// tools/lib/tool-orphans.mjs - our own long-lived helper processes whose parent is gone (`tools\cli hidden --orphans`).
//
// On 2026-11-22 Git Bash on lf-testbox failed with "fork: Resource temporarily unavailable" at about 520 processes
// (104 conhost, 82 node, 45 bash, 34 git, 18 claude). Most were live agents, but some of ours outlived the agent
// that started them: a PreToolUse hook waiting on a stdin nobody would close (68 h), a dev server, shells of an
// ended agent. The game-process reaper (tools/lib/ram.mjs) only looks at ArmaReforger; this covers node, bash, sh,
// PowerShell and timeout started from this repo. planToolOrphans is pure (tool-orphans-selftest); scan/stop are
// the Windows I/O.

import { spawnSync } from "node:child_process";

export const DEFAULT_MIN_AGE_H = 6;
const NAMES = new Set(["node.exe", "bash.exe", "sh.exe", "powershell.exe", "timeout.exe"]);
// A command line that belongs to this repo's tools.
const OURS = /Life-Framework|[\\/]tools[\\/](cli|remote|lib|hooks|visual|validation|test)[\\/.]|agent-box[\\/]|tools[\\/]cli\.mjs/i;
// Meant to outlive the shell that started it.
const DAEMONS = /main-guard\.mjs\s+(worker|watch)|desktop-helper|ship-queue\.mjs\s+(daemon|worker)|wb-serve|visual-serve|warm-pool|pool-agent/i;

// procs: [{ pid, ppid, name, ageH, cmd }]; alive: Set of live pids; keep: pids that must stay (lane-lock holders).
// Returns the orphans to stop: [{ pid, name, ageH, why }].
export function planToolOrphans(procs, { minAgeH = DEFAULT_MIN_AGE_H, keep = new Set() } = {}) {
  const alive = new Set((procs ?? []).map((p) => p.pid));
  const out = [];
  for (const p of procs ?? []) {
    if (!NAMES.has(String(p.name).toLowerCase())) continue;
    if (alive.has(p.ppid) || keep.has(p.pid)) continue;
    if (!(p.ageH >= minAgeH)) continue;
    const cmd = String(p.cmd ?? "");
    if (!OURS.test(cmd) || DAEMONS.test(cmd)) continue;
    out.push({ pid: p.pid, name: p.name, ageH: p.ageH, why: `parent ${p.ppid} gone, running ${p.ageH.toFixed(1)} h`, cmd: cmd.replace(/\s+/g, " ").slice(0, 140) });
  }
  return out;
}

const PS_SCAN = String.raw`
$now = Get-Date
Get-CimInstance Win32_Process | ForEach-Object {
  $a = -1; try { $a = [math]::Round(($now - $_.CreationDate).TotalHours, 2) } catch {}
  [pscustomobject]@{ pid = [int]$_.ProcessId; ppid = [int]$_.ParentProcessId; name = [string]$_.Name; ageH = $a; cmd = [string]$_.CommandLine }
} | ConvertTo-Json -Compress
`;

export function scanProcesses() {
  const r = spawnSync("powershell", ["-NoProfile", "-NonInteractive", "-Command", PS_SCAN], { windowsHide: true, encoding: "utf8", timeout: 30000, maxBuffer: 64 * 1024 * 1024 });
  if (r.status !== 0) return null;
  try {
    const v = JSON.parse(r.stdout);
    return Array.isArray(v) ? v : [v];
  } catch {
    return null;
  }
}

// Stops each planned pid with its tree; returns the pids stopped.
export function stopOrphans(plan) {
  const done = [];
  for (const o of plan) {
    const r = spawnSync("taskkill", ["/PID", String(o.pid), "/T", "/F"], { windowsHide: true, encoding: "utf8", timeout: 15000 });
    if (r.status === 0) done.push(o.pid);
  }
  return done;
}
