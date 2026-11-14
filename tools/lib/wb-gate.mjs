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
import { DEFAULTS, rankMachines } from "./pool-queue.mjs";

// One headless Workbench compile or build at its peak (measured about 2 GB; 2026-11-14 it died after
// 6 s with 1.5 GB free). The floor (minFreeMb) comes on top: need = reserve + floor.
export const WB_RESERVE_MB = 2250;
// A Workbench step that ends this fast without a verdict did not compile anything.
export const EARLY_DEATH_S = 60;
// Exit code of `ci --wb-only` when the Workbench died early on a machine short of RAM: an
// environment failure, retried on another machine once, never counted as a compile failure.
export const ENV_SHORT_CODE = 4;

// local: { busy, queued, limit, freeMb, multiplier } (this PC; freeMb null skips the RAM rule).
// remotes: [{ name, caps, multiplier, state: { freeMb, wbAlive, wbLimit } | null }].
// opts: mode "any" | "local" | "remote" (remote: pool machines only, this PC only when none answers),
// machine (a pin), exclude (names already tried), reserveMb, minFreeMb, roomyMb.
// Returns { where: "local" | <name> | "wait" | "none" (nothing left to try), why, queue?: true }.
export function chooseGate({ local, remotes = [], mode = "any", machine = null, exclude = [], reserveMb = WB_RESERVE_MB, minFreeMb = DEFAULTS.minFreeMb, roomyMb = DEFAULTS.roomyMb }) {
  const gb = (mb) => (mb / 1024).toFixed(1);
  if (mode === "local" || machine === "local") return { where: "local", why: mode === "local" ? "--local" : "pinned to this PC" };
  const wb = remotes.filter((r) => (r.caps ?? []).includes("workbench") && (!machine || r.name === machine));
  const reachable = wb.filter((r) => r.state);
  if (machine) {
    const pick = reachable[0];
    if (pick) return { where: pick.name, why: `pinned (--remote/--machine ${machine}); ${pick.state.freeMb ?? "?"} MB free there` };
    return { where: "local", why: `${machine} with the workbench cap does not answer; running here` };
  }
  const cands = [];
  if (mode !== "remote" && !exclude.includes("local")) {
    cands.push({ name: "local", freeMb: local.freeMb, multiplier: local.multiplier, slot: local.busy + local.queued < local.limit, slotText: `${local.busy}/${local.limit} busy, ${local.queued} queued` });
  }
  for (const r of reachable) {
    if (exclude.includes(r.name)) continue;
    cands.push({ name: r.name, freeMb: r.state.freeMb, multiplier: r.multiplier, slot: (r.state.wbAlive ?? 0) < (r.state.wbLimit ?? 1), slotText: `${r.state.wbAlive ?? 0}/${r.state.wbLimit ?? 1} busy` });
  }
  if (!cands.length) {
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

// One ssh round trip: the box's live Workbench locks and free RAM. null when it does not answer.
export function remoteWbState(m) {
  const ps = [
    `$ld = Join-Path ${tb.psQuote(m.root)} 'tmp\wt\locks'`,
    "$n = 0; foreach ($f in @(Get-ChildItem -LiteralPath $ld -Filter 'workbench-*.lock' -File -ErrorAction SilentlyContinue)) { try { $i = [IO.File]::ReadAllText($f.FullName) | ConvertFrom-Json; if (Get-Process -Id $i.pid -ErrorAction SilentlyContinue) { $n++ } } catch {} }",
    "$os = Get-CimInstance Win32_OperatingSystem",
    "Write-Host \"lf-wb: $n $([int]($os.FreePhysicalMemory / 1024))\"",
  ].join("\n");
  const r = tb.sshPs(m.host, ps, { timeout: 15000, connectTimeout: 4 });
  if (r.status !== 0) return null;
  return parseWbState(r.stdout ?? "", m.workbenchLimit);
}

export function parseWbState(text, limit) {
  const mm = /lf-wb: (\d+) (\d+)/.exec(String(text));
  if (!mm) return null;
  return { wbAlive: Number(mm[1]), freeMb: Number(mm[2]), wbLimit: Number(limit) > 0 ? Number(limit) : 1 };
}
