# tools/validation/validate-worktrees.ps1 - parallel-worktree registry consistency.
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

foreach ($p in $state.worktrees.PSObject.Properties) {
  $slug = $p.Name
  $w = $p.Value
  $registered[$slug] = $true

  if (-not (Test-Path -LiteralPath $w.path)) {
    $errors += "worktree '$slug' registered at missing path $($w.path) (prune it: cli wt prune $slug)"
    continue
  }

  if ($w.branch) {
    git -C $root show-ref --verify "refs/heads/$($w.branch)" 2>$null | Out-Null
    if ($LASTEXITCODE -ne 0) {
      $errors += "worktree '$slug' branch $($w.branch) does not exist"
    }
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

  git -C $root merge-base --is-ancestor $w.branch origin/main 2>$null
  if ($LASTEXITCODE -eq 0) {
    $dirty = @(git -C $w.path status --porcelain).Count -gt 0
    if (-not $dirty) {
      $warnings += "worktree '$slug' is merged into origin/main and clean - prune it (cli wt prune $slug)"
    }
  }
}

# Linked worktrees that exist on disk but were never registered.
$linked = @(git -C $root worktree list --porcelain)
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