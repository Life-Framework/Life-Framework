// Playtest host helpers for `tools\cli host start|stop|status|backup|preflight|restore`.
//
// The host profile lives outside every worktree (default
// %LOCALAPPDATA%\LifeFramework\host, override LF_HOST_DIR), so pruning a
// worktree or wiping a test profile never touches the playtest saves. The
// profile holds no junctions: the game-addons junction stays under the
// worktree's server/profile/serve, created by the CLI's own helper.
import { cpSync, existsSync, mkdirSync, mkdtempSync, readdirSync, readFileSync, renameSync, statSync, writeFileSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { networkInterfaces, tmpdir } from "node:os";
import { dirname, join } from "node:path";
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
// LF_AUTOTEST the DebugWorld test runner stays idle.
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

// A folder path that does not exist yet: base, else base-2, base-3, ...
export function freshDir(base) {
  let dest = base;
  for (let n = 2; existsSync(dest); n++) dest = `${base}-${n}`;
  return dest;
}

export function stamp(d = new Date()) {
  const p = (n) => String(n).padStart(2, "0");
  return `${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())}_${p(d.getHours())}-${p(d.getMinutes())}-${p(d.getSeconds())}`;
}

// Copies the save folder to backups/<stamp>-<reason>. Returns the backup
// path, or null when there is nothing to back up yet.
export function backupSaves(dir, reason, now = new Date()) {
  const { profile, backups } = hostPaths(dir);
  return backupSaveRoot(profile, backups, reason, now);
}

// The same copy for any profile (the e2e test profile included).
export function backupSaveRoot(profile, backups, reason, now = new Date()) {
  const src = saveRoot(profile);
  if (!src) return null;
  const dest = freshDir(join(backups, `${stamp(now)}-${reason.replace(/[^A-Za-z0-9._-]/g, "_")}`));
  mkdirSync(backups, { recursive: true });
  cpSync(src, dest, { recursive: true, verbatimSymlinks: true });
  return dest;
}

// ---------------------------------------------------------------- game updates (#980)
// A Steam update can change how the engine reads a save (1.7.0.41 dropped
// every save). The engine stamps each save point's meta-info.json with the
// build that wrote it (m_sGameVersion); the mod stamps its own payload too
// (LF_SaveStamp). Before the first boot on a new build the owner runs
// `host preflight`: back up, load the save on the new build, check the load
// line, stop (shutdown save), and restore the backup when anything failed.

// The engine's meta-info.json of a save point, or null.
export function readSaveMeta(savePointDir) {
  try {
    return JSON.parse(readFileSync(join(savePointDir, "meta-info.json"), "utf8"));
  } catch {
    return null;
  }
}

// The game build that wrote the newest save point, or null (no save, or a
// meta-info without the field).
export function newestSaveBuild(profile) {
  const first = findSavePoints(profile)[0];
  if (!first) return null;
  const meta = readSaveMeta(first.dir);
  return meta && typeof meta.m_sGameVersion === "string" && meta.m_sGameVersion ? meta.m_sGameVersion : null;
}

// Rewrites the newest save point's engine stamp (the e2e old-build proof).
export function stampNewestSave(profile, build) {
  const first = findSavePoints(profile)[0];
  if (!first) return null;
  const file = join(first.dir, "meta-info.json");
  const meta = JSON.parse(readFileSync(file, "utf8"));
  meta.m_sGameVersion = build;
  writeFileSync(file, JSON.stringify(meta, null, 4));
  return first.dir;
}

// The installed game build: the file version of the server exe, which is
// the string the engine writes as m_sGameVersion (1.9.0.37 for both).
export function exeBuild(exe) {
  if (!exe || !existsSync(exe)) return null;
  const r = spawnSync("powershell", ["-NoProfile", "-NonInteractive", "-Command", `(Get-Item -LiteralPath '${exe.replace(/'/g, "''")}').VersionInfo.FileVersion`], { encoding: "utf8", timeout: 20000, windowsHide: true });
  const v = (r.stdout || "").trim();
  return /^\d+(\.\d+){1,3}$/.test(v) ? v : null;
}

// Whether a boot needs the preflight first: a save exists and its build is
// known and differs from the installed one.
export function buildGate({ saveBuild, gameBuild }) {
  if (!saveBuild || !gameBuild) return { changed: false, reason: saveBuild ? "installed game build unknown" : "save build unknown" };
  if (saveBuild === gameBuild) return { changed: false, reason: `save and game are both ${gameBuild}` };
  return { changed: true, reason: `the save was written by game build ${saveBuild}, the installed game is ${gameBuild}` };
}

// Judges a preflight boot's console.log: the store loaded ("save loaded:",
// LF_PersistenceComponentSerializer.ReportLoad) with nothing damaged, no
// script error, and the shutdown save was written.
export function preflightVerdict(text) {
  const lines = String(text ?? "").split(/\r?\n/);
  const find = (re) => lines.find((l) => re.test(l));
  const damaged = find(/\[LFDebug:Persistence\].*(save load damaged|continued save never reached)/);
  if (damaged) return { ok: false, reason: damaged.trim() };
  const scriptError = find(/SCRIPT\s+\(E\)|Virtual Machine Exception|\bVME\b/);
  if (scriptError) return { ok: false, reason: `script error: ${scriptError.trim()}` };
  const loaded = find(/\[LFDebug:Persistence\] save loaded:/);
  if (!loaded) return { ok: false, reason: "no '[LFDebug:Persistence] save loaded:' line: the engine did not load the save into the LF store" };
  if (!find(/\[LFDebug:Persistence\] save completed \(SHUTDOWN\)/)) return { ok: false, reason: "no shutdown save after the load", loaded: loaded.trim() };
  return { ok: true, reason: loaded.trim() };
}

// The newest backup folder, or the one named.
export function pickBackup(dir, name) {
  const { backups } = hostPaths(dir);
  if (!existsSync(backups)) return null;
  const all = readdirSync(backups).filter((n) => statSync(join(backups, n)).isDirectory()).sort();
  if (name) return all.includes(name) ? join(backups, name) : null;
  return all.length ? join(backups, all[all.length - 1]) : null;
}

// Puts a backup back as the host's save folder. The current save folder is
// moved into backups/<stamp>-replaced-by-restore, never deleted.
export function restoreBackup(dir, backup, now = new Date()) {
  const { profile, backups } = hostPaths(dir);
  if (!backup || !existsSync(backup)) throw new Error(`backup not found: ${backup}`);
  if (!findSavePoints(backup).length) throw new Error(`backup holds no save point: ${backup}`);
  const target = saveRoot(profile) ?? join(profile, "profile", ".save");
  let kept = null;
  if (existsSync(target)) {
    kept = freshDir(join(backups, `${stamp(now)}-replaced-by-restore`));
    mkdirSync(backups, { recursive: true });
    renameSync(target, kept);
  }
  mkdirSync(dirname(target), { recursive: true });
  cpSync(backup, target, { recursive: true, verbatimSymlinks: true });
  return { target, kept };
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
  const re = /Entered online game state|\[LFDebug:(Persistence|Host)\]|SCRIPT\s+\(E\)|VME|Virtual Machine Exception/;
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
    if (args.some((a) => /LF_AUTOTEST|LF_TEST_TIER|LF_E2E|LF_HARNESS|LF_SESSION_IDENTITY/.test(a))) failures.push("host args carry a test or harness define (LF_HARNESS opens the harness gate)");
    if (has(args, "-config")) failures.push("host args pass -config (fatal with -addons)");
    if (args[args.indexOf("-profile") + 1] !== "P") failures.push("host args do not use the host profile");
  }
  if (!/Missions\/LF_PlaytestTown\.conf$/.test(PLAYTEST_MISSION)) failures.push("mission constant is not LF_PlaytestTown.conf");
  const d = hostDir({ LOCALAPPDATA: "L" });
  if (d !== join("L", "LifeFramework", "host")) failures.push(`default host dir is ${d}`);
  if (hostDir({ LF_HOST_DIR: "X", LOCALAPPDATA: "L" }) !== "X") failures.push("LF_HOST_DIR does not override the host dir");
  if (stamp(new Date(2026, 10, 8, 9, 5, 7)) !== "2026-11-08_09-05-07") failures.push("backup stamp format changed");
  const lines = hostLines("a\n12:00 DEFAULT : Entered online game state.\nb\n12:01 SCRIPT : [LFDebug:Persistence] session continued from a save point (-loadSessionSave)\n");
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

    // #980: the engine stamp, the build gate, a restore that keeps what it replaces.
    writeFileSync(join(sp, "meta-info.json"), JSON.stringify({ m_sGameVersion: "1.8.0.13" }));
    if (newestSaveBuild(join(fixture, "profile")) !== "1.8.0.13") failures.push("newestSaveBuild does not read m_sGameVersion");
    stampNewestSave(join(fixture, "profile"), "1.7.0.41");
    if (newestSaveBuild(join(fixture, "profile")) !== "1.7.0.41") failures.push("stampNewestSave did not rewrite m_sGameVersion");
    if (!buildGate({ saveBuild: "1.8.0.13", gameBuild: "1.9.0.37" }).changed) failures.push("a save from another build does not need the preflight");
    if (buildGate({ saveBuild: "1.9.0.37", gameBuild: "1.9.0.37" }).changed) failures.push("a save from this build needs the preflight");
    if (buildGate({ saveBuild: null, gameBuild: "1.9.0.37" }).changed) failures.push("an unknown save build needs the preflight");
    const old = backupSaves(fixture, "build-1.7.0.41-to-1.9.0.37", new Date(2026, 10, 8, 9, 6, 0));
    writeFileSync(join(sp, "meta-info.json"), JSON.stringify({ m_sGameVersion: "bad" }));
    const r = restoreBackup(fixture, pickBackup(fixture, "2026-11-08_09-06-00-build-1.7.0.41-to-1.9.0.37"), new Date(2026, 10, 8, 9, 7, 0));
    if (newestSaveBuild(join(fixture, "profile")) !== "1.7.0.41") failures.push(`restore did not put the backup back (${old} -> ${r.target})`);
    if (!r.kept || readSaveMeta(join(r.kept, "u", "game", "M", "playthrough000", "savepoint000"))?.m_sGameVersion !== "bad") failures.push("restore did not keep the save it replaced");
    if (!pickBackup(fixture)?.endsWith("replaced-by-restore")) failures.push("pickBackup does not return the newest backup");
    const twin = backupSaves(fixture, "stop", new Date(2026, 10, 8, 9, 5, 7));
    if (!twin || !twin.endsWith("2026-11-08_09-05-07-stop-2")) failures.push(`a second backup in the same second did not get its own folder (${twin})`);
  } finally {
    safeRemoveTree(fixture);
  }
  const good = "[LFDebug:Persistence] save loaded: version 32, game build 1.8.0.13, running 1.9.0.37\n[LFDebug:Persistence] save completed (SHUTDOWN)\n";
  if (!preflightVerdict(good).ok) failures.push("preflight rejects a clean load and shutdown save");
  if (preflightVerdict("[LFDebug:Persistence] save completed (SHUTDOWN)\n").ok) failures.push("preflight passes with no load line");
  if (preflightVerdict(good + "[LFDebug:Persistence] save load damaged: 3 payload(s)\n").ok) failures.push("preflight passes a damaged load");
  if (preflightVerdict(good + "SCRIPT    (E): boom\n").ok) failures.push("preflight passes a script error");
  if (preflightVerdict("[LFDebug:Persistence] save loaded: version 32\n").ok) failures.push("preflight passes with no shutdown save");
  return failures;
}
