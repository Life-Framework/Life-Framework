// Remote run-folder GC for the test pool (tools/remote/pool.mjs). Every run on an ssh machine
// gets its own worktree there (<box>/Life-Framework-rt-<slug>[-session][-sN], about 1.4 GB each)
// and nothing removed them while the worktree on this PC existed: 2026-11-14 lf-cloud went to 0 GB
// free and every gate sent there failed the disk floor. This file holds the pure choice (which
// folders go, in which order, which are kept and why) and the PowerShell that runs on the box.
// tools/validation/pool-selftest.mjs proves the choice without a machine.

import { psQuote } from "../remote/testbox.mjs";

export const RT_PREFIX = "Life-Framework-rt-";

// "C:\LF\Life-Framework-rt-x\" and "c:/lf/life-framework-rt-x" are the same folder.
export function normDir(p) {
  return String(p ?? "").replace(/\\/g, "/").replace(/\/+$/, "").toLowerCase();
}

function leafOf(p) {
  return String(p ?? "").replace(/\\/g, "/").replace(/\/+$/, "").split("/").pop();
}

// What the box said about its run folders: lines `lf-pool: disk <gb>` and
// `lf-pool: rt <lock|proc|-> <last write, epoch ms> <path>`.
export function parseRtScan(text) {
  const out = { freeGb: null, entries: [] };
  for (const raw of String(text ?? "").split(/\r?\n/)) {
    const l = raw.trim();
    let m = /^lf-pool: disk (-?[\d.]+)$/.exec(l);
    if (m) {
      out.freeGb = Number(m[1]);
      continue;
    }
    m = /^lf-pool: rt (lock|proc|-) (\d+) (.+)$/.exec(l);
    if (m) out.entries.push({ dir: m[3], name: leafOf(m[3]), busy: m[1] === "-" ? null : m[1], lastMs: Number(m[2]) });
  }
  return out;
}

// Which run folders to remove, oldest first. entries: [{ dir, busy: "lock"|"proc"|null, lastMs }] as
// the box reported them; keep: folders about to be used (never touched); idleMin: a folder written
// to more recently than that is in use. Returns { remove: [{ dir, name, idleMin }], kept: [{ name, why }] }.
// Only folders named Life-Framework-rt-* are ever candidates.
export function planRtGc(entries, { now = Date.now(), idleMin = 60, keep = [] } = {}) {
  const keepSet = new Set(keep.map(normDir));
  const cutoff = now - idleMin * 60000;
  const remove = [];
  const kept = [];
  for (const e of entries) {
    const name = leafOf(e.dir);
    if (!name.startsWith(RT_PREFIX)) continue;
    if (keepSet.has(normDir(e.dir))) kept.push({ name, why: "about to be used" });
    else if (e.busy === "lock") kept.push({ name, why: "live lock" });
    else if (e.busy === "proc") kept.push({ name, why: "process running in it" });
    else if (!Number.isFinite(e.lastMs) || e.lastMs <= 0) kept.push({ name, why: "age unknown" });
    else if (e.lastMs > cutoff) kept.push({ name, why: "written recently" });
    else remove.push({ dir: e.dir, name, idleMin: Math.round((now - e.lastMs) / 60000) });
  }
  remove.sort((a, b) => b.idleMin - a.idleMin || a.name.localeCompare(b.name));
  return { remove, kept };
}

// The free-disk level under which the pool cleans a box before a run: factor x the disk floor.
export function rtGcTargetGb(settings) {
  const floor = Number(settings.minFreeDiskGb);
  const factor = Number(settings.rtGcFreeFactor);
  return Number.isFinite(floor) && Number.isFinite(factor) && floor > 0 && factor > 0 ? floor * factor : 0;
}

export function fmtIdle(min) {
  if (min < 120) return `${min} min`;
  if (min < 2880) return `${Math.round(min / 6) / 10} h`;
  return `${Math.round(min / 144) / 10} d`;
}

// ---------------------------------------------------------------- on the box

// Shared by both scripts. Busy = a lock in <main>\tmp\wt\locks whose rDir is the folder and whose
// holder pid is alive (a lock with no pid counts as alive), or a process whose command line names it.
const PS_COMMON = String.raw`
$ErrorActionPreference = 'Continue'
$inv = [Globalization.CultureInfo]::InvariantCulture
function Get-FreeGb($dir) { try { return ([IO.DriveInfo]((Split-Path -Qualifier $dir) + '\')).AvailableFreeSpace / 1GB } catch { return -1 } }
function Norm($p) { return $p.Replace('/', '\').TrimEnd('\').ToLower() }
function Get-LockDirs($main) {
  $out = @()
  foreach ($f in Get-ChildItem -LiteralPath (Join-Path $main 'tmp\wt\locks') -Filter '*.lock' -ErrorAction SilentlyContinue) {
    try {
      $i = [IO.File]::ReadAllText($f.FullName) | ConvertFrom-Json
      if (-not $i.rDir) { continue }
      if ($i.pid -and -not (Get-Process -Id $i.pid -ErrorAction SilentlyContinue)) { continue }
      $out += (Norm $i.rDir)
    } catch {}
  }
  return $out
}
function Get-ProcText() { return ((Get-CimInstance Win32_Process | ForEach-Object { $_.CommandLine }) -join [Environment]::NewLine).Replace('/', '\').ToLower() }
function Test-Named($procText, $p) { return [regex]::IsMatch($procText, [regex]::Escape($p) + '(?![\w.-])') }
function Get-LastMs($p) {
  $t = @((Get-Item -LiteralPath $p -Force).LastWriteTime)
  $t += @(Get-ChildItem -LiteralPath $p -Directory -Force -ErrorAction SilentlyContinue | ForEach-Object { $_.LastWriteTime })
  $t += @(Get-ChildItem -LiteralPath (Join-Path $p 'tmp') -Force -ErrorAction SilentlyContinue | ForEach-Object { $_.LastWriteTime })
  $m = $t | Sort-Object -Descending | Select-Object -First 1
  return ([DateTimeOffset]$m).ToUnixTimeMilliseconds()
}
`;

// Prints the free disk of the volume holding the run folders, and (unless it is already at or over
// targetGb, when targetGb > 0) one `lf-pool: rt` line per run folder. Never deletes.
export function rtScanScript(parentDir, targetGb) {
  return `$parent = ${psQuote(String(parentDir).replace(/\//g, "\\"))}
$main = Join-Path $parent 'Life-Framework'
$target = ${Number(targetGb) > 0 ? Number(targetGb) : 0}
${PS_COMMON}
$free = Get-FreeGb $parent
Write-Host ('lf-pool: disk ' + $free.ToString('0.##', $inv))
if ($target -gt 0 -and $free -ge $target) { exit 0 }
$locks = @(Get-LockDirs $main)
$procText = Get-ProcText
foreach ($d in Get-ChildItem -LiteralPath $parent -Directory -Filter '${RT_PREFIX}*' -ErrorAction SilentlyContinue) {
  $p = $d.FullName
  $n = Norm $p
  $busy = '-'
  if ($locks -contains $n) { $busy = 'lock' } elseif (Test-Named $procText $n) { $busy = 'proc' }
  $last = 0; try { $last = Get-LastMs $p } catch {}
  Write-Host ('lf-pool: rt ' + $busy + ' ' + $last + ' ' + $p)
}
`;
}

// Removes the planned folders in the given order with the box's own tools\\safe-rm.mjs (it unlinks
// junctions first; the game-addons links into the server install are never followed), stopping as
// soon as targetGb is free (targetGb 0: remove them all). A folder that got a lock, a process or
// a write since the scan (older than cutoffMs no more) is kept. One line per folder:
// `lf-pool: gc removed <free after, GB> <path>`, `lf-pool: gc kept <why> <path>`, `lf-pool: gc fail <path>`.
export function rtRemoveScript(parentDir, dirs, { targetGb = 0, cutoffMs = 0 } = {}) {
  return `$parent = ${psQuote(String(parentDir).replace(/\//g, "\\"))}
$main = Join-Path $parent 'Life-Framework'
$target = ${Number(targetGb) > 0 ? Number(targetGb) : 0}
$cutoff = ${Math.max(0, Math.floor(Number(cutoffMs) || 0))}
$dirs = @(${dirs.map(psQuote).join(",")})
${PS_COMMON}
$safe = Join-Path $main 'tools\\safe-rm.mjs'
$locks = @(Get-LockDirs $main)
$procText = Get-ProcText
$fail = 0
foreach ($p in $dirs) {
  if ($target -gt 0 -and (Get-FreeGb $parent) -ge $target) { break }
  $n = Norm $p
  if (-not ((Split-Path -Leaf $p) -like '${RT_PREFIX}*') -or (Norm (Split-Path -Parent $p)) -ne (Norm $parent)) { Write-Host ('lf-pool: gc kept outside ' + $p); continue }
  if (-not (Test-Path -LiteralPath $p)) { continue }
  if ($locks -contains $n) { Write-Host ('lf-pool: gc kept lock ' + $p); continue }
  if (Test-Named $procText $n) { Write-Host ('lf-pool: gc kept proc ' + $p); continue }
  $last = 0; try { $last = Get-LastMs $p } catch {}
  if ($cutoff -gt 0 -and $last -gt $cutoff) { Write-Host ('lf-pool: gc kept recent ' + $p); continue }
  & node $safe $p | Out-Null
  if ($LASTEXITCODE -or (Test-Path -LiteralPath $p)) { Write-Host ('lf-pool: gc fail ' + $p); $fail = 1; continue }
  Write-Host ('lf-pool: gc removed ' + (Get-FreeGb $parent).ToString('0.##', $inv) + ' ' + $p)
}
git -C $main worktree prune 2>&1 | Out-Null
Write-Host ('lf-pool: gc free ' + (Get-FreeGb $parent).ToString('0.##', $inv))
exit $fail
`;
}

// `lf-pool: gc ...` lines -> { removed: [{ dir, freeGb }], kept: [{ dir, why }], failed: [dir], freeGb }.
export function parseRtRemove(text) {
  const out = { removed: [], kept: [], failed: [], freeGb: null };
  for (const raw of String(text ?? "").split(/\r?\n/)) {
    const l = raw.trim();
    let m;
    if ((m = /^lf-pool: gc removed (-?[\d.]+) (.+)$/.exec(l))) out.removed.push({ dir: m[2], freeGb: Number(m[1]) });
    else if ((m = /^lf-pool: gc kept (\S+) (.+)$/.exec(l))) out.kept.push({ dir: m[2], why: m[1] });
    else if ((m = /^lf-pool: gc fail (.+)$/.exec(l))) out.failed.push(m[1]);
    else if ((m = /^lf-pool: gc free (-?[\d.]+)$/.exec(l))) out.freeGb = Number(m[1]);
  }
  return out;
}
