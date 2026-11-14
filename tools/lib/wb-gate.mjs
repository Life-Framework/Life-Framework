// tools/lib/wb-gate.mjs - where `ci` runs its Workbench steps (the validate-scripts compile and the
// build): this PC when its Workbench limit has room (tools/lib/wb-lock.mjs), else a pool machine
// with the `workbench` cap, a free Workbench slot and the RAM for one, else this PC (it queues).
// --local forces this PC; --remote [name] / --machine <name> force a pool machine. The remote run
// goes through the pool's run-folder sync (pool.syncAndRun), which carries uncommitted state.
// chooseGate is pure; tools/validation/wb-gate-selftest.mjs proves it.

import * as tb from "../remote/testbox.mjs";

export const GATE_NEED_MB = 3500; // one headless Workbench compile or build at its peak, plus margin

// local: { busy, queued, limit }; remotes: [{ name, caps, state: { freeMb, wbAlive, wbLimit } | null }].
// opts: { mode: "any" | "local" | "remote", machine?, needMb? }. Returns { where: "local" | <name>, why }.
export function chooseGate({ local, remotes = [], mode = "any", machine = null, needMb = GATE_NEED_MB }) {
  if (mode === "local") return { where: "local", why: "--local" };
  const wb = remotes.filter((r) => (r.caps ?? []).includes("workbench") && (!machine || r.name === machine));
  const reachable = wb.filter((r) => r.state);
  const room = (r) => r.state.wbAlive < (r.state.wbLimit ?? 1) && (r.state.freeMb ?? 0) >= needMb;
  const localRoom = local.busy + local.queued < local.limit;
  if (mode === "remote" || machine) {
    const pick = reachable.find(room) ?? reachable[0];
    if (pick) return { where: pick.name, why: room(pick) ? "asked for, has room" : "asked for, queues there" };
    return { where: "local", why: `${machine ?? "no pool machine"} with the workbench cap answers; running here` };
  }
  if (localRoom) return { where: "local", why: `this PC has a free Workbench slot (${local.busy}/${local.limit} busy)` };
  const fits = reachable.filter(room).sort((a, b) => (b.state.freeMb ?? 0) - (a.state.freeMb ?? 0));
  if (fits.length) return { where: fits[0].name, why: `this PC is busy (${local.busy}/${local.limit}, ${local.queued} queued); ${fits[0].name} has a free Workbench slot and ${fits[0].state.freeMb} MB free` };
  return { where: "local", why: `this PC is busy and no pool machine has room; queuing here` };
}

// One ssh round trip: the box's live Workbench locks and free RAM. null when it does not answer.
export function remoteWbState(m) {
  const ps = [
    `$ld = Join-Path ${tb.psQuote(m.root)} 'tmp\\wt\\locks'`,
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
