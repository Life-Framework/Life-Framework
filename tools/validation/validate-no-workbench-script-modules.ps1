# tools/validation/validate-no-workbench-script-modules.ps1
#
# Guard: no .gproj in this repo may declare a script module under
# scripts/WorkbenchGame or scripts/WorkbenchGameCommon.
#
# Why: Workbench/WorldEditor-only scripts (SCR_DedicatedServerPlugin,
# SCR_ScriptedWidgetTooltip, texture-processing plugins) do not compile or
# initialize in a headless build context. A commit on 2026-08-30 injected
# workbenchGameCommon into the generated test-core gproj and every headless
# build then crashed at engine init inside nvtt (a Workbench texture plugin
# executing during game creation). This check makes that class of
# misconfiguration a hard gate failure instead of a crash.

$ErrorActionPreference = "Stop"

$root = git rev-parse --show-toplevel
if (-not $root) { Write-Host "ERROR not a git work tree"; exit 1 }

$patterns = @(
  'ScriptModulePathClass\s+workbenchGame',
  '"scripts/WorkbenchGame'
)

$violations = @()
Get-ChildItem -Path $root -Recurse -Filter *.gproj -File |
  Where-Object { $_.FullName -notmatch '\\tools\\mcp\\' } |
  ForEach-Object {
    $content = Get-Content -LiteralPath $_.FullName -Raw
    foreach ($p in $patterns) {
      if ($content -match $p) {
        $violations += "$($_.FullName.Substring($root.Length + 1)): matches '$p'"
      }
    }
  }

if ($violations.Count -gt 0) {
  Write-Host "ERROR Workbench-editor-only script modules found in gproj files:"
  $violations | ForEach-Object { Write-Host "  $_" }
  Write-Host "These scripts (WorkbenchGame / WorkbenchGameCommon) crash headless builds."
  Write-Host "Remove the module declaration from the gproj (see tools/validation/validate-scripts.ps1 core template)."
  exit 1
}

Write-Host "validate-no-workbench-script-modules: OK"
exit 0
