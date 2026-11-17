// Free GPU memory (VRAM) for the test pool (#2629). Each game client takes GPU memory as well as RAM, and
// the laptop (RTX 3070 Ti, 8 GB) runs out of VRAM before RAM with 2 clients at the full profile
// (docs/worktrees.md "Session profiles"). This file reads the free VRAM (nvidia-smi, locally or through the
// same bounded ssh path the RAM read uses) and holds the pure rules the queue applies (pool-queue.mjs
// vramVerdict, rankMachines). Pure but for readVramFreeMb's default executor, so
// tools/validation/pool-vram-selftest.mjs proves it on stubbed nvidia-smi output.
//
// "Unknown" is never a refusal: no nvidia-smi, a failed or timed-out read, or output that is not a number
// skips the VRAM rule (the RAM gate alone decides) and is logged once per machine (noteVramUnknown).
import { spawnSync } from "node:child_process";

export const NVIDIA_SMI_ARGS = ["--query-gpu=memory.free", "--format=csv,noheader,nounits"];
export const VRAM_READ_TIMEOUT_MS = 8000;

// Memory (MB) one game client takes on the GPU, per session profile (the full profile draws at
// 1920x1080 with the high texture tier, lowspec at 640x360 and the 768 MB texture budget). tools/pool.json /
// lf.local.json "sessionReserveClientVramMb" ({ lowspec, full }) overrides these.
export const DEFAULT_CLIENT_VRAM_MB = { lowspec: 700, full: 1800 };
// Free VRAM left after a start: the desktop and the look Workbench share the card (MB).
export const DEFAULT_MIN_FREE_VRAM_MB = 256;

// Free MB from `nvidia-smi --query-gpu=memory.free --format=csv,noheader,nounits` output: one number per GPU,
// and a card with the most free memory is the one a client lands on, so that number wins. null when no line
// is a plain non-negative integer ("[N/A]", "NVIDIA-SMI has failed ...", empty text).
export function parseVramFreeMb(text) {
  const nums = [];
  for (const raw of String(text ?? "").split(/\r?\n/)) {
    const m = /^\s*(\d+)(?:\s*MiB)?\s*$/i.exec(raw);
    if (m) nums.push(Number(m[1]));
  }
  return nums.length ? Math.max(...nums) : null;
}

// Free VRAM (MB) now, or null when unknown. exec(cmd, args, { timeout }) -> { status, stdout, error? } is the
// seam: the default runs nvidia-smi on this machine (timeout-bounded, no window); a pool machine passes
// an executor that goes through its bounded ssh (tools/remote/toolbox sshPs), so a dead link fails in seconds.
export function readVramFreeMb({ exec = localExec, timeoutMs = VRAM_READ_TIMEOUT_MS, args = NVIDIA_SMI_ARGS } = {}) {
  let r;
  try {
    r = exec("nvidia-smi", args, { timeout: timeoutMs });
  } catch {
    return null;
  }
  if (!r || r.error || r.status !== 0) return null;
  return parseVramFreeMb(r.stdout);
}

// Total VRAM (MB) of the card, for `pool status` (free and used); null when unknown. Same seam as the free read.
export const NVIDIA_SMI_TOTAL_ARGS = ["--query-gpu=memory.total", "--format=csv,noheader,nounits"];
export function readVramTotalMb(o = {}) {
  return readVramFreeMb({ ...o, args: NVIDIA_SMI_TOTAL_ARGS });
}

// The same for a remote read: one `lf-pool: vramtotal <mb>` line, nothing when the card does not answer.
export const PS_VRAM_TOTAL_READ = String.raw`try { $t = & nvidia-smi --query-gpu=memory.total --format=csv,noheader,nounits 2>$null; $n = ($t | ForEach-Object { [string]$_ } | Where-Object { $_ -match '^\s*\d+\s*$' } | ForEach-Object { [int]$_.Trim() } | Measure-Object -Maximum).Maximum; if ($null -ne $n) { Write-Host ('lf-pool: vramtotal ' + $n) } } catch {}`;
export function parseVramTotalLine(text) {
  const m = /^lf-pool: vramtotal (\d+)\s*$/m.exec(String(text ?? ""));
  return m ? Number(m[1]) : null;
}

// `pool status` text for one machine: " VRAM 5.9/8.0 GB free (2.1 GB used)" with the total known, the older
// " VRAM 5.9 GB free" without it, "" when the free VRAM is unknown (vramStatusText).
export function vramUsageText(freeMb, totalMb) {
  if (!Number.isFinite(freeMb)) return "";
  if (!Number.isFinite(totalMb) || totalMb < freeMb) return vramStatusText(freeMb);
  const gb = (x) => (x / 1024).toFixed(1);
  return ` VRAM ${gb(freeMb)}/${gb(totalMb)} GB free (${gb(totalMb - freeMb)} GB used)`;
}

function localExec(cmd, args, { timeout }) {
  return spawnSync(cmd, args, { encoding: "utf8", timeout, windowsHide: true });
}

// PowerShell for a remote read: prints one `lf-pool: vram <mb>` line when the card answers, nothing otherwise
// (no nvidia-smi on the box, or a bad answer). Embeddable in the state read beside the RAM line.
export const PS_VRAM_READ = String.raw`try { $v = & nvidia-smi --query-gpu=memory.free --format=csv,noheader,nounits 2>$null; $n = ($v | ForEach-Object { [string]$_ } | Where-Object { $_ -match '^\s*\d+\s*$' } | ForEach-Object { [int]$_.Trim() } | Measure-Object -Maximum).Maximum; if ($null -ne $n) { Write-Host ('lf-pool: vram ' + $n) } } catch {}`;

// The free VRAM (MB) from a remote state read's text; null when the line is missing.
export function parseVramLine(text) {
  const m = /^lf-pool: vram (\d+)\s*$/m.exec(String(text ?? ""));
  return m ? Number(m[1]) : null;
}

// What a session of n clients takes on the GPU (MB). The server has no renderer, so there is no server part.
export function sessionVramReserveMb(settings, clients, profile = "full") {
  const per = settings?.sessionReserveClientVramMb?.[profile] ?? settings?.sessionReserveClientVramMb?.full ?? DEFAULT_CLIENT_VRAM_MB[profile] ?? DEFAULT_CLIENT_VRAM_MB.full;
  return per * Math.max(0, Number(clients) || 0);
}

// The VRAM gate, shaped like the RAM gate. reserveMb: what the job takes (0 for a test-lane job, which starts
// no client); pendingMb: clients still starting, which free VRAM does not show yet. freeMb unknown (null,
// NaN, undefined) skips the rule: { ok: true, unknown: true } (the caller logs it once).
export function vramVerdict({ freeMb, minFreeMb = DEFAULT_MIN_FREE_VRAM_MB, reserveMb = 0, pendingMb = 0 } = {}) {
  if (!(reserveMb > 0)) return { ok: true };
  if (freeMb === null || freeMb === undefined || !Number.isFinite(Number(freeMb))) return { ok: true, unknown: true };
  if (freeMb - pendingMb - reserveMb >= minFreeMb) return { ok: true };
  const gb = (x) => (x / 1024).toFixed(1);
  return {
    ok: false,
    needMb: minFreeMb + reserveMb + pendingMb,
    reason: `free VRAM ${gb(freeMb)} GB minus ${gb(reserveMb + pendingMb)} GB for the client(s)${pendingMb > 0 ? " starting" : ""} would be under ${gb(minFreeMb)} GB`,
  };
}

// GPU memory the clients still starting on a machine will take (MB), from its live lane lock records
// { purpose, at, clients?, profile?, alive? }: a session younger than three minutes (the RAM rule's window).
export function bootingVramMb(infos, settings = {}, now = Date.now()) {
  let mb = 0;
  for (const i of infos) {
    if (!i || i.alive === false || i.purpose !== "session") continue;
    const age = now - Date.parse(i.at);
    if (Number.isFinite(age) && age < 180000) mb += sessionVramReserveMb(settings, i.clients ?? 1, i.profile ?? "full");
  }
  return mb;
}

const unknownLogged = new Set();
// Logs "VRAM unknown" for a machine once per process (log defaults to console.log); returns whether it logged.
export function noteVramUnknown(machine, log = (s) => console.log(s)) {
  if (unknownLogged.has(machine)) return false;
  unknownLogged.add(machine);
  log(`pool: free VRAM of ${machine} unknown (no nvidia-smi or a failed read); placing sessions by RAM only`);
  return true;
}
export function resetVramUnknownLog() {
  unknownLogged.clear();
}

// `pool status` text: " VRAM 5.9 GB free", or "" when unknown.
export function vramStatusText(freeMb) {
  return Number.isFinite(freeMb) ? ` VRAM ${(freeMb / 1024).toFixed(1)} GB free` : "";
}
