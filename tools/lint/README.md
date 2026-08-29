# Lint checks

Drop any runnable script here (`.ps1`, `.cmd`, `.mjs`, `.js`, `.cjs`, `.sh`)
and it runs automatically via:

```
tools\cli lint
```

Each script runs with the repo root as the working directory. Exit `0` =
pass, non-zero = fail (the CLI reports PASS/FAIL per script).

Example - reject tabs in committed Enforce scripts (`no-tabs.ps1`):

```powershell
$bad = git grep -n "`t" -- "*.c" | Select-String -NotMatch '^\s'
if ($bad) { Write-Host $bad; exit 1 }
Write-Host "no tabs in scripts: OK"
```