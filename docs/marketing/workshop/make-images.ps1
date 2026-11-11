# Builds the Workshop images from the website gallery (website/public/docs-images) and the logo.
#   powershell -ExecutionPolicy Bypass -File docs\marketing\workshop\make-images.ps1
# Writes:
#   docs/marketing/workshop/preview.jpg              Workshop preview tile, 1920x1080, under 2 MB
#   docs/marketing/workshop/screenshots/NN-*.jpg     Workshop gallery, 1280x720 (source size)
#   addons/LifeFramework/Missions/LF_PlaytestTown_Preview.jpg  scenario icon, preview and loading screen
#   addons/LifeFramework/thumbnail.png               Workbench project thumbnail, 1024x448
# The scenario image is the source of LF_PlaytestTown_Preview.edds; after changing it, re-import the
# .edds in Workbench (Resource Browser > right-click the jpg > Reimport) or the old pixels stay.
param([string]$Root = (Resolve-Path "$PSScriptRoot\..\..\.."))

Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = "Stop"

$gallery = Join-Path $Root "website\public\docs-images"
$out = Join-Path $Root "docs\marketing\workshop"
$shots = Join-Path $out "screenshots"
New-Item -ItemType Directory -Force $shots | Out-Null

function Save-Jpeg([System.Drawing.Bitmap]$bmp, [string]$path, [int]$quality = 90) {
    $codec = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq "image/jpeg" }
    $params = New-Object System.Drawing.Imaging.EncoderParameters 1
    $params.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality), ([long]$quality)
    $bmp.Save($path, $codec, $params)
    $kb = [int]((Get-Item $path).Length / 1KB)
    if ($kb -ge 2000) { throw "$path is $kb KB; the Workshop limit is 2 MB" }
    Write-Host "wrote $path ($($bmp.Width)x$($bmp.Height), $kb KB)"
}

# Preview: the spawn square, the logo badge, and the scenario name over the sky.
function New-Preview([int]$w, [int]$h) {
    $src = [System.Drawing.Image]::FromFile((Join-Path $gallery "area-1-spawn-square\wide-after.png"))
    $logo = [System.Drawing.Image]::FromFile((Join-Path $Root "website\public\brand\logo-1024.png"))
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $g.DrawImage($src, 0, 0, $w, $h)

    # Darken the top third so the title reads over the sky and the trees.
    $band = [int]($h * 0.42)
    # The brush rectangle is taller than the fill: GDI+ wraps a gradient at its edge and draws a hairline.
    $grad = New-Object System.Drawing.Drawing2D.LinearGradientBrush (New-Object System.Drawing.Rectangle 0, -2, $w, ($band + 4)), ([System.Drawing.Color]::FromArgb(170, 10, 12, 16)), ([System.Drawing.Color]::FromArgb(0, 10, 12, 16)), 90
    $g.FillRectangle($grad, 0, 0, $w, $band)

    # Logo badge, clipped to its circle so the square's dark corners do not show.
    $s = $w / 1920.0
    $d = [int](250 * $s); $x = [int](70 * $s); $y = [int](60 * $s)
    $clip = New-Object System.Drawing.Drawing2D.GraphicsPath
    $clip.AddEllipse($x, $y, $d, $d)
    $g.SetClip($clip)
    $g.DrawImage($logo, $x, $y, $d, $d)
    $g.ResetClip()

    $white = [System.Drawing.Brushes]::White
    $shadow = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(160, 0, 0, 0))
    $blue = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 120, 180, 230))
    $title = New-Object System.Drawing.Font "Bahnschrift SemiBold Condensed", ([single](118 * $s)), ([System.Drawing.FontStyle]::Regular), ([System.Drawing.GraphicsUnit]::Pixel)
    $sub = New-Object System.Drawing.Font "Bahnschrift SemiBold Condensed", ([single](70 * $s)), ([System.Drawing.FontStyle]::Regular), ([System.Drawing.GraphicsUnit]::Pixel)
    $tx = $x + $d + [int](40 * $s)
    $o = [int](4 * $s)
    $g.DrawString("LIFE FRAMEWORK", $title, $shadow, $tx + $o, [int](70 * $s) + $o)
    $g.DrawString("LIFE FRAMEWORK", $title, $white, $tx, [int](70 * $s))
    $g.DrawString("PLAYTEST TOWN", $sub, $shadow, $tx + $o, [int](195 * $s) + $o)
    $g.DrawString("PLAYTEST TOWN", $sub, $blue, $tx + [int](4 * $s), [int](195 * $s))

    $g.Dispose(); $src.Dispose(); $logo.Dispose()
    return $bmp
}

$p = New-Preview 1920 1080
Save-Jpeg $p (Join-Path $out "preview.jpg") 90
Save-Jpeg $p (Join-Path $Root "addons\LifeFramework\Missions\LF_PlaytestTown_Preview.jpg") 90
# Workbench project thumbnail (addons/LifeFramework/thumbnail.png, 1024x448): the top of the preview.
$thumb = New-Object System.Drawing.Bitmap 1024, 448
$tg = [System.Drawing.Graphics]::FromImage($thumb)
$tg.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$tg.DrawImage($p, (New-Object System.Drawing.Rectangle 0, 0, 1024, 448), (New-Object System.Drawing.Rectangle 0, 0, 1920, 840), [System.Drawing.GraphicsUnit]::Pixel)
$tg.Dispose()
$thumb.Save((Join-Path $Root "addons\LifeFramework\thumbnail.png"), [System.Drawing.Imaging.ImageFormat]::Png)
$thumb.Dispose()
$p.Dispose()

# Gallery: the featured shots from website/content/gallery.json, in the order a player meets them.
$picks = @(
    "area-1-spawn-square/wide-after",
    "area-3-store/counter",
    "area-2-bank/detail",
    "area-4-carlot/detail",
    "area-5-fuel-station/wide",
    "area-6-police/wide",
    "area-7-hospital/wide",
    "area-10-cash-van/bags",
    "area-13-hideout/bench"
)
Get-ChildItem $shots -Filter *.jpg | Remove-Item
$n = 1
foreach ($id in $picks) {
    $img = [System.Drawing.Image]::FromFile((Join-Path $gallery ($id.Replace("/", "\") + ".png")))
    $bmp = New-Object System.Drawing.Bitmap $img
    $name = "{0:D2}-{1}.jpg" -f $n, ($id -replace "^area-\d+-", "" -replace "/", "-")
    Save-Jpeg $bmp (Join-Path $shots $name) 92
    $bmp.Dispose(); $img.Dispose()
    $n++
}
