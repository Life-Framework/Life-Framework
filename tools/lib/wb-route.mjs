// tools/lib/wb-route.mjs - may a headless Workbench start on THIS machine, and where does a Workbench
// step go when it may not?
//
// The maintainer's editor lives on this PC (Gaming-Box) and agents never run a headless Workbench on it
// (2026-11-14 decision). That is "workbench" missing from this machine's caps in the pool config
// (lf.local.json "pool": machines: [{ "name": "local", "caps": ["server"] }]). Every path that launches
// a Workbench here asks this module first:
//   localWorkbench(root)      { hasCap, forced, allowed, host }. Allowed = the local machine's caps
//                             include "workbench", or LF_WB_LOCAL=1 (what `--local` on build, ci, rdb,
//                             look, pack and the like sets: an explicit ask from a human).
//   requireLocalWorkbench     throws LocalWorkbenchRefused (a clear message) when it may not.
//   acquireWorkbench[Sync]    the machine's Workbench slot (tools/lib/wb-lock.mjs) behind that check. The
//                             only importer of wb-lock's acquire functions (validate-wb-policy.mjs).
//   routeWorkbench            run a step here when allowed, else on a pool machine with the cap (chosen
//                             like `ci` chooses: live free RAM, speed, Workbench slot), waiting for one
//                             within a budget (LF_WB_RAM_WAIT_S, 1 h) with one line saying why.
// On a pool machine the "local" entry is that machine's own (pool.loadMachines swaps in the entry whose
// hostname is this host), so lf-cloud and lf-testbox keep their cap and run what is routed to them.
// The pure choice is chooseGate/chooseRefresh in tools/lib/wb-gate.mjs; tools/validation/wb-route-selftest.mjs
// proves this module's policy and routing.

import os from "node:os";
import * as pool from "../remote/pool.mjs";
import * as wbGate from "./wb-gate.mjs";
import * as wbLock from "./wb-lock.mjs";

export const FORCE_ENV = "LF_WB_LOCAL";

export class LocalWorkbenchRefused extends Error {
  constructor(message) {
    super(message);
    this.name = "LocalWorkbenchRefused";
    this.code = "LF_NO_LOCAL_WB";
  }
}

// The local machine's entry of a registry (pool.loadMachines), or undefined.
export function localMachine(machines) {
  return (machines ?? []).find((m) => m.via === "local");
}

// Pure: { hasCap, forced, allowed, host }. No local entry at all (a registry without one) keeps the old
// behaviour: allowed.
export function localPolicy({ machines, env = process.env, host = os.hostname() } = {}) {
  const mine = localMachine(machines);
  const hasCap = !mine ? true : (mine.caps ?? []).includes("workbench");
  const forced = env[FORCE_ENV] === "1";
  return { hasCap, forced, allowed: hasCap || forced, host };
}

export function localWorkbench(root, { machines, env = process.env } = {}) {
  return localPolicy({ machines: machines ?? pool.loadMachines(root), env });
}

export function refusalText(purpose, host = os.hostname()) {
  return `${purpose}: this PC (${host}) has no "workbench" cap in its pool config, so no headless Workbench starts here (the maintainer's editor lives on it; agents never run one). It runs on a pool machine with the cap (tools\\cli pool status). A human who really wants it here passes --local or sets ${FORCE_ENV}=1.`;
}

export function requireLocalWorkbench(root, purpose, opts = {}) {
  const p = localWorkbench(root, opts);
  if (!p.allowed) throw new LocalWorkbenchRefused(refusalText(purpose, p.host));
  return p;
}

// wb-lock's slot, behind the check. A caller that can route does so first (routeWorkbench); this is the
// backstop that makes a missed caller fail loudly instead of starting Workbench.
export async function acquireWorkbench(root, opts = {}) {
  requireLocalWorkbench(root, opts.purpose ?? "workbench");
  return wbLock.acquireWorkbench(root, opts);
}

export function acquireWorkbenchSync(root, opts = {}) {
  requireLocalWorkbench(root, opts.purpose ?? "workbench");
  return wbLock.acquireWorkbenchSync(root, opts);
}

// The live view of every machine with the workbench cap, for wbGate.chooseGate / chooseRefresh: this PC's
// Workbench slots and free RAM, each pool machine's over one ssh round trip (null when it does not answer).
// local.hasWb is the policy above, so a PC without the cap is never chosen.
export function workbenchMachines(root, { mode = "any", machine = null } = {}, { loadMachines = pool.loadMachines, remoteState = wbGate.remoteWbState, env = process.env } = {}) {
  const all = loadMachines(root).filter((m) => !m.disabled);
  const machines = all.filter((m) => (m.caps ?? []).includes("workbench"));
  const limit = wbLock.wbLimit(root);
  const localM = localMachine(all);
  const pol = localPolicy({ machines: all, env });
  const local = { busy: wbLock.holders(root, limit).filter((h) => h.alive).length, queued: wbLock.liveTickets(root).length, limit, freeMb: Math.round(os.freemem() / 1048576), multiplier: localM?.budgetMultiplier ?? 1, hasWb: pol.allowed };
  const remotes = mode === "local" || machine === "local" ? [] : machines
    .filter((m) => m.via === "ssh" && (!machine || m.name === machine))
    .map((m) => ({ name: m.name, caps: m.caps, multiplier: m.budgetMultiplier ?? 1, m, state: remoteState(m) }));
  return { local, remotes };
}

const sleepSync = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);

export function waitBudgetS(env = process.env) {
  return Number(env.LF_WB_RAM_WAIT_S) > 0 ? Number(env.LF_WB_RAM_WAIT_S) : 3600;
}

// Picks a pool machine for a Workbench step that may not run here, waiting (re-checked every pollMs) up to
// budgetS. { machine, why } on success, { error } when none turned up. `choose` is injectable for tests:
// () => ({ c: chooseGate result, remotes }).
export async function pickWorkbenchMachine(root, { purpose, budgetS = waitBudgetS(), pollMs = 30000, pin = null, exclude = [], log = console.log, choose, sleep = (ms) => new Promise((r) => setTimeout(r, ms)), now = Date.now } = {}) {
  const settings = pool.poolSettings(root);
  const pick = choose ?? (() => {
    const view = workbenchMachines(root, { mode: "remote", machine: pin });
    return { remotes: view.remotes, c: wbGate.chooseGate({ ...view, mode: "remote", machine: pin, exclude, minFreeMb: settings.minFreeMb, roomyMb: settings.roomyMb }) };
  });
  const t0 = now();
  let cur = pick();
  if (cur.c.where === "wait") log(`${purpose}: waiting for a pool machine with a Workbench: ${cur.c.why}; re-checking every ${Math.round(pollMs / 1000)} s for up to ${Math.round(budgetS / 60)} min`);
  while (cur.c.where === "wait" && now() - t0 < budgetS * 1000) {
    await sleep(pollMs);
    cur = pick();
  }
  if (cur.c.where === "wait" || cur.c.where === "none") return { error: `${purpose}: no pool machine could take a Workbench${cur.c.where === "wait" ? ` within ${Math.round((now() - t0) / 1000)} s` : ""} (${cur.c.why})` };
  if (cur.c.where === "local") return { where: "local", why: cur.c.why };
  const r = cur.remotes.find((x) => x.name === cur.c.where);
  return { machine: r?.m ?? null, name: cur.c.where, why: cur.c.why, queue: !!cur.c.queue };
}

// Runs a Workbench step where it may run. Allowed here: runLocal(). Else on a pool machine
// (pool.syncAndRun carries uncommitted state, like a remote test): { routed: true, code, machine, rDir }.
// code 3 = no machine turned up or the run folder could not be made (an environment failure, not the step's).
export async function routeWorkbench(root, { purpose, suffix, args, script = "tools\\cli.mjs", pin = null, budgetS, log = console.log, runLocal, deps = {} }) {
  const policy = localWorkbench(root, deps);
  if (policy.allowed) return { routed: false, code: await runLocal() };
  const p = await pickWorkbenchMachine(root, { purpose, budgetS, pin, log, ...deps.pick });
  if (p.error || !p.machine) {
    log(`FAIL  ${p.error ?? `${purpose}: no machine to run on`}`);
    return { routed: true, code: 3 };
  }
  log(`${purpose}: this PC has no workbench cap; running on ${p.name}: ${p.why}`);
  const t0 = Date.now();
  const run = deps.syncAndRun ?? pool.syncAndRun;
  const r = await run(root, p.machine, { script, args, suffix, purpose });
  log(`${purpose}: ${p.name} finished (exit ${r.code}) in ${Math.round((Date.now() - t0) / 1000)} s`);
  return { routed: true, code: r.code, machine: p.machine, rDir: r.rDir };
}
