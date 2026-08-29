# tools/validation/validate-repo.ps1 - Life Framework repo validator.
#
# Used by the pre-commit hook (with -Staged) and standalone (full tree) via:
#   tools\cli validate
#
# Checks:
#   - no Workbench/editor artifacts staged (log, *.rdb, *.rdb.lock, *.gproj.user)
#   - no dependency/clone dirs staged (node_modules/, tools/mcp/)
#   - every staged .meta has its resource file present (no orphan metas)
#   - no duplicate resource GUIDs across .meta files
#   - warnings for lowercase 'data/' path references in text resources
#
# Exits 1 on errors (aborts a commit), 0 otherwise.

param(
  [switch]$Staged
)

$ErrorActionPreference = "Stop"
$root = git rev-parse --show-toplevel
if (-not $root) {
  Write-Host "ERROR not a git work tree" -ForegroundColor Red
  exit 1
}

$script:errors = @()
$script:warnings = @()

function Add-Error($msg) { $script:errors += $msg }
function Add-Warning($msg) { $script:warnings += $msg }

# --- staged-file checks ---------------------------------------------------

if ($Staged) {
  $files = @(git diff --cached --name-only --diff-filter=ACMR)
  foreach ($f in $files) {
    if ($f -match '(^|/)log$') { Add-Error "artifact committed: $f (addon 'log' file)" }
    if ($f -match '\.rdb(\.lock)?$') { Add-Error "artifact committed: $f (*.rdb / *.rdb.lock are local Workbench caches)" }
    if ($f -match '\.gproj\.user$') { Add-Error "artifact committed: $f (machine-specific editor state)" }
    if ($f -match '(^|/)node_modules/') { Add-Error "dependency committed: $f (node_modules is not tracked)" }
    if ($f -match '^tools/mcp/[^/]+/') { Add-Error "MCP clone committed: $f (managed clones are ignored - use tools\cli mcp)" }
  }

  foreach ($f in $files) {
    if ($f -match '\.meta$') {
      $res = $f -replace '\.meta$', ''
      if (-not (Test-Path -LiteralPath (Join-Path $root $res))) {
        Add-Error "orphan meta: $f (no resource at $res)"
      }
    }
  }
}

# --- whole-tree checks ------------------------------------------------------

# duplicate resource GUIDs
$guidMap = @{}
$metas = @(git ls-files -- "*.meta")
foreach ($m in $metas) {
  $path = Join-Path $root $m
  if (-not (Test-Path -LiteralPath $path)) { continue }
  $content = Get-Content -LiteralPath $path -Raw -ErrorAction SilentlyContinue
  if (-not $content) { continue }
  if ($content -match '(?m)^\s*Name\s+"\{([0-9A-F]{16})\}') {
    $g = $matches[1]
    if ($guidMap.ContainsKey($g)) {
      Add-Error "duplicate resource GUID {$g}: $m and $($guidMap[$g])"
    } else {
      $guidMap[$g] = $m
    }
  }
}

# lowercase 'data/' references in text resources (reintroduction guard)
$refs = git grep -l -E '/data/' -- "*.meta" "*.emat" "*.et" "*.conf" "*.layer" 2>$null
if ($LASTEXITCODE -eq 0) {
  foreach ($r in $refs) { Add-Warning "lowercase 'data/' path reference: $r (should be 'Data/')" }
}

# placeholder / invalid (non-16-hex) GUID tokens in text resources.
# Resource GUIDs must be real unique 16-hex values - never `{ActionsID}`, `{NEW_*}`.
$textRes = @()
if ($Staged) {
  $textRes = @($files | Where-Object { $_ -match '\.(et|layer|layout|st|conf|emat)$' })
} else {
  $textRes = @(git ls-files -- "*.et" "*.layer" "*.layout" "*.st" "*.conf" "*.emat")
}
foreach ($f in $textRes) {
  $path = Join-Path $root $f
  if (-not (Test-Path -LiteralPath $path)) { continue }
  $content = Get-Content -LiteralPath $path -Raw -ErrorAction SilentlyContinue
  if (-not $content) { continue }
  foreach ($m in [regex]::Matches($content, '\{([0-9A-Za-z_]{1,40})\}')) {
    if ($m.Groups[1].Value -notmatch '^[0-9A-F]{16}$') {
      Add-Error "placeholder/invalid GUID '{$($m.Groups[1].Value)}' in $f (use a real unique 16-hex GUID)"
    }
  }
}

# duplicate widget-instance GUIDs within a single .layout file.
# A widget instance GUID must be unique in its file (slot GUIDs may repeat).
$layoutFiles = @()
if ($Staged) {
  $layoutFiles = @($files | Where-Object { $_ -match '\.layout$' })
} else {
  $layoutFiles = @(git ls-files -- "*.layout")
}
foreach ($f in $layoutFiles) {
  $path = Join-Path $root $f
  if (-not (Test-Path -LiteralPath $path)) { continue }
  $seen = @{}
  foreach ($line in @(Get-Content -LiteralPath $path -ErrorAction SilentlyContinue)) {
    if ($line -match '^\s*\w+WidgetClass\s+"\{([0-9A-F]{16})\}"') {
      $g = $Matches[1]
      if ($seen.ContainsKey($g)) { Add-Error "duplicate widget-instance GUID {$g} in $f" }
      else { $seen[$g] = $true }
    }
  }
}

# --- report -----------------------------------------------------------------

foreach ($w in $script:warnings) { Write-Host "WARN  $w" -ForegroundColor Yellow }
foreach ($e in $script:errors) { Write-Host "ERROR $e" -ForegroundColor Red }

if ($script:errors.Count -gt 0) {
  Write-Host ("validate-repo: {0} error(s), {1} warning(s) - FIXED" -f $script:errors.Count, $script:warnings.Count) -ForegroundColor Red
  exit 1
}
Write-Host ("validate-repo: OK ({0} warning(s))" -f $script:warnings.Count) -ForegroundColor Green
exit 0