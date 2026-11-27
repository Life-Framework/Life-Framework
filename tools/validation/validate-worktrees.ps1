# tools/validation/validate-worktrees.ps1 - parallel-worktree registry consistency.
# inputs: ttl=300
#
# Run by `tools\cli validate` from any checkout. Checks the worktree hub
# (<main-checkout>/tmp/wt/, resolved via git-common-dir so it works from a
# worktree too):
#   - every registered worktree's directory and branch still exist
#   - no two worktrees share a port pair (parallel test runs would collide)
#   - every registered worktree carries its tmp/wt.json marker
#   - orphaned worktrees are flagged; merged-by-ancestry ones are one INFO count (ancestry also matches a fresh worktree)
#   - warns when run inside the main checkout (the world-editor copy)
#
# Cost (#3126): the registry checks above run for every registered worktree (file tests only). The git reads
# (status, merged-into-base) run in full for the current worktree and for any worktree whose index or HEAD was
# written in the last 24 h. Older ones are counted in one INFO line. A clean status is cached per worktree in
# <hub>/validate-worktrees-cache.json, keyed by HEAD, index mtime and the base ref, so an unchanged worktree
# costs no git call on the next run.
#
# Exits 1 on errors, 0 otherwise.

$ErrorActionPreference = "Stop"

$root = git rev-parse --show-toplevel
if (-not $root) {
  Write-Host "ERROR not a git work tree" -ForegroundColor Red
  exit 1
}

$commonDir = git rev-parse --git-common-dir
$mainRoot = if ([System.IO.Path]::IsPathRooted($commonDir)) { Split-Path -Parent $commonDir } else { $root }
$hub = Join-Path $mainRoot "tmp\wt"
$statePath = Join-Path $hub "state.json"
$cachePath = Join-Path $hub "validate-worktrees-cache.json"

if (-not (Test-Path $statePath)) {
  Write-Host "worktrees: none registered yet (run 'cli wt new <feature>' or 'cli wt list' to register)"
  exit 0
}

$state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
$errors = @()
$warnings = @()
$seenPorts = @{}
$registered = @{}
# Worktrees whose branch is an ancestor of origin/<base>. That is ancestry, not a merged PR: a worktree created
# a minute ago has no commit of its own and looks merged too. So these are counted in one INFO line, never a
# WARN each; `cli lead prune-safe` asks GitHub which PRs are MERGED and prunes only those.
$mergedClean = 0
$mergedDirty = 0

# The current worktree is checked in full whatever its age.
$currentFull = [IO.Path]::GetFullPath(([string]$root).Replace('/', '\')).TrimEnd('\').ToLowerInvariant()
$staleCutoff = (Get-Date).AddHours(-24)
$staleSkipped = 0

# Previous run's clean-status cache: slug -> key (HEAD, index ticks, base sha). Unreadable means no cache.
$cache = @{}
try {
  if (Test-Path -LiteralPath $cachePath) {
    $raw = Get-Content -LiteralPath $cachePath -Raw | ConvertFrom-Json
    foreach ($c in $raw.PSObject.Properties) { $cache[$c.Name] = [string]$c.Value }
  }
} catch {
  $cache = @{}
}
$cacheNext = @{}

# Seconds this scan may spend on per-worktree `git status` calls (LF_VALIDATE_WORKTREES_GIT_BUDGET_S overrides).
$gitBudgetSec = 240.0
if ($env:LF_VALIDATE_WORKTREES_GIT_BUDGET_S -match '^\d+(\.\d+)?$') { $gitBudgetSec = [double]$env:LF_VALIDATE_WORKTREES_GIT_BUDGET_S }
$gitClock = [System.Diagnostics.Stopwatch]::StartNew()
$budgetSkipped = 0

# Other agents prune worktrees while this runs (`lead prune-safe`, `wt prune`), and git can refuse a ref read
# for a moment while another git rewrites packed-refs or a lock ("couldn't read .git/packed-refs: Permission
# denied"). A git call here therefore never throws and never prints: it returns { Code; Out }, and a transient
# failure (packed-refs, a lock, permission denied) is retried once after a short pause.
function Invoke-Git {
  param([string[]]$GitArgs)
  for ($try = 1; $try -le 2; $try++) {
    $prev = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
      if ($GitArgs[0] -ne '-C') { $GitArgs = @('-C', $root) + $GitArgs }
      $raw = @(& git @GitArgs 2>&1)
      $code = $LASTEXITCODE
    } finally {
      $ErrorActionPreference = $prev
    }
    $err = @($raw | Where-Object { $_ -is [System.Management.Automation.ErrorRecord] } | ForEach-Object { "$_" })
    $out = @($raw | Where-Object { $_ -isnot [System.Management.Automation.ErrorRecord] } | ForEach-Object { "$_" })
    if ($code -ne 0 -and $try -eq 1 -and (($err -join "`n") -match 'packed-refs|Permission denied|Unable to create .*\.lock|cannot lock ref')) {
      Start-Sleep -Milliseconds 400
      continue
    }
    return @{ Code = $code; Out = $out; Err = ($err -join "`n") }
  }
}

# Whether the registry still lists a slug: a worktree pruned after the first read is not a finding.
function Test-StillRegistered {
  param([string]$Slug)
  try {
    $now = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    return [bool]($now.worktrees.PSObject.Properties.Name -contains $Slug)
  } catch {
    return $true
  }
}

# The worktree's private git dir: a linked worktree's .git file reads "gitdir: <path>". $null when there is none.
function Get-PrivateGitDir {
  param([string]$Path)
  $dotGit = [IO.Path]::Combine($Path, ".git")
  if ([IO.Directory]::Exists($dotGit)) { return $dotGit }
  if (-not [IO.File]::Exists($dotGit)) { return $null }
  $line = [IO.File]::ReadAllText($dotGit).Trim()
  if ($line -match '^gitdir:\s*([^\r\n]+)') {
    $g = $Matches[1]
    if (-not [IO.Path]::IsPathRooted($g)) { $g = Join-Path $Path $g }
    return $g
  }
  return $null
}

# What git writes on a commit, add, checkout or status refresh: the index and HEAD of the private git dir.
# Last is the newer of the two; Head is HEAD's text (the ref or a detached sha); IndexTicks keys the cache.
function Get-WorktreeStamp {
  param([string]$GitDir)
  $stamp = @{ Last = [datetime]::MinValue; Head = ""; IndexTicks = [long]0 }
  if (-not $GitDir) { return $stamp }
  $idx = [IO.Path]::Combine($GitDir, "index")
  $headFile = [IO.Path]::Combine($GitDir, "HEAD")
  if ([IO.File]::Exists($idx)) {
    $t = [IO.File]::GetLastWriteTime($idx)
    $stamp.IndexTicks = $t.Ticks
    if ($t -gt $stamp.Last) { $stamp.Last = $t }
  }
  if ([IO.File]::Exists($headFile)) {
    $t = [IO.File]::GetLastWriteTime($headFile)
    if ($t -gt $stamp.Last) { $stamp.Last = $t }
    $stamp.Head = [IO.File]::ReadAllText($headFile).Trim()
  }
  return $stamp
}

# Ref facts, read once (the scan used to ask git per worktree: show-ref, rev-parse, merge-base each time).
$headsRes = Invoke-Git @('for-each-ref', '--format=%(refname:short)', 'refs/heads')
$heads = @{}
if ($headsRes.Code -eq 0) {
  foreach ($h in $headsRes.Out) { if ($h) { $heads[$h] = $true } }
} else {
  $warnings += "could not list local branches (git said: $($headsRes.Err.Trim())); branch checks skipped"
}
$mergedByBase = @{}   # base branch -> hashtable of local branches merged into origin/<base>, or $null when origin/<base> is gone
$baseShaByBase = @{}  # base branch -> origin/<base> sha ('' when gone), part of the cache key

foreach ($p in $state.worktrees.PSObject.Properties) {
  $slug = $p.Name
  $w = $p.Value
  $registered[$slug] = $true
  # A `wt new` in flight (its checkout runs outside the lock); warm-pool trees
  # live in $state.warm and are never checked here.
  if ($w.status -eq 'creating') { continue }

  # .NET file tests, not Test-Path: on PS 5.1 Test-Path costs milliseconds per call, and this loop makes several per worktree.
  if (-not [IO.Directory]::Exists([string]$w.path)) {
    if (-not (Test-StillRegistered $slug)) { continue }   # pruned while we scanned
    $errors += "worktree '$slug' registered at missing path $($w.path) (prune it: cli wt prune $slug)"
    continue
  }
  # A directory with no .git is not a worktree any more: a prune in flight, or what one left behind.
  $dotGitPath = [IO.Path]::Combine([string]$w.path, ".git")
  if (-not ([IO.Directory]::Exists($dotGitPath) -or [IO.File]::Exists($dotGitPath))) {
    if (-not (Test-StillRegistered $slug)) { continue }
    $warnings += "worktree '$slug' at $($w.path) is not a git worktree (removed or being removed; prune it: cli wt prune $slug)"
    continue
  }

  if ($w.branch -and $headsRes.Code -eq 0 -and -not $heads.ContainsKey([string]$w.branch)) {
    if (Test-StillRegistered $slug) { $errors += "worktree '$slug' branch $($w.branch) does not exist" }
    continue
  }

  $portKey = "$($w.ports.gamePort)/$($w.ports.a2sPort)"
  if ($seenPorts.ContainsKey($portKey)) {
    $errors += "port collision: '$slug' and '$($seenPorts[$portKey])' both use $portKey"
  } else {
    $seenPorts[$portKey] = $slug
  }

  if (-not [IO.File]::Exists([IO.Path]::Combine([string]$w.path, "tmp\wt.json"))) {
    $warnings += "worktree '$slug' is missing its tmp/wt.json marker (run 'cli wt list' to repair)"
  }

  # Git reads below run only for the current worktree and for one written in the last 24 h. A worktree
  # with no private git dir is always checked (nothing to judge its age by).
  $isCurrent = ([IO.Path]::GetFullPath([string]$w.path).TrimEnd('\').ToLowerInvariant() -eq $currentFull)
  $gitDir = Get-PrivateGitDir $w.path
  $stamp = Get-WorktreeStamp $gitDir
  if (-not $isCurrent -and $gitDir -and $stamp.Last -lt $staleCutoff) {
    $staleSkipped++
    continue
  }

  $baseBranch = if ($w.base) { [string]$w.base } else { 'main' }
  # A stacked worktree's base branch is deleted from origin once its PR merges;
  # a missing ref is "nothing to compare", not a crash.
  if (-not $mergedByBase.ContainsKey($baseBranch)) {
    $has = Invoke-Git @('rev-parse', '--verify', '--quiet', "refs/remotes/origin/$baseBranch")
    $set = $null
    $baseShaByBase[$baseBranch] = ''
    if ($has.Code -eq 0) {
      $baseShaByBase[$baseBranch] = [string](@($has.Out | Where-Object { $_ }) | Select-Object -First 1)
      $m = Invoke-Git @('for-each-ref', '--merged', "refs/remotes/origin/$baseBranch", '--format=%(refname:short)', 'refs/heads')
      if ($m.Code -eq 0) {
        $set = @{}
        foreach ($b in $m.Out) { if ($b) { $set[$b] = $true } }
      }
    }
    $mergedByBase[$baseBranch] = $set
  }
  $merged = $mergedByBase[$baseBranch]
  if ($merged -and $w.branch -and $merged.ContainsKey([string]$w.branch)) {
    $key = "$($stamp.Head)|$($stamp.IndexTicks)|$($baseShaByBase[$baseBranch])"
    if (-not $isCurrent -and $cache.ContainsKey($slug) -and $cache[$slug] -eq $key) {
      $cacheNext[$slug] = $key
      $mergedClean++
      continue
    }
    # Time cap: with hundreds of merged worktrees and a cold cache this loop ran one `git status` each and passed the
    # runner's 600 s per-check budget three times (main guard 2026-11-24). The "merged and clean, prune it" warning is
    # advice, so past the budget the rest are counted in one INFO line and read on a later run (the cache fills up).
    if (-not $isCurrent -and $gitClock.Elapsed.TotalSeconds -ge $gitBudgetSec) { $budgetSkipped++; continue }
    $st = Invoke-Git @('-C', [string]$w.path, 'status', '--porcelain')
    if ($st.Code -ne 0) { continue }   # the worktree vanished or is locked right now; not a finding
    $dirty = @($st.Out | Where-Object { $_ }).Count
    if ($dirty -eq 0) {
      if (-not $isCurrent) {
        $mergedClean++
        # Keyed after the status call: git may refresh the index while it reads, and that write is the baseline.
        $after = Get-WorktreeStamp $gitDir
        $cacheNext[$slug] = "$($after.Head)|$($after.IndexTicks)|$($baseShaByBase[$baseBranch])"
      }
    } elseif (-not $isCurrent) {
      # The current worktree is the one being worked on: its edits are expected, not a finding.
      $mergedDirty++
    }
  }
}

# Linked worktrees that exist on disk but were never registered.
$linkedRes = Invoke-Git @('worktree', 'list', '--porcelain')
$linked = if ($linkedRes.Code -eq 0) { @($linkedRes.Out) } else { @() }
for ($i = 0; $i -lt $linked.Count; $i++) {
  if ($linked[$i] -match '^branch refs/heads/(ws/.+)$') {
    $b = $Matches[1]
    $pathLine = $linked[$i - 1]
    if ($pathLine -match '^worktree (.+)$') {
      $wtPath = $Matches[1]
      $slug = $b.Substring(3)
      if (-not $registered.ContainsKey($slug)) {
        $warnings += "unregistered worktree '$slug' at $wtPath (run 'cli wt list' to register it)"
      }
    }
  }
}

# Agents write only in their own worktree. An untracked file under addons/ or
# tools/ of the main checkout (the world-editor copy) is a stray write, like
# the 0-byte file an agent left there on 2026-11-08. Scratch goes in
# <worktree>/tmp/ (gitignored).
$straysRes = Invoke-Git @('-C', $mainRoot, 'ls-files', '--others', '--exclude-standard', '--', 'addons', 'tools')
$strays = if ($straysRes.Code -eq 0) { @($straysRes.Out | Where-Object { $_ }) } else { @() }
foreach ($s in ($strays | Select-Object -First 10)) {
  $warnings += "main checkout has an untracked file: $(Join-Path $mainRoot $s) (agents: scratch goes in <worktree>\tmp\; delete it if it is yours)"
}
if ($strays.Count -gt 10) { $warnings += "main checkout has $($strays.Count - 10) more untracked file(s) under addons/ or tools/" }

# Running in the main checkout? Heavy commands refuse there by design.
if ($commonDir -eq ".git") {
  $warnings += "running in the main checkout (world-editor copy) - build/test/dev/serve/ci refuse to run here"
}

if ($staleSkipped -gt 0) {
  Write-Host ("INFO  {0} worktree(s) with no change in 24 h not checked (index and HEAD older than that; cli wt list shows them)" -f $staleSkipped) -ForegroundColor DarkGray
}

if ($mergedClean -gt 0 -or $mergedDirty -gt 0) {
  Write-Host ("INFO  worktree hygiene: {0} clean and {1} edited worktree(s) are ancestors of origin/main (a fresh worktree counts too); ``cli lead prune-safe --dry-run`` lists those whose PR is MERGED" -f $mergedClean, $mergedDirty) -ForegroundColor DarkGray
}

if ($budgetSkipped -gt 0) {
  Write-Host ("INFO  {0} merged worktree(s) not status-checked: the {1:N0} s git budget ran out (they are read on the next run)" -f $budgetSkipped, $gitBudgetSec) -ForegroundColor DarkGray
}

# Persist the clean-status cache for the next run (only worktrees visited and clean this run are kept).
$cacheChanged = ($cacheNext.Count -ne $cache.Count)
if (-not $cacheChanged) {
  foreach ($k in $cacheNext.Keys) { if ($cache[$k] -ne $cacheNext[$k]) { $cacheChanged = $true; break } }
}
if ($cacheChanged) {
  try {
    [IO.File]::WriteAllText($cachePath, ($cacheNext | ConvertTo-Json))
  } catch {
    # A cache that cannot be written only costs the next run its git calls.
  }
}

foreach ($warn in $warnings) { Write-Host "WARN  $warn" -ForegroundColor Yellow }
foreach ($e in $errors) { Write-Host "ERROR $e" -ForegroundColor Red }

if ($errors.Count -gt 0) {
  Write-Host ("validate-worktrees: {0} error(s), {1} warning(s) - FIXED" -f $errors.Count, $warnings.Count) -ForegroundColor Red
  exit 1
}
Write-Host ("validate-worktrees: OK ({0} warning(s))" -f $warnings.Count) -ForegroundColor Green
exit 0
