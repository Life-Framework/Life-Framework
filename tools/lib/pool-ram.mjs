// tools/lib/pool-ram.mjs - RAM admission by measured peaks (docs/worktrees.md "RAM admission").
//
// The pool's gate used to compare free RAM, once, at the start, with a hand-set reserve per session
// (server 600, client 800-850 MB) and nothing for a test. The laptop (15.2 GB, often 1 GB free) fell
// under its 1.5 GB floor anyway, because
//   - the reserves were below real peaks: an Everon island session is 2.5 GB (server) + 2.9 GB (client);
//   - a Workbench (rdb refresh, look, compile), the main guard's runs and anything started outside the
//     pool hold no lane lock, so no reserve: only the free RAM they already took showed;
//   - a job that was still growing (a Workbench loading, an island server streaming) counted for what
//     it held at that second, not for the peak it was about to reach;
//   - a test reserved nothing for itself beyond the floor, so it could land 0.8 GB under it.
//
// Now the gate counts, per job kind and world class, the measured peak (a high percentile of the
// peaks recorded after real runs, seeded below until there are samples), and it counts every
// ArmaReforger process on the machine, whoever started it: what each one still has to grow to its
// peak is memory the free-RAM reading does not show yet (unrealisedMb). Pure functions; the process
// scan and the peak file are the only I/O (readPeaks / writePeaks, scanScript / parseScan).
//
//   admit()        one verdict: free RAM less what running jobs still grow into less this job's peak
//                  must stay at or over the floor; else it queues. An island session also refuses
//                  while another island-sized job runs and the machine lacks both peaks plus the floor.
//   recordLive()   called on every scan: tracks each live process's highest working set and, when the
//                  process is gone, records that as one observed peak sample for its kind and world.

import { mkdirSync, readFileSync, renameSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";

// World class of a job: "island" is vanilla Everon (CoreOnEveron: terrain, navmesh, AI streaming),
// "small" is every other world the pool boots (TestLogic, GridWorld, DebugWorld, the town).
export const ISLAND = "island";
export const SMALL = "small";

// Seeds (MB) used until a key has MIN_SAMPLES observed peaks. Measured 2026-11-10 (debug world, docs
// "Session profiles"), 2026-11-14 (test servers on lf-cloud) and 2026-11-16 (Everon island session on the
// laptop: server 2.5 GB, client 2.9 GB; Workbench 3.7 GB). Keys: kind:world[:profile].
export const SEED_PEAKS_MB = {
  "server:small": 700,
  "server:island": 2500,
  "client:small": 1200,
  "client:small:lowspec": 1100,
  "client:island": 2900,
  "test:small": 800,
  "test:island": 2500,
  "workbench:small": 3700,
  // A look Workbench on the pack's island (MainWorld, play mode) streams Everon like a server and a client in one
  // process: seeded at the island session's 2.5 + 2.9 GB until real samples come in (#2347).
  "workbench:island": 5400,
};
export const MIN_SAMPLES = 3;
export const MAX_SAMPLES = 40;
export const PEAK_PERCENTILE = 0.9;
// A sample below this is a process caught while it was still starting, not a peak.
export const MIN_SAMPLE_MB = 200;

export function worldClass(world) {
  const w = String(world ?? "");
  // CoreOnEveron (vanilla Everon plus the core mode), the pack's MainWorld (a SubScene of Eden), Eden itself.
  return w === ISLAND || /CoreOnEveron|(^|[/}])MainWorld\.ent$|(^|\/)Eden\.ent$/i.test(w) || /^(everon|island)$/i.test(w.trim()) ? ISLAND : SMALL;
}

export function peakKey(kind, world = SMALL, profile) {
  const w = world === ISLAND ? ISLAND : SMALL;
  return profile && kind === "client" && w === SMALL && profile !== "full" ? `${kind}:${w}:${profile}` : `${kind}:${w}`;
}

// Nearest-rank percentile of the finite numbers in values; null when none.
export function percentile(values, p = PEAK_PERCENTILE) {
  const v = values.filter((x) => Number.isFinite(x)).sort((a, b) => a - b);
  if (!v.length) return null;
  return v[Math.min(v.length - 1, Math.max(0, Math.ceil(p * v.length) - 1))];
}

// The reserve for one process kind (MB): the p90 of the recorded peaks once MIN_SAMPLES exist, else
// the seed. A measured number never drops under what the pool config already sets (floorMb).
export function peakMb(peaks, kind, world = SMALL, profile, floorMb = 0) {
  const key = peakKey(kind, world, profile);
  const samples = peaks?.[key];
  const measured = Array.isArray(samples) && samples.length >= MIN_SAMPLES ? percentile(samples) : null;
  const seed = SEED_PEAKS_MB[key] ?? SEED_PEAKS_MB[peakKey(kind, world)] ?? 0;
  return Math.max(Math.round(measured ?? seed), Math.round(floorMb) || 0);
}

// What a job will take at its peak (MB). kinds: session (server + clients), test, workbench.
// settings carries the configured per-profile session reserve as a floor (q.sessionReserveMb).
export function jobReserveMb(settings, peaks, { kind, clients = 1, profile = "full", world = SMALL } = {}) {
  const w = worldClass(world);
  if (kind === "workbench") return peakMb(peaks, "workbench", w);
  if (kind === "test") return peakMb(peaks, "test", w, undefined, settings?.testReserveMb ?? 0);
  const own = settings?.sessionReserve?.[profile];
  const serverFloor = own?.serverMb ?? settings?.sessionReserveServerMb ?? 0;
  const clientFloor = own?.clientMb ?? settings?.sessionReserveClientMb ?? 0;
  return peakMb(peaks, "server", w, undefined, serverFloor) + peakMb(peaks, "client", w, profile, clientFloor) * Math.max(0, Number(clients) || 0);
}

// ---------------------------------------------------------------- process scan

// PowerShell that prints one "lf-pool: proc <kind> <pid> <workingSetMb> <island 0|1> <privateMb>" line per
// ArmaReforger process (private = committed bytes, which a working-set trim does not shrink): workbench (a Workbench exe), client (the game, a -client launch or the Steam
// exe), server (a dedicated server; a test server is one). Run here or over ssh; same output.
export const PS_PROC_SCAN = String.raw`
Get-CimInstance Win32_Process -Filter "Name LIKE 'ArmaReforger%'" | ForEach-Object {
  $c = [string]$_.CommandLine; $n = [string]$_.Name
  $k = $null
  if ($n -like '*Workbench*') { $k = 'workbench' } elseif ($n -like 'ArmaReforgerServer*') { $k = 'server' } elseif ($n -like 'ArmaReforgerSteam*') { $k = 'client' }
  if ($k) { $i = 0; if ($c -match 'CoreOnEveron') { $i = 1 }; Write-Host ("lf-pool: proc " + $k + " " + $_.ProcessId + " " + [int]($_.WorkingSetSize / 1MB) + " " + $i + " " + [int]($_.PrivatePageCount / 1MB)) }
}
`;

// [{ kind, pid, wsMb, world, privMb }] from PS_PROC_SCAN output.
export function parseScan(text) {
  const out = [];
  for (const l of String(text ?? "").split(/\r?\n/)) {
    const m = /^lf-pool: proc (workbench|server|client) (\d+) (\d+) ([01])(?: (\d+))?$/.exec(l.trim());
    if (m) out.push({ kind: m[1], pid: Number(m[2]), wsMb: Number(m[3]), world: m[4] === "1" ? ISLAND : SMALL, ...(m[5] !== undefined ? { privMb: Number(m[5]) } : {}) });
  }
  return out;
}

// Memory the running processes will still take that the free-RAM reading does not show yet (MB): for
// each, its kind's peak less what it has already taken: the larger of its working set now and the
// highest one seen (maxMb, withSeenMax), and nothing once its private bytes reach the peak (it has
// loaded; Windows only paged it out, which free RAM already counts as free). A Workbench that
// is still loading, an island server still streaming, a guard run's servers all count here, started by
// the pool or not. profile is the profile client peaks are read for.
export function unrealisedMb(procs, peaks, profile = "full") {
  let mb = 0;
  for (const p of procs ?? []) {
    const peak = peakMb(peaks, p.kind, p.world, p.kind === "client" ? profile : undefined);
    if (Number(p.privMb) >= peak) continue;
    mb += Math.max(0, peak - Math.max(Number(p.wsMb) || 0, Number(p.maxMb) || 0));
  }
  return Math.round(mb);
}

// procs with the highest working set recordLive has seen for each (maxMb), from the live file of `tag`.
export function withSeenMax(hubDir, procs, tag = "local") {
  const seen = readJson(liveFile(hubDir, tag), {});
  return (procs ?? []).map((p) => (seen[p.pid]?.maxMb ? { ...p, maxMb: seen[p.pid].maxMb } : p));
}

// The processes counted as island-sized: an island server or client, or a Workbench.
export function islandCount(procs) {
  return (procs ?? []).filter((p) => p.world === ISLAND && (p.kind === "server" || p.kind === "client")).length;
}

// ---------------------------------------------------------------- admission

// One verdict for a job on a machine.
//   freeMb       live free RAM
//   minFreeMb    the floor
//   reserveMb    the job's peak (jobReserveMb)
//   pendingMb    memory of lane-lock jobs in their first seconds, before they have processes (lockGapMb)
//   growthMb     unrealisedMb of the running processes
//   island       the job is island-sized; islandRunning how many island processes already run there
// Returns { ok, needMb, reason }. needMb is the free RAM that would admit it.
export function admit({ freeMb, minFreeMb, reserveMb = 0, pendingMb = 0, growthMb = 0, island = false, islandRunning = 0, what = "the job" }) {
  const gb = (x) => (x / 1024).toFixed(1);
  const committed = (Number(pendingMb) || 0) + (Number(growthMb) || 0);
  const needMb = minFreeMb + reserveMb + committed;
  if (!Number.isFinite(freeMb)) return { ok: true, needMb };
  if (freeMb < needMb) {
    const why = [`free RAM ${gb(freeMb)} GB`, `${gb(reserveMb)} GB peak for ${what}`];
    if (committed > 0) why.push(`${gb(committed)} GB the running jobs still grow into`);
    return { ok: false, needMb, reason: `${why.join(" minus ")} would be under ${gb(minFreeMb)} GB` };
  }
  // One island at a time: a second island-sized job needs its peak and the floor with nothing else
  // counted away, i.e. the machine must have the room for both, not just for the part that shows.
  if (island && islandRunning > 0 && freeMb < needMb + reserveMb) {
    return { ok: false, needMb: needMb + reserveMb, reason: `an island session already runs here; a second needs ${gb(needMb + reserveMb)} GB free, ${gb(freeMb)} GB is` };
  }
  return { ok: true, needMb };
}

// Memory (MB) the pool's own lane-lock jobs will take in their first spawnGapS seconds, before their
// processes exist to be scanned. Older locks are counted through their processes (unrealisedMb), so
// nothing is counted twice. infos: [{ purpose, at, clients?, profile?, world? }].
export const SPAWN_GAP_S = 45;
export function lockGapMb(infos, settings, peaks, now = Date.now(), gapS = SPAWN_GAP_S) {
  let mb = 0;
  for (const i of infos ?? []) {
    if (!i || i.alive === false || !i.purpose) continue;
    const age = now - Date.parse(i.at);
    if (!Number.isFinite(age) || age > gapS * 1000) continue;
    mb += jobReserveMb(settings, peaks, { kind: i.purpose === "session" ? "session" : "test", clients: i.clients ?? 1, profile: i.profile ?? "full", world: i.world ?? SMALL });
  }
  return mb;
}

// ---------------------------------------------------------------- observed peaks

export function peaksFile(hubDir) {
  return join(hubDir, "pool-peaks.json");
}
function liveFile(hubDir, tag = "local") {
  return join(hubDir, `pool-ram-live-${String(tag).replace(/[^a-z0-9-]/gi, "_")}.json`);
}
function readJson(file, fallback) {
  try {
    return JSON.parse(readFileSync(file, "utf8"));
  } catch {
    return fallback;
  }
}
function writeJson(file, value) {
  try {
    mkdirSync(dirname(file), { recursive: true });
    const tmp = `${file}.${process.pid}.tmp`;
    writeFileSync(tmp, JSON.stringify(value));
    renameSync(tmp, file);
  } catch {}
}

export function readPeaks(hubDir) {
  const v = readJson(peaksFile(hubDir), {});
  return v && typeof v === "object" ? v : {};
}

// Appends one observed peak (MB) to its key, keeping the newest MAX_SAMPLES. Returns the new peaks.
export function recordPeak(peaks, kind, world, profile, mb) {
  if (!Number.isFinite(mb) || mb < MIN_SAMPLE_MB) return peaks;
  const key = peakKey(kind, world, profile);
  const list = Array.isArray(peaks[key]) ? peaks[key] : [];
  return { ...peaks, [key]: [...list, Math.round(mb)].slice(-MAX_SAMPLES) };
}

// Called with every scan of this machine: `live` is { pid: { key parts, maxMb } } from the last call.
// Updates each live process's highest working set and, for a process that is gone, records its
// highest as one sample. Pure: returns { live, peaks, recorded }.
export function trackPeaks(prevLive, procs, peaks, profile = "full") {
  const live = {};
  const now = new Set((procs ?? []).map((p) => p.pid));
  for (const p of procs ?? []) {
    const prev = prevLive?.[p.pid];
    live[p.pid] = { kind: p.kind, world: p.world, profile: p.kind === "client" ? profile : undefined, maxMb: Math.max(Number(p.wsMb) || 0, prev?.maxMb ?? 0) };
  }
  let out = peaks;
  let recorded = 0;
  for (const [pid, v] of Object.entries(prevLive ?? {})) {
    if (now.has(Number(pid))) continue;
    const before = out;
    out = recordPeak(out, v.kind, v.world, v.profile, v.maxMb);
    if (out !== before) recorded++;
  }
  return { live, peaks: out, recorded };
}

// I/O wrapper: tracks the scan of machine `tag` against the files under hubDir (pids are per machine)
// and returns the peaks to use.
export function recordLive(hubDir, procs, { profile = "full", tag = "local" } = {}) {
  const peaks = readPeaks(hubDir);
  const t = trackPeaks(readJson(liveFile(hubDir, tag), {}), procs, peaks, profile);
  writeJson(liveFile(hubDir, tag), t.live);
  if (t.recorded) writeJson(peaksFile(hubDir), t.peaks);
  return t.peaks;
}

// A compact { key: p90 } of the peaks, for the PowerShell holder's config and `pool status`.
export function peakTable(peaks) {
  const out = {};
  for (const key of Object.keys(SEED_PEAKS_MB)) {
    const [kind, world, profile] = key.split(":");
    out[key] = peakMb(peaks, kind, world, profile);
  }
  return out;
}
