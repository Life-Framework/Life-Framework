// tools/playtest/opening/_scratch.mjs: a helper for the opening-day beats and the `opening` runner, not a beat (a leading
// `_` keeps it out of the glob). It holds the two rules the run keeps breaking (#1691, #1692):
//
//   Host folders. One `--host` cannot suit every beat: `host config apply` and `host password` refuse a folder inside the
//   repo (a secret there could be committed), while B7 and B9 only run on a folder under <repo>/tmp/. So the runner gives
//   each beat a folder of its own, by the beat's `// opening-run: host=<placement>` header (default `outside`):
//     outside  a folder outside the repo          B1, B2, B6, B8
//     tmp      a folder under <repo>/tmp/         B7, B9
//     log      the folder of the latest earlier beat that left a console.log, else `outside`   B11
//   `--host <dir>` is the base: when it already suits the placement the beat's folder is <dir>/<beat> (B1, B7, ...); when it
//   does not, the folder moves to the right side, still named after the base: <repo>/tmp/opening-hosts/<name>/<beat> or
//   <os tmp>/lf-opening-hosts/<name>/<beat>, <name> being the base folder's name. With no --host the name is run-<stamp>.
//   The header may also say `timeout=<seconds>` (the longest the beat may run; --timeout overrides it for every beat).
//
//   Stopping what a beat started. A beat that starts a watcher or a server records the pid (`recordPid`) and calls
//   `reapHost` in a `finally`; the runner calls it again after every script, so a failed, killed or timed-out beat leaves
//   nothing running. `reapHost` works by pid only: the pids the beat recorded plus the pid in the host folder's host.json
//   (what `host start` wrote). It never kills a pid it did not read from one of those two places.
// Proof: tools/validation/opening-fixes-selftest.mjs.

import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { tmpdir } from "node:os";
import { basename, dirname, isAbsolute, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { newestConsoleLog, hostPaths, pidAlive, readHostState } from "../../host/host.mjs";

const REPO = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
export const PIDS_FILE = "opening-pids.json";

// A pid is trusted only for MAX_AGE_H hours after it was written: a pid in an old file may belong to something else by now.
const MAX_AGE_H = 6;
const recent = (at) => !at || !Number.isFinite(Date.parse(at)) || Date.now() - Date.parse(at) < MAX_AGE_H * 3600e3;

const sleepMs = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);

/** `// opening-run: host=tmp timeout=1800` in the first lines of a beat script -> { host, timeout }. */
export function readDirective(file) {
  let head = "";
  try {
    head = readFileSync(file, "utf8").split(/\r?\n/).slice(0, 12).join("\n");
  } catch {
    // an unreadable script fails when it runs
  }
  const m = /^\/\/\s*opening-run:\s*(.+)$/m.exec(head);
  const out = { host: "outside", timeout: 0 };
  if (!m) return out;
  for (const tok of m[1].trim().split(/\s+/)) {
    const [k, v] = tok.split("=");
    if (k === "host" && ["outside", "tmp", "log"].includes(v)) out.host = v;
    else if (k === "timeout" && Number(v) > 0) out.timeout = Number(v);
  }
  return out;
}

const inside = (child, parent) => {
  const r = relative(resolve(parent), resolve(child));
  return !!r && !r.startsWith("..") && !isAbsolute(r);
};

/**
 * The host folder for one beat. root: the repo (its tmp/ is where `tmp` beats live); base: --host or null; name: the run's
 * name (the base folder's name, or run-<stamp>); beat: "B7"; placement: outside | tmp | log. Pure apart from the `log` lookup.
 */
export function beatHostDir({ root, base, name, beat, placement, earlier = [] }) {
  const underTmp = (d) => inside(d, join(root, "tmp"));
  const outside = () => (base && !inside(base, root) && resolve(base) !== resolve(root) ? join(base, beat) : join(tmpdir(), "lf-opening-hosts", name, beat));
  if (placement === "tmp") return base && underTmp(base) ? join(base, beat) : join(root, "tmp", "opening-hosts", name, beat);
  if (placement === "log") {
    for (const d of [...earlier].reverse()) if (newestConsoleLog(hostPaths(d).profile)) return d;
  }
  return outside();
}

export const runName = (base, now = new Date()) => (base ? basename(resolve(base)) : `run-${now.toISOString().replace(/[-:T]/g, "").slice(0, 14)}-${process.pid}`);

// ---- stopping what a beat started ----

const readPids = (host) => {
  try {
    const list = JSON.parse(readFileSync(join(host, PIDS_FILE), "utf8"))?.pids;
    return Array.isArray(list) ? list.filter((p) => Number.isInteger(p?.pid) && p.pid > 0) : [];
  } catch {
    return [];
  }
};

/** Writes down a process the beat started, so `reapHost` can stop it by pid. */
export function recordPid(host, role, pid) {
  if (!pid) return;
  mkdirSync(host, { recursive: true });
  const pids = readPids(host).filter((p) => p.pid !== pid);
  pids.push({ pid, role, at: new Date().toISOString() });
  writeFileSync(join(host, PIDS_FILE), JSON.stringify({ pids }, null, 2));
}

/** Ends one process; `tree` takes its children too. */
export function killPid(pid, { tree = false } = {}) {
  if (!pid || pid === process.pid) return;
  if (process.platform === "win32") spawnSync("taskkill", ["/PID", String(pid), "/F", ...(tree ? ["/T"] : [])], { windowsHide: true, timeout: 30000 });
  else {
    try {
      process.kill(pid, "SIGKILL");
    } catch {
      // already gone
    }
  }
}

/**
 * Stops everything a beat started in `host`: the recorded watchers first (so nothing restarts the server), then the server
 * through `host stop` (the shutdown save) with a forced kill if it stays up. Returns { stopped, leftover }: what was alive
 * when it began and was stopped, and what is still alive after.
 */
export function reapHost({ host, cli = join(REPO, "tools", "cli.mjs"), say = () => {} }) {
  const stopped = [];
  const watchers = readPids(host).filter((p) => recent(p.at) && pidAlive(p.pid));
  for (const p of watchers) {
    killPid(p.pid);
    stopped.push(p);
    say(`stopped ${p.role} (pid ${p.pid})`);
  }
  const found = readHostState(host);
  const st = found && recent(found.startedAt) ? found : null;
  if (st?.pid && pidAlive(st.pid)) {
    spawnSync(process.execPath, [cli, "host", "stop"], { cwd: REPO, env: { ...process.env, LF_HOST_DIR: host, LF_QUIET_CHILD: "1" }, encoding: "utf8", timeout: 240000, windowsHide: true });
    stopped.push({ pid: st.pid, role: "host server" });
    say(`stopped the host server (pid ${st.pid})`);
    if (pidAlive(st.pid)) killPid(st.pid, { tree: true });
  }
  const end = Date.now() + 3000;
  const all = [...watchers, ...(st?.pid ? [{ pid: st.pid, role: "host server" }] : [])];
  while (all.some((p) => pidAlive(p.pid)) && Date.now() < end) sleepMs(100);
  const leftover = all.filter((p) => pidAlive(p.pid));
  if (existsSync(join(host, PIDS_FILE))) writeFileSync(join(host, PIDS_FILE), JSON.stringify({ pids: leftover }, null, 2));
  return { stopped, leftover };
}
