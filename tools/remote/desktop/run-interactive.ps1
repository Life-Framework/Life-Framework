# Run a program in the signed-in desktop session (session 1) from ssh (session 0), through a one-shot
# scheduled task (LogonType Interactive). Waits for it unless -NoWait; the task is removed either way.
param([Parameter(Mandatory)][string]$Exe, [string]$WorkDir = '', [string]$Arguments = '', [int]$WaitSec = 60, [switch]$NoWait)
$name = 'LF-Interactive-' + [guid]::NewGuid().ToString('N').Substring(0,8)
if (-not $WorkDir) { $WorkDir = Split-Path $Exe -Parent }
if (-not $WorkDir) { $WorkDir = $env:TEMP }
$a = New-ScheduledTaskAction -Execute $Exe -Argument $Arguments -WorkingDirectory $WorkDir
$p = New-ScheduledTaskPrincipal -UserId (whoami) -LogonType Interactive -RunLevel Limited
$s = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit (New-TimeSpan -Hours 2)
Register-ScheduledTask -TaskName $name -Action $a -Principal $p -Settings $s | Out-Null
Start-ScheduledTask -TaskName $name
if (-not $NoWait) {
  $t0 = Get-Date
  do { Start-Sleep -Milliseconds 500; $st = (Get-ScheduledTask -TaskName $name).State } while ($st -eq 'Running' -and ((Get-Date) - $t0).TotalSeconds -lt $WaitSec)
  "task $name state=$st result=$((Get-ScheduledTaskInfo -TaskName $name).LastTaskResult)"
}
Unregister-ScheduledTask -TaskName $name -Confirm:$false
