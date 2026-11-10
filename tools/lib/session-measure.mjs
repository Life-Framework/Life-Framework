// `tools\cli session measure`: RAM of a session's processes, per profile or per lever.
//
// Takes one session lane (the same queue `session up` uses), then for each variant boots the server
// plus N clients through the CLI's own session launcher, optionally runs a proof verb, lets the
// processes settle, reads each process's working set and private (committed) bytes, and closes them.
// The lane stays held between variants, so a measurement queues once. Numbers print as a table and,
// with --out, as JSON.
//
// Working set depends on memory pressure (Windows trims it when free RAM is short); private bytes
// do not, so both are reported. The pool's reserve (tools/pool.json "sessionReserve") comes from the
// working-set numbers measured here at idle plus the proof.

import { spawnSync } from "node:child_process";
import { writeFileSync } from "node:fs";
import { freemem } from "node:os";
import { PROFILES, LEVERS, PROFILE_LEVERS } from "../../agent-box/session-profile.mjs";

// "full", "lowspec", or "tex+res" (levers joined with +): the variant's profile name and lever list.
// "full/2" runs that variant with 2 clients only (without it, every --clients count).
export function parseVariant(text) {
  const m = /^(.*?)(?:\/(\d))?$/.exec(String(text).trim());
  const t = m[1];
  const only = m[2] ? Number(m[2]) : null;
  if (PROFILES.includes(t)) return { name: t, profile: t, levers: [...PROFILE_LEVERS[t]], only };
  const levers = t.split("+").map((s) => s.trim()).filter(Boolean);
  for (const l of levers) if (!LEVERS[l]) throw new Error(`variant '${t}': unknown lever '${l}' (${Object.keys(LEVERS).join(", ")}) or profile (${PROFILES.join(", ")})`);
  return { name: t, profile: "custom", levers, only };
}

// One Get-Process line per pid: "<id> <ws MB> <private MB> <peak ws MB> <cpu s>".
export function parseSample(text) {
  const out = [];
  for (const line of String(text).split(/\r?\n/)) {
    const m = /^(\d+) (\d+) (\d+) (\d+) (\d+)$/.exec(line.trim());
    if (m) out.push({ pid: Number(m[1]), wsMb: Number(m[2]), privMb: Number(m[3]), peakMb: Number(m[4]), cpuS: Number(m[5]) });
  }
  return out;
}

export function sampleProcs(pids, pwsh = "powershell.exe") {
  const ids = pids.filter(Boolean).join(",");
  if (!ids) return [];
  const ps = `Get-Process -Id ${ids} -ErrorAction SilentlyContinue | ForEach-Object { "$($_.Id) $([int]($_.WorkingSet64/1MB)) $([int]($_.PrivateMemorySize64/1MB)) $([int]($_.PeakWorkingSet64/1MB)) $([int]$_.CPU)" }`;
  const r = spawnSync(pwsh, ["-NoProfile", "-NonInteractive", "-Command", ps], { encoding: "utf8", windowsHide: true });
  return parseSample(r.stdout ?? "");
}

// rows: [{ variant, clients, serverWs, clientWs: [..], ... }] as one aligned table.
export function formatTable(rows) {
  const head = ["variant", "clients", "server ws", "server priv", "client ws (each)", "client priv (each)", "total ws", "free MB at sample", "proof"];
  const lines = [head.join(" | ")];
  for (const r of rows) {
    lines.push([r.variant, r.clients, r.serverWs, r.serverPriv, r.clientWs.join("/") || "-", r.clientPriv.join("/") || "-", r.totalWs, r.freeMb, r.proof ?? "-"].join(" | "));
  }
  return lines.join("\n");
}

// The per-profile reserve this implies: server MB, and MB per extra client (the 2-client total minus
// the 1-client total when both were measured, else the mean client working set).
export function reserveFrom(rows, variant) {
  const mine = rows.filter((r) => r.variant === variant);
  const one = mine.find((r) => r.clients === 1);
  const two = mine.find((r) => r.clients === 2);
  const any = one ?? two;
  if (!any) return null;
  const serverMb = any.serverWs;
  const mean = (a) => (a.length ? a.reduce((x, y) => x + y, 0) / a.length : 0);
  const clientMb = one && two ? two.totalWs - one.totalWs : Math.round(mean(any.clientWs));
  return { serverMb, clientMb };
}

export function saveJson(path, rows) {
  writeFileSync(path, JSON.stringify({ at: new Date().toISOString(), rows }, null, 1));
}

export function freeMb() {
  return Math.round(freemem() / 1048576);
}
