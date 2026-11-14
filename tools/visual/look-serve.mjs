// tools/visual/look-serve.mjs - one long-lived look Workbench for the whole machine.
//
//   tools\cli look serve              start the server on this checkout's state (or say it is up)
//   tools\cli look serve --status     pid, snapshot, idle time, health
//   tools\cli look serve --stop       stop it (--remove also removes the viewer checkout)
//   tools\cli look <area> --serve     use the server, starting it when none is up
//   tools\cli look <area> --fresh     never use the server
//
// The server is a hidden Workbench on a dedicated detached checkout beside the main one
// (Life-Framework-look-viewer) running tools/visual/wb/LF_VisualShotsPlugin.c in server mode.
// Every request carries a snapshot commit of the caller's worktree (HEAD plus uncommitted and
// untracked files, tools/remote/testbox.mjs). The viewer checks it out (only differing files
// are written) and the server shoots it. What changed between the snapshot the server last
// showed and the requested one decides how:
//   identical                                      shoot as is (reload none)
//   only modified, existing .layer / .et files     reopen the world (proven live, docs/visual-testing.md);
//   (files outside addons/ never reach Workbench: a change there alone also reopens)
//   anything else (a .c, .meta, an added, deleted or renamed file, the plugin) a new Workbench on the snapshot
// A script change under a live Workbench crashed it in the probe (0xC0000005, 2 of 2), so a .c
// always restarts. A server that is gone, hung or crashed never fails the caller: look kills
// what is left (and its crash reporter), forgets it, and runs a fresh Workbench as before.

import { copyFileSync, existsSync, lstatSync, mkdirSync, readFileSync, readdirSync, renameSync, rmSync, statSync, writeFileSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { createHash } from "node:crypto";
import { dirname, extname, join } from "node:path";
import { fileURLToPath } from "node:url";
import * as wt from "../wt.mjs";
import { removeWorktree } from "../link-safety.mjs";
import { ADDONS } from "../lib/addons.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const PLUGIN = join(HERE, "wb", "LF_VisualShotsPlugin.c");
export const VIEWER_NAME = "Life-Framework-look-viewer";
// Only extensions proven live: change the file, reopen, read the changed value back from status.txt.
export const FAST_PATH_EXT = [".layer", ".et"];
export const ALIVE_STALE_MS = 15000; // the plugin rewrites alive.txt every 3 s while idle
const BOOT_BUDGET_S = 150;
const STOP_WAIT_MS = 15000;
// Raw files a request owns; serve.txt and alive.txt belong to the server and are never cleared by a request.
export const REQUEST_FILES = /^(plan\.txt|go\.txt|go\.tmp|done\.txt|status\.txt|.*\.png)$/;

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const short = (sha) => String(sha || "").slice(0, 10);

// ------------------------------------------------------------------ pure parts (selftest)

// `git diff --name-status --no-renames A B` -> [{ status, path }]
export function parseNameStatus(text) {
  const out = [];
  for (const line of String(text).split(/\r?\n/)) {
    const m = /^([A-Z])[0-9]*\t(.+)$/.exec(line);
    if (m) out.push({ status: m[1], path: m[2].replace(/\\/g, "/") });
  }
  return out;
}

// How the server must take a request: { reload: none|reopen|restart, reason }.
// resourceDatabase.rdb is not an input: it is Workbench's own cache of the files (-forceUpdate rebuilds it),
// its bytes differ between worktrees at the same commit and size, so comparing it would restart every request.
// What it caches is decided by the files: an added, deleted or renamed file or a .meta restarts anyway.
export function classifyChange({ from, to, changes = [], pluginChanged = false }) {
  if (!from) return { reload: "restart", reason: "no server" };
  if (pluginChanged) return { reload: "restart", reason: "the look plugin changed" };
  if (!changes.length) return from === to ? { reload: "none", reason: "same snapshot" } : { reload: "reopen", reason: "same files, other commit" };
  for (const c of changes) {
    // Workbench loads only addons/ (the gproj and -addonsDir); tools, docs and the rest cannot change a picture.
    // The plugin it runs is copied from tools/ and compared by its hash above. Still a reopen, never "none".
    if (!c.path.startsWith("addons/")) continue;
    if (c.status !== "M") return { reload: "restart", reason: `${c.status} ${c.path}` };
    if (!FAST_PATH_EXT.includes(extname(c.path).toLowerCase())) return { reload: "restart", reason: `M ${c.path}` };
  }
  const inAddons = changes.filter((c) => c.path.startsWith("addons/")).length;
  return { reload: "reopen", reason: inAddons ? `${inAddons} world/prefab file(s) changed` : "only files outside addons/ changed" };
}

// Why a server's answer cannot be used (null when it can): it must be this request's, complete, with the world open.
export function answerProblem(status, id) {
  if (status.request !== id) return `status.txt answers request ${status.request}, not ${id}`;
  if (!status.world) return `the server never opened the world${status.errors.length ? ` (${status.errors.join("; ")})` : ""}`;
  if (!status.done) return "status.txt has no done line";
  return null;
}

export function goText(id, reload) {
  return `${id}\n${reload}\n`;
}

// done.txt answers this request only when its first line is the id.
export function doneMatches(text, id) {
  return String(text).split(/\r?\n/)[0].trim() === String(id);
}

export function aliveFresh(mtimeMs, now, limit = ALIVE_STALE_MS) {
  return typeof mtimeMs === "number" && now - mtimeMs <= limit;
}

// ------------------------------------------------------------------ paths and state

export function viewerPath(root) {
  return join(dirname(wt.mainRootOf(root)), VIEWER_NAME);
}
export const profileOf = (viewer) => join(viewer, "server", "profile", "visual-serve");
export const rawOf = (profile) => join(profile, "profile", "lf-visual");
const stateFile = (root) => join(wt.ensureHub(root), "look-serve.json");

export function readServeState(root) {
  try {
    return JSON.parse(readFileSync(stateFile(root), "utf8"));
  } catch {
    return null;
  }
}
function writeServeState(root, st) {
  const f = stateFile(root);
  writeFileSync(f + ".tmp", JSON.stringify(st, null, 2) + "\n");
  renameSync(f + ".tmp", f);
}
function clearServeState(root) {
  rmSync(stateFile(root), { force: true });
}

const sha1File = (f) => (existsSync(f) ? createHash("sha1").update(readFileSync(f)).digest("hex") : "");
export const pluginHash = () => sha1File(PLUGIN);

// ------------------------------------------------------------------ processes

export function workbenchAlive(pid) {
  if (!pid || !wt.pidAlive(pid)) return false;
  const r = spawnSync("tasklist", ["/FI", `PID eq ${pid}`, "/FO", "CSV", "/NH"], { encoding: "utf8", windowsHide: true, timeout: 10000 });
  if (r.status !== 0) return true;
  return /ArmaReforgerWorkbench/i.test(r.stdout || "");
}

// Children of the server (a CrashReporter after an access violation) outlive it on the hidden desktop.
function childPids(pid) {
  const r = spawnSync("powershell", ["-NoProfile", "-Command", `Get-CimInstance Win32_Process -Filter "ParentProcessId=${Number(pid)}" | ForEach-Object { '{0}|{1}' -f $_.ProcessId, $_.Name }`], { encoding: "utf8", windowsHide: true, timeout: 15000 });
  return (r.stdout || "").split(/\r?\n/).map((l) => l.trim().split("|")).filter((p) => Number(p[0])).map(([id, name]) => ({ pid: Number(id), name }));
}

function killTree(pid) {
  spawnSync("taskkill", ["/PID", String(pid), "/T", "/F"], { windowsHide: true, timeout: 15000 });
}

// Ends the server and everything it left, and forgets it. Safe on a dead pid.
export function killServer(root, st, log = console.log) {
  if (st?.pid) {
    const kids = childPids(st.pid);
    if (wt.pidAlive(st.pid)) killTree(st.pid);
    for (const k of kids) {
      if (wt.pidAlive(k.pid)) {
        killTree(k.pid);
        log(`look serve: closed ${k.name} (pid ${k.pid}) the server left`);
      }
    }
  }
  clearServeState(root);
}

// A server process is there (it may be busy with another caller's request: no heartbeat while it shoots).
// Decides whether a look tries the server at all; serverHealth, under the build lock, decides whether it is sound.
export function serverUp(root) {
  const st = readServeState(root);
  return !!st && workbenchAlive(st.pid);
}

// { state, live, why }: live means the process is a Workbench and its heartbeat is fresh. Only meaningful
// while holding the build lock (no request in flight); the plugin beats right before every done.txt.
export function serverHealth(root, now = Date.now()) {
  const st = readServeState(root);
  if (!st) return { state: null, live: false, why: "no server" };
  if (!workbenchAlive(st.pid)) return { state: st, live: false, why: `process ${st.pid} is gone` };
  const alive = join(rawOf(st.profile), "alive.txt");
  let mtime = null;
  try {
    mtime = statSync(alive).mtimeMs;
  } catch {}
  if (!aliveFresh(mtime, now)) return { state: st, live: false, why: mtime === null ? "no alive.txt" : `alive.txt ${Math.round((now - mtime) / 1000)} s old` };
  return { state: st, live: true, why: "ok" };
}

export async function stopServer(root, st, log = console.log) {
  if (!st) return;
  try {
    writeFileSync(join(rawOf(st.profile), "stop.txt"), "1\n");
  } catch {}
  const t0 = Date.now();
  while (wt.pidAlive(st.pid) && Date.now() - t0 < STOP_WAIT_MS) await sleep(250);
  if (wt.pidAlive(st.pid)) log(`look serve: pid ${st.pid} did not stop in ${STOP_WAIT_MS / 1000} s; killed`);
  killServer(root, st, log);
  rmSync(join(rawOf(st.profile), "stop.txt"), { force: true });
}

// ------------------------------------------------------------------ viewer checkout

function git(cwd, args) {
  return spawnSync("git", ["-C", cwd, ...args], { encoding: "utf8", windowsHide: true, timeout: 120000 });
}
const gitOut = (cwd, args) => (git(cwd, args).stdout || "").trim();

// Brings the viewer to `sha` with nothing else in it. { ok, why }
export function checkoutViewer(root, viewer, sha) {
  if (!existsSync(viewer)) {
    const add = git(wt.mainRootOf(root), ["worktree", "add", "--detach", viewer, sha]);
    if (add.status !== 0) return { ok: false, why: `git worktree add: ${(add.stderr || add.stdout).trim()}` };
  } else {
    const co = git(viewer, ["checkout", "--detach", "--force", "--quiet", sha]);
    if (co.status !== 0) return { ok: false, why: `checkout: ${(co.stderr || co.stdout).trim().split(/\r?\n/)[0]}` };
  }
  // Untracked, non-ignored files would be loaded with the snapshot; the viewer is ours, so they go.
  for (const line of gitOut(viewer, ["status", "--porcelain", "-uall"]).split(/\r?\n/)) {
    if (!line.startsWith("?? ")) continue;
    const f = join(viewer, line.slice(3).replace(/^"|"$/g, ""));
    try {
      if (lstatSync(f).isFile()) rmSync(f, { force: true });
    } catch {}
  }
  return viewerAt(viewer, sha);
}

// The guard before every shot: HEAD is the snapshot and no tracked or untracked file differs from it.
export function viewerAt(viewer, sha) {
  if (!existsSync(viewer)) return { ok: false, why: "no viewer checkout" };
  const head = gitOut(viewer, ["rev-parse", "HEAD"]);
  if (head !== sha) return { ok: false, why: `viewer at ${short(head)}, not ${short(sha)}` };
  const dirty = gitOut(viewer, ["status", "--porcelain", "-uall"]);
  if (dirty) return { ok: false, why: `viewer has local changes: ${dirty.split(/\r?\n/)[0]}` };
  return { ok: true };
}

// The plugin of the caller's look.mjs (the protocol both sides speak), and a first rdb so the first boot
// does not register every resource from nothing. After that the viewer's Workbench keeps its own rdb.
function syncSideFiles(root, viewer) {
  for (const { name } of ADDONS) {
    const src = join(root, "addons", name, "resourceDatabase.rdb");
    const fallback = join(wt.mainRootOf(root), "addons", name, "resourceDatabase.rdb");
    const from = existsSync(src) ? src : fallback;
    const dest = join(viewer, "addons", name, "resourceDatabase.rdb");
    if (existsSync(from) && !existsSync(dest)) {
      mkdirSync(dirname(dest), { recursive: true });
      copyFileSync(from, dest);
    }
  }
  const injected = join(viewer, "addons", "LifeFramework", "Scripts", "WorkbenchGame", "LFVisual");
  mkdirSync(injected, { recursive: true });
  copyFileSync(PLUGIN, join(injected, "LF_VisualShotsPlugin.c"));
}

// ------------------------------------------------------------------ start / request

async function startServer(root, viewer, snap, { exe, workbenchArgs, log }) {
  const { quietSpawnDetached } = await import("../lib/quiet-launch.mjs");
  const profile = profileOf(viewer);
  const raw = rawOf(profile);
  mkdirSync(raw, { recursive: true });
  for (const f of readdirSync(raw)) rmSync(join(raw, f), { force: true });
  writeFileSync(join(raw, "serve.txt"), "1\n");
  syncSideFiles(root, viewer);
  const t0 = Date.now();
  let pid;
  try {
    // Over ssh (a pool machine) there is no desktop to render to and ssh ends its children on disconnect:
    // the server starts on the signed-in user's desktop through an interactive scheduled task, like a remote client.
    const il = await import("../lib/interactive-launch.mjs");
    if (il.interactiveNeeded()) pid = il.interactiveSpawnDetached(exe, workbenchArgs(viewer, profile), { cwd: dirname(exe), marker: profile });
    else pid = quietSpawnDetached(exe, workbenchArgs(viewer, profile), { cwd: dirname(exe) });
  } catch (e) {
    return { ok: false, why: `start failed: ${e.message}` };
  }
  const st = { pid, viewer, profile, sha: snap.sha, plugin: pluginHash(), started: new Date(t0).toISOString(), lastUsed: new Date(t0).toISOString(), by: root };
  writeServeState(root, st);
  const alive = join(raw, "alive.txt");
  while (Date.now() - t0 < BOOT_BUDGET_S * 1000) {
    if (!wt.pidAlive(pid)) break;
    try {
      if (statSync(alive).mtimeMs >= t0) return { ok: true, state: st, bootS: (Date.now() - t0) / 1000 };
    } catch {}
    await sleep(250);
  }
  const why = wt.pidAlive(pid) ? `no heartbeat in ${BOOT_BUDGET_S} s` : "Workbench exited while starting";
  killServer(root, st, log);
  return { ok: false, why, profile };
}

async function sendRequest(st, id, reload, plan, budgetS) {
  const raw = rawOf(st.profile);
  for (const f of readdirSync(raw)) if (REQUEST_FILES.test(f)) rmSync(join(raw, f), { force: true });
  writeFileSync(join(raw, "plan.txt"), plan);
  writeFileSync(join(raw, "go.tmp"), goText(id, reload));
  const t0 = Date.now();
  renameSync(join(raw, "go.tmp"), join(raw, "go.txt"));
  const done = join(raw, "done.txt");
  while (Date.now() - t0 < budgetS * 1000) {
    await sleep(150);
    if (existsSync(done)) {
      let text = "";
      try {
        text = readFileSync(done, "utf8");
      } catch {}
      if (doneMatches(text, id)) {
        rmSync(join(raw, "go.txt"), { force: true }); // the plugin cannot delete it itself
        const statusFile = join(raw, "status.txt");
        return { ok: true, statusText: existsSync(statusFile) ? readFileSync(statusFile, "utf8") : "", seconds: (Date.now() - t0) / 1000, raw };
      }
    }
    if (!wt.pidAlive(st.pid)) return { ok: false, why: `the server (pid ${st.pid}) died during the request` };
  }
  return { ok: false, why: `no answer in ${budgetS} s` };
}

// One request through the server. The caller holds the build lock.
// { ok: true, served, sha, reason, raw, statusText, seconds, id } or { ok: false, why }.
export async function serveLook(root, { plan, budgetS, exe, workbenchArgs, snapshot, allowStart, ramGate, log = console.log, parseStatus }) {
  const snap = snapshot(root);
  const viewer = viewerPath(root);
  const health = serverHealth(root);
  if (!health.live && health.state) {
    log(`look serve: dropping the old server (${health.why})`);
    killServer(root, health.state, log);
  }
  if (!health.live && !allowStart) return { ok: false, why: "no server up (start one with: tools\\cli look serve)" };

  let st = health.live ? health.state : null;
  let decision;
  if (st) {
    const diff = git(root, ["diff", "--name-status", "--no-renames", st.sha, snap.sha]);
    if (diff.status !== 0) decision = { reload: "restart", reason: `cannot diff ${short(st.sha)}..${short(snap.sha)}` };
    else {
      decision = classifyChange({ from: st.sha, to: snap.sha, changes: parseNameStatus(diff.stdout), pluginChanged: pluginHash() !== st.plugin });
    }
  } else decision = classifyChange({ from: null });

  if (decision.reload === "reopen") {
    // The viewer must be exactly the last snapshot before a partial checkout; anything else restarts.
    // The world is closed first (an unload request): files changing under an open world crashed the reopen.
    let co = viewerAt(viewer, st.sha);
    if (co.ok) {
      const un = await sendRequest(st, `${process.pid}-${Date.now()}-unload`, "unload", "", 120);
      co = un.ok ? checkoutViewer(root, viewer, snap.sha) : { ok: false, why: `unload: ${un.why}` };
    }
    if (!co.ok) decision = { reload: "restart", reason: co.why };
  }

  let served = decision.reload === "restart" ? "server-restart" : decision.reload === "reopen" ? "server-reopen" : "server-same";
  let reload = decision.reload;
  if (decision.reload === "restart") {
    if (st) await stopServer(root, st, log);
    st = null;
    const ram = await ramGate();
    if (ram) return { ok: false, why: ram };
    const co = checkoutViewer(root, viewer, snap.sha);
    if (!co.ok) return { ok: false, why: co.why };
    log(`look serve: starting the server Workbench on ${short(snap.sha)} (${decision.reason})`);
    const started = await startServer(root, viewer, snap, { exe, workbenchArgs, log });
    if (!started.ok) return { ok: false, why: started.why };
    log(`look serve: up in ${Math.round(started.bootS)} s (pid ${started.state.pid})`);
    st = started.state;
    reload = "none"; // a new Workbench opens the world from disk
  }

  const at = viewerAt(viewer, snap.sha);
  if (!at.ok) {
    killServer(root, st, log);
    return { ok: false, why: at.why };
  }
  const id = `${process.pid}-${Date.now()}`;
  const res = await sendRequest(st, id, reload, plan, budgetS);
  if (!res.ok) {
    killServer(root, st, log);
    return { ok: false, why: res.why };
  }
  const status = parseStatus(res.statusText);
  const why = answerProblem(status, id);
  if (why) {
    killServer(root, st, log);
    return { ok: false, why };
  }
  writeServeState(root, { ...st, sha: snap.sha, lastUsed: new Date().toISOString(), by: root });
  return { ok: true, served, sha: snap.sha, reason: decision.reason, raw: res.raw, statusText: res.statusText, seconds: res.seconds, id };
}

// ------------------------------------------------------------------ the `look serve` command

export async function serveCommand(root, flags, { exe, workbenchArgs, snapshot, ramGate, acquire, release }) {
  let health = serverHealth(root);
  let st = health.state;
  if (flags.status) {
    if (!st) {
      console.log("look serve: no server");
      return 0;
    }
    const idle = Math.round((Date.now() - Date.parse(st.lastUsed)) / 1000);
    console.log(`look serve: ${health.live ? "up" : `NOT HEALTHY (${health.why}; a busy server skips its heartbeat)`}  pid ${st.pid}  snapshot ${short(st.sha)}  idle ${idle} s  last caller ${st.by}`);
    console.log(`  viewer ${st.viewer}`);
    return health.live ? 0 : 1;
  }
  const lockErr = acquire(120000);
  if (lockErr) {
    console.log(`FAIL  look serve: ${lockErr}`);
    return 3;
  }
  try {
    health = serverHealth(root);
    st = health.state;
    if (flags.stop) {
      if (st) {
        await stopServer(root, st);
        console.log(`look serve: stopped pid ${st.pid}`);
      } else console.log("look serve: no server");
      if (flags.remove) {
        const viewer = viewerPath(root);
        if (existsSync(viewer)) {
          const rm = removeWorktree(wt.mainRootOf(root), viewer, { force: true });
          console.log(rm.status === 0 ? `look serve: removed ${viewer}` : `FAIL  look serve: could not remove ${viewer}: ${(rm.stderr || "").trim()}`);
          if (rm.status !== 0) return 1;
        }
      }
      return 0;
    }
    if (health.live) {
      console.log(`look serve: already up (pid ${st.pid}, snapshot ${short(st.sha)}); a look request brings it to your state`);
      return 0;
    }
    if (st) killServer(root, st);
    const ram = await ramGate();
    if (ram) {
      console.log(`FAIL  ${ram}`);
      return 3;
    }
    const snap = snapshot(root);
    const viewer = viewerPath(root);
    const co = checkoutViewer(root, viewer, snap.sha);
    if (!co.ok) {
      console.log(`FAIL  look serve: ${co.why}`);
      return 1;
    }
    const started = await startServer(root, viewer, snap, { exe, workbenchArgs, log: console.log });
    if (!started.ok) {
      console.log(`FAIL  look serve: ${started.why}${started.profile ? ` (logs under ${join(started.profile, "logs")})` : ""}`);
      return 1;
    }
    console.log(`look serve: up in ${Math.round(started.bootS)} s, pid ${started.state.pid}, snapshot ${short(snap.sha)}; it stops itself after 10 min with no request`);
    return 0;
  } finally {
    release();
  }
}
