# tools/validation/validate-docs-sync.ps1 - keep docs/features.md current with feature code.
#
# docs/features.md is the contract doc every agent reads to know what a system
# does. When a change touches feature/core scripts but not that file, the doc
# drifts and the next audit reads stale rows as live bugs (the 2026-08-29 sweep
# claimed several already-fixed exploits were still open).
#
# This check is WARN-only: it never blocks a commit or gate. It exists to nudge
# agents to update the affected feature's rows in the SAME change.
#
# Invoked by `tools\cli validate` (the CLI runs every script in this folder).

$ErrorActionPreference = "Stop"
$root = git rev-parse --show-toplevel
if (-not $root) {
  Write-Host "ERROR not a git work tree" -ForegroundColor Red
  exit 1
}

$script:warnings = @()
function Add-Warning($msg) { $script:warnings += $msg }

# --- changed paths ---------------------------------------------------------

$changed = @{}
function Get-ChangedPaths {
  param([string[]]$Arguments)
  $ErrorActionPreference = 'Continue'
  return @(git @Arguments 2>$null)
}

$diffs = @(
  @(Get-ChangedPaths @('diff', '--name-only', 'origin/main..HEAD')),
  @(Get-ChangedPaths @('diff', '--cached', '--name-only', '--diff-filter=ACMR')),
  @(Get-ChangedPaths @('diff', '--name-only'))
)
foreach ($lines in $diffs) {
  foreach ($f in $lines) {
    if ($f) { $changed[$f] = $true }
  }
}

# --- feature code changed but docs/features.md not? ------------------------

$featureFiles = @($changed.Keys | Where-Object {
  $_ -match '^addons/LifeFramework/Scripts/Game/' -and
  $_ -match '\.c$' -and
  $_ -notmatch 'Scripts/Game/Tests/'
} | Sort-Object)

if ($featureFiles.Count -eq 0) {
  Write-Host "validate-docs-sync: OK (no feature code changed)" -ForegroundColor Green
  exit 0
}

if (-not $changed.ContainsKey('docs/features.md')) {
  Add-Warning ("feature code changed but docs/features.md not updated - refresh the affected feature's rows in the SAME change (stale rows read as live bugs): " + ($featureFiles -join ', '))
}

# --- report -----------------------------------------------------------------

foreach ($w in $script:warnings) { Write-Host "WARN  $w" -ForegroundColor Yellow }

if ($script:warnings.Count -gt 0) {
  Write-Host ("validate-docs-sync: OK ({0} warning(s))" -f $script:warnings.Count) -ForegroundColor Green
  exit 0
}
Write-Host "validate-docs-sync: OK" -ForegroundColor Green
exit 0
