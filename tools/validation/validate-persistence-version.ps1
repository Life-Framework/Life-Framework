# tools/validation/validate-persistence-version.ps1
#
# EL_PersistenceComponentSerializer.c writes one version integer.
# RegisterPersistencePayloads is the only order. Serialize walks that list.
# Deserialize walks the same list and stops at the version stored in the save.
# The written version, the list length, and the read order must agree.
# A written version that skips a payload fails. This check does not change
# the version.
#
# Exit codes: 0 = contract holds, 1 = the written version, the list length,
# and the read order disagree (or a fixture selftest failed).

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
  if ($argument -match "survival") { return "survival" }
  return $null
}

function Format-Order($items) {
  return (@($items) -join ", ")
}

# Body includes the surrounding braces. $null when the signature is absent.
function Get-BraceBody([string]$text, [string]$signaturePattern) {
  $match = [regex]::Match($text, $signaturePattern)
  if (-not $match.Success) { return $null }
  $start = $text.IndexOf("{", $match.Index)
  if ($start -lt 0) { return $null }
  $depth = 0
  for ($i = $start; $i -lt $text.Length; $i++) {
    $char = $text[$i]
    if ($char -eq "{") { $depth++ }
    elseif ($char -eq "}") {
      $depth--
      if ($depth -eq 0) {
        return $text.Substring($start, $i - $start + 1)
      }
    }
  }
  return $null
}

function Get-PayloadList([string]$text) {
  $body = Get-BraceBody $text 'RegisterPersistencePayloads\s*\(\s*notnull\s+array<string>\s+order\s*\)'
  if (-not $body) {
    return @{ Order = @(); Error = "payload list not found" }
  }
  $order = New-Object System.Collections.Generic.List[string]
  $seen = @{}
  foreach ($match in [regex]::Matches($body, 'Insert\(\s*"([^"]+)"\s*\)')) {
    $payload = Get-PayloadToken $match.Groups[1].Value
    if ($seen.ContainsKey($payload)) {
      return @{ Order = @(); Error = "payload $payload is listed more than once" }
    }
    $seen[$payload] = $true
    $order.Add($payload)
  }
  if ($order.Count -eq 0) {
    return @{ Order = @(); Error = "payload list is empty" }
  }
  return @{ Order = @($order.ToArray()); Error = $null }
}

function Get-LoopBody([string]$text, [int]$openIndex) {
  $start = $text.IndexOf("{", $openIndex)
  if ($start -lt 0) { return $null }
  $depth = 0
  for ($i = $start; $i -lt $text.Length; $i++) {
    $char = $text[$i]
    if ($char -eq "{") { $depth++ }
    elseif ($char -eq "}") {
      $depth--
      if ($depth -eq 0) {
        return $text.Substring($start, $i - $start + 1)
      }
    }
  }
  return $null
}

# Read order is the list when Deserialize walks it from index 0 and stops at
# the stored version. An explicit ladder is its own order and can disagree.
function Get-ReadOrder([string]$text, $list) {
  $body = Get-BraceBody $text 'bool\s+Deserialize\s*\('
  if (-not $body) {
    return @{ Order = @(); Error = "Deserialize function not found" }
  }

  $walk = [regex]::Match($body, 'for\s*\(\s*int\s+index\s*=\s*0\s*;\s*index\s*<\s*order\.Count\(\)\s*;\s*index\+\+\s*\)')
  $explicit = [regex]::Matches($body, 'ReadPersistencePayload\s*\([^)]*"([^"]+)"')
  $ladder = [regex]::Matches($body, 'version\s*>=\s*\d+')

  if ($walk.Success) {
    $loopBody = Get-LoopBody $body $walk.Index
    if (-not $loopBody) {
      return @{ Order = @(); Error = "read loop has no body" }
    }
    if ($loopBody -notmatch 'if\s*\(\s*index\s*>=\s*version\s*\)') {
      return @{ Order = @(); Error = "read does not stop at the stored version" }
    }
    $stopAt = $loopBody.IndexOf("index")
    $stopMatch = [regex]::Match($loopBody, 'index\s*>=\s*version')
    $getMatch = [regex]::Match($loopBody, 'order\.Get\(\s*index\s*\)')
    if (-not $stopMatch.Success -or -not $getMatch.Success -or $stopMatch.Index -gt $getMatch.Index) {
      return @{ Order = @(); Error = "read does not walk the payload list" }
    }
    if ($loopBody -match '\bcontinue\b') {
      return @{ Order = @(); Error = "written version skips a payload" }
    }
    if ($explicit.Count -gt 0 -or $ladder.Count -gt 0) {
      return @{ Order = @(); Error = "read walks the list and also keeps a second order" }
    }
    return @{ Order = @($list); Error = $null }
  }

  if ($explicit.Count -gt 0) {
    $order = New-Object System.Collections.Generic.List[string]
    foreach ($match in $explicit) {
      $order.Add((Get-PayloadToken $match.Groups[1].Value))
    }
    return @{ Order = @($order.ToArray()); Error = $null }
  }

  return @{ Order = @(); Error = "read order not found" }
}

function Get-WriteOrder([string]$text, $list) {
  $body = Get-BraceBody $text 'ESerializeResult\s+Serialize\s*\('
  if (-not $body) {
    return @{ Order = @(); Error = "Serialize function not found" }
  }

  $explicit = New-Object System.Collections.Generic.List[string]
  foreach ($match in [regex]::Matches($body, 'context\.Write\(([^)]+)\)')) {
    $token = Get-WriteToken $match.Groups[1].Value
    if ($token) { $explicit.Add($token) }
  }

  $walk = [regex]::Match($body, 'foreach\s*\(\s*string\s+payload\s*:\s*order\s*\)')
  if ($walk.Success) {
    if ($body -notmatch 'RegisterPersistencePayloads\s*\(\s*order\s*\)') {
      return @{ Order = @(); Error = "Serialize does not register the payload list" }
    }
    if ($body -notmatch 'WritePersistencePayload\s*\(\s*persistence\s*,\s*context\s*,\s*payload\s*\)') {
      return @{ Order = @(); Error = "Serialize does not walk the payload list" }
    }
    if ($explicit.Count -gt 0) {
      return @{ Order = @($explicit.ToArray()); Error = $null }
    }
    return @{ Order = @($list); Error = $null }
  }

  return @{ Order = @($explicit.ToArray()); Error = $null }
}

function Test-HandlerCoverage([string]$text, $list, [int]$written) {
  $writeBody = Get-BraceBody $text 'void\s+WritePersistencePayload\s*\('
  $readBody = Get-BraceBody $text 'void\s+ReadPersistencePayload\s*\('
  if (-not $writeBody) { return "WritePersistencePayload not found" }
  if (-not $readBody) { return "ReadPersistencePayload not found" }
  foreach ($payload in @($list)) {
    $quoted = '"' + $payload + '"'
    if ($writeBody.IndexOf($quoted) -lt 0) {
      return "written version $written skips payload $payload"
    }
    if ($readBody.IndexOf($quoted) -lt 0) {
      return "read order skips payload $payload"
    }
  }
  return $null
}

# Returns $null when the text agrees, or an error string.
function Test-PersistenceContract([string]$text) {
  $listResult = Get-PayloadList $text
  if ($listResult.Error) { return $listResult.Error }
  $list = @($listResult.Order)

  $writes = [regex]::Matches($text, 'WriteValue\(\s*"version"\s*,\s*(\d+)\s*\)')
  if ($writes.Count -ne 1) {
    return "expected one version integer, found $($writes.Count)"
  }
  $written = [int]$writes[0].Groups[1].Value

  if ($written -ne $list.Count) {
    if ($written -gt $list.Count) {
      return "written version $written skips a payload (list length $($list.Count))"
    }
    return "written version $written disagrees with list length $($list.Count)"
  }

  $readResult = Get-ReadOrder $text $list
  if ($readResult.Error) { return $readResult.Error }
  $readOrder = @($readResult.Order)
  $listText = Format-Order $list
  $readText = Format-Order $readOrder
  if ($readText -ne $listText) {
    return "written version $written disagrees with the read order (list: $listText; read: $readText)"
  }

  $writeResult = Get-WriteOrder $text $list
  if ($writeResult.Error) { return $writeResult.Error }
  $writeText = Format-Order @($writeResult.Order)
  if ($writeText -ne $listText) {
    return "written version $written disagrees with the write order (list: $listText; write: $writeText)"
  }

  $handlerError = Test-HandlerCoverage $text $list $written
  if ($handlerError) { return $handlerError }
  return $null
}

$good = @'
void RegisterPersistencePayloads(notnull array<string> order)
{
	order.Insert("survival");
	order.Insert("organizations");
	order.Insert("hideout");
	order.Insert("vehicles");
}
ESerializeResult Serialize()
{
	array<string> order = new array<string>();
	RegisterPersistencePayloads(order);
	context.WriteValue("version", 4);
	context.Write(accountRecords);
	context.Write(bankRecords);
	foreach (string payload : order)
	{
		WritePersistencePayload(persistence, context, payload);
	}
}
bool Deserialize()
{
	array<string> order = new array<string>();
	RegisterPersistencePayloads(order);
	int version;
	context.ReadValue("version", version);
	for (int index = 0; index < order.Count(); index++)
	{
		if (index >= version)
			break;
		ReadPersistencePayload(persistence, context, order.Get(index));
	}
}
void WritePersistencePayload(notnull EL_PersistenceComponent persistence, notnull SaveContext context, string payload)
{
	if (payload == "survival") return;
	if (payload == "organizations") return;
	if (payload == "hideout") return;
	if (payload == "vehicles") return;
}
void ReadPersistencePayload(notnull EL_PersistenceComponent persistence, notnull LoadContext context, string payload)
{
	if (payload == "survival") return;
	if (payload == "organizations") return;
	if (payload == "hideout") return;
	if (payload == "vehicles") return;
}
'@

# Written version 4 claims a payload the list never registered.
$badSkip = @'
void RegisterPersistencePayloads(notnull array<string> order)
{
	order.Insert("survival");
	order.Insert("organizations");
	order.Insert("vehicles");
}
ESerializeResult Serialize()
{
	array<string> order = new array<string>();
	RegisterPersistencePayloads(order);
	context.WriteValue("version", 4);
	foreach (string payload : order)
	{
		WritePersistencePayload(persistence, context, payload);
	}
}
bool Deserialize()
{
	array<string> order = new array<string>();
	RegisterPersistencePayloads(order);
	int version;
	context.ReadValue("version", version);
	for (int index = 0; index < order.Count(); index++)
	{
		if (index >= version)
			break;
		ReadPersistencePayload(persistence, context, order.Get(index));
	}
}
void WritePersistencePayload(notnull EL_PersistenceComponent persistence, notnull SaveContext context, string payload)
{
	if (payload == "survival") return;
	if (payload == "organizations") return;
	if (payload == "vehicles") return;
}
void ReadPersistencePayload(notnull EL_PersistenceComponent persistence, notnull LoadContext context, string payload)
{
	if (payload == "survival") return;
	if (payload == "organizations") return;
	if (payload == "vehicles") return;
}
'@

$badVersion = @'
void RegisterPersistencePayloads(notnull array<string> order)
{
	order.Insert("survival");
	order.Insert("organizations");
	order.Insert("hideout");
	order.Insert("vehicles");
}
ESerializeResult Serialize()
{
	RegisterPersistencePayloads(order);
	context.WriteValue("version", 3);
	foreach (string payload : order)
	{
		WritePersistencePayload(persistence, context, payload);
	}
}
bool Deserialize()
{
	for (int index = 0; index < order.Count(); index++)
	{
		if (index >= version)
			break;
		ReadPersistencePayload(persistence, context, order.Get(index));
	}
}
void WritePersistencePayload(notnull EL_PersistenceComponent persistence, notnull SaveContext context, string payload)
{
	if (payload == "survival") return;
	if (payload == "organizations") return;
	if (payload == "hideout") return;
	if (payload == "vehicles") return;
}
void ReadPersistencePayload(notnull EL_PersistenceComponent persistence, notnull LoadContext context, string payload)
{
	if (payload == "survival") return;
	if (payload == "organizations") return;
	if (payload == "hideout") return;
	if (payload == "vehicles") return;
}
'@

$badRead = @'
void RegisterPersistencePayloads(notnull array<string> order)
{
	order.Insert("survival");
	order.Insert("organizations");
	order.Insert("hideout");
	order.Insert("vehicles");
}
ESerializeResult Serialize()
{
	RegisterPersistencePayloads(order);
	context.WriteValue("version", 4);
	foreach (string payload : order)
	{
		WritePersistencePayload(persistence, context, payload);
	}
}
bool Deserialize()
{
	if (version >= 1)
		ReadPersistencePayload(persistence, context, "survival");
	if (version >= 2)
		ReadPersistencePayload(persistence, context, "organizations");
	if (version >= 4)
		ReadPersistencePayload(persistence, context, "vehicles");
	if (version >= 3)
		ReadPersistencePayload(persistence, context, "hideout");
}
void WritePersistencePayload(notnull EL_PersistenceComponent persistence, notnull SaveContext context, string payload)
{
	if (payload == "survival") return;
	if (payload == "organizations") return;
	if (payload == "hideout") return;
	if (payload == "vehicles") return;
}
void ReadPersistencePayload(notnull EL_PersistenceComponent persistence, notnull LoadContext context, string payload)
{
	if (payload == "survival") return;
	if (payload == "organizations") return;
	if (payload == "hideout") return;
	if (payload == "vehicles") return;
}
'@

$badWrite = @'
void RegisterPersistencePayloads(notnull array<string> order)
{
	order.Insert("survival");
	order.Insert("organizations");
	order.Insert("hideout");
	order.Insert("vehicles");
}
ESerializeResult Serialize()
{
	context.WriteValue("version", 4);
	context.Write(survivalRecords);
	context.Write(orgRecords);
	context.Write(vehicleRecords);
	context.Write(hideoutRecords);
}
bool Deserialize()
{
	for (int index = 0; index < order.Count(); index++)
	{
		if (index >= version)
			break;
		ReadPersistencePayload(persistence, context, order.Get(index));
	}
}
void WritePersistencePayload(notnull EL_PersistenceComponent persistence, notnull SaveContext context, string payload)
{
	if (payload == "survival") return;
	if (payload == "organizations") return;
	if (payload == "hideout") return;
	if (payload == "vehicles") return;
}
void ReadPersistencePayload(notnull EL_PersistenceComponent persistence, notnull LoadContext context, string payload)
{
	if (payload == "survival") return;
	if (payload == "organizations") return;
	if (payload == "hideout") return;
	if (payload == "vehicles") return;
}
'@

$badTwice = @'
void RegisterPersistencePayloads(notnull array<string> order)
{
	order.Insert("organizations");
	order.Insert("organizations");
}
ESerializeResult Serialize()
{
	context.WriteValue("version", 2);
}
bool Deserialize()
{
}
'@

$badTwoIntegers = @'
void RegisterPersistencePayloads(notnull array<string> order)
{
	order.Insert("organizations");
}
ESerializeResult Serialize()
{
	context.WriteValue("version", 2);
	context.WriteValue("version", 2);
}
bool Deserialize()
{
}
'@

$fixtureErrors = @()
$goodError = Test-PersistenceContract $good
if ($goodError) { $fixtureErrors += "good fixture was rejected: $goodError" }

$skipError = Test-PersistenceContract $badSkip
if (-not $skipError) {
  $fixtureErrors += "written version that skips a payload was accepted"
} elseif ($skipError -notmatch "skips a payload") {
  $fixtureErrors += "skip fixture failed for the wrong reason: $skipError"
}

$versionError = Test-PersistenceContract $badVersion
if (-not $versionError) {
  $fixtureErrors += "version/list disagreement was accepted"
} elseif ($versionError -notmatch "disagrees with list length") {
  $fixtureErrors += "short version failed for the wrong reason: $versionError"
}

$readError = Test-PersistenceContract $badRead
if (-not $readError) {
  $fixtureErrors += "vehicles-before-hideouts read order was accepted"
} elseif ($readError -notmatch "read order") {
  $fixtureErrors += "read-order fixture failed for the wrong reason: $readError"
}

$writeError = Test-PersistenceContract $badWrite
if (-not $writeError) {
  $fixtureErrors += "vehicles-before-hideouts write order was accepted"
} elseif ($writeError -notmatch "write order") {
  $fixtureErrors += "write-order fixture failed for the wrong reason: $writeError"
}

$twiceError = Test-PersistenceContract $badTwice
if (-not $twiceError) {
  $fixtureErrors += "a payload listed twice was accepted"
} elseif ($twiceError -notmatch "more than once") {
  $fixtureErrors += "duplicate payload failed for the wrong reason: $twiceError"
}

$twoIntegersError = Test-PersistenceContract $badTwoIntegers
if (-not $twoIntegersError) {
  $fixtureErrors += "two version integers were accepted"
} elseif ($twoIntegersError -notmatch "one version integer") {
  $fixtureErrors += "two version integers failed for the wrong reason: $twoIntegersError"
}

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

Write-Host "validate-persistence-version: OK (version 4 matches the payload list; a skipped payload fails)"
exit 0
