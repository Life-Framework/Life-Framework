# Compiles lf-procaudio.cs in memory (Smart App Control blocks unsigned exes on this box) and runs it. See the .cs header.
Add-Type -TypeDefinition (Get-Content -Raw (Join-Path $PSScriptRoot 'lf-procaudio.cs'))
exit [LfProcAudio]::Main([string[]]$args)
