// tools/lib/look-job.mjs - the pure parts of a look job: phase timing, the warm look server's idle and RAM
// rules, and the mirror decision. No I/O; tools/validation/look-job-selftest.mjs proves it.
//
// The warm look server (tools/visual/look-serve.mjs) is one resident Workbench per pool machine. Its RAM is
// not accounted a second way: tools/lib/pool-ram.mjs already scans every Workbench process on the machine
// (admission counts it through free RAM and what it still grows into), and a look's own start is a
// "workbench" job at that module's measured peak. This file only decides *when the resident one is kept*.

import { admit, jobReserveMb, unrealisedMb } from "./pool-ram.mjs";

// Idle minutes before the resident Workbench closes itself (docs/worktrees.md "sessions flow like a river":
// a lane is held only while it is used). The plugin's own idle limit is the backstop (SERVE_IDLE_MS).
export const LOOK_IDLE_MIN = 5;

// ---------------------------------------------------------------- phase timing

// Seconds per named phase, in the order they were added. add() sums a phase that happens twice.
export function timeline() {
  const order = [];
  const secs = {};
  const t = {
    add(name, s) {
      if (!Number.isFinite(s) || s < 0) return t;
      if (!(name in secs)) order.push(name);
      secs[name] = (secs[name] ?? 0) + s;
      return t;
    },
    // Runs fn and adds its wall-clock to `name`.
    async span(name, fn, now = Date.now) {
      const t0 = now();
      try {
        return await fn();
      } finally {
        t.add(name, (now() - t0) / 1000);
      }
    },
    get: () => Object.fromEntries(order.map((k) => [k, Math.round(secs[k] * 10) / 10])),
  };
  return t;
}

// "queue 217.0 s, sync 5.1 s" from { queue: 217, sync: 5.1 }.
export function phaseText(phases) {
  const e = Object.entries(phases ?? {}).filter(([, v]) => Number.isFinite(v));
  return e.length ? e.map(([k, v]) => `${k} ${v.toFixed(1)} s`).join(", ") : "no phases";
}

// ---------------------------------------------------------------- the resident server's lifetime

// Whether the resident look Workbench should close now.
//   lastUsedMs   its last request (or its start)
//   starved      a job of the pool is short of RAM on this machine (admission said "queue")
//   busy         a request is in flight: never closed under it
// Closes right away when a job is starved, else after LOOK_IDLE_MIN minutes without a request.
export function idleVerdict({ lastUsedMs, now = Date.now(), idleMin = LOOK_IDLE_MIN, starved = false, busy = false }) {
  if (busy) return { close: false, why: "a request is running" };
  if (starved) return { close: true, why: "a job is short of RAM here; the resident Workbench closes at once" };
  const idleMs = now - lastUsedMs;
  if (Number.isFinite(idleMs) && idleMs >= idleMin * 60000) return { close: true, why: `idle ${(idleMs / 60000).toFixed(1)} min (limit ${idleMin})` };
  return { close: false, why: "in use" };
}

// ---------------------------------------------------------------- RAM, on the pool's admission

// The RAM verdict for starting the look Workbench (a fresh look, or the server's start or restart) on a
// machine, by the pool's own admission (pool-ram.admit): the measured Workbench peak, what the running
// processes there still grow into, the floor on top. A resident look Workbench that this start replaces
// gives its working set back first (it stops before the new one starts), so it is not counted against it.
//   state { freeMb, procs }   the machine's live reading (pool.remoteState, or the local scan)
//   residentPid               the look server's Workbench pid, when one is up
export function lookStartVerdict({ state, settings, peaks = {}, residentPid = null, world }) {
  const procs = state?.procs ?? null;
  const resident = residentPid ? (procs ?? []).find((p) => p.pid === residentPid) : null;
  const others = procs ? procs.filter((p) => p !== resident) : null;
  const freeMb = Number(state?.freeMb) + (resident ? Number(resident.wsMb) || 0 : 0);
  return admit({
    freeMb: Number.isFinite(freeMb) ? freeMb : NaN,
    minFreeMb: settings.minFreeMb,
    reserveMb: jobReserveMb(settings, peaks, { kind: "workbench", world }),
    growthMb: others ? unrealisedMb(others, peaks) : 0,
    what: "the look Workbench",
  });
}

// A request to a live resident server needs no new RAM: it is already counted as a running process.
export const requestNeedsRam = (serverLive) => !serverLive;

// ---------------------------------------------------------------- the mirror

// How the resident Workbench takes another worktree's files. `changes` is `git diff --name-status` between
// the snapshot it shows and the requested one, [{ status, path }]. The mirror is the viewer checkout on the
// machine; what Workbench has open decides what may be changed under it.
//   none     the same files: shoot
//   reopen   only files Workbench never loads changed, or only files of `reopenExt` (modified in place):
//            write them, reopen the world
//   restart  anything else under addons/ (added, deleted, renamed, a script, a .meta): new Workbench
export function mirrorMode({ changes = [], reopenExt = [], pluginChanged = false, hasServer = true }) {
  if (!hasServer) return { reload: "restart", reason: "no server" };
  if (pluginChanged) return { reload: "restart", reason: "the look plugin changed" };
  if (!changes.length) return { reload: "none", reason: "same files" };
  let addons = 0;
  for (const c of changes) {
    if (!c.path.startsWith("addons/")) continue;
    addons++;
    const ext = (c.path.match(/\.[^./]+$/)?.[0] ?? "").toLowerCase();
    if (c.status !== "M" || !reopenExt.includes(ext)) return { reload: "restart", reason: `${c.status} ${c.path}` };
  }
  return { reload: "reopen", reason: addons ? `${addons} addon file(s) changed in place` : "only files outside addons/ changed" };
}
