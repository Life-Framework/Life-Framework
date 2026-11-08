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
#   4. The red-proof is not a placeholder. Two shapes are reported:
#      - placeholder: the comment says pending / TODO / TBD / not run / not
#        observed / not booted, or carries almost no text. FAIL: break the
#        guarded code, watch the test go red, restore it, and record that line.
#      - copied: the comment names another test's GetName() (an `area/name`
#        id defined in a different file) and none of this file's own names.
#        FAIL: the comment records a different test's failure.
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

$namePattern = 'GetName\(\)\s*\{\s*return\s+"([^"]+)"'
$placeholderPattern = '(?i)\b(pending|todo|tbd|fixme|not\s+(yet\s+)?(run|observed|booted)|was\s+not\s+(run|booted))\b'

# Pass 1: every test file and the GetName() ids it defines.
$files = @()
$owner = @{}
Get-ChildItem $testsDir -Filter "*.c" | Where-Object { $_.Name -ne $generatedName } | ForEach-Object {
  $text = Get-Content $_.FullName -Raw
  $classes = @([regex]::Matches($text, 'class\s+((?:EL_Test_|LF_Test_)\w+)\s*:\s*EL_Test') | ForEach-Object { $_.Groups[1].Value })
  if ($classes.Count -eq 0) { return }

  $names = @([regex]::Matches($text, $namePattern) | ForEach-Object { $_.Groups[1].Value })
  foreach ($n in $names) { $owner[$n] = $_.Name }
  $files += [pscustomobject]@{ Name = $_.Name; Text = $text; Names = $names }
}

# Pass 2: presence, then placeholder and copied red-proofs.
$violations = @()
$placeholders = @()
$copied = @()
foreach ($file in $files) {
  $lines = $file.Text -split "\r?\n"
  $start = -1
  for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match '^\s*//\s*red-proof:') { $start = $i; break }
  }

  if ($start -lt 0) {
    $violations += "$($file.Name): no '// red-proof:' comment - record how these assertions were observed failing"
    continue
  }

  # The red-proof block is the red-proof line plus the comment lines that follow it,
  # up to the first non-comment line or the tier comment.
  $block = @()
  for ($i = $start; $i -lt $lines.Count; $i++) {
    $line = $lines[$i]
    if ($line -notmatch '^\s*//') { break }
    if ($i -gt $start -and $line -match '^\s*//\s*tier:') { break }
    $block += ($line -replace '^\s*//\s*', '')
  }
  $proof = (($block -join ' ') -replace '^red-proof:\s*', '').Trim()

  if ($proof.Length -lt 20 -or $proof -match $placeholderPattern) {
    $placeholders += "$($file.Name): red-proof is a placeholder, not an observed failure"
  }

  $mentioned = @([regex]::Matches($proof, '\b[a-z0-9][a-z0-9-]*/[a-z0-9][a-z0-9/-]*\b') | ForEach-Object { $_.Value } | Where-Object { $owner.ContainsKey($_) })
  if ($mentioned.Count -gt 0) {
    $own = @($mentioned | Where-Object { $file.Names -contains $_ })
    if ($own.Count -eq 0) {
      $others = ($mentioned | Sort-Object -Unique) -join ", "
      $copied += "$($file.Name): red-proof names $others (defined in another file), not this file's own test ($($file.Names -join ', '))"
    }
  }
}

$violations += $placeholders
$violations += $copied

if ($violations.Count -gt 0) {
  $failed = $true
  Write-Host "validate-tests: FAILED"
  foreach ($v in $violations) { Write-Host "  - $v" }
}

if ($failed) { exit 1 }

Write-Host "validate-tests: OK (registry in sync, red-proofs observed, 0 placeholder, 0 copied)"
exit 0
