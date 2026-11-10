# shots-image.ps1 - image work for agent-box/shots.mjs, with System.Drawing (no npm image library).
#   powershell -NoProfile -File shots-image.ps1 <spec.json>
# spec.op:
#   norm   { in, out, width }                 png (or bmp) -> png scaled to `width`; prints "stats <mean> <sd> <skyFrac>"
#   sheet  { tiles:[{file,label,marks:[{name,x,y}]}], out, cols, maxWidth, quality }   contact sheet jpeg
#   crop   { in, out, x, y, w, h, maxSide }   native-resolution crop, scaled down only if over maxSide
Add-Type -AssemblyName System.Drawing
$spec = Get-Content -Raw -LiteralPath $args[0] | ConvertFrom-Json

function Save-Jpeg($bmp, $path, $quality) {
  $codec = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }
  $p = New-Object System.Drawing.Imaging.EncoderParameters(1)
  $p.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter([System.Drawing.Imaging.Encoder]::Quality, [long]$quality)
  $bmp.Save($path, $codec, $p)
}

function New-Canvas($w, $h) {
  $b = New-Object System.Drawing.Bitmap($w, $h)
  $g = [System.Drawing.Graphics]::FromImage($b)
  $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
  $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
  return @($b, $g)
}

if ($spec.op -eq 'norm') {
  $src = [System.Drawing.Image]::FromFile($spec.in)
  $w = [int]$spec.width
  $h = [int][math]::Round($src.Height * $w / $src.Width)
  $c = New-Canvas $w $h
  $c[1].DrawImage($src, 0, 0, $w, $h)
  $src.Dispose()
  # Coarse stats over a 48x27 grid: mean luma, spread, and the share of near-uniform bright-blue/grey top-row pixels.
  $n = 0; $sum = 0.0; $sq = 0.0
  for ($gy = 0; $gy -lt 27; $gy++) { for ($gx = 0; $gx -lt 48; $gx++) {
    $px = $c[0].GetPixel([int](($gx + 0.5) * $w / 48), [int](($gy + 0.5) * $h / 27))
    $l = 0.299 * $px.R + 0.587 * $px.G + 0.114 * $px.B
    $sum += $l; $sq += $l * $l; $n++
  } }
  $mean = $sum / $n; $sd = [math]::Sqrt([math]::Max(0, $sq / $n - $mean * $mean))
  $c[0].Save($spec.out, [System.Drawing.Imaging.ImageFormat]::Png)
  $c[1].Dispose(); $c[0].Dispose()
  Write-Output ("stats {0:N1} {1:N1}" -f $mean, $sd)
  exit 0
}

if ($spec.op -eq 'crop') {
  $src = [System.Drawing.Bitmap]::FromFile($spec.in)
  $x = [math]::Max(0, [int]$spec.x); $y = [math]::Max(0, [int]$spec.y)
  $w = [math]::Min([int]$spec.w, $src.Width - $x); $h = [math]::Min([int]$spec.h, $src.Height - $y)
  if ($w -lt 1 -or $h -lt 1) { Write-Output "error crop outside the image ($($src.Width)x$($src.Height))"; exit 2 }
  $side = [int]$spec.maxSide
  $scale = 1.0
  if ($side -gt 0 -and [math]::Max($w, $h) -gt $side) { $scale = $side / [double][math]::Max($w, $h) }
  $ow = [int][math]::Round($w * $scale); $oh = [int][math]::Round($h * $scale)
  $c = New-Canvas $ow $oh
  $c[1].DrawImage($src, (New-Object System.Drawing.Rectangle(0, 0, $ow, $oh)), $x, $y, $w, $h, [System.Drawing.GraphicsUnit]::Pixel)
  $src.Dispose()
  $c[0].Save($spec.out, [System.Drawing.Imaging.ImageFormat]::Png)
  $c[1].Dispose(); $c[0].Dispose()
  Write-Output "ok ${ow}x${oh}"
  exit 0
}

if ($spec.op -eq 'sheet') {
  $cols = [int]$spec.cols
  $tiles = @($spec.tiles)
  $rows = [int][math]::Ceiling($tiles.Count / $cols)
  $gap = 4
  $tw = [int][math]::Floor(([int]$spec.maxWidth - $gap * ($cols + 1)) / $cols)
  $th = [int][math]::Round($tw * 9 / 16)
  $lab = 20
  $W = $gap + $cols * ($tw + $gap)
  $H = $gap + $rows * ($th + $lab + $gap)
  $c = New-Canvas $W $H
  $g = $c[1]
  $g.Clear([System.Drawing.Color]::FromArgb(24, 24, 28))
  $font = New-Object System.Drawing.Font('Consolas', 10, [System.Drawing.FontStyle]::Bold)
  $small = New-Object System.Drawing.Font('Consolas', 8, [System.Drawing.FontStyle]::Bold)
  $white = [System.Drawing.Brushes]::White
  $shadow = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(200, 0, 0, 0))
  $pen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(255, 255, 220, 40), 2)
  for ($i = 0; $i -lt $tiles.Count; $i++) {
    $t = $tiles[$i]
    $ox = $gap + ($i % $cols) * ($tw + $gap)
    $oy = $gap + [int][math]::Floor($i / $cols) * ($th + $lab + $gap)
    if ($t.file -and (Test-Path -LiteralPath $t.file)) {
      $img = [System.Drawing.Image]::FromFile($t.file)
      $g.DrawImage($img, $ox, $oy, $tw, $th)
      $sx = $tw / [double]$img.Width
      $img.Dispose()
      foreach ($m in @($t.marks)) {
        if ($null -eq $m) { continue }
        $mx = $ox + [double]$m.x * $sx; $my = $oy + [double]$m.y * $sx
        $g.DrawEllipse($pen, [single]($mx - 3), [single]($my - 3), 6, 6)
        $g.DrawString([string]$m.name, $small, $shadow, [single]($mx + 5), [single]($my - 5))
        $g.DrawString([string]$m.name, $small, [System.Drawing.Brushes]::Yellow, [single]($mx + 4), [single]($my - 6))
      }
    } else {
      $g.FillRectangle($shadow, $ox, $oy, $tw, $th)
      $g.DrawString('no image', $font, $white, $ox + 8, $oy + 8)
    }
    $g.DrawString([string]$t.label, $font, $white, [single]$ox, [single]($oy + $th + 3))
  }
  Save-Jpeg $c[0] $spec.out ([int]$spec.quality)
  $g.Dispose(); $c[0].Dispose()
  Write-Output "ok ${W}x${H}"
  exit 0
}

Write-Output "error unknown op $($spec.op)"
exit 2
