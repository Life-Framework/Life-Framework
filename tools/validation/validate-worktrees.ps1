# tools/validation/validate-worktrees.ps1 - parallel-worktree registry consistency.
# inputs: ttl=300
#
# Run by `tools\cli validate` from any checkout. Checks the worktree hub
# (<main-checkout>/tmp/wt/, resolved via git-common-dir so it works from a
# worktree too):
#   - every registered worktree's directory and branch still exist
#   - no two worktrees share a port pair (parallel test runs would collide)
#   - every registered worktree carries its tmp/wt.json marker
#   - merged / orphaned worktrees are flagged so they get pruned
#   - warns when run inside the main checkout (the world-editor copy)
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

if (-not (Test-Path $statePath)) {
  Write-Host "worktrees: none registered yet (run 'cli wt new <feature>' or 'cli wt list' to register)"
  exit 0
}

$state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
$errors = @()
$warnings = @()
$seenPorts = @{}
$registered = @{}

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

# Ref facts, read once (the scan used to ask git per worktree: show-ref, rev-parse, merge-base each time).
$headsRes = Invoke-Git @('for-each-ref', '--format=%(refname:short)', 'refs/heads')
$heads = @{}
if ($headsRes.Code -eq 0) {
  foreach ($h in $headsRes.Out) { if ($h) { $heads[$h] = $true } }
} else {
  $warnings += "could not list local branches (git said: $($headsRes.Err.Trim())); branch checks skipped"
}
$mergedByBase = @{}   # base branch -> hashtable of local branches merged into origin/<base>, or $null when origin/<base> is gone

foreach ($p in $state.worktrees.PSObject.Properties) {
  $slug = $p.Name
  $w = $p.Value
  $registered[$slug] = $true
  # A `wt new` in flight (its checkout runs outside the lock); warm-pool trees
  # live in $state.warm and are never checked here.
  if ($w.status -eq 'creating') { continue }

  if (-not (Test-Path -LiteralPath $w.path)) {
    if (-not (Test-StillRegistered $slug)) { continue }   # pruned while we scanned
    $errors += "worktree '$slug' registered at missing path $($w.path) (prune it: cli wt prune $slug)"
    continue
  }
  # A directory with no .git is not a worktree any more: a prune in flight, or what one left behind.
  if (-not (Test-Path -LiteralPath (Join-Path $w.path ".git"))) {
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

  if (-not (Test-Path -LiteralPath (Join-Path $w.path "tmp\wt.json"))) {
    $warnings += "worktree '$slug' is missing its tmp/wt.json marker (run 'cli wt list' to repair)"
  }

  $baseBranch = if ($w.base) { [string]$w.base } else { 'main' }
  # A stacked worktree's base branch is deleted from origin once its PR merges;
  # a missing ref is "nothing to compare", not a crash.
  if (-not $mergedByBase.ContainsKey($baseBranch)) {
    $has = Invoke-Git @('rev-parse', '--verify', '--quiet', "refs/remotes/origin/$baseBranch")
    $set = $null
    if ($has.Code -eq 0) {
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
    $st = Invoke-Git @('-C', [string]$w.path, 'status', '--porcelain')
    if ($st.Code -ne 0) { continue }   # the worktree vanished or is locked right now; not a finding
    if (@($st.Out | Where-Object { $_ }).Count -eq 0) {
      $warnings += "worktree '$slug' is merged into origin/$baseBranch and clean - prune it (cli wt prune $slug)"
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

foreach ($warn in $warnings) { Write-Host "WARN  $warn" -ForegroundColor Yellow }
foreach ($e in $errors) { Write-Host "ERROR $e" -ForegroundColor Red }

if ($errors.Count -gt 0) {
  Write-Host ("validate-worktrees: {0} error(s), {1} warning(s) - FIXED" -f $errors.Count, $warnings.Count) -ForegroundColor Red
  exit 1
}
Write-Host ("validate-worktrees: OK ({0} warning(s))" -f $warnings.Count) -ForegroundColor Green
exit 0