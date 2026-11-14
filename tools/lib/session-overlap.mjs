// Client 1 launches WITH the server, not after it (tools/cli.mjs sessionLaunch).
//
// Measured 2026-11-14 on lf-cloud (docs/worktrees.md "Session boot time"): the server is ready 10-15 s
// after its launch; a client needs 28-32 s from its launch to its connect attempt (render device, shader
// reload, 16 s compiling Game scripts), so the client was the critical path and the 10-15 s of server
// boot was dead time in front of it. Starting both at once hides the server boot behind the client's own.
//
// Nothing is shared between sessions: every session still boots its own server on this worktree's
// scripts, so a claimed session can never run stale code. The lane lock, the RAM reserve and the idle
// reaping are untouched (the client starts after the lane moved to the server's pid).
//
// The risk is a client that reaches its connect attempt before the server listens (a fast client on a
// slow server). firstClientVerdict() watches the client's own log and says "retry" for that case, so the
// worst case is the old serial time plus a grace period, never a failed session.

// What the client log says (console.log of the client profile).
export const CONNECT_TRY = /Starting RPL client/;
export const CONNECTED = /Client L2 connection established/;

// A client that tried to connect and is still unconnected this long after the later of "the server was
// ready" and "the attempt was seen" never will connect: retry it.
export const GRACE_MS = 20000;
// A client that has not even tried to connect this long after its launch (and the server is ready) is
// stuck: retry it. Laptop and cloud clients take 30-60 s to get there, so this is generous.
export const HARD_MS = 150000;

// Whether client 1 starts with the server. LF_SESSION_SERIAL=1 restores the old order (the A/B switch
// the benchmark uses, and a way out if a machine misbehaves).
export function overlapPlan({ clients, env = process.env }) {
  if (env.LF_SESSION_SERIAL === "1") return { early: false, reason: "LF_SESSION_SERIAL=1" };
  if (!(clients > 0)) return { early: false, reason: "no clients" };
  return { early: true, reason: "client 1 starts with the server" };
}

// One look at client 1 while the server finishes booting and the client connects. Times are ms epoch.
//   serverReadyAt: when the server log said it was ready, or 0 while it is still booting
//   launchedAt:    when the client was launched
//   attemptSeenAt: when its log first showed a connect attempt, or 0
//   connected:     its log shows the connection established
//   alive:         its process is running
//   retried:       it was already relaunched once
// Returns "connected" | "wait" | "retry" | "stuck". "stuck" means do not wait further and do not relaunch:
// the normal join check reports the failure with its own reason.
export function firstClientVerdict({ now, serverReadyAt, launchedAt, attemptSeenAt = 0, connected = false, alive = true, retried = false, graceMs = GRACE_MS, hardMs = HARD_MS }) {
  if (connected) return "connected";
  const again = retried ? "stuck" : "retry";
  // A dead client is relaunched once, whatever the server is doing (it started before the server was
  // ready, and a launch that failed on that race leaves nothing to wait for).
  if (!alive) return again;
  // Until the server is ready nothing can be judged: a connect attempt before it is the race itself,
  // and the verdict falls out of the grace period below once the server is up.
  if (!serverReadyAt) return "wait";
  if (attemptSeenAt && now - Math.max(serverReadyAt, attemptSeenAt) > graceMs) return again;
  if (!attemptSeenAt && now - Math.max(serverReadyAt, launchedAt) > hardMs) return again;
  return "wait";
}
