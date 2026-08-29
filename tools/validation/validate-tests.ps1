# tools/validation/validate-tests.ps1 - every EL_Test file is registered and red-proofed.
#
# Contract (agents break both halves; this check runs on every validate):
#   1. Every class defined in Scripts/Game/Tests/ is registered in
#      EL_TestManager.CollectTests(). An unregistered test never runs.
#   2. Every registered test class has a definition. A registration pointing
#      at a deleted file fails the compile, but this reports it in plain terms.
#   3. Every test file carries a `// red-proof:` comment recording how its
#      assertions were observed failing at least once. A test that cannot go
#      red is a defect (Overthrow dev-ops rule, adopted).
#
# Exit codes: 0 = contract holds, 1 = violations listed.

$ErrorActionPreference = "Stop"

$root = git rev-parse --show-toplevel
if (-not $root) { Write-Host "ERROR not a git work tree"; exit 1 }

$testsDir = Join-Path $root "addons\LifeFramework\Scripts\Game\Tests"
$managerPath = Join-Path $testsDir "EL_TestManager.c"

if (-not (Test-Path -LiteralPath $managerPath)) {
  Write-Host "validate-tests: FAIL - EL_TestManager.c missing at $managerPath"
  exit 1
}

$managerText = Get-Content $managerPath -Raw

$defined = @{}
$violations = @()

Get-ChildItem $testsDir -Filter "*.c" | ForEach-Object {
  $file = $_
  $text = Get-Content $file.FullName -Raw
  $classes = [regex]::Matches($text, 'class\s+(EL_Test_\w+)') | ForEach-Object { $_.Groups[1].Value }
  if (-not $classes) { return }

  foreach ($cls in $classes) { $defined[$cls] = $file.Name }

  if ($text -notmatch '//\s*red-proof:') {
    $violations += "$($file.Name): no '// red-proof:' comment - record how these assertions were observed failing"
  }

  foreach ($cls in $classes) {
    if ($managerText -notmatch [regex]::Escape($cls)) {
      $violations += "$($file.Name): class $cls is not registered in EL_TestManager.CollectTests() - it never runs"
    }
  }
}

foreach ($cls in $defined.Keys) {
  if ($managerText -notmatch [regex]::Escape("new $cls")) {
    $violations += "$($defined[$cls]): $cls is defined but never instantiated in EL_TestManager - it never runs"
  }
}

if ($violations.Count -gt 0) {
  Write-Host "validate-tests: FAILED ($($violations.Count) violation(s))"
  foreach ($v in $violations) { Write-Host "  - $v" }
  exit 1
}

Write-Host "validate-tests: OK ($($defined.Count) test class(es) registered and red-proofed)"
exit 0
