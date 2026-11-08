# Record the screen plus system audio to an mp4. Runs in the desktop session (run-interactive.ps1).
# Video: ddagrab (DXGI output -Output) or gdigrab (whole desktop); audio: lf-loopback.ps1 piped in.
param([int]$Seconds = 10, [string]$Out = "$PSScriptRoot\clip.mp4", [int]$Output = 0, [ValidateSet('ddagrab', 'gdigrab')][string]$Grabber = 'ddagrab')
$ff = (Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\Gyan.FFmpeg*" -Recurse -Filter ffmpeg.exe -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
if (-not $ff) { $ff = (Get-Command ffmpeg -ErrorAction Stop).Source }
$lb = Join-Path $PSScriptRoot 'lf-loopback.ps1'
$fmt = (powershell -NoProfile -ExecutionPolicy Bypass -File $lb --info).Split(' ')   # rate channels bits float|int
$afmt = if ($fmt[3] -eq 'float') { 'f32le' } else { "s$($fmt[2])le" }
$video = if ($Grabber -eq 'ddagrab') { "-f lavfi -i ddagrab=output_idx=$($Output):framerate=30,hwdownload,format=bgra" } else { '-f gdigrab -framerate 30 -i desktop' }
$cmd = "powershell -NoProfile -ExecutionPolicy Bypass -File `"$lb`" $($Seconds + 3) | `"$ff`" -hide_banner -loglevel error -y $video " +
  "-f $afmt -ar $($fmt[0]) -ac $($fmt[1]) -i pipe:0 " +
  "-map 0:v -map 1:a -c:v libx264 -preset veryfast -pix_fmt yuv420p -c:a aac -b:a 160k -t $Seconds `"$Out`" 2> `"$Out.log`""
cmd /c $cmd
# Duration and audio levels, for a quick "did it record sound" check (mean_volume -91 dB = silence).
cmd /c "`"$ff`" -hide_banner -i `"$Out`" -map 0:a -af volumedetect -f null NUL 2>&1" | Select-String 'Duration|mean_volume|max_volume' | ForEach-Object { $_.Line.Trim() } | Set-Content "$Out.txt"
