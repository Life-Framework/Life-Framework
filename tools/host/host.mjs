// Playtest host helpers for `tools\cli host start|stop|status|backup`.
//
// The host profile lives outside every worktree (default
// %LOCALAPPDATA%\LifeFramework\host, override LF_HOST_DIR), so pruning a
// worktree or wiping a test profile never touches the playtest saves. The
// profile holds no junctions: the game-addons junction stays under the
// worktree's server/profile/serve, created by the CLI's own helper.
import { cpSync, existsSync, mkdirSync, mkdtempSync, readdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { networkInterfaces, tmpdir } from "node:os";
import { join } from "node:path";
import { safeRemoveTree } from "../link-safety.mjs";

export const PLAYTEST_MISSION = "{1054CC8646357708}Missions/LF_PlaytestTown.conf";
export const GAME_PORT = 2001;
// -profile <dir> puts script files ($profile:) in <dir>\profile.
export const STOP_FILE = ["profile", "lf-host-stop"];
export const READY_LINE = "Entered online game state.";

export function hostDir(env = process.env) {
  if (env.LF_HOST_DIR) return env.LF_HOST_DIR;
  const base = env.LOCALAPPDATA || join(env.USERPROFILE || ".", "AppData", "Local");
  return join(base, "LifeFramework", "host");
}

export function hostPaths(dir) {
  return {
    dir,
    profile: join(dir, "profile"),
    backups: join(dir, "backups"),
    state: join(dir, "host.json"),
  };
}

// The launch flags. No -config (fatal with -addons), no -bindPort (ignored
// in -server mode, the server listens on 2001), no test defines: without
// EL_AUTOTEST the DebugWorld test runner stays idle.
export function hostArgs({ mission = PLAYTEST_MISSION, addonsDir, profile, loadSave }) {
  const args = [
    "-server", mission,
    "-addonsDir", addonsDir,
    "-addons", "LifeFramework",
    "-profile", profile,
    "-maxFPS", "60",
    "-logLevel", "normal",
    "-disableCrashReporter",
    "-noBackend",
  ];
  if (loadSave) args.push("-loadSessionSave");
  return args;
}

// Every save point the engine wrote under the profile (a folder holding
// meta-info.json), newest first. Logs are skipped.
export function findSavePoints(profile) {
  const out = [];
  const walk = (dir, depth) => {
    if (depth > 10 || !existsSync(dir)) return;
    let entries;
    try {
      entries = readdirSync(dir, { withFileTypes: true });
    } catch {
      return;
    }
    for (const e of entries) {
      if (e.isSymbolicLink()) continue;
      const p = join(dir, e.name);
      if (e.isFile() && e.name === "meta-info.json") out.push({ dir, mtime: statSync(p).mtimeMs });
      else if (e.isDirectory() && e.name !== "logs" && e.name !== "game-addons") walk(p, depth + 1);
    }
  };
  walk(profile, 0);
  return out.sort((a, b) => b.mtime - a.mtime);
}

// The folder that holds the save points: the common root of every save
// point path up to and including its ".save" segment.
export function saveRoot(profile) {
  const first = findSavePoints(profile)[0];
  if (!first) return null;
  const rel = first.dir.slice(profile.length).split(/[\\/]/).filter(Boolean);
  const i = rel.indexOf(".save");
  return i >= 0 ? join(profile, ...rel.slice(0, i + 1)) : first.dir;
}

export function stamp(d = new Date()) {
  const p = (n) => String(n).padStart(2, "0");
  return `${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())}_${p(d.getHours())}-${p(d.getMinutes())}-${p(d.getSeconds())}`;
}

// Copies the save folder to backups/<stamp>-<reason>. Returns the backup
// path, or null when there is nothing to back up yet.
export function backupSaves(dir, reason, now = new Date()) {
  const { profile, backups } = hostPaths(dir);
  const src = saveRoot(profile);
  if (!src) return null;
  const dest = join(backups, `${stamp(now)}-${reason}`);
  mkdirSync(backups, { recursive: true });
  cpSync(src, dest, { recursive: true, verbatimSymlinks: true });
  return dest;
}

export function readHostState(dir) {
  const { state } = hostPaths(dir);
  try {
    return JSON.parse(readFileSync(state, "utf8"));
  } catch {
    return null;
  }
}

export function writeHostState(dir, s) {
  mkdirSync(dir, { recursive: true });
  writeFileSync(hostPaths(dir).state, JSON.stringify(s, null, 2));
}

export function pidAlive(pid) {
  if (!pid) return false;
  try {
    process.kill(pid, 0);
    return true;
  } catch {
    return false;
  }
}

export function newestConsoleLog(profile) {
  const logs = join(profile, "logs");
  if (!existsSync(logs)) return null;
  const dirs = readdirSync(logs).filter((d) => d.startsWith("logs_")).sort();
  for (let i = dirs.length - 1; i >= 0; i--) {
    const f = join(logs, dirs[i], "console.log");
    if (existsSync(f)) return f;
  }
  return null;
}

// The lines a host run is judged by: world ready, session start/continue,
// state applied on load, the host stop, and the shutdown save.
export function hostLines(text) {
  const re = /Entered online game state|\[ELDebug:(Persistence|Host)\]|SCRIPT\s+\(E\)|VME|Virtual Machine Exception/;
  return text.split(/\r?\n/).filter((l) => re.test(l));
}

export function lanAddresses() {
  const out = [];
  for (const list of Object.values(networkInterfaces())) {
    for (const a of list ?? []) {
      if (a.family === "IPv4" && !a.internal) out.push(a.address);
    }
  }
  return out;
}

// Pure checks for the validate selftest.
export function selftestHost() {
  const failures = [];
  const fresh = hostArgs({ addonsDir: "A", profile: "P", loadSave: false });
  const reload = hostArgs({ addonsDir: "A", profile: "P", loadSave: true });
  const has = (args, flag) => args.includes(flag);
  if (fresh[0] !== "-server" || fresh[1] !== PLAYTEST_MISSION) failures.push("first boot does not run the playtest mission");
  if (has(fresh, "-loadSessionSave")) failures.push("a boot with no save passes -loadSessionSave");
  if (!has(reload, "-loadSessionSave")) failures.push("a boot with a save does not pass -loadSessionSave");
  for (const args of [fresh, reload]) {
    if (args.some((a) => /EL_AUTOTEST|EL_TEST_TIER|EL_E2E|LF_HARNESS|LF_SESSION_IDENTITY/.test(a))) failures.push("host args carry a test or harness define (LF_HARNESS opens the harness gate)");
    if (has(args, "-config")) failures.push("host args pass -config (fatal with -addons)");
    if (args[args.indexOf("-profile") + 1] !== "P") failures.push("host args do not use the host profile");
  }
  if (!/Missions\/LF_PlaytestTown\.conf$/.test(PLAYTEST_MISSION)) failures.push("mission constant is not LF_PlaytestTown.conf");
  const d = hostDir({ LOCALAPPDATA: "L" });
  if (d !== join("L", "LifeFramework", "host")) failures.push(`default host dir is ${d}`);
  if (hostDir({ LF_HOST_DIR: "X", LOCALAPPDATA: "L" }) !== "X") failures.push("LF_HOST_DIR does not override the host dir");
  if (stamp(new Date(2026, 10, 8, 9, 5, 7)) !== "2026-11-08_09-05-07") failures.push("backup stamp format changed");
  const lines = hostLines("a\n12:00 DEFAULT : Entered online game state.\nb\n12:01 SCRIPT : [ELDebug:Persistence] session continued from a save point (-loadSessionSave)\n");
  if (lines.length !== 2) failures.push(`hostLines kept ${lines.length} lines, expected 2`);
  // Backup round trip on a fixture host dir.
  const fixture = mkdtempSync(join(tmpdir(), "lf-host-"));
  try {
    const sp = join(fixture, "profile", "profile", ".save", "u", "game", "M", "playthrough000", "savepoint000");
    if (backupSaves(fixture, "stop") !== null) failures.push("backup with no save returned a path");
    mkdirSync(sp, { recursive: true });
    writeFileSync(join(sp, "meta-info.json"), "{}");
    const dest = backupSaves(fixture, "stop", new Date(2026, 10, 8, 9, 5, 7));
    if (!dest || !dest.endsWith("2026-11-08_09-05-07-stop")) failures.push(`backup path is ${dest}`);
    else if (!existsSync(join(dest, "u", "game", "M", "playthrough000", "savepoint000", "meta-info.json"))) failures.push("backup did not copy the save point");
  } finally {
    safeRemoveTree(fixture);
  }
  return failures;
}
