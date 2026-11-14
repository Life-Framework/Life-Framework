// tools\cli workshop boot-check <coreId> <packId> [--host lf-cloud] [--no-client] [--plan] [--budget <s>]  (#371)
//
// The proof a Workshop upload needs: a CLEAN dedicated server that gets both mods from the Workshop
// alone. A green build in the repo proves nothing about what a stranger downloads. On the pool box
// (lf-cloud) this makes a fresh run folder with an empty profile and an empty -addonsDir, writes a
// server.json that lists the two Workshop ids (copied from server/example/server.json, so the check
// proves the file a host copies), and boots ArmaReforgerServer.exe with -config and NO -addons: the
// only way a mod can load is by the server downloading it. It reads the console.log, then (unless
// --no-client) starts a game client from the box's signed-in desktop, with no local addons either,
// and checks the server log for the join. It prints one SUMMARY line and the stage that failed.
//
// Never uploads anything. Stages (the first failure is the verdict):
//   ids       both ids are 16 hex digits, not placeholders (a placeholder stops here, offline)
//   clean     the box's server install has no LifeFramework addon of its own to hide a missing item
//   mods      the log names no missing or failed mod ("mods not found" is the failure you get with ids
//             that do not exist on the Workshop, or an item that is still private)
//   scripts   no `SCRIPT (E)` / VME in the log
//   ready     the server reached "Entered online game state."
//   join      (unless --no-client) a client joined; the server printed a player line
//
// Placeholder ids run the boot anyway with `--force-boot`: that is how the tool is proven ready
// before real ids exist (it must stop at `mods`, with the server's own "not found" line quoted).
// The pure parts (buildServerJson, judgeBootLog, remoteScript) are proven by
// tools/validation/workshop-boot-check-selftest.mjs.

import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { psQuote, scp, sshPs } from "../remote/testbox.mjs";

export const DEFAULT_HOST = "lf-cloud";
export const BOOT_PORT = 2399;
export const A2S_PORT = 17799;
export const READY_LINE = "Entered online game state.";
const BOX_ROOT = "C:/LF/bootcheck";
const SERVER_FALLBACK = "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger Server";
const CLIENT_FALLBACK = "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger";

export const isHexId = (s) => /^[0-9A-Fa-f]{16}$/.test(String(s ?? ""));

// server.json for the clean boot: the shipped example with the two ids, a private name, and ports
// that no pool lane uses.
export function buildServerJson(example, coreId, packId) {
  const j = JSON.parse(JSON.stringify(example));
  j.bindPort = BOOT_PORT;
  j.publicPort = BOOT_PORT;
  j.a2s = { address: "0.0.0.0", port: A2S_PORT };
  j.game.name = "LF boot-check";
  j.game.visible = false;
  j.game.passwordAdmin = "bootcheck";
  j.game.mods = [
    { modId: coreId, name: "Life Framework" },
    { modId: packId, name: "Life Framework: Everon 1989" },
  ];
  return j;
}

// Argument list of the clean server: -config only; no -addons, and an addonsDir the run just made.
export function serverArgs(run) {
  return ["-config", `${run}/server.json`, "-profile", `${run}/profile`, "-addonsDir", `${run}/addons`, "-maxFPS", "30", "-logLevel", "normal", "-disableCrashReporter"];
}

// The client has no -addons and no -noBackend either: like a stranger's game it asks the backend for
// the server's mods on join.
export function clientArgs(run, port = BOOT_PORT) {
  return ["-client", `127.0.0.1:${port}`, "-addonsDir", `${run}/client-addons`, "-profile", `${run}/client-profile`, "-window", "-screenWidth", "640", "-screenHeight", "360", "-noSound", "-noSplash", "-noFocus", "-disableCrashReporter"];
}

const MOD_PROBLEM = /BACKENDs+(E)|(workshop|addon|mod)\b[^\n]*\b(not found|missing|failed|unable|could not|couldn't|cannot|can't|doesn't exist|does not exist|not available|download(?:ing)? (?:error|failed))|\b(not found|missing|failed|unable|could not|cannot)\b[^\n]*\b(workshop|addon|mod)\b/i;
const SCRIPT_ERROR = /SCRIPT\s+\(E\)|\bVME\b|Virtual Machine Exception/;
const JOIN_LINE = /\[LFDebug:CharacterCreation\] flow start for player \d+|\bPlayer\b[^\n]*\b(connected|joined)\b|### Updating player/i;

// Judges one boot's server log. wantIds are the ids the server.json listed. Returns
// { stage, ok, line, notes } where stage is the first failing stage ("pass" when all hold) and line
// is the log line that decided it.
export function judgeBootLog(text, { wantIds = [], client = true } = {}) {
  const lines = String(text ?? "").split(/\r?\n/);
  const first = (re) => lines.find((l) => re.test(l));
  const notes = [];
  const ids = wantIds.filter(isHexId);
  const modLine = lines.find((l) => MOD_PROBLEM.test(l) && !/LFDebug/.test(l)) ?? lines.find((l) => ids.some((id) => new RegExp(`${id}[^\\n]*(error|fail|not found|missing)`, "i").test(l)));
  if (modLine) return { stage: "mods", ok: false, line: modLine.trim(), notes };
  const bad = first(SCRIPT_ERROR);
  if (bad) return { stage: "scripts", ok: false, line: bad.trim(), notes };
  if (!first(new RegExp(READY_LINE.replace(".", "\\.")))) {
    return { stage: "ready", ok: false, line: lines.filter(Boolean).slice(-1)[0]?.trim() ?? "(empty log)", notes: [...notes, `no "${READY_LINE}" line`] };
  }
  if (client) {
    const join = first(JOIN_LINE);
    if (!join) return { stage: "join", ok: false, line: "", notes: [...notes, "no player line after the client started"] };
    return { stage: "pass", ok: true, line: join.trim(), notes };
  }
  return { stage: "pass", ok: true, line: READY_LINE, notes };
}

// The PowerShell that runs on the box. One short script: make the run folder, write server.json,
// preflight, start the server, poll console.log up to budgetS for the ready line or a mod problem,
// optionally start the client in the signed-in desktop (a one-shot scheduled task) and wait for a
// player line, stop what it started, and print the log between markers. Bounded: it never waits
// past budgetS.
export function remoteScript({ run, serverJsonB64, budgetS, client, clientJoinS = 150 }) {
  const mods = MOD_PROBLEM.source;
  return `
$ErrorActionPreference = 'Stop'
$run = ${psQuote(run)}
$local = 'C:/LF/Life-Framework/lf.local.json'
$srv = ${psQuote(SERVER_FALLBACK)}
if (Test-Path $local) { try { $c = Get-Content $local -Raw | ConvertFrom-Json; if ($c.ENFUSION_SERVER_PATH) { $srv = $c.ENFUSION_SERVER_PATH } } catch {} }
$cli = ${psQuote(CLIENT_FALLBACK)}
New-Item -ItemType Directory -Force "$run/profile","$run/addons","$run/client-profile","$run/client-addons" | Out-Null
[IO.File]::WriteAllBytes("$run/server.json", [Convert]::FromBase64String(${psQuote(serverJsonB64)}))
$own = @(Get-ChildItem "$srv/addons" -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -like 'LifeFramework*' -or $_.Name -like 'LF_*' })
if ($own.Count) { Write-Host ("BOOTCHECK-DIRTY " + (($own | ForEach-Object Name) -join ',')); exit 3 }
if (Get-NetUDPEndpoint -LocalPort ${BOOT_PORT} -ErrorAction SilentlyContinue) { Write-Host 'BOOTCHECK-PORT-BUSY'; exit 4 }
$exe = Join-Path $srv 'ArmaReforgerServer.exe'
$sa = ${psQuote(serverArgs("__RUN__").join(" "))}.Replace('__RUN__', $run)
$p = Start-Process -FilePath $exe -ArgumentList $sa -WorkingDirectory $srv -PassThru -WindowStyle Hidden -RedirectStandardOutput "$run/stdout.txt" -RedirectStandardError "$run/stderr.txt"
$t0 = Get-Date
$ready = $false; $problem = $false; $joined = $false; $clientStarted = $false
function LogText { $f = Get-ChildItem "$run/profile/logs" -Recurse -Filter console.log -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1; if ($f) { try { $fs = New-Object IO.FileStream($f.FullName, 'Open', 'Read', 'ReadWrite'); $sr = New-Object IO.StreamReader($fs); $t = $sr.ReadToEnd(); $sr.Close(); $t } catch { '' } } else { '' } }
$modRe = [regex]::new(${psQuote(mods)}, 'IgnoreCase')
while (((Get-Date) - $t0).TotalSeconds -lt ${budgetS}) {
  Start-Sleep -Seconds 2
  $log = LogText
  if ($p.HasExited) { break }
  foreach ($l in ($log -split "\`n")) { if ($l -notmatch 'LFDebug' -and $modRe.IsMatch($l)) { $problem = $true; break } }
  if ($problem) { Start-Sleep -Seconds 3; break }
  if (-not $ready -and $log -match 'Entered online game state\\.') { $ready = $true; if (-not ${client ? "$true" : "$false"}) { break } }
  if ($ready -and ${client ? "$true" : "$false"} -and -not $clientStarted) {
    $clientStarted = $true
    $ca = ${psQuote(clientArgs("__RUN__").join(" "))}.Replace('__RUN__', $run)
    $name = 'LF-BootCheck-' + [guid]::NewGuid().ToString('N').Substring(0,8)
    $act = New-ScheduledTaskAction -Execute (Join-Path $cli 'ArmaReforgerSteam.exe') -Argument $ca -WorkingDirectory $cli
    $pr = New-ScheduledTaskPrincipal -UserId (whoami) -LogonType Interactive -RunLevel Limited
    Register-ScheduledTask -TaskName $name -Action $act -Principal $pr | Out-Null
    Start-ScheduledTask -TaskName $name
    $tc = Get-Date
  }
  if ($clientStarted -and ($log -match 'flow start for player|### Updating player')) { $joined = $true; break }
  if ($clientStarted -and ((Get-Date) - $tc).TotalSeconds -gt ${clientJoinS}) { break }
}
Start-Sleep -Seconds 2
$final = LogText
if ($clientStarted) {
  Stop-ScheduledTask -TaskName $name -ErrorAction SilentlyContinue; Unregister-ScheduledTask -TaskName $name -Confirm:\$false -ErrorAction SilentlyContinue
  Get-Process ArmaReforgerSteam -ErrorAction SilentlyContinue | Where-Object { $_.StartTime -gt $t0 } | Stop-Process -Force -ErrorAction SilentlyContinue
}
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
Write-Host ("BOOTCHECK-RUN " + $run + " seconds=" + [int]((Get-Date) - $t0).TotalSeconds + " exited=" + $p.HasExited)
Write-Host '=====LOG-BEGIN====='
Write-Host $final
Write-Host '=====LOG-END====='
Write-Host '=====STDERR-BEGIN====='
Write-Host (Get-Content "$run/stderr.txt" -Tail 40 -ErrorAction SilentlyContinue | Out-String)
Write-Host '=====STDERR-END====='
exit 0
`;
}

export function parseRemoteOutput(out) {
  const log = /=====LOG-BEGIN=====\r?\n([\s\S]*?)=====LOG-END=====/.exec(out)?.[1] ?? "";
  const stderr = /=====STDERR-BEGIN=====\r?\n([\s\S]*?)=====STDERR-END=====/.exec(out)?.[1] ?? "";
  const dirty = /BOOTCHECK-DIRTY (.*)/.exec(out)?.[1]?.trim();
  const busy = /BOOTCHECK-PORT-BUSY/.test(out);
  const run = /BOOTCHECK-RUN (\S+)/.exec(out)?.[1] ?? "";
  return { log, stderr, dirty, busy, run };
}

function flagValue(args, name, fallback) {
  const i = args.indexOf(name);
  return i >= 0 && args[i + 1] ? args[i + 1] : fallback;
}

export function summary(res, { coreId, packId, host }) {
  const tail = res.line ? ` line=${JSON.stringify(res.line.slice(0, 200))}` : "";
  return `SUMMARY workshop-boot-check ${res.ok ? "ok" : "fail"} stage=${res.stage} host=${host} core=${coreId} pack=${packId}${tail}`;
}

export async function main(args, { root }) {
  const ids = args.filter((a) => !a.startsWith("--") && args[args.indexOf(a) - 1] !== "--host" && args[args.indexOf(a) - 1] !== "--budget");
  const [coreId, packId] = ids;
  if (!coreId || !packId || args.includes("--help")) {
    console.log("usage: tools\\cli workshop boot-check <coreId> <packId> [--host lf-cloud] [--no-client] [--plan] [--force-boot] [--budget <s>]");
    return coreId && packId ? 0 : 2;
  }
  const host = flagValue(args, "--host", DEFAULT_HOST);
  const budgetS = Number(flagValue(args, "--budget", "420"));
  const client = !args.includes("--no-client");
  const stageOneProblems = [];
  for (const [what, id] of [["core", coreId], ["pack", packId]]) {
    if (!isHexId(id)) stageOneProblems.push(`${what} id "${id}" is not 16 hex digits (a PLACEHOLDER until the first upload, #370)`);
  }
  if (stageOneProblems.length && !args.includes("--force-boot")) {
    const res = { ok: false, stage: "ids", line: stageOneProblems.join("; ") };
    console.log(summary(res, { coreId, packId, host }));
    console.log("hint: with `--force-boot` the server boots anyway, which proves the tool up to the server's own \"mod not found\" line.");
    return 1;
  }
  const examplePath = join(root, "server", "example", "server.json");
  if (!existsSync(examplePath)) {
    console.log("FAIL  server/example/server.json is missing");
    return 1;
  }
  const serverJson = buildServerJson(JSON.parse(readFileSync(examplePath, "utf8")), coreId, packId);
  const stamp = new Date().toISOString().replace(/[-:T]/g, "").slice(0, 14);
  const run = `${BOX_ROOT}/run-${stamp}`;
  const outDir = join(root, "tmp", "boot-check");
  mkdirSync(outDir, { recursive: true });
  const jsonPath = join(outDir, `server-${stamp}.json`);
  writeFileSync(jsonPath, JSON.stringify(serverJson, null, 2) + "\n");
  const script = remoteScript({ run, serverJsonB64: Buffer.from(JSON.stringify(serverJson)).toString("base64"), budgetS, client: client && !stageOneProblems.length });
  if (args.includes("--plan")) {
    console.log(`plan: ssh ${host}; run folder ${run}; server ${serverArgs(run).join(" ")}`);
    if (client) console.log(`plan: client ${clientArgs(run).join(" ")}`);
    console.log(`plan: server.json written to ${jsonPath}`);
    console.log("SUMMARY workshop-boot-check ok stage=plan (nothing was run)");
    return 0;
  }
  console.log(`boot-check: ${host}, clean run folder ${run}, budget ${budgetS}s, mods ${coreId} + ${packId}`);
  const r = sshPs(host, script, { timeout: (budgetS + 120) * 1000 });
  const out = (r.stdout ?? "") + (r.stderr ?? "");
  const p = parseRemoteOutput(out);
  const logPath = join(outDir, `console-${stamp}.log`);
  writeFileSync(logPath, p.log || out);
  if (p.dirty) {
    const res = { ok: false, stage: "clean", line: `the box's server install holds ${p.dirty}; remove it so a missing Workshop item cannot hide` };
    console.log(summary(res, { coreId, packId, host }));
    return 1;
  }
  if (p.busy) {
    console.log(summary({ ok: false, stage: "clean", line: `UDP ${BOOT_PORT} is in use on ${host}` }, { coreId, packId, host }));
    return 1;
  }
  if (!p.run) {
    console.log(`FAIL  ssh ${host} gave no boot-check result (exit ${r.status}): ${out.slice(0, 400)}`);
    console.log(`SUMMARY workshop-boot-check fail stage=ssh host=${host} log=${logPath}`);
    return 1;
  }
  const res = judgeBootLog(p.log, { wantIds: [coreId, packId], client: client && !stageOneProblems.length });
  if (stageOneProblems.length && res.ok) res.ok = false;
  console.log(`${summary(res, { coreId, packId, host })} log=${logPath}`);
  if (!res.ok && res.stage === "mods") console.log("the server could not get a mod. With placeholder ids that is expected; with real ids check the item is Public or Unlisted and its id is the one the Workshop page shows.");
  return res.ok ? 0 : 1;
}

export { scp };
