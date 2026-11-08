# Compiles lf-loopback.cs in memory (Smart App Control blocks unsigned exes on this box) and runs it.
Add-Type -TypeDefinition (Get-Content -Raw (Join-Path $PSScriptRoot 'lf-loopback.cs'))
exit [LfLoopback]::Main([string[]]$args)
