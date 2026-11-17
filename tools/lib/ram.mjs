// tools/lib/ram.mjs - where a machine's RAM goes (by role), and which game or Workbench processes no one owns.
// `tools\cli ram` prints both; the pool reaps the orphans (reapOrphansThrottled, every 10 min per machine).
// docs/architecture/ram-budget.md has the measurements and the role table this produces.
//
// Pure parts (roleOf, groupRoles, planOrphans, parseFacts) are proven by tools/validation/ram-selftest.mjs; the
// PowerShell (PS_SNAPSHOT, PS_FACTS) runs here or over ssh with the same output.
//
// An orphan is only ever an ArmaReforger process launched by our tools: its -profile is a checkout's
// <root>\server\profile\<x> or a session folder under %LOCALAPPDATA%\LifeFramework\session. The user's own game,
// server or Workbench (any other -profile, or none) is never a candidate, and neither is the look viewer
// (visual-serve: look-serve closes itself when idle, pool.reapRemoteLookServer when a job is starved).

import { spawnSync } from "node:child_process";
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";

// ---------------------------------------------------------------- roles (the whole process table)

export const ROLES = [
  "claude agents",
  "MCP servers (claude children)",
  "tools/cli node",
  "main guard",
  "game: test servers",
  "game: session servers",
  "game: clients",
  "Workbench",
  "T3",
  "git",
  "shells/ssh",
  "svc: Defender",
  "svc: Search",
  "svc: NVIDIA",
  "svc: Steam",
  "Windows",
  "other user apps",
];

const WINDOWS = /^(svchost|System|Registry|Memory Compression|csrss|lsass|services|dwm|explorer|smss|wininit|winlogon|fontdrvhost|Secure System|RuntimeBroker|sihost|ctfmon|StartMenuExperienceHost|ShellExperienceHost|TextInputHost|ApplicationFrameHost|dllhost|audiodg|WmiPrvSE|spoolsv|WUDFHost|taskhostw|ShellHost|LsaIso|MoUsoCoreWorker|SecurityHealth|smartscreen|backgroundTaskHost|System Idle Process|SearchApp|UserOOBEBroker|unsecapp|dasHost|AggregatorHost|SgrmBroker|WidgetService|Widgets|LockApp)(\.exe)?$/i;

// The role of one process. byPid: Map pid -> process (for the parent).
export function roleOf(p, byPid = new Map()) {
  const n = String(p.name ?? "");
  const c = String(p.cmd ?? "");
  const parent = byPid.get(p.ppid);
  if (/^claude(\.exe)?$/i.test(n)) return "claude agents";
  if (/^ArmaReforgerWorkbench/i.test(n)) return "Workbench";
  if (/^ArmaReforgerServer/i.test(n)) return /\Wprofile\Wtest("|\s|$)/i.test(c) ? "game: test servers" : "game: session servers";
  if (/^ArmaReforgerSteam/i.test(n)) return "game: clients";
  if (/^node/i.test(n) && /main-guard/i.test(c)) return "main guard";
  if (/^node/i.test(n) && /^claude/i.test(String(parent?.name ?? ""))) return "MCP servers (claude children)";
  if (/^node/i.test(n) && /Life-Framework|tools[\\/]|agent-box[\\/]/i.test(c)) return "tools/cli node";
  if (/^(t3|T3 Code)/i.test(n)) return "T3";
  if (/^git/i.test(n)) return "git";
  if (/^(MsMpEng|MpDefenderCoreService|NisSrv)/i.test(n)) return "svc: Defender";
  if (/^(SearchIndexer|SearchProtocolHost|SearchFilterHost|SearchHost)/i.test(n)) return "svc: Search";
  if (/^(NVIDIA|nvcontainer|NVDisplay)/i.test(n)) return "svc: NVIDIA";
  if (/^steam/i.test(n)) return "svc: Steam";
  if (/^(powershell|pwsh|bash|cmd|conhost|sh|ssh|sshd|sshd-session|OpenConsole)(\.exe)?$/i.test(n)) return "shells/ssh";
  if (WINDOWS.test(n)) return "Windows";
  return "other user apps";
}

// { role: { n, wsMb, privMb } } plus mcpPerClaude ({ claudePid: count }) and the claude count.
export function groupRoles(procs) {
  const byPid = new Map((procs ?? []).map((p) => [p.pid, p]));
  const roles = {};
  const mcpPerClaude = {};
  for (const p of procs ?? []) {
    const r = roleOf(p, byPid);
    roles[r] ??= { n: 0, wsMb: 0, privMb: 0 };
    roles[r].n++;
    roles[r].wsMb += Number(p.ws) || 0;
    roles[r].privMb += Number(p.priv) || 0;
    if (r === "claude agents") mcpPerClaude[p.pid] ??= 0;
    if (r === "MCP servers (claude children)") mcpPerClaude[p.ppid] = (mcpPerClaude[p.ppid] ?? 0) + 1;
  }
  return { roles, mcpPerClaude };
}

// The whole process table as one JSON line: { totalMb, freeMb, commitMb, commitLimitMb, procs: [...] }.
export const PS_SNAPSHOT = String.raw`
$os = Get-CimInstance Win32_OperatingSystem
$procs = @(Get-CimInstance Win32_Process | ForEach-Object {
  $cl = [string]$_.CommandLine; if ($cl.Length -gt 400) { $cl = $cl.Substring(0, 400) }
  [pscustomobject]@{ pid = $_.ProcessId; ppid = $_.ParentProcessId; name = $_.Name; ws = [int]($_.WorkingSetSize / 1MB); priv = [int]($_.PrivatePageCount / 1MB); cmd = $cl }
})
$o = [pscustomobject]@{ totalMb = [int]($os.TotalVisibleMemorySize / 1024); freeMb = [int]($os.FreePhysicalMemory / 1024); commitMb = [int](($os.TotalVirtualMemorySize - $os.FreeVirtualMemory) / 1024); commitLimitMb = [int]($os.TotalVirtualMemorySize / 1024); procs = $procs }
Write-Output ('lf-ram: snapshot ' + ($o | ConvertTo-Json -Depth 3 -Compress))
`;

// ---------------------------------------------------------------- orphans (ArmaReforger processes only)

// Facts about each ArmaReforger process that decide whether anyone owns it, gathered where it runs:
//   profile        its -profile argument
//   root, rootOk   for <root>\server\profile\<x>: the checkout, and whether it still exists
//   rec            for a session folder <dir>\(server|clientN): its session.json (stoppedAt, serverPid, client pids),
//                  or 'none' when the file is missing
export const PS_FACTS = String.raw`
$rows = @()
foreach ($p in @(Get-CimInstance Win32_Process -Filter "Name LIKE 'ArmaReforger%'")) {
  $c = [string]$p.CommandLine; $prof = $null
  if ($c -match '-profile\s+"([^"]+)"') { $prof = $matches[1] } elseif ($c -match '-profile\s+(\S+)') { $prof = $matches[1] }
  $root = $null; $rootOk = $null; $rec = $null
  if ($prof -and $prof -match '^(.+?)\Wserver\Wprofile\W[^\\/]+$') { $root = $matches[1]; $rootOk = [bool](Test-Path -LiteralPath $root) }
  if ($prof -and $prof -match '^(.+\WLifeFramework\Wsession\W[^\\/]+)\W(server|client\d+)$') {
    $f = Join-Path $matches[1] 'session.json'
    if (Test-Path -LiteralPath $f) {
      try { $s = [IO.File]::ReadAllText($f) | ConvertFrom-Json; $rec = [pscustomobject]@{ stoppedAt = $s.stoppedAt; serverPid = $s.serverPid; clientPids = @($s.clients | ForEach-Object { $_.pid }) } } catch { $rec = 'unreadable' }
    } else { $rec = 'none' }
  }
  $age = $null; if ($p.CreationDate) { $age = [int]((Get-Date) - $p.CreationDate).TotalMinutes }
  $rows += [pscustomobject]@{ pid = $p.ProcessId; name = $p.Name; ageMin = $age; wsMb = [int]($p.WorkingSetSize / 1MB); privMb = [int]($p.PrivatePageCount / 1MB); profile = $prof; root = $root; rootOk = $rootOk; rec = $rec; tool = [bool]($c -match '-exitAfterInit'); look = [bool]($c -match 'LF_VisualShotsPlugin|visual-serve') }
}
Write-Output ('lf-ram: facts ' + (ConvertTo-Json -InputObject @($rows) -Depth 4 -Compress))
`;

export function parseLine(text, tag) {
  const m = new RegExp(`^lf-ram: ${tag} (.*)$`, "m").exec(String(text ?? ""));
  if (!m) return null;
  try {
    return JSON.parse(m[1]);
  } catch {
    return null;
  }
}
export const parseFacts = (text) => {
  const v = parseLine(text, "facts");
  return Array.isArray(v) ? v : v ? [v] : null;
};

// Minutes a process may live before it is a candidate at all (it may still be registering its owner).
export const GRACE_MIN = 10;
// A test server past this is no test run's: test runs are budgeted well under it (docs/worktrees.md budgets).
export const TEST_MAX_MIN = 120;
// A Workbench started with -exitAfterInit (rdb refresh, compile) past this has hung.
export const TOOL_MAX_MIN = 60;

const isTestProfile = (p) => /\Wserver\Wprofile\Wtest$/i.test(String(p ?? ""));
const isServer = (f) => /^ArmaReforgerServer/i.test(String(f.name ?? ""));
const isWb = (f) => /^ArmaReforgerWorkbench/i.test(String(f.name ?? ""));

// [{ pid, name, wsMb, why }] to stop. Conservative: only our processes, never a young one, never the look viewer.
export function planOrphans(facts, { graceMin = GRACE_MIN, testMaxMin = TEST_MAX_MIN, toolMaxMin = TOOL_MAX_MIN } = {}) {
  const out = [];
  for (const f of facts ?? []) {
    if (!f || !f.profile) continue; // not launched by our tools
    const ours = f.root || f.rec !== null && f.rec !== undefined;
    if (!ours || f.look) continue;
    const age = Number(f.ageMin);
    if (!Number.isFinite(age) || age < graceMin) continue;
    let why = null;
    if (f.root && f.rootOk === false) why = `its checkout ${f.root} is gone`;
    else if (f.rec === "none") why = "its session folder has no session.json";
    else if (f.rec && typeof f.rec === "object") {
      const pids = [f.rec.serverPid, ...(f.rec.clientPids ?? [])].map(Number);
      if (f.rec.stoppedAt) why = `its session was stopped at ${f.rec.stoppedAt}`;
      else if (!pids.includes(Number(f.pid))) why = "its session record names other processes (a newer session took the folder)";
    } else if (isServer(f) && isTestProfile(f.profile) && age > testMaxMin) why = `a test server running ${age} min (runs are budgeted under ${testMaxMin})`;
    else if (isWb(f) && f.tool && age > toolMaxMin) why = `an rdb/compile Workbench running ${age} min (hung)`;
    if (why) out.push({ pid: Number(f.pid), name: f.name, wsMb: Number(f.wsMb) || 0, why });
  }
  return out;
}

// ---------------------------------------------------------------- I/O

// Runs `script` on machine m (local or ssh) and returns its stdout, or null.
export function runOn(m, script, { timeout = 30000, sshPs } = {}) {
  try {
    if (!m || m.via === "local") {
      const r = spawnSync("powershell", ["-NoProfile", "-NonInteractive", "-Command", script], { windowsHide: true, encoding: "utf8", timeout, maxBuffer: 64 * 1024 * 1024 });
      return r.status === 0 ? r.stdout : null;
    }
    const r = sshPs(m.host, script, { timeout, connectTimeout: 5 });
    return r.status === 0 ? r.stdout : null;
  } catch {
    return null;
  }
}

// Stops the planned orphans on m (taskkill /T /F, by pid; each pid re-checked to still be ArmaReforger).
export function killScript(plan) {
  return plan
    .map((o) => `$q = Get-Process -Id ${Number(o.pid)} -ErrorAction SilentlyContinue; if ($q -and $q.ProcessName -like 'ArmaReforger*') { taskkill /T /F /PID ${Number(o.pid)} | Out-Null; Write-Output 'lf-ram: killed ${Number(o.pid)}' }`)
    .join("\n");
}

// Plans (and unless dryRun stops) the orphans on m. Returns { plan, killed } or null when m did not answer.
export function reapOrphans(m, { dryRun = false, log = console.log, sshPs, opts } = {}) {
  const facts = parseFacts(runOn(m, PS_FACTS, { sshPs }));
  if (!facts) return null;
  const plan = planOrphans(facts, opts);
  let killed = [];
  if (plan.length && !dryRun) {
    const outText = runOn(m, killScript(plan), { sshPs }) ?? "";
    killed = [...outText.matchAll(/^lf-ram: killed (\d+)/gm)].map((x) => Number(x[1]));
  }
  for (const o of plan) log(`pool: ${dryRun ? "would stop" : killed.includes(o.pid) ? "stopped" : "could not stop"} orphan ${o.name} pid ${o.pid} on ${m?.name ?? "local"} (${o.why}; ${(o.wsMb / 1024).toFixed(1)} GB)`);
  return { plan, killed };
}

// reapOrphans for each machine at most once per everyMs (a stamp per machine in the hub), so the pool's wait loops
// can call it on every tick.
// LF_ORPHAN_REAP=0 turns it off; the pool selftests' RAM seam (LF_POOL_TEST_FREE_MB) does too, so a fixture
// never scans or stops this PC's real processes.
export function reapOrphansThrottled(hubDir, machines, { everyMs = 600000, now = Date.now(), env = process.env, ...o } = {}) {
  if (env.LF_ORPHAN_REAP === "0" || Number(env.LF_POOL_TEST_FREE_MB) > 0) return [];
  const file = join(hubDir, "orphan-reap.json");
  let stamps = {};
  try {
    stamps = JSON.parse(readFileSync(file, "utf8"));
  } catch {}
  const due = (machines ?? []).filter((m) => !(now - (Number(stamps[m.name]) || 0) < everyMs));
  if (!due.length) return [];
  for (const m of due) stamps[m.name] = now;
  try {
    mkdirSync(hubDir, { recursive: true });
    writeFileSync(file, JSON.stringify(stamps));
  } catch {}
  const out = [];
  for (const m of due) {
    const r = reapOrphans(m, o);
    if (r) out.push({ machine: m.name, ...r });
  }
  return out;
}
