# tools/validation/gen-test-registry.ps1 - the test registration check (#2610, #2618).
# inputs: addons/**, tools/lib/test-world.mjs, tools/validation/validate-registries.mjs
#
# There is no generated test registry any more. A test is a class `X : LF_Test` with its own
# `[LF_TestInfo(LF_TestTier.X[, pack: true])]` directly above it; LF_TestManager finds it by reflection
# and runs the suite sorted by class name. Nothing is generated, so `tools\cli regen-tests` (which runs this
# with -Write) only checks: every test has exactly one attribute, pack tests say pack: true, and no
# central list came back (tools/lib/test-world.mjs testInfoProblems, the rule validate-registries runs).
#
# Usage: gen-test-registry.ps1 [-Check] [-Write]   (both only check; exit 1 lists the problems)
param(
  [switch]$Write,
  [switch]$Check
)
$ErrorActionPreference = "Stop"
$root = git rev-parse --show-toplevel
if (-not $root) { Write-Host "ERROR not a git work tree"; exit 1 }
& node (Join-Path $root "tools/validation/validate-registries.mjs")
if ($LASTEXITCODE -ne 0) { exit 1 }
if ($Write) { Write-Host "regen-tests: nothing to generate; each test registers itself with [LF_TestInfo] (#2610)" }
exit 0
