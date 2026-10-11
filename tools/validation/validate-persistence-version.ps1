# tools/validation/validate-persistence-version.ps1
#
# EL_PersistenceComponentSerializer.c writes one version integer. Each payload
# is named once. The written version and the named append order must agree.
# Today version 4 appends vehicles after hideouts. This check does not change
# that version; it fails when the file disagrees with itself.
#
# Exit codes: 0 = contract holds, 1 = the written version and the named order disagree.

$ErrorActionPreference = "Stop"

function Get-PayloadToken([string]$word) {
  $lower = $word.ToLowerInvariant()
  if ($lower.StartsWith("organiz")) { return "organizations" }
  if ($lower.StartsWith("hideout")) { return "hideout" }
  if ($lower.StartsWith("vehicle")) { return "vehicles" }
  return $lower
}

function Get-WriteToken([string]$argument) {
  if ($argument -match "hideout") { return "hideout" }
  if ($argument -match "vehicle") { return "vehicles" }
  if ($argument -match "org") { return "organizations" }
  return $null
}

# Returns $null when the text agrees, or an error string.
function Test-PersistenceContract([string]$text) {
  $writes = [regex]::Matches($text, 'WriteValue\(\s*"version"\s*,\s*(\d+)\s*\)')
  if ($writes.Count -ne 1) {
    return "expected one version integer, found $($writes.Count)"
  }
  $written = [int]$writes[0].Groups[1].Value

  $named = @{}
  $namedOrder = @()
  foreach ($match in [regex]::Matches($text, 'Version\s+(\d+)\s+appends\s+([A-Za-z]+)')) {
    $version = [int]$match.Groups[1].Value
    $payload = Get-PayloadToken $match.Groups[2].Value
    if ($named.ContainsKey($version) -and $named[$version] -ne $payload) {
      return "version $version names '$($named[$version])' and '$payload'"
    }
    if (-not $named.ContainsKey($version)) {
      $named[$version] = $payload
      $namedOrder += $version
    }
  }

  $seenPayload = @{}
  foreach ($version in $named.Keys) {
    $payload = $named[$version]
    if ($seenPayload.ContainsKey($payload)) {
      return "payload $payload is named more than once"
    }
    $seenPayload[$payload] = $version
  }

  for ($version = 2; $version -le $written; $version++) {
    if (-not $named.ContainsKey($version)) {
      return "written version $written disagrees with the named order: version $version is not named"
    }
  }
  foreach ($version in $named.Keys) {
    if ($version -lt 2 -or $version -gt $written) {
      return "named version $version disagrees with written version $written"
    }
  }

  $start = $text.IndexOf("ESerializeResult Serialize")
  $end = $text.IndexOf("bool Deserialize")
  if ($start -lt 0 -or $end -lt $start) {
    return "Serialize function not found"
  }
  $body = $text.Substring($start, $end - $start)
  $writeOrder = @()
  foreach ($match in [regex]::Matches($body, 'context\.Write\(([^)]+)\)')) {
    $token = Get-WriteToken $match.Groups[1].Value
    if ($token) { $writeOrder += $token }
  }

  $expected = @()
  for ($version = 2; $version -le $written; $version++) { $expected += $named[$version] }
  $expectedText = $expected -join ", "
  $actualText = $writeOrder -join ", "
  if ($actualText -ne $expectedText) {
    return "written version $written disagrees with the named order (named: $expectedText; written: $actualText)"
  }
  return $null
}

$good = @'
//! Version 2 appends organizations. Version 3 appends hideout ownership after that array.
//! Version 4 appends vehicles after the hideout array.
ESerializeResult Serialize(
		context.WriteValue("version", 4);
		context.Write(accountRecords);
		context.Write(bankRecords);
		context.Write(survivalRecords);
		context.Write(orgRecords);
		context.Write(hideoutRecords);
		context.Write(vehicleRecords);
)
bool Deserialize(
'@

$badVersion = @'
//! Version 2 appends organizations. Version 3 appends hideout ownership.
//! Version 4 appends vehicles after the hideout array.
ESerializeResult Serialize(
		context.WriteValue("version", 3);
		context.Write(orgRecords);
		context.Write(hideoutRecords);
		context.Write(vehicleRecords);
)
bool Deserialize(
'@

$badOrder = @'
//! Version 2 appends organizations. Version 3 appends hideout ownership.
//! Version 4 appends vehicles after the hideout array.
ESerializeResult Serialize(
		context.WriteValue("version", 4);
		context.Write(orgRecords);
		context.Write(vehicleRecords);
		context.Write(hideoutRecords);
)
bool Deserialize(
'@

$badTwice = @'
//! Version 2 appends organizations. Version 3 appends organizations.
ESerializeResult Serialize(
		context.WriteValue("version", 3);
		context.Write(orgRecords);
)
bool Deserialize(
'@

$badTwoIntegers = @'
//! Version 2 appends organizations.
ESerializeResult Serialize(
		context.WriteValue("version", 2);
		context.WriteValue("version", 2);
		context.Write(orgRecords);
)
bool Deserialize(
'@

$fixtureErrors = @()
if (Test-PersistenceContract $good) { $fixtureErrors += "good fixture was rejected: $(Test-PersistenceContract $good)" }
if (-not (Test-PersistenceContract $badVersion)) { $fixtureErrors += "version/name disagreement was accepted" }
if (-not (Test-PersistenceContract $badOrder)) { $fixtureErrors += "vehicles-before-hideouts write order was accepted" }
if (-not (Test-PersistenceContract $badTwice)) { $fixtureErrors += "a payload named twice was accepted" }
if (-not (Test-PersistenceContract $badTwoIntegers)) { $fixtureErrors += "two version integers were accepted" }
if ($fixtureErrors.Count -gt 0) {
  Write-Host "validate-persistence-version: FAIL (fixture selftest)"
  foreach ($err in $fixtureErrors) { Write-Host "  - $err" }
  exit 1
}

$root = git rev-parse --show-toplevel
if (-not $root) { Write-Host "ERROR not a git work tree"; exit 1 }
$path = Join-Path $root "addons\LifeFramework\Scripts\Game\Persistence\Serializers\EL_PersistenceComponentSerializer.c"
if (-not (Test-Path -LiteralPath $path)) {
  Write-Host "validate-persistence-version: FAIL missing $path"
  exit 1
}
$text = [System.IO.File]::ReadAllText($path)
$errorText = Test-PersistenceContract $text
if ($errorText) {
  Write-Host "validate-persistence-version: FAIL $errorText"
  exit 1
}

Write-Host "validate-persistence-version: OK (version 4 appends vehicles after hideouts; fixture disagreements fail)"
exit 0
