// tools/lib/game-update.mjs - is the Arma Reforger client, server and tools install on this machine at one game version?
//
// 2026-11-27 ~15:03: Steam updated the client (app 1874880) but not the dedicated server (app 1874900), which stays
// suspended while any ArmaReforgerServer.exe runs. Every `session up` then booted a server that refused the client
// ("Client uses unsupported protocol version") and burned ~5 min to "server not ready"; the agents' retries kept a
// server running, which kept Steam from updating. This module reads what is on disk, in milliseconds, so
// `session up`, `status`, `pool status` and `game-update` can say it at once (docs/worktrees.md "Game update guard").
//
// Two signals, because the apps' build ids are different numbers even when they are in step (client 26639613, server
// 26634883 on one good day): the exe's own game version (client ArmaReforgerSteam.exe against ArmaReforgerServer.exe,
// the version the protocol follows), and each app's Steam manifest (buildid against TargetBuildID, StateFlags).
import { closeSync, existsSync, openSync, readFileSync, readSync, statSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { spawnSync } from "node:child_process";
import { localSetting } from "./local-config.mjs";

export const APPS = {
  client: { id: 1874880, label: "client", exe: "ArmaReforgerSteam.exe", dir: "Arma Reforger" },
  server: { id: 1874900, label: "server", exe: "ArmaReforgerServer.exe", dir: "Arma Reforger Server" },
  tools: { id: 1874910, label: "tools", exe: null, dir: "Arma Reforger Tools" },
};

const STEAM_COMMON = "C:/Program Files (x86)/Steam/steamapps/common";

// Steam's StateFlags bits that mean "an update is wanted or on its way": UpdateRequired 2, UpdateRunning 256,
// UpdatePaused 512, UpdateStarted 1024, Downloading 1048576, Staging 2097152, Committing 4194304.
export const PENDING_FLAGS = 2 | 256 | 512 | 1024 | 1048576 | 2097152 | 4194304;

// Steam's .acf is quoted key/value text; a flat read of the top-level keys is all this needs.
export function parseAcf(text) {
  const get = (k) => (new RegExp(`"${k}"\\s+"([^"]*)"`).exec(text) || [])[1] ?? "";
  const num = (k) => {
    const v = get(k);
    return v === "" ? null : Number(v);
  };
  return {
    appid: num("appid"),
    buildid: get("buildid"),
    target: get("TargetBuildID"),
    flags: num("StateFlags") ?? 0,
    lastUpdated: num("LastUpdated") ?? 0,
    toDownload: num("BytesToDownload") ?? 0,
    downloaded: num("BytesDownloaded") ?? 0,
  };
}

// The game version the exe was built as. The build number (1.9.0.42) lives only in the version resource's string
// table (UTF-16 "FileVersion" key); the binary VS_FIXEDFILEINFO (signature 0xFEEF04BD) carries 1.9.0.0. The string is
// read first, the fixed info is the fallback. Reads the file in blocks; null when the file or both are missing.
const KEY = Buffer.concat([Buffer.from("FileVersion", "utf16le"), Buffer.alloc(2)]);
const SIG = Buffer.from([0xbd, 0x04, 0xef, 0xfe]);
const VERSION_TEXT = /^[0-9]+([.][0-9]+){1,3}$/;

function stringVersionAt(buf, i, end) {
  let p = i + KEY.length;
  while (p + 1 < end && buf[p] === 0 && buf[p + 1] === 0) p += 2;
  let text = "";
  while (p + 1 < end && (buf[p] !== 0 || buf[p + 1] !== 0) && text.length < 32) {
    text += String.fromCharCode(buf.readUInt16LE(p));
    p += 2;
  }
  return VERSION_TEXT.test(text) ? text : null;
}

export function exeVersion(file) {
  let fd;
  let fixed = null;
  try {
    fd = openSync(file, "r");
    const size = statSync(file).size;
    const block = 4 * 1024 * 1024;
    const keep = 128;
    const buf = Buffer.alloc(block + keep);
    let carry = 0;
    for (let pos = 0; pos < size; pos += block) {
      const n = readSync(fd, buf, carry, Math.min(block, size - pos), pos);
      const end = carry + n;
      const view = buf.subarray(0, end);
      for (let i = view.indexOf(KEY); i !== -1; i = view.indexOf(KEY, i + 1)) {
        const v = stringVersionAt(buf, i, end);
        if (v) return v;
      }
      if (!fixed) {
        for (let i = view.indexOf(SIG); i !== -1 && i + 16 <= end; i = view.indexOf(SIG, i + 1)) {
          if (buf.readUInt32LE(i + 4) !== 0x00010000) continue;
          const ms = buf.readUInt32LE(i + 8);
          const ls = buf.readUInt32LE(i + 12);
          if (ms || ls) {
            fixed = `${ms >>> 16}.${ms & 0xffff}.${ls >>> 16}.${ls & 0xffff}`;
            break;
          }
        }
      }
      carry = Math.min(keep, end);
      buf.copy(buf, 0, end - carry, end);
    }
  } catch {
    return null;
  } finally {
    if (fd !== undefined) closeSync(fd);
  }
  return fixed;
}

export function cmpVersion(a, b) {
  const pa = String(a).split(".").map(Number);
  const pb = String(b).split(".").map(Number);
  for (let i = 0; i < Math.max(pa.length, pb.length); i++) {
    const d = (pa[i] || 0) - (pb[i] || 0);
    if (d) return d < 0 ? -1 : 1;
  }
  return 0;
}

const trimExe = (p) => String(p).replace(/[\\/]+$/, "").replace(/[\\/][^\\/]*\.exe$/i, "");

// The three install folders: process env > lf.local.json (the same keys everything else reads), else the Steam defaults.
// An ENFUSION_SERVER_PATH may name the exe itself.
export function dirsFromEnv(env = {}) {
  return {
    client: trimExe(env.ENFUSION_GAME_PATH || `${STEAM_COMMON}/${APPS.client.dir}`),
    server: trimExe(env.ENFUSION_SERVER_PATH || `${STEAM_COMMON}/${APPS.server.dir}`),
    tools: trimExe(env.ENFUSION_WORKBENCH_PATH || `${STEAM_COMMON}/${APPS.tools.dir}`),
  };
}

export function installDirs(root, env = process.env, setting = localSetting) {
  const pick = {};
  for (const k of ["ENFUSION_GAME_PATH", "ENFUSION_SERVER_PATH", "ENFUSION_WORKBENCH_PATH"]) pick[k] = env[k] || setting(root, k) || "";
  return dirsFromEnv(pick);
}

// SteamCMD keeps the manifest in <install>/steamapps, the Steam client in <library>/steamapps.
export function manifestFor(dir, appId) {
  for (const m of [join(dir, "steamapps", `appmanifest_${appId}.acf`), join(dir, "..", "..", `appmanifest_${appId}.acf`)]) {
    try {
      if (existsSync(m)) return { path: resolve(m), ...parseAcf(readFileSync(m, "utf8")) };
    } catch {}
  }
  return null;
}

// What is installed: per app the manifest, and for client and server the exe's game version.
export function readInstall(dirs, { version = exeVersion, manifest = manifestFor } = {}) {
  const out = {};
  for (const [k, app] of Object.entries(APPS)) {
    out[k] = { dir: dirs[k], manifest: manifest(dirs[k], app.id), version: app.exe ? version(join(dirs[k], app.exe)) : null };
  }
  return out;
}

const side = (a) => `${a.version ? a.version : "version unknown"}${a.manifest?.buildid ? ` (build ${a.manifest.buildid})` : ""}`;

// Problems, most important first, and the app keys that need a Steam update. ok when none.
export function evaluate(install) {
  const problems = [];
  const stale = [];
  const { client, server } = install;
  if (client.version && server.version) {
    const c = cmpVersion(client.version, server.version);
    if (c !== 0) {
      stale.push(c > 0 ? "server" : "client");
      problems.push(`client ${side(client)} vs server ${side(server)}: the server is ${c > 0 ? "behind" : "ahead of"} the client`);
    }
  }
  for (const [k, a] of Object.entries(install)) {
    const m = a.manifest;
    if (!m) continue;
    const wants = m.target && m.buildid && m.target !== m.buildid;
    const queued = (m.flags & PENDING_FLAGS) !== 0;
    if (!wants && !queued) continue;
    if (!stale.includes(k)) stale.push(k);
    const why = wants ? `Steam wants build ${m.target}` : `StateFlags ${m.flags}: an update is queued`;
    problems.push(`${APPS[k].label} app ${APPS[k].id} is on build ${m.buildid || "unknown"}, ${why}`);
  }
  return { ok: problems.length === 0, problems, stale };
}

export const FIX_HINT = "Fix: tools\\cli game-update (waits for running servers, updates the server app, refuses while a session is held)";

// The one line `status`, `pool status` and `session up` print; null when the install is in step.
export function pendingLine(install) {
  const v = evaluate(install);
  if (v.ok) return null;
  return `GAME UPDATE PENDING: ${v.problems.join("; ")}. ${FIX_HINT}`;
}

export function okLine(install) {
  const c = install.client;
  const s = install.server;
  return `game: client ${c.version ?? "version unknown"} and server ${s.version ?? "version unknown"} in step (builds ${c.manifest?.buildid ?? "?"} / ${s.manifest?.buildid ?? "?"})`;
}

// Reads this machine's install and returns { install, verdict, line } (line is the pending text or null).
export function check(root, opts = {}) {
  const install = readInstall(opts.dirs ?? installDirs(root), opts);
  return { install, verdict: evaluate(install), line: pendingLine(install) };
}

// ArmaReforger exes running out of one of the install folders (never by name alone: a Workbench or another install is
// not this one). Windows: one CIM query; [] elsewhere.
export function runningUnder(dirs, runner = spawnSync) {
  if (process.platform !== "win32") return [];
  const r = runner("powershell", ["-NoProfile", "-NonInteractive", "-Command", "Get-CimInstance Win32_Process -Filter \"Name like 'ArmaReforger%'\" | ForEach-Object { \"$($_.ProcessId)|$($_.ExecutablePath)\" }"], { windowsHide: true, encoding: "utf8", timeout: 20000 });
  const roots = Object.entries(dirs).map(([k, d]) => [k, resolve(d).toLowerCase() + "\\"]);
  const found = [];
  for (const line of String(r.stdout ?? "").split(/\r?\n/)) {
    const [pid, path] = line.split("|");
    if (!pid || !path) continue;
    const p = resolve(path).toLowerCase();
    const hit = roots.find(([, d]) => p.startsWith(d));
    if (hit) found.push({ pid: Number(pid), app: hit[0], exe: path });
  }
  return found;
}

// Live lane locks that belong to a session (the lanes `session up` takes) on this PC.
export function heldSessions(machines, lockInfo, pidAlive, laneLocks) {
  const held = [];
  for (const m of machines) {
    if (m.via && m.via !== "local") continue;
    for (const name of laneLocks(m)) {
      const info = lockInfo(name);
      if (info && info.purpose === "session" && pidAlive(info.pid)) held.push({ lock: name, root: info.root ?? "", pid: info.pid });
    }
  }
  return held;
}

// The update, as one bounded procedure over injected effects (the selftest drives it with fakes).
//   deps: { read() -> install, held() -> [], running(apps[]) -> [], trigger(appId), sleep(ms), now() -> ms, log(line) }
// Returns { code, why }: 0 in step (or updated), 1 refused or ran out of its budget.
export async function updateRun(opts, deps) {
  const { waitMs = 20 * 60 * 1000, updateMs = 30 * 60 * 1000, pollMs = 10000, dryRun = false } = opts;
  const log = deps.log ?? (() => {});
  let v = evaluate(deps.read());
  if (v.ok) {
    log("game-update: client, server and tools are in step; nothing to update");
    return { code: 0, why: "in-step" };
  }
  for (const p of v.problems) log(`game-update: ${p}`);
  const held = deps.held();
  if (held.length) {
    log(`game-update: refusing while a session is held (${held.map((h) => h.lock + (h.root ? ` by ${h.root}` : "")).join(", ")}); 'session down' there first, then run this again`);
    return { code: 1, why: "session-held" };
  }
  const apps = v.stale;
  // Steam suspends an update while a process of that app runs: wait for them to end, never stop one.
  const t0 = deps.now();
  let blockers = deps.running(apps);
  while (blockers.length) {
    if (deps.now() - t0 >= waitMs) {
      log(`game-update: ${blockers.length} process(es) of ${apps.join("+")} still running after ${Math.round(waitMs / 60000)} min (pids ${blockers.map((b) => b.pid).join(", ")}); Steam will not update under them. Nothing was stopped; try again when they end`);
      return { code: 1, why: "servers-running" };
    }
    log(`game-update: waiting for ${blockers.length} running ${apps.join("+")} process(es) to finish (pids ${blockers.map((b) => b.pid).join(", ")})`);
    await deps.sleep(pollMs);
    const again = deps.held();
    if (again.length) {
      log(`game-update: a session was started meanwhile (${again.map((h) => h.lock).join(", ")}); refusing`);
      return { code: 1, why: "session-held" };
    }
    blockers = deps.running(apps);
  }
  if (dryRun) {
    log(`game-update: dry run; would ask Steam to update ${apps.map((k) => `${k} (app ${APPS[k].id})`).join(", ")}`);
    return { code: 0, why: "dry-run" };
  }
  for (const k of apps) {
    log(`game-update: asking Steam to update ${k} (app ${APPS[k].id})`);
    deps.trigger(APPS[k].id);
  }
  const t1 = deps.now();
  for (;;) {
    await deps.sleep(pollMs);
    v = evaluate(deps.read());
    if (v.ok) {
      log("game-update: client, server and tools are in step now");
      return { code: 0, why: "updated" };
    }
    if (deps.now() - t1 >= updateMs) {
      log(`game-update: still not in step after ${Math.round(updateMs / 60000)} min: ${v.problems.join("; ")}. Open Steam > Downloads; run this again once it finishes`);
      return { code: 1, why: "update-timeout" };
    }
  }
}
