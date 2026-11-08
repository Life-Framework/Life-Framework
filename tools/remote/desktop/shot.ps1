# Screenshot the whole desktop (every active display) to a png. Runs in the desktop session.
param([string]$Out = "$PSScriptRoot\shot.png")
Add-Type -AssemblyName System.Windows.Forms, System.Drawing
[System.Windows.Forms.Screen]::AllScreens | ForEach-Object { "$($_.DeviceName) primary=$($_.Primary) bounds=$($_.Bounds)" } | Set-Content ($Out + '.txt')
$b = [System.Windows.Forms.SystemInformation]::VirtualScreen
$bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($b.Left, $b.Top, 0, 0, $bmp.Size)
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
