# tools/validation/validate-tests.ps1 - every EL_Test file is registered and red-proofed.
#
# Contract (mirrors gen-test-registry.ps1):
#   1. The registry is machine-generated: gen-test-registry.ps1 derives
#      EL_TestRegistrations.generated.c from the test files. An unregistered
#      test never runs; a registered test that was deleted fails the compile.
#   2. Every test class carries a `// tier:` comment and the generated registry
#      must be in sync (regenerate with `tools\cli regen-tests`).
#   3. Every test file carries a `// red-proof:` comment recording how its
#      assertions were observed failing at least once.
#
# Exit codes: 0 = contract holds, 1 = violations listed.

$ErrorActionPreference = "Stop"

$root = git rev-parse --show-toplevel
if (-not $root) { Write-Host "ERROR not a git work tree"; exit 1 }

$testsDir = Join-Path $root "addons\LifeFramework\Scripts\Game\Tests"
$generator = Join-Path $root "tools\validation\gen-test-registry.ps1"
$generatedName = "EL_TestRegistrations.generated.c"

$failed = $false

& $generator -Check
if ($LASTEXITCODE -ne 0) { $failed = $true }

$violations = @()
Get-ChildItem $testsDir -Filter "*.c" | Where-Object { $_.Name -ne $generatedName } | ForEach-Object {
  $text = Get-Content $_.FullName -Raw
  $classes = [regex]::Matches($text, 'class\s+(EL_Test_\w+)\s*:\s*EL_Test') | ForEach-Object { $_.Groups[1].Value }
  if (-not $classes) { return }

  if ($text -notmatch '//\s*red-proof:') {
    $violations += "$($_.Name): no '// red-proof:' comment - record how these assertions were observed failing"
  }
}

if ($violations.Count -gt 0) {
  $failed = $true
  Write-Host "validate-tests: FAILED"
  foreach ($v in $violations) { Write-Host "  - $v" }
}

if ($failed) { exit 1 }

Write-Host "validate-tests: OK (registry in sync, red-proofs present)"
exit 0