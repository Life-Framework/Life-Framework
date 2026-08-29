# tools/lint/check-no-ternaries.ps1 - reject ternary operators in Enforce scripts.
#
# EnforceScript has no ternary (`cond ? a : b` is a compile error - AGENTS.md
# "Hard EnforceScript lessons"). This greps for the space-?-space signature,
# which avoids question marks ending string literals or doc comments.
#
# Run via: tools\cli lint

$ErrorActionPreference = "Stop"

$hits = git grep -n -P '\s\?\s' -- "addons/LifeFramework/Scripts" 2>$null
if ($LASTEXITCODE -eq 0 -and $hits) {
  Write-Host $hits
  Write-Host "check-no-ternaries: ternary operator found - use if/else" -ForegroundColor Red
  exit 1
}

Write-Host "check-no-ternaries: OK (no ternary operators)" -ForegroundColor Green
exit 0