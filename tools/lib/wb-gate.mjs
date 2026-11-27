// tools/lib/wb-gate.mjs - where `ci` runs its Workbench steps (the validate-scripts compile and the
// build). Every machine with the `workbench` cap (this PC and the reachable pool machines) is ranked
// the way the pool ranks machines for tests (pool-queue.rankMachines): live free RAM less a Workbench
// reserve, weighted by speed (budgetMultiplier). A machine whose headroom after the reserve is under
// the RAM floor (minFreeMb) is never chosen while another has room; a full Workbench limit
// (tools/lib/wb-lock.mjs, workbenchLimit per machine) means "queue there". Nobody has the RAM: wait.
// --local forces this PC; --remote [name] / --machine <name> pin (a pin is honoured even when short;
// agents do not pin, a human or the lead can). A remote that does not answer falls back to this PC.
// The remote run goes through the pool's run-folder sync (pool.syncAndRun), which carries
// uncommitted state. chooseGate and earlyDeath are pure; tools/validation/wb-gate-selftest.mjs
// proves them.

import * as tb from "../remote/testbox.mjs";
import { PS_GPU_PROBE, parseProbe, verdict } from "./gpu-session.mjs";
import { DEFAULTS, rankMachines } from "./pool-queue.mjs";
import { SEED_PEAKS_MB } from "./pool-ram.mjs";

// One headless Workbench compile, build or rdb refresh at its peak: 3.7 GB measured on the laptop
// 2026-11-16 (2.25 GB before; it died after 6 s with 1.5 GB free on 2026-11-14). The floor (minFreeMb)
// comes on top: need = reserve + floor. The number is the pool's seed in pool-ram.mjs, which the pool
// gate counts for every running Workbench too.
export const WB_RESERVE_MB = SEED_PEAKS_MB["workbench:small"];
// A Workbench step that ends this fast without a verdict did not compile anything.
export const EARLY_DEATH_S = 60;
// Exit code of `ci --wb-only` when the Workbench died early on a machine short of RAM: an
// environment failure, retried on another machine once, never counted as a compile failure.
export const ENV_SHORT_CODE = 4;

// local: { busy, queued, limit, freeMb, multiplier, hasWb } (this PC; freeMb null skips the RAM rule).
// hasWb false = this PC's pool caps lack "workbench" (the maintainer's editor lives there, agents never
// start a headless Workbench on it): it is never a candidate and no fallback lands on it, so "nobody
// answers" is "wait" (the caller re-checks within a budget), never "run here". Only an explicit
// `mode: "local"` / `machine: "local"` (--local from a human, which sets hasWb true via LF_WB_LOCAL)
// still returns it. Unset hasWb means true, as before.
// remotes: [{ name, caps, multiplier, state: { freeMb, wbAlive, wbLimit, gpu?: { ok, line } } | null }]. A machine whose
// state.gpu.ok is false (nobody signed in to its desktop, or the session disconnected: ssh has no display output, so
// no Workbench renders there, tools/lib/gpu-session.mjs) is never a candidate. A pin on one, or every Workbench machine
// that way with no cap here to fall back on, is { where: "none", why: <the one line> } at once, never a wait.
// opts: mode "any" | "local" | "remote" (remote: pool machines only, this PC only when none answers),
// machine (a pin), exclude (names already tried), reserveMb, minFreeMb, roomyMb.
// Returns { where: "local" | <name> | "wait" | "none" (nothing left to try), why, queue?: true }.
export function chooseGate({ local, remotes = [], mode = "any", machine = null, exclude = [], reserveMb = WB_RESERVE_MB, minFreeMb = DEFAULTS.minFreeMb, roomyMb = DEFAULTS.roomyMb }) {
  const gb = (mb) => (mb / 1024).toFixed(1);
  const localOk = local?.hasWb !== false;
  if (mode === "local" || machine === "local") return { where: "local", why: mode === "local" ? "--local" : "pinned to this PC" };
  const wb = remotes.filter((r) => (r.caps ?? []).includes("workbench") && (!machine || r.name === machine));
  const answered = wb.filter((r) => r.state);
  const gpuBad = answered.filter((r) => r.state.gpu?.ok === false);
  const reachable = answered.filter((r) => r.state.gpu?.ok !== false);
  if (machine) {
    const pinned = answered[0];
    if (pinned?.state.gpu?.ok === false) return { where: "none", why: pinned.state.gpu.line };
    const pick = reachable[0];
    if (pick) return { where: pick.name, why: `pinned (--remote/--machine ${machine}); ${pick.state.freeMb ?? "?"} MB free there` };
    if (!localOk) return { where: "wait", why: `${machine} with the workbench cap does not answer, and this PC has no workbench cap to fall back on` };
    return { where: "local", why: `${machine} with the workbench cap does not answer; running here` };
  }
  const cands = [];
  if (mode !== "remote" && !exclude.includes("local") && localOk) {
    cands.push({ name: "local", freeMb: local.freeMb, multiplier: local.multiplier, slot: local.busy + local.queued < local.limit, slotText: `${local.busy}/${local.limit} busy, ${local.queued} queued` });
  }
  for (const r of reachable) {
    if (exclude.includes(r.name)) continue;
    cands.push({ name: r.name, freeMb: r.state.freeMb, multiplier: r.multiplier, slot: (r.state.wbAlive ?? 0) < (r.state.wbLimit ?? 1), slotText: `${r.state.wbAlive ?? 0}/${r.state.wbLimit ?? 1} busy` });
  }
  if (!cands.length) {
    if (gpuBad.length && !localOk) return { where: "none", why: gpuBad.map((r) => r.state.gpu.line).join("; ") };
    if (!localOk) {
      if (exclude.filter((n) => n !== "local").length) return { where: "none", why: `no other machine with the workbench cap answers (tried ${exclude.join(", ")}), and this PC has no workbench cap` };
      return { where: "wait", why: "no pool machine with the workbench cap answers, and this PC has no workbench cap to run it on" };
    }
    if (!exclude.includes("local")) return { where: "local", why: "no pool machine with the workbench cap answers; running here" };
    return { where: "none", why: `no other machine with the workbench cap answers (tried ${exclude.join(", ")})` };
  }
  const rows = rankMachines(cands, { minFreeMb, reserveMb, roomyMb });
  const byName = new Map(cands.map((c) => [c.name, c]));
  const label = (n) => (n === "local" ? "this PC" : n);
  const ramText = (r) => (r.headroomMb === null ? "free RAM unknown" : `${gb(byName.get(r.name).freeMb)} GB free`);
  const shortText = rows.filter((r) => r.room === false).map((r) => `${label(r.name)} ${ramText(r)}`).join(", ");
  const skipped = shortText ? `; skipped (short of ${gb(reserveMb + minFreeMb)} GB): ${shortText}` : "";
  // RAM room (or unknown RAM: a remote that answers ssh but not the RAM read) and a free slot.
  const ready = rows.find((r) => r.room !== false && byName.get(r.name).slot);
  if (ready) return { where: ready.name, why: `${label(ready.name)} has a free Workbench slot (${byName.get(ready.name).slotText}) and ${ramText(ready)}${skipped}` };
  // RAM room but every such Workbench slot busy: queue on the best of those (its Workbench lock queues FIFO).
  const roomy = rows.find((r) => r.room === true);
  if (roomy) return { where: roomy.name, queue: true, why: `${label(roomy.name)} has ${ramText(roomy)} but its Workbench slots are busy (${byName.get(roomy.name).slotText}); queuing there${skipped}` };
  return { where: "wait", why: `no machine has ${gb(reserveMb + minFreeMb)} GB free for a Workbench (${rows.map((r) => `${label(r.name)} ${ramText(r)}`).join(", ")})` };
}

// A Workbench step that gave no verdict within EARLY_DEATH_S on a machine short of RAM is an
// environment failure (the 2026-11-14 ship: exit after 6 s with 1.5 GB free), not a compile failure.
export function earlyDeath({ status, seconds, text, freeMb, reserveMb = WB_RESERVE_MB, minFreeMb = DEFAULTS.minFreeMb }) {
  if (status === 0) return null;
  if (!(seconds < EARLY_DEATH_S)) return null;
  if (!/NO VERDICT/.test(String(text ?? ""))) return null;
  if (!Number.isFinite(freeMb) || freeMb >= reserveMb + minFreeMb) return null;
  return `Workbench exited after ${Math.round(seconds)} s with no verdict and ${(freeMb / 1024).toFixed(1)} GB free (needs ${((reserveMb + minFreeMb) / 1024).toFixed(1)})`;
}

// Where a stale resourceDatabase.rdb is refreshed (a headless Workbench, ~15 s per addon, but it queued
// 10-50 min behind the hub `build` lock and this PC's one Workbench slot). Built on chooseGate's rule
// (free Workbench slot AND RAM for a Workbench, ranked by free RAM and speed): this PC when it has a
// free slot, no live `build` lock (local.buildLocked) and the RAM; else a pool machine that chooseGate
// finds with a free slot and the RAM (never one it would only queue on); else this PC again, which
// waits for its slot as before. Only a machine with the `rdb` cap (needCap) is a candidate: its refresh
// is known to register resources (lf-cloud's Workbench exits 0 and registers none, 2026-11-14). mode
// "auto" (default), "local", or "pool" (a pool machine first even when this PC is free). Pure;
// tools/validation/rdb-refresh-where-selftest.mjs proves it.
// A PC without the workbench cap (local.hasWb false, tools/lib/wb-route.mjs) never refreshes: the rdb goes
// to the best pool machine with the rdb cap even when it can only queue there (its Workbench lock queues
// FIFO), and with none reachable the answer is "wait" (the caller re-checks within a budget). The one way
// to a local refresh is mode "local", which a human reaches with `rdb --local` or LF_WB_LOCAL=1 (hasWb true).
export function chooseRefresh({ local, remotes = [], mode = "auto", needCap = "rdb", reserveMb = WB_RESERVE_MB, minFreeMb = DEFAULTS.minFreeMb, roomyMb = DEFAULTS.roomyMb }) {
  if (mode === "local" && local?.hasWb !== false) return { where: "local", why: "LF_RDB_REFRESH_WHERE=local" };
  remotes = remotes.filter((r) => (r.caps ?? []).includes(needCap));
  const need = reserveMb + minFreeMb;
  if (local?.hasWb === false) {
    const c = chooseGate({ local, remotes, mode: "remote", reserveMb, minFreeMb, roomyMb });
    const nocap = "this PC has no workbench cap, so the refresh goes to a pool machine with the rdb cap";
    if (c.where !== "local" && c.where !== "wait" && c.where !== "none") return { where: c.where, queue: c.queue, why: `${nocap}; ${c.why}` };
    return { where: "wait", why: `${nocap}, and none is available (${c.why})` };
  }
  const slotFree = !local.buildLocked && local.busy + local.queued < local.limit;
  const ramOk = local.freeMb == null || local.freeMb >= need;
  if (mode === "auto" && slotFree && ramOk) return { where: "local", why: `this PC has a free Workbench slot (${local.busy}/${local.limit} busy) and ${local.freeMb ?? "?"} MB free` };
  const c = chooseGate({ local, remotes, mode: "remote", reserveMb, minFreeMb, roomyMb });
  if (c.where !== "local" && c.where !== "wait" && c.where !== "none" && !c.queue) {
    const here = local.buildLocked ? "this PC's build lock is held" : !slotFree ? `this PC is busy (${local.busy}/${local.limit}, ${local.queued} queued)` : !ramOk ? `this PC has ${local.freeMb} MB free (needs ${need})` : "asked for a pool machine";
    return { where: c.where, why: `${here}; ${c.why}` };
  }
  return { where: "local", why: `no pool machine has a free Workbench slot and the RAM (${c.why}); refreshing here` };
}

// One ssh round trip: the box's live Workbench locks and free RAM. null when it does not answer.
export function remoteWbState(m) {
  const ps = [
    PS_GPU_PROBE,
    `$ld = Join-Path ${tb.psQuote(m.root)} 'tmp\\wt\\locks'`,
    "$n = 0; foreach ($f in @(Get-ChildItem -LiteralPath $ld -Filter 'workbench-*.lock' -File -ErrorAction SilentlyContinue)) { try { $i = [IO.File]::ReadAllText($f.FullName) | ConvertFrom-Json; if (Get-Process -Id $i.pid -ErrorAction SilentlyContinue) { $n++ } } catch {} }",
    "$os = Get-CimInstance Win32_OperatingSystem",
    "Write-Host \"lf-wb: $n $([int]($os.FreePhysicalMemory / 1024))\"",
    // The resident look Workbench (tools/visual/look-serve.mjs): its pid and the seconds since its last request.
    `$ls = Join-Path ${tb.psQuote(m.root)} 'tmp\\wt\\look-serve.json'`,
    "if (Test-Path -LiteralPath $ls) { try { $j = [IO.File]::ReadAllText($ls) | ConvertFrom-Json; if (Get-Process -Id $j.pid -ErrorAction SilentlyContinue) { $idle = [int]((Get-Date).ToUniversalTime() - ([datetime]$j.lastUsed).ToUniversalTime()).TotalSeconds; Write-Host \"lf-look: $($j.pid) $idle\" } } catch {} }",
  ].join("\n");
  const r = tb.sshPs(m.host, ps, { timeout: 15000, connectTimeout: 4 });
  if (r.status !== 0) return null;
  return parseWbState(r.stdout ?? "", m.workbenchLimit, m.name);
}

export function parseWbState(text, limit, name = "this machine") {
  const mm = /lf-wb: (\d+) (\d+)/.exec(String(text));
  if (!mm) return null;
  const lk = /lf-look: (\d+) (-?\d+)/.exec(String(text));
  const probe = parseProbe(text);
  const v = probe ? verdict(probe, name) : null;
  return { ...(v ? { gpu: { ok: v.ok, line: v.line, note: v.note } } : {}), wbAlive: Number(mm[1]), freeMb: Number(mm[2]), wbLimit: Number(limit) > 0 ? Number(limit) : 1, ...(lk ? { lookServer: { pid: Number(lk[1]), idleS: Math.max(0, Number(lk[2])) } } : {}) };
}
