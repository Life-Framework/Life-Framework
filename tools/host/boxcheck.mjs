// Host box check (#1496, the dry run of #311). Read only: measures a machine (cores, clock, RAM,
// free disk, the Arma Reforger Server install) and compares it with the 40-player box spec in
// docs/ops/rented-box.md step 2. The numbers below are quoted from that table; the selftest
// fails when the page and these constants drift apart.

import { existsSync } from "node:fs";
import { cpus, totalmem } from "node:os";
import { join } from "node:path";
import { exeBuild } from "./host.mjs";
import { freeBytesOf } from "./disk.mjs";

const GB = 1024 ** 3;

// docs/ops/rented-box.md step 2: CPU "4 or more cores, fast single-thread", RAM "16 GB",
// Disk "60 GB SSD". The page gives no clock figure, so the clock is reported and never judged.
export const SPEC = Object.freeze({ cores: 4, ramGb: 16, diskGb: 60 });

export const DEFAULT_SERVER_DIR = "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger Server";
export const SERVER_EXE = "ArmaReforgerServer.exe";

// A "physical memory" reading is a little under the nominal size (the firmware keeps some), so
// a 16 GB box reads 15.x GiB. Accept a reading within 5% of the spec.
const RAM_TOLERANCE = 0.95;

const fmtGb = (bytes) => `${(bytes / GB).toFixed(1)} GB`;

// facts: { cores, clockMhz, ramBytes, freeBytes, installDir, installFound, build } with null for
// what could not be read. Returns { items: [{ item, status: "pass"|"fail"|"info", text }], ok, failed }.
export function compare(facts, spec = SPEC) {
  const items = [];
  const add = (item, status, text) => items.push({ item, status, text });

  if (!Number.isFinite(facts.cores)) add("cores", "fail", `could not read the core count (want ${spec.cores} or more)`);
  else if (facts.cores >= spec.cores) add("cores", "pass", `${facts.cores} logical cores (want ${spec.cores} or more)`);
  else add("cores", "fail", `${facts.cores} logical cores, want ${spec.cores} or more`);

  if (Number.isFinite(facts.clockMhz) && facts.clockMhz > 0) add("clock", "info", `${(facts.clockMhz / 1000).toFixed(2)} GHz (the spec asks for fast single-thread and gives no figure; not judged)`);
  else add("clock", "info", "clock not readable (the spec gives no figure; not judged)");

  const wantRam = spec.ramGb * GB;
  if (!Number.isFinite(facts.ramBytes)) add("ram", "fail", `could not read the RAM (want ${spec.ramGb} GB)`);
  else if (facts.ramBytes >= wantRam * RAM_TOLERANCE) add("ram", "pass", `${fmtGb(facts.ramBytes)} (want ${spec.ramGb} GB)`);
  else add("ram", "fail", `${fmtGb(facts.ramBytes)}, want ${spec.ramGb} GB`);

  const wantDisk = spec.diskGb * GB;
  if (!Number.isFinite(facts.freeBytes)) add("disk", "fail", `could not read the free disk space (want ${spec.diskGb} GB)`);
  else if (facts.freeBytes >= wantDisk) add("disk", "pass", `${fmtGb(facts.freeBytes)} free (want ${spec.diskGb} GB)`);
  else add("disk", "fail", `${fmtGb(facts.freeBytes)} free, want ${spec.diskGb} GB`);

  const where = facts.installDir ? ` in ${facts.installDir}` : "";
  if (!facts.installFound) add("install", "fail", `${SERVER_EXE} not found${where} (Steam app 1874900)`);
  else if (!facts.build) add("install", "fail", `${SERVER_EXE} found${where} but its build could not be read`);
  else add("install", "pass", `${SERVER_EXE} build ${facts.build}${where}`);

  const failed = items.filter((i) => i.status === "fail").length;
  return { items, ok: failed === 0, failed };
}

// Facts for this machine. `serverDir` is the install folder to look in.
export function localFacts(serverDir = DEFAULT_SERVER_DIR) {
  const cpu = cpus();
  const exe = join(serverDir, SERVER_EXE);
  const installFound = existsSync(exe);
  let freeBytes = null;
  try {
    freeBytes = freeBytesOf(installFound ? serverDir : process.cwd());
  } catch {
    freeBytes = null;
  }
  return {
    cores: cpu.length || null,
    clockMhz: cpu.length ? Math.max(...cpu.map((c) => c.speed || 0)) : null,
    ramBytes: totalmem(),
    freeBytes,
    installDir: serverDir,
    installFound,
    build: installFound ? exeBuild(exe) : null,
  };
}

// The PowerShell a Windows box runs for its facts; one `lf-box: key=value` line each.
export function remoteScript(serverDir, psQuote) {
  const dir = psQuote(serverDir.replace(/\//g, "\\"));
  return [
    "$ErrorActionPreference = 'Continue'",
    "$cpu = @(Get-CimInstance Win32_Processor)",
    "Write-Output ('lf-box: cores=' + ($cpu | Measure-Object -Property NumberOfLogicalProcessors -Sum).Sum)",
    "Write-Output ('lf-box: clockMhz=' + ($cpu | Measure-Object -Property MaxClockSpeed -Maximum).Maximum)",
    "Write-Output ('lf-box: ramBytes=' + (Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory)",
    `$dir = ${dir}`,
    "$exe = Join-Path $dir 'ArmaReforgerServer.exe'",
    "$found = Test-Path -LiteralPath $exe",
    "Write-Output ('lf-box: installFound=' + $found)",
    "if ($found) { Write-Output ('lf-box: build=' + (Get-Item -LiteralPath $exe).VersionInfo.FileVersion) }",
    "$letter = $dir.Substring(0, 1)",
    "if (-not (Test-Path -LiteralPath ($letter + ':\\'))) { $letter = 'C' }",
    "Write-Output ('lf-box: freeBytes=' + (Get-PSDrive -Name $letter).Free)",
  ].join("\n");
}

// Parses the `lf-box:` lines of a remote run into facts (what is missing stays null).
export function parseRemote(text, serverDir = DEFAULT_SERVER_DIR) {
  const kv = {};
  for (const line of String(text ?? "").split(/\r?\n/)) {
    const m = /^lf-box: (\w+)=(.*)$/.exec(line.trim());
    if (m) kv[m[1]] = m[2].trim();
  }
  const num = (k) => (/^\d+(\.\d+)?$/.test(kv[k] ?? "") ? Number(kv[k]) : null);
  return {
    cores: num("cores"),
    clockMhz: num("clockMhz"),
    ramBytes: num("ramBytes"),
    freeBytes: num("freeBytes"),
    installDir: serverDir,
    installFound: /^true$/i.test(kv.installFound ?? ""),
    build: /^\d+(\.\d+){1,3}$/.test(kv.build ?? "") ? kv.build : null,
  };
}
