// tools/remote/pool.mjs - the test pool: every machine that can boot a test
// server, one slot per concurrent server, a fair queue across agents.
//
// Machines come from tools/pool.json, overridden per machine by the "pool" key
// of the gitignored lf.local.json (docs/setup.md "Machine-local settings").
// A slot is a lock: slot 1 is the machine's own server-2001 lock (the one
// serve/host/session already take), slot k > 1 is server-2001-s<k>. A machine
// that can run more servers at once (separate network namespaces, a VM per
// slot) raises "slots"; nothing else changes.
//
// acquireSlot() writes a ticket into <hub>/pool/queue, then loops: a ticket
// may try a machine only when no older live ticket is waiting for that machine
// (FIFO per machine), the local machine first, then every reachable remote.
// First free slot wins. Tickets and locks whose owner pid is gone are stolen.
//
// A remote slot is held by a "holder": one ssh session that creates the lock
// file on the remote machine (pid = the holder's PowerShell), prepares the
// remote run folder, then watches its own ancestor processes. When this
// process ends (normal exit, ctrl-c, crash) the local ssh dies, sshd drops the
// connection, the holder's ancestors vanish, and within ~2 s it stops every
// process still running out of the run folder and deletes the lock. A lock
// whose holder pid is dead is reclaimed by the next acquirer on that machine.

import { spawn, spawnSync } from "node:child_process";
import { randomBytes } from "node:crypto";
import { appendFileSync, existsSync, mkdirSync, readdirSync, readFileSync, renameSync, unlinkSync, writeFileSync } from "node:fs";
import { hostname } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import * as wt from "../wt.mjs";
import { localConfig, localSetting } from "../lib/local-config.mjs";
import * as tb from "./testbox.mjs";

const POOL_JSON = join(dirname(fileURLToPath(import.meta.url)), "..", "pool.json");
const HEALTH_TTL_MS = 60000;
const REMOTE_RETRY_MS = 4000;
export const SLOT_ENV = "LF_SERVER_SLOT_HELD";

export function slotLockName(i) {
  return i === 1 ? "server-2001" : `server-2001-s${i}`;
}

// Every ssh to a pool machine is bounded: ConnectTimeout 4 s plus these overall limits.
export const PROBE_TIMEOUT_MS = 12000; // probe, lock read
export const HOLDER_FIRST_BYTE_MS = 40000; // no output at all from the holder
export const HOLDER_TIMEOUT_MS = 180000; // holder never reports a slot, busy, or error

function sleep(ms) {
  return new Promise((r) => setTimeout(r, ms));
}

function pidAlive(pid) {
  if (!pid) return false;
  try {
    process.kill(pid, 0);
    return true;
  } catch (e) {
    return e.code === "EPERM";
  }
}

function readJson(file, fallback) {
  try {
    return JSON.parse(readFileSync(file, "utf8").replace(/^\uFEFF/, ""));
  } catch {
    return fallback;
  }
}

// ---------------------------------------------------------------- reclaim rules

// A remote lock this PC's run took (origin = this host) whose local owner
// process is gone: the holder outlived it (Windows sshd can keep the session
// and the holder alive after the local ssh dies). Returns a reason or null.
export function remoteOwnerGone(info, { self = hostname(), alive = pidAlive } = {}) {
  if (!info || !info.origin || !info.originPid) return null;
  if (String(info.origin).toLowerCase() !== String(self).toLowerCase()) return null;
  if (alive(Number(info.originPid))) return null;
  return `owner pid ${info.originPid} on ${info.origin} is gone`;
}

// How long a pool run may hold a slot with no server process before the hang
// guard takes it: max(5 min, 10x the median run of that purpose).
const HANG_MIN_MS = 300000;
export function hangLimitMs(runs, purpose) {
  const d = runs
    .filter((r) => r.purpose === purpose && (r.result === "pass" || r.result === "fail"))
    .slice(-30)
    .map((r) => Number(r.durS) * 1000)
    .filter((x) => x > 0)
    .sort((a, b) => a - b);
  const median = d.length ? d[Math.floor(d.length / 2)] : 0;
  return Math.max(HANG_MIN_MS, 10 * median);
}

// A pool-taken lock (it names a purpose; serve/host/session locks do not)
// held past the limit while no server runs for it. Returns a reason or null.
export function hangReason(info, { now = Date.now(), limitMs, serverAlive }) {
  if (!info || !info.purpose || serverAlive !== false) return null;
  const age = now - Date.parse(info.at);
  if (!Number.isFinite(age) || age <= limitMs) return null;
  return `held ${Math.round(age / 60000)} min (limit ${Math.round(limitMs / 60000)} min) with no server process alive`;
}

function noticeFile(root, pid, lockName) {
  const d = join(poolDir(root), "reclaimed");
  mkdirSync(d, { recursive: true });
  return join(d, `${pid}-${lockName}.json`);
}

// Logs a reclaim in runs.jsonl (pool stats counts them) and leaves a notice
// the owner prints when it releases or loses the slot.
function noteReclaim(root, m, info, kind, reason) {
  const rec = {
    at: new Date().toISOString(),
    slug: info.slug ?? "?",
    purpose: "reclaim",
    victimPurpose: info.purpose ?? null,
    machine: m.name,
    slot: info.name ?? null,
    kind,
    reason,
    result: "reclaimed",
  };
  try {
    appendFileSync(runsFile(root), JSON.stringify(rec) + "\n");
  } catch {}
  const owner = m.via === "local" ? info.pid : String(info.origin ?? "").toLowerCase() === hostname().toLowerCase() ? info.originPid : null;
  if (owner) {
    try {
      writeFileSync(noticeFile(root, owner, info.name), JSON.stringify(rec));
    } catch {}
  }
  console.log(`pool: reclaimed ${m.name} ${info.name} from ${info.slug ?? "?"} ${info.purpose ?? ""}: ${reason}`);
}

// The owner's side: print (once) why the pool took this process's slot.
export function takeNotice(root, lockName, pid = process.pid) {
  const f = noticeFile(root, pid, lockName);
  const n = readJson(f, null);
  if (!n) return null;
  try {
    unlinkSync(f);
  } catch {}
  console.log(`WARN  pool: the scheduler reclaimed this run's slot ${n.machine} ${lockName}: ${n.reason}`);
  return n;
}

export function poolDir(root) {
  const d = join(wt.hubDirOf(root), "pool");
  mkdirSync(join(d, "queue"), { recursive: true });
  return d;
}

// ---------------------------------------------------------------- registry

function normalize(m) {
  const via = m.via === "ssh" ? "ssh" : "local";
  return {
    name: String(m.name),
    via,
    host: via === "ssh" ? String(m.host || m.name) : null,
    root: via === "ssh" ? String(m.root || "C:/LF/Life-Framework").replace(/\\/g, "/").replace(/\/$/, "") : null,
    slots: Math.max(1, Number(m.slots) || 1),
    caps: Array.isArray(m.caps) ? m.caps.map(String) : ["server"],
    disabled: !!m.disabled,
  };
}

// Committed registry, then lf.local.json "pool": { "machines": [ { "name", ... } ] }
// entries merge by name ({"name": "lf-testbox", "disabled": true} turns one off).
// LF_TESTBOX / LF_TESTBOX_ROOT keep working: they add or retarget that host.
export function loadMachines(root) {
  const base = readJson(POOL_JSON, { machines: [{ name: "local", via: "local", slots: 1 }] });
  const byName = new Map();
  for (const m of base.machines ?? []) byName.set(m.name, { ...m });
  const over = localConfig(root).pool;
  for (const m of over?.machines ?? []) {
    if (!m?.name) continue;
    byName.set(m.name, { ...(byName.get(m.name) ?? {}), ...m });
  }
  const box = localSetting(root, "LF_TESTBOX");
  if (box && ![...byName.values()].some((m) => m.via === "ssh" && (m.host || m.name) === box)) {
    byName.set(box, { name: box, via: "ssh", host: box, slots: 1, caps: ["server"] });
  }
  const boxRoot = localSetting(root, "LF_TESTBOX_ROOT");
  const out = [...byName.values()].map(normalize).filter((m) => !m.disabled);
  if (boxRoot) for (const m of out) if (m.via === "ssh" && m.host === (box || m.host)) m.root = boxRoot.replace(/\\/g, "/").replace(/\/$/, "");
  // On a machine that is itself a pool member (the box), its own ssh entry is
  // this machine: drop it so a run there never ssh-es to itself.
  const self = hostname().toLowerCase();
  return out.filter((m) => !(m.via === "ssh" && m.host.toLowerCase() === self));
}

export function findMachine(machines, name) {
  const n = String(name).toLowerCase();
  return machines.find((m) => m.name.toLowerCase() === n || (m.host && m.host.toLowerCase() === n)) ?? null;
}

// ---------------------------------------------------------------- health

function healthFile(root) {
  return join(poolDir(root), "health.json");
}

// { reachable, build, at, ms }. Remote answers are cached for a minute in the
// hub, so a queue of waiting agents does not ssh a dead box every poll.
export function probe(root, m, { fresh = false } = {}) {
  if (m.via === "local") return { reachable: true, build: tb.serverBuildId(tb.localServerDir(root)), at: Date.now(), ms: 0 };
  const file = healthFile(root);
  const cache = readJson(file, {});
  const hit = cache[m.name];
  if (!fresh && hit && Date.now() - hit.at < HEALTH_TTL_MS) return hit;
  const t0 = Date.now();
  const res = tb.sshPs(m.host, [`$main = ${tb.psQuote(m.root)}`, tb.PS_BUILDID, "Write-Host 'lf-pool: ok'"].join("\n"), { timeout: PROBE_TIMEOUT_MS, connectTimeout: 4 });
  const out = res.stdout ?? "";
  const h = { reachable: /lf-pool: ok/.test(out), build: /lf-remote: buildid (\d+)/.exec(out)?.[1] ?? null, at: Date.now(), ms: Date.now() - t0 };
  const now = readJson(file, {});
  now[m.name] = h;
  try {
    writeFileSync(file, JSON.stringify(now, null, 2));
  } catch {}
  return h;
}

function markHealth(root, m, h) {
  const hf = healthFile(root);
  const hc = readJson(hf, {});
  const prev = hc[m.name] ?? {};
  hc[m.name] = { ...prev, ...h, build: h.build ?? (h.reachable ? prev.build : null) ?? null, at: Date.now() };
  try {
    writeFileSync(hf, JSON.stringify(hc, null, 2));
  } catch {}
}

// ---------------------------------------------------------------- queue

function queueDir(root) {
  return join(poolDir(root), "queue");
}

export function liveTickets(root) {
  const dir = queueDir(root);
  const out = [];
  for (const f of readdirSync(dir).filter((x) => x.endsWith(".json")).sort()) {
    const t = readJson(join(dir, f), null);
    if (!t || !pidAlive(t.pid)) {
      try {
        unlinkSync(join(dir, f));
      } catch {}
      continue;
    }
    out.push({ ...t, file: f });
  }
  return out;
}

function writeTicket(root, t) {
  const f = `${String(Date.now()).padStart(15, "0")}-${String(process.pid).padStart(7, "0")}-${randomBytes(3).toString("hex")}.json`;
  writeFileSync(join(queueDir(root), f), JSON.stringify(t));
  return f;
}

function dropTicket(root, f) {
  try {
    unlinkSync(join(queueDir(root), f));
  } catch {}
}

// ---------------------------------------------------------------- local slots

function localLockFile(root, name) {
  return join(wt.hubDirOf(root), "locks", `${name}.lock`);
}

// Servers a dead lock owner left behind still hold UDP 2001; stop the ones
// running out of that owner's worktree before the slot is handed on.
function stopLocalOrphans(ownerRoot) {
  if (!ownerRoot) return;
  const ps = `$a = ${tb.psQuote(join(ownerRoot, "server", "profile").replace(/\//g, "\\"))}; Get-CimInstance Win32_Process -Filter "Name LIKE 'ArmaReforgerServer%'" | Where-Object { $_.CommandLine -and $_.CommandLine.IndexOf($a, [StringComparison]::OrdinalIgnoreCase) -ge 0 } | ForEach-Object { Write-Output \"pool: stopping orphan server pid $($_.ProcessId)\"; taskkill /T /F /PID $_.ProcessId | Out-Null }`;
  const r = spawnSync("powershell", ["-NoProfile", "-NonInteractive", "-Command", ps], { windowsHide: true, encoding: "utf8", timeout: 30000 });
  if (r.stdout?.trim()) console.log(r.stdout.trim());
}

// Whether an ArmaReforgerServer runs out of that worktree's server/profile.
function localServerAlive(ownerRoot) {
  if (!ownerRoot) return false;
  const ps = `$a = ${tb.psQuote(join(ownerRoot, "server", "profile").replace(/\//g, "\\"))}; $n = @(Get-CimInstance Win32_Process -Filter "Name LIKE 'ArmaReforgerServer%'" | Where-Object { $_.CommandLine -and $_.CommandLine.IndexOf($a, [StringComparison]::OrdinalIgnoreCase) -ge 0 }).Count; Write-Output "lf-pool: servers $n"`;
  const r = spawnSync("powershell", ["-NoProfile", "-NonInteractive", "-Command", ps], { windowsHide: true, encoding: "utf8", timeout: 30000 });
  const mm = /lf-pool: servers (\d+)/.exec(r.stdout ?? "");
  // No answer counts as alive: the guard never takes a slot on a guess.
  return mm ? Number(mm[1]) > 0 : true;
}

// opts.serverAlive / opts.runs are test seams.
export function tryLocal(root, m, i, meta, opts = {}) {
  const name = slotLockName(i);
  const file = localLockFile(root, name);
  if (existsSync(file)) {
    const info = readJson(file, null);
    if (!info) return null;
    if (pidAlive(info.pid)) {
      // Hang guard: the cheap age check first, the process scan only past it.
      const limitMs = hangLimitMs(opts.runs ?? readRuns(root), info.purpose);
      if (!info.purpose || !(Date.now() - Date.parse(info.at) > limitMs)) return null;
      const serverAlive = (opts.serverAlive ?? localServerAlive)(info.root);
      const reason = hangReason(info, { limitMs, serverAlive });
      if (!reason) return null;
      noteReclaim(root, m, { ...info, name }, "hang", reason);
      try {
        unlinkSync(file);
      } catch {}
    } else stopLocalOrphans(info.root);
  }
  try {
    wt.acquireLock(root, name, { waitMs: 0, meta });
  } catch {
    return null;
  }
  return {
    machine: m,
    index: i,
    lockName: name,
    local: true,
    release: () => {
      takeNotice(root, name);
      wt.releaseLock(root, name);
    },
  };
}

// Inside a remote run the holder already owns the box's slot: the env names
// it and its token; the lock file must still carry that token.
export function heldSlotFromEnv(root) {
  const v = process.env[SLOT_ENV];
  if (!v) return null;
  const [name, token] = v.split(":");
  const info = readJson(localLockFile(root, name), null);
  if (!info || info.token !== token || !pidAlive(info.pid)) {
    throw new Error(`${SLOT_ENV}=${name}: the pool holder no longer owns that lock (released or reclaimed)`);
  }
  return { machine: { name: "local", via: "local" }, index: 1, lockName: name, local: true, inherited: true, release: () => {} };
}

// ---------------------------------------------------------------- remote slots

export function remoteRunDir(m, slug, i = 1, suffix = "") {
  return `${dirname(m.root).replace(/\\/g, "/")}/Life-Framework-rt-${slug}${suffix}${i > 1 ? `-s${i}` : ""}`;
}

// The holder: acquire (when cfg.slots is non-empty), prepare the run folder,
// report what the folder already has, then hold until its ssh session ends.
// Stops processes whose command line names a run folder (never deletes files,
// so a junction under it is never followed).
const PS_STOP_UNDER = String.raw`
function Stop-Under($dir) {
  if (-not $dir) { return }
  $a = $dir.Replace('/', '\').TrimEnd('\') + '\'
  $b = $a.Replace('\', '/')
  Get-CimInstance Win32_Process | Where-Object { $_.ProcessId -ne $PID -and $_.CommandLine -and ($_.CommandLine.IndexOf($a, [StringComparison]::OrdinalIgnoreCase) -ge 0 -or $_.CommandLine.IndexOf($b, [StringComparison]::OrdinalIgnoreCase) -ge 0) } | ForEach-Object { Write-Host "lf-pool: stopping leftover $($_.Name) pid $($_.ProcessId)"; & taskkill /T /F /PID $_.ProcessId 2>&1 | Out-Null }
}
function Test-Server($dir) {
  if (-not $dir) { return $false }
  $a = $dir.Replace('/', '\').TrimEnd('\') + '\'
  $b = $a.Replace('\', '/')
  return [bool](@(Get-CimInstance Win32_Process -Filter "Name LIKE 'ArmaReforgerServer%'" | Where-Object { $_.CommandLine -and ($_.CommandLine.IndexOf($a, [StringComparison]::OrdinalIgnoreCase) -ge 0 -or $_.CommandLine.IndexOf($b, [StringComparison]::OrdinalIgnoreCase) -ge 0) }).Count)
}
`;

const PS_HOLDER = String.raw`
$cfg = $cfgJson | ConvertFrom-Json
$main = $cfg.main
$ld = Join-Path $main 'tmp\wt\locks'
New-Item -ItemType Directory -Force -Path $ld | Out-Null
function Test-Alive($p) { if (-not $p) { return $false }; return [bool](Get-Process -Id $p -ErrorAction SilentlyContinue) }
${PS_STOP_UNDER}
$enc = New-Object System.Text.UTF8Encoding($false)
$got = -1
$slots = @($cfg.slots)
for ($i = 0; $i -lt $slots.Count -and $got -lt 0; $i++) {
  $s = $slots[$i]
  $f = Join-Path $ld ($s.lock + '.lock')
  for ($try = 0; $try -lt 3; $try++) {
    $fs = $null
    try { $fs = [IO.File]::Open($f, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write) } catch { $fs = $null }
    if ($fs) {
      $info = [ordered]@{ pid = $PID; at = (Get-Date).ToUniversalTime().ToString('o'); name = $s.lock; token = $cfg.token; rDir = $s.dir; slug = $cfg.slug; purpose = $cfg.purpose; origin = $cfg.origin; originPid = $cfg.originPid; root = $s.dir }
      $bytes = $enc.GetBytes(($info | ConvertTo-Json -Compress))
      $fs.Write($bytes, 0, $bytes.Length); $fs.Close()
      $got = $i; break
    }
    $raw = $null; try { $raw = [IO.File]::ReadAllText($f) } catch {}
    $held = $null; if ($raw) { try { $held = $raw | ConvertFrom-Json } catch {} }
    if ($held -and (Test-Alive $held.pid)) { Write-Host ('lf-pool: holder ' + $raw.Trim()); Write-Host ('lf-pool: holder-srv ' + (Test-Server $held.rDir)); break }
    if (-not $held) { $age = 0; try { $age = ((Get-Date) - (Get-Item -LiteralPath $f -ErrorAction Stop).LastWriteTime).TotalSeconds } catch { continue }; if ($age -lt 60) { Write-Host 'lf-pool: holder {}'; break } }
    if ($held) { Write-Host "lf-pool: reclaiming stale $($s.lock) (pid $($held.pid) gone)"; Stop-Under $held.rDir }
    Remove-Item -LiteralPath $f -Force -ErrorAction SilentlyContinue
  }
}
if ($slots.Count -gt 0 -and $got -lt 0) { Write-Host 'lf-pool: busy'; exit 0 }
$dir = $cfg.dir
$lockFile = $null
if ($got -ge 0) { $dir = $slots[$got].dir; $lockFile = Join-Path $ld ($slots[$got].lock + '.lock'); Write-Host "lf-pool: acquired $($slots[$got].lock) $($got + 1)" }
try {
  if (-not (Test-Path -LiteralPath (Join-Path $main '.git'))) { Write-Host "lf-pool: error no clone at $main (tools\cli testbox provision)"; return }
  git -C $main cat-file -e "$($cfg.base)^{commit}" 2>$null
  if ($LASTEXITCODE) { git -C $main fetch -q origin 2>&1 | Out-Null; Write-Host 'lf-pool: fetched origin' }
  if (-not (Test-Path -LiteralPath (Join-Path $dir '.git'))) {
    git -C $main worktree add -q --detach $dir $cfg.base 2>&1 | Out-Null
    if ($LASTEXITCODE) { Write-Host "lf-pool: error worktree add $dir failed"; return }
    Write-Host 'lf-pool: new run folder'
  }
  New-Item -ItemType Directory -Force -Path (Join-Path $dir 'tmp\remote-in') | Out-Null
  foreach ($c in @($cfg.candidates)) { if ($c) { git -C $dir cat-file -e "$c^{commit}" 2>$null; if (-not $LASTEXITCODE) { Write-Host "lf-pool: has $c" } } }
  $rdb = Join-Path $dir 'addons\LifeFramework\resourceDatabase.rdb'
  if (Test-Path -LiteralPath $rdb) { Write-Host "lf-pool: rdb $((Get-FileHash -Algorithm SHA1 -LiteralPath $rdb).Hash.ToLower())" }
  $marker = [ordered]@{ origin = $cfg.origin; slug = $cfg.slug; worktree = $cfg.worktree; at = (Get-Date).ToUniversalTime().ToString('o') }
  [IO.File]::WriteAllText((Join-Path $dir 'tmp\pool-owner.json'), ($marker | ConvertTo-Json -Compress), $enc)
  Write-Host 'lf-pool: ready'
  if ($got -lt 0) { return }
  $anc = @(); $p = $PID
  for ($k = 0; $k -lt 6; $k++) {
    $x = Get-CimInstance Win32_Process -Filter "ProcessId=$p"
    if (-not $x) { break }
    $pp = [int]$x.ParentProcessId
    $px = Get-Process -Id $pp -ErrorAction SilentlyContinue
    if (-not $px) { break }
    $anc += [pscustomobject]@{ Id = $pp; Start = $px.StartTime }
    if ($px.ProcessName -like 'sshd*') { break }
    $p = $pp
  }
  while ($true) {
    Start-Sleep -Seconds 2
    $gone = @($anc | Where-Object { $q = Get-Process -Id $_.Id -ErrorAction SilentlyContinue; (-not $q) -or ($q.StartTime -ne $_.Start) })
    if ($gone.Count -gt 0) { break }
    try { [Console]::Out.WriteLine('lf-pool: hb'); [Console]::Out.Flush() } catch { break }
  }
} finally {
  if ($lockFile) {
    Stop-Under $dir
    try { $h = [IO.File]::ReadAllText($lockFile) | ConvertFrom-Json; if ($h.token -eq $cfg.token) { Remove-Item -LiteralPath $lockFile -Force } } catch {}
  }
}
`;

export function holderScript(cfg) {
  return `$cfgJson = @'\n${JSON.stringify(cfg)}\n'@\n$main = ${tb.psQuote(cfg.main)}\n${tb.PS_BUILDID}\n${PS_HOLDER}`;
}

const liveHolders = new Set();
let exitHooked = false;
function hookExit() {
  if (exitHooked) return;
  exitHooked = true;
  // Killing the local ssh ends the remote session; the holder notices and frees the slot.
  process.on("exit", () => {
    for (const c of liveHolders) {
      try {
        c.kill();
      } catch {}
    }
  });
  for (const sig of ["SIGINT", "SIGTERM", "SIGBREAK", "SIGHUP"]) {
    process.on(sig, () => {
      console.log(`\npool: ${sig}; releasing slots`);
      process.exit(130);
    });
  }
}

// Starts a holder. Resolves { slot } once it holds a slot and the run folder
// is ready, { busy: [holder json...] } when every slot there is taken, or
// { error } when the machine could not be used.
export function startHolder(root, m, cfg, { onLost } = {}) {
  hookExit();
  return new Promise((resolvePromise) => {
    const child = spawn("ssh", tb.sshPsArgs(m.host, holderScript(cfg), { connectTimeout: 4 }), { windowsHide: true, stdio: ["pipe", "pipe", "pipe"] });
    liveHolders.add(child);
    const prep = { has: new Set(), rdb: null, fetched: false, fresh: false };
    const busy = [];
    let acquired = null;
    let settled = false;
    let buf = "";
    let errText = "";
    const done = (v) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      clearTimeout(silent);
      resolvePromise(v);
    };
    const timer = setTimeout(() => {
      child.kill();
      done({ error: `${m.name}: no answer from the holder within ${HOLDER_TIMEOUT_MS / 1000} s` });
    }, HOLDER_TIMEOUT_MS);
    // A box that takes the connection and then says nothing is as good as down.
    const silent = setTimeout(() => {
      child.kill();
      done({ error: `${m.name}: ssh connected but nothing came back within ${HOLDER_FIRST_BYTE_MS / 1000} s` });
    }, HOLDER_FIRST_BYTE_MS);
    const onLine = (line) => {
      clearTimeout(silent);
      const t = line.trim();
      let mm;
      if ((mm = /^lf-pool: acquired (\S+) (\d+)/.exec(t))) acquired = { lockName: mm[1], index: Number(mm[2]) };
      else if ((mm = /^lf-pool: has ([0-9a-f]{40})/.exec(t))) prep.has.add(mm[1]);
      else if ((mm = /^lf-pool: rdb ([0-9a-f]{40})/.exec(t))) prep.rdb = mm[1];
      else if (t === "lf-pool: fetched origin") prep.fetched = true;
      else if ((mm = /^lf-remote: buildid (d+)/.exec(t))) prep.build = mm[1];
      else if (t === "lf-pool: new run folder") prep.fresh = true;
      else if ((mm = /^lf-pool: holder-srv (True|False)$/.exec(t))) {
        if (busy.length) busy[busy.length - 1].serverAlive = mm[1] === "True";
      } else if ((mm = /^lf-pool: holder (.*)$/.exec(t))) busy.push(readJsonText(mm[1]));
      else if (t === "lf-pool: busy") done({ busy });
      else if ((mm = /^lf-pool: error (.*)$/.exec(t))) done({ error: `${m.name}: ${mm[1]}` });
      else if (/^lf-pool: (reclaiming|stopping)/.test(t)) console.log(`pool: ${m.name}: ${t.slice(9)}`);
      else if (t === "lf-pool: ready") {
        const dir = acquired ? cfg.slots[acquired.index - 1].dir : cfg.dir;
        const slot = {
          machine: m,
          index: acquired?.index ?? 0,
          lockName: acquired?.lockName ?? null,
          local: false,
          token: cfg.token,
          rDir: dir,
          prep,
          child,
          lost: false,
          release: () => {
            liveHolders.delete(child);
            try {
              child.kill();
            } catch {}
          },
        };
        done({ slot });
        if (!acquired) slot.release();
        else child.on("close", () => {
          if (liveHolders.has(child)) {
            slot.lost = true;
            takeNotice(root, acquired.lockName);
            onLost?.(slot);
          }
        });
      } else if (t && t !== "lf-pool: hb" && !settled) errText += `${t}\n`;
    };
    const feed = (chunk) => {
      buf += chunk.toString("utf8");
      let nl;
      while ((nl = buf.indexOf("\n")) >= 0) {
        onLine(buf.slice(0, nl).replace(/\r$/, ""));
        buf = buf.slice(nl + 1);
      }
    };
    child.stdout.on("data", feed);
    child.stderr.on("data", feed);
    child.on("error", (e) => done({ error: `${m.name}: ssh ${e.message}` }));
    child.on("close", (code) => {
      liveHolders.delete(child);
      if (buf) onLine(buf);
      done({ error: `${m.name}: holder exited (${code}) ${errText.trim().split(/\r?\n/).slice(-3).join(" | ")}` });
    });
  });
}

// Takes a remote slot back from a holder the rules above condemned: only when
// the lock still names that holder (pid + token) and the pid is still that
// PowerShell (not a reused pid), kill the holder's tree by pid, stop what runs
// out of its run folder, delete the lock. Returns true when the lock is gone.
export function reclaimRemote(root, m, info, kind, reason) {
  if (!info?.name || !info.pid || !info.token) return false;
  const ps = [
    PS_STOP_UNDER,
    `$f = Join-Path ${tb.psQuote(m.root)} ${tb.psQuote(`tmp\\wt\\locks\\${info.name}.lock`)}`,
    "$h = $null; try { $h = [IO.File]::ReadAllText($f) | ConvertFrom-Json } catch {}",
    `if (-not $h -or $h.token -ne ${tb.psQuote(info.token)} -or [int]$h.pid -ne ${Number(info.pid)}) { Write-Host 'lf-pool: reclaim skipped (lock changed)'; exit 0 }`,
    `$p = Get-CimInstance Win32_Process -Filter "ProcessId=${Number(info.pid)}"`,
    "if ($p -and $p.Name -like 'powershell*') { Write-Host \"lf-pool: stopping holder pid $($p.ProcessId)\"; & taskkill /T /F /PID $p.ProcessId 2>&1 | Out-Null }",
    "Stop-Under $h.rDir",
    "Remove-Item -LiteralPath $f -Force -ErrorAction SilentlyContinue",
    "if (-not (Test-Path -LiteralPath $f)) { Write-Host 'lf-pool: reclaimed' }",
  ].join("\n");
  const r = tb.sshPs(m.host, ps, { timeout: 60000, connectTimeout: 4 });
  const out = r.stdout ?? "";
  for (const l of out.split(/\r?\n/)) if (/^lf-pool: (stopping|reclaim skipped)/.test(l.trim())) console.log(`pool: ${m.name}: ${l.trim().slice(9)}`);
  if (!/lf-pool: reclaimed/.test(out)) return false;
  noteReclaim(root, m, info, kind, reason);
  return true;
}

// The verdict on one busy remote holder: { kind, reason } to reclaim, or null.
export function remoteBusyVerdict(info, runs, { self = hostname(), alive = pidAlive, now = Date.now() } = {}) {
  const gone = remoteOwnerGone(info, { self, alive });
  if (gone) return { kind: "dead-owner", reason: gone };
  const hang = hangReason(info, { now, limitMs: hangLimitMs(runs, info?.purpose), serverAlive: info?.serverAlive });
  if (hang) return { kind: "hang", reason: hang };
  return null;
}

function readJsonText(s) {
  try {
    return JSON.parse(s);
  } catch {
    return {};
  }
}

// Commits the box may already have, cheapest delta first: the snapshot itself,
// what this worktree last sent there, and origin/main.
function syncStateFile(root) {
  return join(root, "tmp", "remote", "pool-sync.json");
}

export function readSyncState(root) {
  return readJson(syncStateFile(root), {});
}

export function writeSyncState(root, machine, rec) {
  const s = readSyncState(root);
  s[machine] = { ...(s[machine] ?? {}), ...rec, at: new Date().toISOString() };
  mkdirSync(dirname(syncStateFile(root)), { recursive: true });
  writeFileSync(syncStateFile(root), JSON.stringify(s, null, 2));
}

function originMain(root) {
  const r = spawnSync("git", ["-C", root, "rev-parse", "-q", "--verify", "origin/main"], { windowsHide: true, encoding: "utf8" });
  return r.status === 0 ? r.stdout.trim() : null;
}

export function slugFor(root) {
  return wt.slugOfRoot(root) || root.replace(/[\\/]+$/, "").split(/[\\/]/).pop().replace(/^Life-Framework-ws-/, "").toLowerCase().replace(/[^a-z0-9-]/g, "-");
}

export function holderConfig(root, m, snap, { slots = m.slots, purpose = "test", suffix = "" } = {}) {
  const slug = slugFor(root);
  const base = originMain(root) ?? snap.head;
  const last = readSyncState(root)[m.name]?.sha;
  const slotList = [];
  for (let i = 1; i <= slots; i++) slotList.push({ lock: slotLockName(i), dir: remoteRunDir(m, slug, i, suffix) });
  return {
    main: m.root,
    token: randomBytes(8).toString("hex"),
    slug,
    purpose,
    origin: hostname(),
    originPid: process.pid,
    worktree: root.replace(/\\/g, "/"),
    base,
    candidates: [...new Set([snap.sha, snap.head, last, base].filter(Boolean))],
    slots: slotList,
    dir: remoteRunDir(m, slug, 1, suffix),
  };
}

// Whether a snapshot's own CLI honours LF_SERVER_SLOT_HELD (older branches do
// not; their remote run would queue on the lock the holder owns).
function snapshotSupportsPool(root, sha) {
  const r = spawnSync("git", ["-C", root, "show", `${sha}:tools/cli.mjs`], { windowsHide: true, encoding: "utf8", maxBuffer: 64 * 1024 * 1024 });
  return r.status === 0 && r.stdout.includes(SLOT_ENV);
}

// ---------------------------------------------------------------- acquire

// The machines a run may use under the current config (re-read every call).
export function resolveMachines(root, opts = {}) {
  const mode = opts.mode ?? "any";
  const all = loadMachines(root);
  let machines;
  if (opts.machine) {
    const m = findMachine(all, opts.machine);
    if (!m) throw new Error(`pool: no machine '${opts.machine}' (known: ${all.map((x) => x.name).join(", ")}); see tools/pool.json`);
    machines = [m];
  } else if (mode === "local") machines = all.filter((m) => m.via === "local");
  else if (mode === "remote") machines = all.filter((m) => m.via === "ssh");
  else machines = all;
  return machines.filter((m) => m.caps.includes("server"));
}

// Temp file + rename: a reader never sees a half-written ticket (and deletes it).
function rewriteTicket(root, f, t) {
  const file = join(queueDir(root), f);
  try {
    writeFileSync(`${file}.tmp`, JSON.stringify(t));
    renameSync(`${file}.tmp`, file);
  } catch {}
}

// opts: mode "any"|"local"|"remote", machine (name or ssh host), purpose,
// waitMs. Returns a slot with .release(), .machine, .waitedMs, and for a
// remote slot .rDir, .prep, .snap.
export async function acquireSlot(root, opts = {}) {
  const purpose = opts.purpose ?? "test";
  const mode = opts.mode ?? "any";
  const started = Date.now();
  if (process.env[SLOT_ENV]) {
    const s = heldSlotFromEnv(root);
    s.waitedMs = 0;
    return s;
  }
  let machines = resolveMachines(root, opts);
  if (machines.length === 0) throw new Error(`pool: no machine for --${mode}`);

  // Remote runs ship a snapshot; take it once, when a remote first comes into
  // play (what the worktree is when the run was asked for, or when a remote
  // was enabled while it waited).
  let snap = null;
  let snapTried = false;
  const withSnap = (list) => {
    if (!snapTried && list.some((m) => m.via === "ssh")) {
      snapTried = true;
      try {
        snap = tb.snapshotCommit(root);
        if (!snapshotSupportsPool(root, snap.sha)) {
          console.log(`pool: this snapshot's tools/cli.mjs predates the pool (no ${SLOT_ENV}); merge origin/main to run remotely. Local only.`);
          snap = null;
        }
      } catch (e) {
        console.log(`pool: snapshot failed (${e.message}); local only`);
      }
    }
    return snap ? list : list.filter((m) => m.via === "local");
  };
  machines = withSnap(machines);
  if (machines.length === 0) throw new Error("pool: no machine can take this run");

  const hereBuild = tb.serverBuildId(tb.localServerDir(root));
  const skipNote = new Set();
  const note = (key, msg) => {
    if (skipNote.has(key)) return;
    skipNote.add(key);
    console.log(msg);
  };
  const downNow = new Set();
  const usable = (m) => {
    if (m.via === "local") return true;
    if (downNow.has(m.name)) return false;
    // Only a cached answer here: the holder attempt itself is the probe (it
    // reports the build), so a healthy box costs no extra ssh round trip.
    const h = readJson(healthFile(root), {})[m.name];
    if (!h || Date.now() - h.at > HEALTH_TTL_MS) return true;
    if (!h.reachable) {
      note(`down-${m.name}`, `pool: ${m.name} does not answer ssh (cached ${Math.round((Date.now() - h.at) / 1000)} s); skipping it`);
      return false;
    }
    if (h.build && hereBuild && h.build !== hereBuild) {
      if (opts.machine) {
        note(`build-${m.name}`, `WARN  pool: ${m.name} server build ${h.build} differs from this PC ${hereBuild}; running there because it was asked for`);
        return true;
      }
      note(`build-${m.name}`, `WARN  pool: ${m.name} server build ${h.build} differs from this PC ${hereBuild}; skipping it (tools\\cli testbox provision updates it)`);
      return false;
    }
    return true;
  };
  if (!machines.some((m) => m.via === "local") && !machines.some(usable)) {
    throw new Error(`pool: no reachable machine for this run (${machines.map((m) => m.name).join(", ")})`);
  }

  const slug = slugFor(root);
  const pinned = !!opts.machine || mode !== "any";
  const ticketBody = () => ({ pid: process.pid, at: new Date(started).toISOString(), slug, purpose, root, pinned, machines: machines.map((m) => m.name) });
  const ticket = writeTicket(root, ticketBody());
  const lastTry = new Map();
  const waitMs = opts.waitMs ?? 3600000;
  let announced = false;
  try {
    for (;;) {
      // Every poll re-reads the pool config: a machine enabled or disabled in
      // lf.local.json while this ticket waits counts now, and the ticket's
      // "wants" follow it (same file, so its place in the queue is kept).
      if (!opts.machine) {
        let now = machines;
        try {
          now = withSnap(resolveMachines(root, opts));
        } catch {}
        if (now.map((m) => m.name).join("|") !== machines.map((m) => m.name).join("|")) {
          console.log(`pool: machines now ${now.map((m) => m.name).join(", ") || "(none enabled)"}`);
          machines = now;
          rewriteTicket(root, ticket, ticketBody());
        }
      }
      const tickets = liveTickets(root);
      const mine = tickets.findIndex((t) => t.file === ticket);
      const older = mine < 0 ? [] : tickets.slice(0, mine);
      for (const m of machines) {
        if (older.some((t) => (t.machines ?? []).includes(m.name))) continue;
        if (m.via === "local") {
          for (let i = 1; i <= m.slots; i++) {
            const s = tryLocal(root, m, i, { slug, purpose, root });
            if (s) return finish(s);
          }
          continue;
        }
        if (Date.now() - (lastTry.get(m.name) ?? 0) < REMOTE_RETRY_MS) continue;
        lastTry.set(m.name, Date.now());
        if (!usable(m)) continue;
        const cfg = holderConfig(root, m, snap, { purpose });
        const attemptAt = Date.now();
        const r = await startHolder(root, m, cfg, {
          onLost: (s) => console.log(`WARN  pool: lost the ssh session holding ${s.machine.name} ${s.lockName}; the run there is being stopped`),
        });
        if (r.slot || r.busy) markHealth(root, m, { reachable: true, build: r.slot?.prep.build ?? null });
        if (r.slot && !usable(m)) {
          r.slot.release();
          continue;
        }
        if (r.slot) {
          r.slot.snap = snap;
          return finish(r.slot, attemptAt);
        }
        // Busy: a holder whose owner on this PC died, or one hung with no
        // server, is reclaimed now and the machine retried on the next poll.
        if (r.busy?.length) {
          const runs = readRuns(root);
          for (const b of r.busy) {
            const v = remoteBusyVerdict(b, runs);
            if (v && reclaimRemote(root, m, b, v.kind, v.reason)) lastTry.delete(m.name);
          }
        }
        if (r.error) {
          // Down for the rest of this run and the health window; the next
          // machine (or this PC) takes the job.
          downNow.add(m.name);
          markHealth(root, m, { reachable: false, error: r.error });
          const next = machines.filter((x) => x.name !== m.name && usable(x)).map((x) => x.name);
          console.log(`pool: ${r.error}; ${m.name} marked down for this run, ${next.length ? `falling back to ${next.join(", ")}` : "no other machine"}`);
          if (!next.length) throw new Error(`pool: no reachable machine for this run (${m.name} is down)`);
        }
      }
      const ahead = older.length;
      if (!announced) {
        announced = true;
        console.log(`pool: every slot is busy (${machines.map((m) => m.name).join(", ")}); queued with ${ahead} ahead`);
      }
      if (Date.now() - started > waitMs) throw new Error(`pool: no slot within ${Math.round(waitMs / 60000)} min`);
      await sleep(1000);
    }
  } finally {
    dropTicket(root, ticket);
  }

  // waited = time in the queue; the ssh connect and run-folder prep of the
  // winning attempt count as setup, not waiting.
  function finish(s, attemptAt = Date.now()) {
    s.waitedMs = attemptAt - started;
    s.setupMs = Date.now() - attemptAt;
    s.startedAt = attemptAt;
    const where = s.local ? `this PC (${hostname()})` : `${s.machine.name} ${s.rDir}`;
    const setup = s.local ? "" : `, holder + prep ${(s.setupMs / 1000).toFixed(1)} s`;
    console.log(`pool: ${purpose} runs on ${where}, slot ${s.lockName}; waited ${(s.waitedMs / 1000).toFixed(1)} s${setup}`);
    return s;
  }
}

// ---------------------------------------------------------------- run log

function runsFile(root) {
  return join(poolDir(root), "runs.jsonl");
}

export function recordRun(root, slot, { purpose, code, extra = {} }) {
  const durMs = Date.now() - (slot.startedAt ?? Date.now());
  const rec = {
    at: new Date().toISOString(),
    slug: slugFor(root),
    purpose,
    machine: slot.machine.name,
    slot: slot.lockName,
    waitS: Math.round((slot.waitedMs ?? 0) / 100) / 10,
    durS: Math.round(durMs / 100) / 10,
    code,
    result: code === 0 ? "pass" : code === 1 ? "fail" : "error",
    ...extra,
  };
  try {
    appendFileSync(runsFile(root), JSON.stringify(rec) + "\n");
  } catch {}
  console.log(`pool: ${purpose} on ${slot.machine.name}: exit ${code}, waited ${rec.waitS} s, ran ${rec.durS} s`);
  return rec;
}

export function readRuns(root) {
  const f = runsFile(root);
  if (!existsSync(f)) return [];
  return readFileSync(f, "utf8").split(/\r?\n/).filter(Boolean).map((l) => readJsonText(l));
}

// ---------------------------------------------------------------- status / stats / prune

function localTime(iso) {
  const d = new Date(iso);
  if (Number.isNaN(d.getTime())) return String(iso);
  const p = (n) => String(n).padStart(2, "0");
  return `${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())} ${p(d.getHours())}:${p(d.getMinutes())}:${p(d.getSeconds())}`;
}

function ago(iso) {
  const s = Math.round((Date.now() - Date.parse(iso)) / 1000);
  if (!Number.isFinite(s)) return "?";
  return s < 120 ? `${s}s` : `${Math.round(s / 60)}m`;
}

function holderText(info) {
  if (!info) return "free";
  const who = info.slug ? `${info.slug}${info.purpose ? ` ${info.purpose}` : ""}` : info.root ? info.root.split(/[\\/]/).pop() : "?";
  const from = info.origin ? ` from ${info.origin}` : "";
  return `busy: ${who}${from} (pid ${info.pid}, ${ago(info.at)})`;
}

function remoteLocks(m) {
  const names = [];
  for (let i = 1; i <= m.slots; i++) names.push(slotLockName(i));
  const ps = [
    `$ld = Join-Path ${tb.psQuote(m.root)} 'tmp\\wt\\locks'`,
    `foreach ($n in @(${names.map(tb.psQuote).join(",")})) { $f = Join-Path $ld ($n + '.lock'); if (Test-Path -LiteralPath $f) { $raw = [IO.File]::ReadAllText($f).Trim(); $alive = $false; try { $i = $raw | ConvertFrom-Json; $alive = [bool](Get-Process -Id $i.pid -ErrorAction SilentlyContinue) } catch {}; Write-Host \"lf-pool: lock $n $alive $raw\" } else { Write-Host \"lf-pool: lock $n free\" } }`,
  ].join("\n");
  const r = tb.sshPs(m.host, ps, { timeout: PROBE_TIMEOUT_MS, connectTimeout: 4 });
  const out = {};
  for (const l of (r.stdout ?? "").split(/\r?\n/)) {
    const mm = /^lf-pool: lock (\S+) (free|True|False)(?: (.*))?$/.exec(l.trim());
    if (!mm) continue;
    out[mm[1]] = mm[2] === "free" ? null : { ...readJsonText(mm[3] ?? "{}"), alive: mm[2] === "True" };
  }
  return r.status === 0 ? out : null;
}

export function cmdPoolStatus(root) {
  const machines = loadMachines(root);
  const hereBuild = tb.serverBuildId(tb.localServerDir(root));
  console.log("pool machines:");
  for (const m of machines) {
    const h = probe(root, m, { fresh: true });
    const where = m.via === "local" ? `this PC (${hostname()})` : `ssh ${m.host}`;
    const build = h.build ? `build ${h.build}${hereBuild && h.build !== hereBuild ? " (DIFFERS)" : ""}` : "build ?";
    console.log(`  ${m.name.padEnd(12)} ${where.padEnd(24)} ${h.reachable ? `up (${h.ms} ms)` : "DOWN"}  ${build}  caps ${m.caps.join(",")}`);
    if (!h.reachable) continue;
    const locks = m.via === "local" ? null : remoteLocks(m);
    for (let i = 1; i <= m.slots; i++) {
      const name = slotLockName(i);
      let info;
      if (m.via === "local") {
        info = readJson(localLockFile(root, name), null);
        if (info && !pidAlive(info.pid)) info = { ...info, stale: true };
      } else {
        info = locks ? locks[name] : undefined;
        if (info && !info.alive) info = { ...info, stale: true };
        else if (info && remoteOwnerGone(info)) info = { ...info, ownerGone: true };
      }
      let text = info === undefined ? "unknown" : info?.stale ? `stale (pid ${info.pid} gone; next acquirer reclaims it)` : holderText(info);
      if (info?.ownerGone) text += `; STALE: owner pid ${info.originPid} gone here, the next queued run reclaims it`;
      console.log(`      slot ${i} ${name.padEnd(16)} ${text}`);
    }
  }
  const q = liveTickets(root);
  console.log(`queue: ${q.length} waiting`);
  for (const t of q) console.log(`  ${t.slug ?? "?"} ${t.purpose ?? ""} pid ${t.pid}, ${ago(t.at)}, wants ${(t.machines ?? []).join("|")}${t.pinned ? " (pinned)" : ""}`);
  const runs = readRuns(root).filter((r) => r.purpose !== "reclaim").slice(-20);
  console.log(`last ${runs.length} run(s):`);
  for (const r of runs) {
    console.log(`  ${localTime(r.at)}  ${String(r.machine).padEnd(11)} ${String(r.slug).padEnd(22)} ${String(r.purpose).padEnd(14)} wait ${String(r.waitS).padStart(6)} s  ran ${String(r.durS).padStart(6)} s  ${r.result}${r.passed !== undefined ? ` ${r.passed}/${r.total}` : ""}`);
  }
  return 0;
}

export function cmdPoolStats(root) {
  const all = readRuns(root);
  const reclaims = all.filter((r) => r.purpose === "reclaim");
  const runs = all.filter((r) => r.purpose !== "reclaim");
  const by = new Map();
  for (const r of runs) {
    const k = r.machine;
    if (!by.has(k)) by.set(k, []);
    by.get(k).push(r);
  }
  console.log(`pool stats: ${runs.length} run(s)`);
  for (const [k, rs] of by) {
    const avg = (f) => (rs.reduce((a, r) => a + (Number(r[f]) || 0), 0) / rs.length).toFixed(1);
    const pass = rs.filter((r) => r.result === "pass").length;
    const rc = reclaims.filter((r) => r.machine === k);
    const rcText = rc.length ? `  reclaimed ${rc.length} (dead owner ${rc.filter((r) => r.kind === "dead-owner").length}, hang ${rc.filter((r) => r.kind === "hang").length})` : "";
    console.log(`  ${k.padEnd(12)} runs ${String(rs.length).padStart(4)}  pass ${pass}  avg wait ${avg("waitS")} s  avg run ${avg("durS")} s${rcText}`);
  }
  if (reclaims.length) {
    console.log(`reclaimed slots (last ${Math.min(5, reclaims.length)} of ${reclaims.length}):`);
    for (const r of reclaims.slice(-5)) console.log(`  ${localTime(r.at)}  ${String(r.machine).padEnd(11)} ${r.slot}  ${r.slug} ${r.victimPurpose ?? ""}: ${r.kind}, ${r.reason}`);
  }
  return 0;
}

// Removes run folders on the remotes whose origin worktree is gone: the folder
// says which PC and slug it belongs to (tmp/pool-owner.json); only this PC's
// are touched. --unmarked also takes folders from before the marker whose slug
// has no worktree here. Folders behind a live slot lock are kept.
export function cmdPoolPrune(root, flags = []) {
  const dry = flags.includes("--dry-run");
  const unmarked = flags.includes("--unmarked");
  const known = new Set(Object.keys(wt.readState(root).worktrees ?? {}));
  const known2 = new Set(wt.listWorktrees(root).map((w) => w.path.replace(/\\/g, "/").toLowerCase()));
  let fail = 0;
  for (const m of loadMachines(root).filter((x) => x.via === "ssh")) {
    if (!probe(root, m, { fresh: true }).reachable) {
      console.log(`pool prune: ${m.name} down; skipped`);
      continue;
    }
    const list = tb.sshPs(m.host, [
      `$main = ${tb.psQuote(m.root)}`,
      "$busy = @(Get-ChildItem -LiteralPath (Join-Path $main 'tmp\\wt\\locks') -Filter 'server-2001*.lock' -ErrorAction SilentlyContinue | ForEach-Object { try { $i = Get-Content -Raw -LiteralPath $_.FullName | ConvertFrom-Json; if ($i.rDir -and (Get-Process -Id $i.pid -ErrorAction SilentlyContinue)) { $i.rDir } } catch {} })",
      "git -C $main worktree list --porcelain | Where-Object { $_ -like 'worktree *' } | ForEach-Object { $d = $_.Substring(9); if ((Split-Path -Leaf $d) -like 'Life-Framework-rt-*') { $o = Join-Path $d 'tmp\\pool-owner.json'; $own = ''; if (Test-Path -LiteralPath $o) { $own = (Get-Content -Raw -LiteralPath $o).Trim() }; $b = [bool]($busy | Where-Object { $_.Replace('\\','/').TrimEnd('/') -ieq $d.Replace('\\','/').TrimEnd('/') }); Write-Host \"lf-pool: dir $b $d|$own\" } }",
    ].join("\n"), { timeout: 60000 });
    const victims = [];
    for (const l of (list.stdout ?? "").split(/\r?\n/)) {
      const mm = /^lf-pool: dir (True|False) ([^|]+)\|(.*)$/.exec(l.trim());
      if (!mm) continue;
      const dir = mm[2];
      const own = readJsonText(mm[3] || "null");
      const leaf = dir.replace(/\\/g, "/").split("/").pop();
      let reason = null;
      if (own && own.origin) {
        if (String(own.origin).toLowerCase() !== hostname().toLowerCase()) continue;
        const wtGone = own.worktree ? !known2.has(String(own.worktree).toLowerCase()) && !existsSync(own.worktree) : !known.has(own.slug);
        if (wtGone) reason = `worktree ${own.worktree ?? own.slug} is gone`;
      } else if (unmarked) {
        const slug = leaf.replace(/^Life-Framework-rt-/, "").replace(/-session$/, "").replace(/-s\d+$/, "");
        if (!known.has(slug)) reason = `unmarked, no worktree '${slug}' here`;
      }
      if (!reason) continue;
      if (mm[1] === "True") {
        console.log(`pool prune: ${m.name} ${dir} is in use; kept`);
        continue;
      }
      victims.push(dir);
      console.log(`pool prune: ${m.name} ${dir}: ${reason}${dry ? " (dry run)" : ""}`);
    }
    if (dry || victims.length === 0) continue;
    const code = tb.removeRemoteDirs(m.host, m.root, victims);
    if (code !== 0) fail = 1;
  }
  return fail;
}

export function cmdPool(root, args = []) {
  const [sub, ...rest] = args;
  if (sub === "status" || !sub) return cmdPoolStatus(root);
  if (sub === "stats") return cmdPoolStats(root);
  if (sub === "prune") return cmdPoolPrune(root, rest);
  console.log(`usage: tools\\cli pool status|stats|prune [--dry-run] [--unmarked]
  status   machines, reachability, server build, slots and holders, queue, last 20 runs
  stats    runs per machine: count, passes, average wait and run time
  prune    remove remote run folders whose worktree on this PC is gone (junction-safe)
machines: tools/pool.json, overridden by "pool" in lf.local.json`);
  return sub === "help" ? 0 : 2;
}

// Syncs this worktree to <box>/Life-Framework-rt-<slug><suffix> without taking
// a slot, then runs `node <script> <args>` there (sessions: the remote
// `session up` takes the box's server-2001 itself and hands it to its server).
export async function syncAndRun(root, m, { script, args, suffix = "-session", purpose = "session" }) {
  const snap = tb.snapshotCommit(root);
  const cfg = holderConfig(root, m, snap, { slots: 0, purpose, suffix });
  const r = await startHolder(root, m, cfg);
  if (!r.slot) {
    console.log(`FAIL  ${r.error ?? `${m.name} busy`}`);
    return { code: 3 };
  }
  r.slot.snap = snap;
  const out = await tb.runRemote(root, r.slot, { args, script, writeSyncState: (rt, name, rec) => writeSyncState(rt, `${name}${suffix}`, rec), fetchLog: false });
  return { ...out, rDir: r.slot.rDir };
}

// Runs `node <script> <args>` in an existing run folder, no sync (a live
// session server runs out of it; a checkout would change files under it).
export function runIn(m, dir, script, args) {
  const ps = [`Set-Location -LiteralPath ${tb.psQuote(dir)}`, `& node ${tb.psQuote(script)} ${args.map(tb.psQuote).join(" ")}`, "exit $LASTEXITCODE"].join("\n");
  return tb.sshPsStream(m.host, ps, () => {});
}

// Used by sessions: the first machine with a free slot 1 lock, without
// holding it (session up takes it itself and hands it to its server).
export function freeSlotOn(root, m) {
  if (m.via === "local") {
    const info = readJson(localLockFile(root, slotLockName(1)), null);
    return !info || !pidAlive(info.pid);
  }
  const locks = remoteLocks(m);
  if (!locks) return false;
  const info = locks[slotLockName(1)];
  return !info || !info.alive;
}
