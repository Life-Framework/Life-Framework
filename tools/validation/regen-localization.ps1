# tools/validation/regen-localization.ps1 - keep the runtime string tables in sync with the .st source.
#
# Language/everonlife_localization.st is the source of truth for every UI string.
# The per-language runtime tables (everonlife_localization.<lang>.conf) are compiled
# artifacts the engine loads at boot; when they drift from the .st the game logs
# "GUI (W): Missing string ID = EL-..." for every key the .st has but the .conf does not.
#
# This script derives the runtime .conf files from the .st, so adding a key to the
# .st and running -Write refreshes every language. Bare run (e.g. `cli validate`)
# is check mode: it fails when a .conf is out of sync, so a drifted table cannot
# slip back in silently.
#
# Modes:
#   -Write  regenerate all Language/everonlife_localization.<lang>.conf from the .st
#   -Check  regenerate to strings and compare with disk (exit 1 on drift)
#
# Fallback rule: a key missing its Target_<lang> falls back to Target_en_us so a
# contributor who adds a key in English only does not strand the other languages
# with a raw-key render. A key missing both is skipped (the .st validator flags it).
#
# Exit codes: 0 = written / in sync, 1 = drift or parse failure.

param(
  [switch]$Write,
  [switch]$Check
)

$ErrorActionPreference = "Stop"

$root = git rev-parse --show-toplevel
if (-not $root) { Write-Host "ERROR not a git work tree"; exit 1 }

$stPath = Join-Path $root "addons\LifeFramework\Language\everonlife_localization.st"
$dir = Split-Path -Parent $stPath

# Order matters: Ids[] and Texts[] in a StringTableRuntime are position-parallel,
# so both arrays must be emitted in the same .st item order.
$languages = @(
  @{ Code = "en_us"; Target = "Target_en_us" },
  @{ Code = "de_de"; Target = "Target_de_de" },
  @{ Code = "fr_fr"; Target = "Target_fr_fr" },
  @{ Code = "it_it"; Target = "Target_it_it" },
  @{ Code = "es_es"; Target = "Target_es_es" },
  @{ Code = "pt_pt"; Target = "Target_pt_pt" },
  @{ Code = "pt_br"; Target = "Target_pt_br" }
)

if (-not (Test-Path -LiteralPath $stPath)) {
  Write-Host "regen-localization: FAILED - source not found: $stPath"
  exit 1
}

$stText = [System.IO.File]::ReadAllText($stPath, [System.Text.Encoding]::UTF8)

# Each item is a flat property block: CustomStringTableItem "{GUID}" { Id ... Target_xx ... }
$blockPattern = 'CustomStringTableItem\s+"\{[0-9A-Fa-f]+\}"\s*\{([^}]*)\}'
$items = @()
foreach ($m in [regex]::Matches($stText, $blockPattern)) {
  $body = $m.Groups[1].Value
  $idMatch = [regex]::Match($body, 'Id\s+"([^"]+)"')
  if (-not $idMatch.Success) { continue }
  $item = [ordered]@{ Id = $idMatch.Groups[1].Value }
  foreach ($lang in $languages) {
    $t = [regex]::Match($body, [regex]::Escape($lang.Target) + '\s+"([^"]*)"')
    $item[$lang.Target] = if ($t.Success) { $t.Groups[1].Value } else { $null }
  }
  $items += $item
}

if ($items.Count -eq 0) {
  Write-Host "regen-localization: FAILED - no CustomStringTableItem entries parsed from $stPath"
  exit 1
}

function New-ConfContent {
  param($Language)
  $sb = New-Object System.Text.StringBuilder
  [void]$sb.Append("StringTableRuntime {")
  [void]$sb.Append("`r`n Ids {")
  foreach ($item in $items) { [void]$sb.Append("`r`n  `"" + $item.Id + "`"") }
  [void]$sb.Append("`r`n }")
  [void]$sb.Append("`r`n Texts {")
  foreach ($item in $items) {
    $text = $item[$Language.Target]
    if ($null -eq $text -or $text -eq "") { $text = $item["Target_en_us"] }
    if ($null -eq $text) { continue }
    [void]$sb.Append("`r`n  `"" + $text + "`"")
  }
  [void]$sb.Append("`r`n }")
  [void]$sb.Append("`r`n}")
  [void]$sb.Append("`r`n")
  return $sb.ToString()
}

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$failed = $false

foreach ($lang in $languages) {
  $confPath = Join-Path $dir "everonlife_localization.$($lang.Code).conf"
  $content = New-ConfContent $lang

  if ($Write) {
    [System.IO.File]::WriteAllText($confPath, $content, $utf8NoBom)
    Write-Host "regen-localization: wrote $($lang.Code).conf ($($items.Count) keys)"
    continue
  }

  if (-not (Test-Path -LiteralPath $confPath)) {
    Write-Host "regen-localization: FAILED - $($lang.Code).conf missing (run 'tools\cli regen-localization' with -Write)"
    $failed = $true
    continue
  }
  $onDisk = [System.IO.File]::ReadAllText($confPath, [System.Text.Encoding]::UTF8)
  if ($onDisk -ne $content) {
    Write-Host "regen-localization: FAILED - $($lang.Code).conf out of sync with the .st (run regen-localization -Write)"
    $failed = $true
  }
}

# Every #EL-/#LF- key referenced in code, layouts, or prefabs must exist in the .st,
# or the game logs "GUI (W): Missing string ID = <key>" at runtime. This is the other
# half of the same drift: a key can be missing from the source, not just from the confs.
$stIdSet = @{}
foreach ($item in $items) { $stIdSet[$item.Id] = $true }

$addonDir = Join-Path $root "addons\LifeFramework"
$refPattern = '#((?:EL-|LF-)[A-Za-z0-9_]+)'
$missingRefs = @()
if (Test-Path -LiteralPath $addonDir) {
  Get-ChildItem $addonDir -Recurse -Include *.layout,*.et,*.c -File | Where-Object {
    $_.FullName -notmatch '\\Language\\'
  } | ForEach-Object {
    $text = [System.IO.File]::ReadAllText($_.FullName, [System.Text.Encoding]::UTF8)
    foreach ($m in [regex]::Matches($text, $refPattern)) {
      $key = $m.Groups[1].Value
      if (-not $stIdSet.ContainsKey($key)) {
        $missingRefs += "$key (referenced in $($_.Name))"
      }
    }
  }
}
$missingRefs = $missingRefs | Sort-Object -Unique
if ($missingRefs.Count -gt 0) {
  Write-Host "regen-localization: FAILED - $($missingRefs.Count) referenced key(s) not declared in the .st (add them to Language/everonlife_localization.st then re-run):"
  foreach ($r in $missingRefs) { Write-Host "  - $r" }
  exit 1
}

if ($failed) { exit 1 }

if ($Write) {
  Write-Host "regen-localization: OK ($($items.Count) keys -> $($languages.Count) languages)"
  exit 0
}

Write-Host "regen-localization: OK ($($items.Count) keys, all $($languages.Count) language tables in sync)"
exit 0