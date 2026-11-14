#!/usr/bin/env node
// Multi-client session: one dedicated server plus N game clients on this PC,
// each client with its own -profile, joined over 127.0.0.1:2001.
//
//   node agent-box/session.mjs up --clients 2     boot the server, launch the clients
//   node agent-box/session.mjs up --clients 2 --mission playtest   the pack header (LF_Everon1989.conf), harness build
//   node agent-box/session.mjs restart            save, stop, load, rejoin (playtest mission)
//   node agent-box/session.mjs wait [--timeout 600]   block until a starting session is ready (exit 0), or say why not
//   node agent-box/session.mjs down               close every process the session started
//   node agent-box/session.mjs status             pids, profiles, logs
//   node agent-box/session.mjs reset              clock out, clear wanted, respawn: every proof verb runs this first (--no-reset skips)
//   node agent-box/session.mjs state <client>     faction, duty, wanted, cash, bank, job, org
//   node agent-box/session.mjs film <seconds> [--client 1] [--fps 12]   in-engine clip of one client
//   node agent-box/session.mjs proof --film [--film-client 1]          the proof, filmed in-engine
//
// Any harness verb targets one client with --client <n>:
//   node agent-box/harness.mjs --client 2 look
//
// The server runs the bare DebugWorld (no mission header, no test define), so
// the world's game mode keeps dev tools open and the harness RPCs pass
// LF_DevGate. The playtest mission (the pack's LF_Everon1989.conf, or the core's LF_PlaytestTown.conf
// without the pack) closes that gate.
// `up --mission playtest` runs that mission the way `tools\cli host start` does,
// with -scrDefine LF_HARNESS on the server and every client: the harness
// (stand, pose, use) passes its own gate in that build only, the debug menu
// stays closed, and a client without the define is refused at the door.
// `restart` (playtest only) stops the server through the host stop file (the
// engine writes its SHUTDOWN save), boots it with -loadSessionSave, and
// rejoins the same client profiles in the same order (same player ids, so the
// same LF_SESSION_IDENTITY accounts).
//
// `up` and `down` run through `tools\cli session` because the server boot
// shares the CLI's machine-wide server-2001 lock and addon junction helper.
// The session dir lives outside every worktree, one per checkout (default
// %LOCALAPPDATA%\LifeFramework\session\<checkout folder>, override
// LF_SESSION_DIR), and holds no junctions.

import "../tools/lib/quiet-gate.mjs";
import { spawnSync } from "node:child_process";
import { randomBytes } from "node:crypto";
import { existsSync, mkdirSync, mkdtempSync, readdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { basename, dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { extendLease, readReaped, sessionBase as leaseBase, sessionDirOf, takeHoldWarning, touchLease } from "../tools/lib/session-lease.mjs";
import { addonsArgs, launchAddons, PACK_MISSION, playtestMission } from "../tools/lib/addons.mjs";
import { applyClientSettings, parseLevers, parseProfile, resolveProfile } from "./session-profile.mjs";
export { applyClientSettings, resolveProfile };

export const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..");

export const SERVER_WORLD = "Worlds/DebugWorld/DebugWorld.ent";
export const MAX_CLIENTS = 3;
// LF_SESSION_WINDOW=1280x720 sets the window when no --res is given.
export function parseWindow(text) {
  const m = /^(\d{3,4})x(\d{3,4})$/.exec(String(text || ""));
  if (!m) return { width: 640, height: 360, gap: 8 };
  return { width: Number(m[1]), height: Number(m[2]), gap: 8 };
}
export const WINDOW = parseWindow(process.env.LF_SESSION_WINDOW);
// Client render size. Screenshots are taken at this size (System.MakeScreenshot
// writes the window's backbuffer), so evidence needs a playable one: 1280x720
// by default, --res 1920x1080 for final evidence. Clients render on the hidden
// LF-Agents desktop, so a large window never covers the user's screen or takes
// focus; --visible puts them on the user's desktop so tools/film can record.
export const DEFAULT_RES = process.env.LF_SESSION_WINDOW || "1280x720";

export function parseRes(args) {
  const i = args.indexOf("--res");
  const text = i === -1 ? DEFAULT_RES : String(args[i + 1] ?? "");
  const m = /^(\d{3,4})x(\d{3,4})$/.exec(text);
  if (!m) throw new Error(`--res must be WIDTHxHEIGHT (for example 1920x1080): ${text}`);
  const res = { width: Number(m[1]), height: Number(m[2]) };
  if (res.width < 640 || res.height < 360) throw new Error("--res is at least 640x360");
  return res;
}
export const SERVER_READY = "Entered online game state.";
// The server refuses a client whose script checksum differs
// ("remote script source code checksum does not match"), so clients compile
// with the same define as the server.
export const SESSION_DEFINE = "LF_SESSION_IDENTITY";
// Opens the harness gate on the playtest mission (LF_DevGate.HarnessBuild).
// Only a session passes it; tools\cli host start never does.
export const HARNESS_DEFINE = "LF_HARNESS";
export const MISSIONS = ["debug", "playtest"];

export function parseMission(args) {
  const i = args.indexOf("--mission");
  if (i === -1) return "debug";
  const m = args[i + 1];
  if (!MISSIONS.includes(m)) throw new Error(`--mission must be ${MISSIONS.join(" or ")}`);
  return m;
}

// Both sides compile the same defines, or the server refuses the client
// ("remote script source code checksum does not match").
export function defineArgs(mission = "debug") {
  const args = ["-scrDefine", SESSION_DEFINE];
  if (mission === "playtest") args.push("-scrDefine", HARNESS_DEFINE);
  return args;
}

// One dir per checkout (named after the checkout folder) holds its session.json,
// client profiles, and server saves. A shared dir let one worktree's `up`
// overwrite another's record, profiles, and save between its runs. The server
// port is still one per machine (the server-2001 lock); liveSessionsElsewhere
// finds clients another checkout left running.
// The dir logic lives in tools/lib/session-lease.mjs: the pool reads a holder's
// session dir and lease from there without importing this file.
export function sessionBase(env = process.env) {
  return leaseBase(env);
}

export function sessionDir(env = process.env, root = ROOT) {
  return sessionDirOf(root, env);
}

// Recorded, not stopped sessions in other checkouts' dirs; alive(pid) decides.
export function liveSessionsElsewhere(dir, alive, base = sessionBase()) {
  let names = [];
  try {
    names = readdirSync(base, { withFileTypes: true }).filter((d) => d.isDirectory()).map((d) => d.name);
  } catch {
    return [];
  }
  const out = [];
  // base itself: a session started before the dirs were split.
  for (const other of [base, ...names.map((n) => join(base, n))]) {
    if (resolve(other).toLowerCase() === resolve(dir).toLowerCase()) continue;
    const s = readSession(other);
    if (s && !s.stoppedAt && (alive(s.serverPid) || (s.clients ?? []).some((c) => alive(c.pid)))) out.push({ dir: other, root: s.root, serverUp: !!alive(s.serverPid) });
  }
  return out;
}

// Clients left running by a session whose server is gone (crashed or closed by hand) hold memory and windows
// but no lane. Closes only the pids recorded in that session's own session.json (never by process name) and
// marks it stopped. kill(pid) does the closing. Returns the reaped dirs.
export function reapOrphanSessions(dir, alive, kill, base = sessionBase()) {
  const reaped = [];
  for (const o of liveSessionsElsewhere(dir, alive, base)) {
    if (o.serverUp) continue;
    const s = readSession(o.dir);
    if (!s) continue;
    for (const c of s.clients ?? []) if (alive(c.pid)) kill(c.pid);
    writeSession(o.dir, { ...s, stoppedAt: new Date().toISOString(), reapedOrphan: true });
    reaped.push(o.dir);
  }
  return reaped;
}

// -profile <dir> puts $profile: in <dir>\profile and logs in <dir>\logs.
export function sessionPaths(dir, clients = MAX_CLIENTS) {
  const out = { dir, state: join(dir, "session.json"), server: join(dir, "server"), clients: [] };
  for (let n = 1; n <= clients; n++) {
    const profile = join(dir, `client${n}`);
    out.clients.push({ n, profile, box: join(profile, "profile", "agent-box"), logs: join(profile, "logs") });
  }
  return out;
}

export function clientBox(dir, n) {
  return sessionPaths(dir, n).clients[n - 1].box;
}

// Every `up` names its clients with a fresh id (-lfSession). The harness
// addresses each command to it, and a client runs only its own session's
// commands (LF_AgentCommand.SessionAccepts).
export function newSessionId() {
  return randomBytes(6).toString("hex");
}

const samePath = (a, b) => resolve(String(a)).replaceAll("\\", "/").toLowerCase() === resolve(String(b)).replaceAll("\\", "/").toLowerCase();

// Pure: the address for a command to box, given the recorded session.
// null when box is not a session client's box (Workbench, default profile).
export function sessionAddress(box, root, dir, s) {
  const boxes = sessionPaths(dir, MAX_CLIENTS).clients.map((c) => c.box);
  if (!boxes.some((b) => samePath(b, box))) return null;
  if (!s || s.stoppedAt) return { error: "no_live_session" };
  if (!s.id) return { error: "session_has_no_id (started by an older harness; down and up again)" };
  if (!samePath(s.root, root)) return { error: `session_owned_by_other_checkout ${s.root}` };
  return { id: s.id };
}

export function sessionForBox(box, root, dir = sessionDir()) {
  const a = sessionAddress(box, root, dir, readSession(dir));
  if (a?.error === "no_live_session") {
    const reaped = readReaped(dir);
    if (reaped) return { error: `no_live_session (the pool closed it: ${reaped.why})` };
  }
  return a;
}

// The session lane's port: 2001 is the engine default (and the LAN-facing lane); another lane
// listens on 127.0.0.1:<port> (-addr is the only flag that moves a -server boot) with its own A2S
// port. Local only: clients of a lane-2 session join 127.0.0.1:<port>.
export function laneArgs(port) {
  if (!port || port === 2001) return [];
  return ["-addr", `127.0.0.1:${port}`, "-a2sPort", String(17777 + (port - 2001) * 10)];
}

// rp: the resolved session profile (session-profile.mjs); absent means full.
// addons: the -addons list (tools/lib/addons.mjs launchAddons); "playtest" boots that list's header.
export function serverArgs({ addonsDir, profile, mission = "debug", loadSave = false, port = 2001, rp = resolveProfile("full"), addons = launchAddons() }) {
  const args = [
    "-server", mission === "playtest" ? playtestMission(addons) : SERVER_WORLD,
    "-addonsDir", addonsDir,
    ...addonsArgs(addons),
    "-profile", profile,
    "-maxFPS", String(rp.server?.maxFps ?? 30),
    "-logLevel", "normal",
    ...(rp.server?.args ?? []),
    // -noBackend clients all carry the NULL identity and the same Steam name;
    // LF_Utils.GetPlayerUID gives each player id its own identity under this define.
    ...defineArgs(mission),
    "-disableCrashReporter",
    "-noBackend",
  ];
  if (loadSave) args.push("-loadSessionSave");
  // LF_PERF=1: the server prints its LF_Perf counters every minute (docs/architecture/performance.md).
  if (process.env.LF_PERF === "1") args.push("-lfPerf");
  args.push(...laneArgs(port));
  return args;
}

// Small windows in a row along the top of the screen, so the user's desk
// stays usable. -screenWidth/-screenHeight/-posX/-posY need -window.
export function windowFor(n, res = WINDOW) {
  return { x: (n - 1) * (res.width + WINDOW.gap), y: 0, width: res.width, height: res.height };
}

export function clientArgs({ n, addonsDir, profile, address = "127.0.0.1", port = 2001, mission = "debug", session = "", res = WINDOW, sound = false, rp = resolveProfile("full"), addons = launchAddons() }) {
  const w = windowFor(n, res);
  return [
    "-client", port && port !== 2001 ? `${address}:${port}` : address,
    "-addonsDir", addonsDir,
    ...addonsArgs(addons),
    "-profile", profile,
    "-window",
    "-screenWidth", String(w.width),
    "-screenHeight", String(w.height),
    "-posX", String(w.x),
    "-posY", String(w.y),
    "-forceUpdate",
    "-noFocus",
    "-noSplash",
    ...(sound ? [] : ["-noSound"]),
    "-maxFPS", String(rp.client?.maxFps ?? 30),
    ...(rp.client?.args ?? []),
    ...defineArgs(mission),
    "-disableCrashReporter",
    "-noBackend",
    ...(session ? ["-lfSession", session] : []),
  ];
}

// --profile full|lowspec (default lowspec; LF_SESSION_PROFILE overrides the default), --levers a,b to
// run a subset (session-profile.mjs). Returns the resolved profile a session records.
export function parseSessionProfile(args) {
  const profile = parseProfile(args);
  return resolveProfile(profile, parseLevers(args, profile));
}

// The profile the session recorded in dir runs with ("full" when there is none, or it predates profiles).
export function recordedProfile(dir = sessionDir()) {
  return readSession(dir)?.profile ?? "full";
}

// Throws for a tool that takes pictures when the live session is lowspec; the message says what to run.
export function requireFullProfile(what, dir = sessionDir()) {
  const s = readSession(dir);
  if (s && !s.stoppedAt && s.profile === "lowspec") throw new Error(`${what} needs a full-profile session and this one is lowspec (640x360, lowest textures): 'node agent-box/session.mjs down', then 'up --profile full'`);
}

// The profile a recorded session ran with; a session of before profiles was full.
export function profileOf(state) {
  return resolveProfile(state?.profile ?? "full", state?.levers);
}

// An explicit --res wins; else the profile's window (lowspec 640x360); else the usual default.
export function sessionRes(args, rp) {
  if (args.includes("--res") || !rp?.client?.res) return parseRes(args);
  return { ...rp.client.res };
}

export function parseClients(args) {
  const i = args.indexOf("--clients");
  // One client by default: a second costs ~18 s of boot and 0.7 to 1.5 GB; ask for --clients 2 when the proof needs two players.
  const n = i === -1 ? 1 : Number(args[i + 1]);
  // 0 is a server-only session (the only kind a remote pool machine runs).
  if (!Number.isInteger(n) || n < 0 || n > MAX_CLIENTS) throw new Error(`--clients must be 0..${MAX_CLIENTS}`);
  return n;
}

export function readSession(dir) {
  try {
    return JSON.parse(readFileSync(sessionPaths(dir).state, "utf8"));
  } catch {
    return null;
  }
}

export function writeSession(dir, s) {
  mkdirSync(dir, { recursive: true });
  writeFileSync(sessionPaths(dir).state, JSON.stringify(s, null, 2));
}

export function newestLog(logsDir) {
  if (!existsSync(logsDir)) return null;
  const dirs = readdirSync(logsDir).filter((d) => d.startsWith("logs_")).sort();
  for (let i = dirs.length - 1; i >= 0; i--) {
    const f = join(logsDir, dirs[i], "console.log");
    if (existsSync(f)) return f;
  }
  return null;
}

// "faction=police duty=1 ..." -> { faction: "police", duty: "1", ... }
export function parseState(detail) {
  const out = {};
  for (const part of String(detail ?? "").trim().split(/\s+/)) {
    const eq = part.indexOf("=");
    if (eq > 0) out[part.slice(0, eq)] = part.slice(eq + 1);
  }
  return out;
}

// The lowspec profile: the default, what asks for full, what each side's args and settings carry.
export function selftestProfile() {
  const failures = [];
  const low = parseSessionProfile([]);
  if (low.profile !== "lowspec") failures.push(`the default profile is ${low.profile}, not lowspec`);
  for (const flag of [["--res", "1920x1080"], ["--visible"], ["--sound"], ["--profile", "full"]]) {
    if (parseSessionProfile(flag).profile !== "full") failures.push(`${flag.join(" ")} does not mean a full profile`);
  }
  if (parseSessionProfile(["--res", "1280x720", "--profile", "lowspec"]).profile !== "lowspec") failures.push("an explicit --profile lowspec lost to --res");
  try {
    parseSessionProfile(["--profile", "tiny"]);
    failures.push("--profile tiny was accepted");
  } catch {}
  try {
    parseSessionProfile(["--levers", "nope"]);
    failures.push("--levers nope was accepted");
  } catch {}
  if (parseSessionProfile(["--levers", "tex,res"]).levers.join() !== "tex,res") failures.push("--levers did not replace the profile list");
  const full = resolveProfile("full");
  const sf = serverArgs({ addonsDir: "A", profile: "SP", rp: full });
  const sl = serverArgs({ addonsDir: "A", profile: "SP", rp: resolveProfile("custom", ["nds", "sfps"]) });
  if (sf.includes("-nds") || sf[sf.indexOf("-maxFPS") + 1] !== "30") failures.push("a full server is not the old server");
  if (sl[sl.indexOf("-nds") + 1] !== "1" || sl[sl.indexOf("-maxFPS") + 1] !== "20") failures.push("a server lever (nds, sfps) does not reach the server args");
  if (serverArgs({ addonsDir: "A", profile: "SP", rp: low }).includes("-nds")) failures.push("a lowspec server changes the replication range (-nds)");
  const cf = clientArgs({ n: 1, addonsDir: "A", profile: "P", rp: full });
  const cl = clientArgs({ n: 1, addonsDir: "A", profile: "P", rp: low, res: sessionRes([], low) });
  if (cf[cf.indexOf("-maxFPS") + 1] !== "30" || cf[cf.indexOf("-screenWidth") + 1] !== "640") failures.push("a full client is not the old client");
  if (cl[cl.indexOf("-maxFPS") + 1] !== "15") failures.push("a lowspec client is not capped at 15 fps");
  if (cl[cl.indexOf("-screenWidth") + 1] !== "640" || cl[cl.indexOf("-screenHeight") + 1] !== "360") failures.push("a lowspec client is not 640x360");
  if (!cl.includes("-noSound")) failures.push("a lowspec client has sound");
  if (cl.filter((a) => a === "-scrDefine").length !== cf.filter((a) => a === "-scrDefine").length) failures.push("lowspec changed the script defines (the server would refuse the client)");
  if (sessionRes(["--res", "1920x1080"], low).width !== 1920) failures.push("an explicit --res lost to the profile");
  const dir = mkdtempSync(join(tmpdir(), "lf-profile-"));
  try {
    const steam = join(dir, "steam");
    mkdirSync(join(steam, "config"), { recursive: true });
    const vdf = ["\"users\"", "{", "  \"76561198000000001\"", "  {", "    \"MostRecent\" \"1\"", "  }", "}", ""];
    writeFileSync(join(steam, "config", "loginusers.vdf"), vdf.join(String.fromCharCode(10)));
    const written = applyClientSettings(join(dir, "c1"), low, steam);
    const eng = written.find((p) => p.includes("app1874880_user76561198000000001") && p.endsWith("ReforgerEngineSettings.conf"));
    if (!eng || !/TextureDetail 4/.test(readFileSync(eng, "utf8"))) failures.push("lowspec did not write TextureDetail 4 into the Steam user's settings");
    applyClientSettings(join(dir, "c1"), full, steam);
    if (existsSync(eng)) failures.push("a full session left the lowspec settings in the client profile");
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
  return failures;
}

export function selftestSession() {
  const failures = [];
  {
    // red-proof 2026-11-14: with sessionRelay never relaying it printed "a proof verb with a remote session ran on this PC" and "reset with a remote session ran on this PC".
    // A remote session: proofs and reset run there (a proof on this PC found no session), --local
    // keeps them here, and a proof's out dir in this worktree maps into the run folder and comes home.
    const ptr = { machine: "lf-testbox", dir: "C:/LF/Life-Framework-rt-x-session" };
    if (!sessionRelay("asset-states-proof", ["{AB}P/x.et"], ptr, true).relay) failures.push("a proof verb with a remote session ran on this PC");
    if (!sessionRelay("reset", [], ptr, false).relay) failures.push("reset with a remote session ran on this PC");
    const pinned = sessionRelay("asset-states-proof", ["x", "--local"], ptr, true);
    if (pinned.relay || pinned.rest.includes("--local")) failures.push("--local did not keep a proof on this PC");
    if (sessionRelay("asset-states-proof", [], null, true).relay) failures.push("a proof relayed with no remote session");
    if (sessionRelay("up", [], ptr, false).relay || sessionRelay("selftest", [], ptr, false).relay) failures.push("a non-session command was relayed");
    const root = join(tmpdir(), "lf-wt");
    const m = remoteArgs(["{AB}P/x.et", "hand,ground", join(root, "tmp", "assets", "l", "states"), join(tmpdir(), "elsewhere")], root, ptr.dir);
    if (m.args[2] !== `${ptr.dir}/tmp/assets/l/states` || m.args[0] !== "{AB}P/x.et" || m.args[3] !== join(tmpdir(), "elsewhere")) failures.push(`remoteArgs mapped the wrong args: ${JSON.stringify(m.args)}`);
    if (m.back.length !== 1 || m.back[0].local !== join(root, "tmp", "assets", "l", "states")) failures.push("remoteArgs does not bring the out dir home");
  }
  {
    const base = mkdtempSync(join(tmpdir(), "lf-orph-"));
    const live = new Set([11, 22, 33]);
    const w = (n, rec) => writeSession(join(base, n), rec);
    w("me", { serverPid: 1, clients: [] });
    w("healthy", { serverPid: 11, clients: [{ n: 1, pid: 22 }] });
    w("orphan", { serverPid: 99, clients: [{ n: 1, pid: 33 }] });
    const killed = [];
    const reaped = reapOrphanSessions(join(base, "me"), (pid) => live.has(pid), (pid) => { killed.push(pid); live.delete(pid); }, base);
    if (reaped.length !== 1 || killed.join() !== "33") failures.push(`orphan reap killed ${killed.join()} (want only 33)`);
    if (liveSessionsElsewhere(join(base, "me"), (pid) => live.has(pid), base).filter((o) => !o.serverUp).length) failures.push("an orphan still listed after the reap");
    rmSync(base, { recursive: true, force: true });
  }
  const p = sessionPaths("S", 2);
  if (p.clients.length !== 2) failures.push("sessionPaths did not list 2 clients");
  if (p.clients[1].box !== join("S", "client2", "profile", "agent-box")) failures.push(`client 2 box is ${p.clients[1].box}`);
  if (clientBox("S", 3) !== join("S", "client3", "profile", "agent-box")) failures.push("clientBox(3) is wrong");
  const a1 = clientArgs({ n: 1, addonsDir: "A", profile: "P1" });
  const a2 = clientArgs({ n: 2, addonsDir: "A", profile: "P2" });
  if (a1[0] !== "-client" || a1[1] !== "127.0.0.1") failures.push("client does not join 127.0.0.1");
  if (!a1.includes("-window")) failures.push("client is not windowed");
  if (a1[a1.indexOf("-profile") + 1] !== "P1") failures.push("client 1 does not use its own profile");
  if (a1[a1.indexOf("-posX") + 1] === a2[a2.indexOf("-posX") + 1]) failures.push("two clients share a window position");
  const hd = parseRes(["--res", "1920x1080"]);
  const big = clientArgs({ n: 2, addonsDir: "A", profile: "P2", res: hd });
  if (big[big.indexOf("-screenWidth") + 1] !== "1920" || big[big.indexOf("-screenHeight") + 1] !== "1080") failures.push("--res 1920x1080 does not size the client");
  if (Number(big[big.indexOf("-posX") + 1]) < 1920) failures.push("client 2 overlaps client 1 at 1920x1080");
  const def = parseRes([]);
  if (def.width < 1280 || def.height < 720) failures.push("the default client is smaller than 1280x720");
  try {
    parseRes(["--res", "big"]);
    failures.push("--res big was accepted");
  } catch {}
  const s = serverArgs({ addonsDir: "A", profile: "SP" });
  if (s[0] !== "-server" || s[1] !== SERVER_WORLD) failures.push("server does not run the bare DebugWorld");
  if (s.some((x) => /LF_AUTOTEST|LF_PlaytestTown|LF_Everon1989.conf|-config/.test(x))) failures.push("server args carry a test define, the playtest mission, or -config");
  if (s[s.indexOf("-scrDefine") + 1] !== SESSION_DEFINE) failures.push("server does not define LF_SESSION_IDENTITY");
  if (a1[a1.indexOf("-scrDefine") + 1] !== SESSION_DEFINE) failures.push("client script define differs from the server (checksum refusal)");
  if (s.includes(HARNESS_DEFINE) || a1.includes(HARNESS_DEFINE)) failures.push("the bare DebugWorld session carries LF_HARNESS");
  if (s.includes("-loadSessionSave")) failures.push("a fresh session loads a save");
  const ps = serverArgs({ addonsDir: "A", profile: "SP", mission: "playtest" });
  const pc = clientArgs({ n: 1, addonsDir: "A", profile: "P1", mission: "playtest" });
  if (ps[1] !== PACK_MISSION) failures.push("--mission playtest does not run the pack's mission header");
  for (const args of [s, ps, pc, a1]) if (args[args.indexOf("-addons") + 1] !== "LifeFramework,LF_Everon1989") failures.push(`a session launch loads ${args[args.indexOf("-addons") + 1]}, not the core and the pack`);
  const defs = (args) => args.filter((x, i) => args[i - 1] === "-scrDefine").sort().join(",");
  if (defs(ps) !== [HARNESS_DEFINE, SESSION_DEFINE].sort().join(",")) failures.push(`playtest server defines are ${defs(ps)}`);
  if (defs(pc) !== defs(ps)) failures.push("playtest client defines differ from the server (checksum refusal)");
  if (!serverArgs({ addonsDir: "A", profile: "SP", mission: "playtest", loadSave: true }).includes("-loadSessionSave")) failures.push("a restart does not load the save");
  const sc = clientArgs({ n: 1, addonsDir: "A", profile: "P1", session: "ab12" });
  if (sc[sc.indexOf("-lfSession") + 1] !== "ab12") failures.push("a client is not named with its session id");
  if (newSessionId() === newSessionId() || !/^[0-9a-f]{12}$/.test(newSessionId())) failures.push("session ids must be fresh hex tokens");
  const box1 = clientBox("S", 1);
  const live = { id: "ab12", root: "C:/x/wt-a" };
  if (sessionAddress(box1, "C:/x/wt-a", "S", live)?.id !== "ab12") failures.push("the caller's own session is addressed by id");
  if (!sessionAddress(box1, "C:\\X\\WT-A", "S", live)?.id) failures.push("root comparison must ignore case and slashes");
  if (!/other_checkout/.test(sessionAddress(box1, "C:/x/wt-b", "S", live)?.error ?? "")) failures.push("another checkout's session must be refused");
  if (!sessionAddress(box1, "C:/x/wt-a", "S", { ...live, stoppedAt: "t" })?.error) failures.push("a stopped session must be refused");
  if (sessionAddress(join("W", "agent-box"), "C:/x/wt-a", "S", live) !== null) failures.push("a Workbench box is not a session box");
  if (parseMission([]) !== "debug" || parseMission(["--mission", "playtest"]) !== "playtest") failures.push("parseMission");
  try {
    parseMission(["--mission", "prod"]);
    failures.push("--mission prod was accepted");
  } catch {}
  if (laneArgs(2001).length || laneArgs(2011).join(" ") !== "-addr 127.0.0.1:2011 -a2sPort 17877") failures.push("laneArgs: lane 1 keeps the defaults, lane 2 gets -addr and its A2S port");
  if (!serverArgs({ addonsDir: "A", profile: "SP", port: 2011 }).includes("127.0.0.1:2011") || serverArgs({ addonsDir: "A", profile: "SP" }).includes("-addr")) failures.push("serverArgs lane port");
  if (clientArgs({ n: 1, addonsDir: "A", profile: "P", port: 2011 })[1] !== "127.0.0.1:2011" || clientArgs({ n: 1, addonsDir: "A", profile: "P" })[1] !== "127.0.0.1") failures.push("clientArgs lane port");
  if (sessionDir({ LF_SESSION_DIR: "X" }) !== "X") failures.push("LF_SESSION_DIR does not override");
  if (sessionDir({ LOCALAPPDATA: "L" }, "C:/x/wt-a") !== join("L", "LifeFramework", "session", "wt-a")) failures.push("the session dir is not per checkout");
  if (sessionDir({ LOCALAPPDATA: "L" }, "C:/x/wt-a") === sessionDir({ LOCALAPPDATA: "L" }, "C:/x/wt-b")) failures.push("two checkouts share a session dir");
  const st = parseState("faction=police duty=1 wanted=0 cash=250 bank=1000 job=none org=none");
  if (st.faction !== "police" || st.cash !== "250" || st.org !== "none") failures.push("parseState lost a field");
  try {
    parseClients(["--clients", "9"]);
    failures.push("--clients 9 was accepted");
  } catch {}
  if (parseClients([]) !== 1) failures.push("default client count is not 1");
  const civ0 = "faction=CIVILIAN duty=0 wanted=0 rank=0 cash=500 bank=0 job=UNEMPLOYED org=none toast=none";
  const civ1 = "faction=CIVILIAN duty=0 wanted=1 rank=0 cash=650 bank=0 job=UNEMPLOYED org=none toast=none";
  const cop = `faction=POLICE duty=1 wanted=0 rank=0 cash=500 bank=0 job=UNEMPLOYED org=none toast=${ROB_ALERT}`;
  if (judgeProof(civ0, civ1, cop).length) failures.push(`judgeProof failed a good run: ${judgeProof(civ0, civ1, cop)}`);
  if (!judgeProof(civ0, civ1, cop.replace(ROB_ALERT, "none")).length) failures.push("judgeProof passed an officer with no alert");
  if (!judgeProof(civ0, civ0, cop).length) failures.push("judgeProof passed a robbery with no payout");
  const m1 = "faction=CIVILIAN duty=0 wanted=0 rank=0 cash=500 bank=0 job=UNEMPLOYED org=none player=1 toast=none known=0 name=none";
  const m2 = "faction=CIVILIAN duty=0 wanted=0 rank=0 cash=500 bank=0 job=UNEMPLOYED org=none player=2 toast=none known=0 name=none";
  const g1 = m1.replace("cash=500", "cash=450");
  const g2 = m2.replace("cash=500", "cash=550").replace("known=0", "known=1");
  if (judgeInteraction(m1, m2, g1, g2).length) failures.push(`judgeInteraction failed a good run: ${judgeInteraction(m1, m2, g1, g2)}`);
  if (!judgeInteraction(m1, m2, m1, g2).length) failures.push("judgeInteraction passed a giver who kept the cash");
  if (!judgeInteraction(m1, m2, g1, g2.replace("cash=550", "cash=600")).length) failures.push("judgeInteraction passed cash out of thin air");
  if (!judgeInteraction(m1, m2, g1, g2.replace("known=1", "known=0")).length) failures.push("judgeInteraction passed a receiver who never learned the name");
  const d1 = "faction=CIVILIAN duty=0 wanted=1 dispatch=0 lastdispatch=none alarm=normal";
  const d2 = `faction=POLICE duty=1 wanted=0 toast=${ROB_ALERT} dispatch=1 lastdispatch=#LF-Offence_Robbery@#LF-Sign_Fuel_Title alarm=none`;
  const q1 = d1.replace("alarm=normal", "alarm=loud");
  const q2 = d2.replace("duty=1", "duty=0").replace("faction=POLICE", "faction=CIVILIAN");
  if (judgeDispatch(d1, d2, q1, q2).length) failures.push(`judgeDispatch failed a good run: ${judgeDispatch(d1, d2, q1, q2)}`);
  if (!judgeDispatch(d1, d2.replace("dispatch=1", "dispatch=0"), q1, q2).length) failures.push("judgeDispatch passed an officer who got no dispatch");
  if (!judgeDispatch(d1, d2, d1, q2).length) failures.push("judgeDispatch passed a normal alarm with nobody on duty");
  if (!judgeDispatch(d1, d2, q1, q2.replace("dispatch=1", "dispatch=2")).length) failures.push("judgeDispatch passed a dispatch with nobody on duty");
  if (!judgeDispatch(d1, d2.replace("@#LF", "@Ada_#LF"), q1, q2).length) failures.push("judgeDispatch passed a dispatch naming the robber");
  const o1 = "faction=CIVILIAN duty=0 wanted=0 rank=0 cash=500 bank=0 job=UNEMPLOYED org=none player=1";
  const oa1 = o1.replace("cash=500", "cash=250").replace("org=none", `org=${ORG_NAME}`);
  const oa2 = o1.replace("player=1", "player=2").replace("org=none", `org=${ORG_NAME}`);
  if (judgeOrg(o1, oa1, oa2, "cannot_perform", GATE_OPENED).length) failures.push(`judgeOrg failed a good run: ${judgeOrg(o1, oa1, oa2, "cannot_perform", GATE_OPENED)}`);
  if (!judgeOrg(o1, oa1, o1, "cannot_perform", GATE_OPENED).length) failures.push("judgeOrg passed an invitee who never joined");
  if (!judgeOrg(o1, oa1, oa2, GATE_OPENED, GATE_OPENED).length) failures.push("judgeOrg passed a gate that opened for a stranger");
  if (!judgeOrg(o1, oa1.replace("cash=250", "cash=500"), oa2, "cannot_perform", GATE_OPENED).length) failures.push("judgeOrg passed a founding with no fee");
  const seen = "player=1 downed=0 life=ALIVE | player=2 downed=1 life=INCAPACITATED";
  const shotOk = "shot player=2 refused=0 alive=1 downed=1";
  const down2 = "wanted=0 player=2 downed=1 life=INCAPACITATED";
  if (judgeDowned(shotOk, down2, "wanted=0 player=1", seen, seen).length) failures.push(`judgeDowned failed a good run: ${judgeDowned(shotOk, down2, "wanted=0 player=1", seen, seen)}`);
  if (!judgeDowned("shot player=2 refused=0 alive=0 downed=0", down2, "wanted=0 player=1", seen, seen).length) failures.push("judgeDowned passed a kill");
  if (!judgeDowned(shotOk, down2, "wanted=3 player=1", seen, seen).length) failures.push("judgeDowned passed a murder report");
  if (!judgeDowned(shotOk, down2, "wanted=0 player=1", "player=2 downed=0 life=ALIVE", seen).length) failures.push("judgeDowned passed a client that sees the victim standing");
  if (judgeBleed("120", 180).length) failures.push("judgeBleed failed a good read");
  if (!judgeBleed("-1", 180).length) failures.push("judgeBleed passed a client that reads no clock");
  if (!judgeBleed("200", 180).length) failures.push("judgeBleed passed a time above the config");
  const sn = "me=MEDIC/1/0 cash=300 downed=0 life=ALIVE bleed=-1 veh=Car:d=1.2:eng=1:lock=0:ign=0:stolen=1|Van:d=5:eng=0:lock=1:ign=1:stolen=0 door=Door:d=2:locked=0 bags=3 chop=-2 toast=Paid dispatches=1 lastdispatch=Robbery";
  const pseen = parseSeen(sn);
  if (!pseen || pseen.faction !== "MEDIC" || pseen.duty !== "1" || pseen.cash !== 300 || pseen.veh.length !== 2 || pseen.veh[0].eng !== "1" || pseen.veh[1].lock !== "1" || pseen.door[0].locked !== "0" || pseen.bags !== 3 || pseen.dispatches !== 1) failures.push(`parseSeen read ${JSON.stringify(pseen)}`);
  if (parseSeen("no_character") !== null) failures.push("parseSeen accepted a non-line");
  if (parseSeen(sn.replace(/veh=\S+/, "veh=none")).veh.length !== 0) failures.push("parseSeen did not read veh=none as empty");
  {
    const reads = [sn.replace("bags=3", "bags=0"), sn];
    const on = () => ({ detail: reads.shift() ?? sn });
    const ok = clientSees(on, () => {}, 2, (s) => (s.bags === 3 ? [] : [`bags=${s.bags}`]), { budgetMs: 5000 });
    if (ok.fails.length) failures.push(`clientSees did not wait for the replicated value: ${ok.fails}`);
    const never = clientSees(() => ({ detail: sn.replace("bags=3", "bags=0") }), () => {}, 2, (s) => (s.bags === 3 ? [] : [`bags=${s.bags}`]), { budgetMs: 0 });
    if (never.fails.length !== 1 || !never.fails[0].includes("client 2 reads")) failures.push("clientSees passed a value that never replicated");
    if (!clientSees(() => ({ detail: "no_character" }), () => {}, 2, () => [], { budgetMs: 0 }).fails.length) failures.push("clientSees passed an unreadable line");
  }
  failures.push(...selftestProfile());
  return failures;
}

// The M1-12 interaction proof on a running `up --clients 2` session: both
// civilians; client 1 stands beside client 2, introduces themselves, shows the
// ID card, and hands over GIVE_AMOUNT cash through the amount sheet. Cash must
// move by exactly that amount on both sides and client 2 must know one name.
export const GIVE_AMOUNT = 50;

export function judgeInteraction(before1, before2, after1, after2) {
  const fails = [];
  const b1 = parseState(before1);
  const b2 = parseState(before2);
  const a1 = parseState(after1);
  const a2 = parseState(after2);
  if (Number(a1.cash) !== Number(b1.cash) - GIVE_AMOUNT) fails.push(`client 1 cash ${b1.cash} -> ${a1.cash}, expected -${GIVE_AMOUNT}`);
  if (Number(a2.cash) !== Number(b2.cash) + GIVE_AMOUNT) fails.push(`client 2 cash ${b2.cash} -> ${a2.cash}, expected +${GIVE_AMOUNT}`);
  if (!(Number(a2.known) >= 1)) fails.push(`client 2 knows ${a2.known} name(s), expected the introduced name`);
  return fails;
}

// Both clients pick civilian, spawn in town, and sign form R-89 (M1-08); skipped once
// characters exist.
export function arriveCivilians(on, wait) {
  for (const n of [1, 2]) {
    const ui = on(n, { verb: "ui" });
    if (ui.status === "ok" && ui.detail.includes("CivilianButton")) {
      on(n, { verb: "click", widget: "CivilianButton" });
      wait(1500);
      on(n, { verb: "click", widget: "TownButton" });
    }
  }
  wait(4000);
  const names = { 1: "Ada", 2: "Bea" };
  for (const n of [1, 2]) {
    const ui = on(n, { verb: "ui" });
    if (ui.status === "ok" && ui.detail.includes("SignButton")) {
      on(n, { verb: "type", widget: "RegFirstName", text: names[n] });
      on(n, { verb: "type", widget: "RegLastName", text: "Tester" });
      on(n, { verb: "type", widget: "RegAge", text: "30" });
      on(n, { verb: "click", widget: "SignButton" });
    }
  }
  wait(10000);
}

// The M1-17 organization proof on a running `up --clients 2` session: client 1
// opens the registry ledger, founds ORG_NAME (fee in carried cash), and takes the
// alley yard lease; client 2 is refused at the gate; client 1 walks to client 2 and
// chooses "Invite to"; client 2 faces client 1 and chooses "Join"; client 2 opens
// the hideout gate. `--shot` photographs the ledger page on client 1.
export const ORG_NAME = "AlleyKings";
export const ORG_FEE = 250;
export const ORG_GATE = "Roadmap_Hideout_Door";
export const GATE_OPENED = "SCR_DoorUserAction";

export function judgeOrg(before1, after1, after2, gateBefore, gateAfter) {
  const fails = [];
  const b1 = parseState(before1);
  const a1 = parseState(after1);
  const a2 = parseState(after2);
  if (a1.org !== ORG_NAME) fails.push(`client 1 org=${a1.org}, expected ${ORG_NAME}`);
  if (a2.org !== ORG_NAME) fails.push(`client 2 org=${a2.org}, expected ${ORG_NAME}`);
  if (Number(a1.cash) !== Number(b1.cash) - ORG_FEE) fails.push(`client 1 cash ${b1.cash} -> ${a1.cash}, expected the ${ORG_FEE} fee`);
  if (gateBefore !== "cannot_perform") fails.push(`the gate before joining answered ${gateBefore}, expected cannot_perform`);
  if (gateAfter !== GATE_OPENED) fails.push(`the gate after joining answered ${gateAfter}, expected ${GATE_OPENED} (performed)`);
  return fails;
}

// The M1-05 card proof on a running `up --clients 2` session: client 1 is a
// civilian who robs Roadmap_Test_Till (the instant world action; the fuel
// station till is robbed through its attendant's dialogue since #936), client 2 is an on-duty officer (debug
// menu, dev gate open on the bare DebugWorld) who receives the alert. Prints
// `state` for both before and after, then judges the after line.
export const ROB_TARGET = "Roadmap_Test_Till";
// The officer's last toast after a robbery: the radio dispatch line (LF_Dispatch, M1-20).
export const ROB_ALERT = "#LF-Dispatch_Radio";

export function judgeProof(before1, after1, after2) {
  const fails = [];
  const b1 = parseState(before1);
  const a1 = parseState(after1);
  const a2 = parseState(after2);
  if (a1.faction !== "CIVILIAN") fails.push(`client 1 is ${a1.faction}, expected CIVILIAN`);
  if (!(Number(a1.cash) > Number(b1.cash))) fails.push(`client 1 cash ${b1.cash} -> ${a1.cash}, expected a payout`);
  if (a1.bank !== b1.bank) fails.push(`client 1 bank moved ${b1.bank} -> ${a1.bank}; a till pays cash only`);
  if (!(Number(a1.wanted) > Number(b1.wanted))) fails.push(`client 1 wanted ${b1.wanted} -> ${a1.wanted}, expected a raise`);
  if (a2.faction !== "POLICE" || a2.duty !== "1") fails.push(`client 2 is ${a2.faction} duty=${a2.duty}, expected on-duty POLICE`);
  if (a2.toast !== ROB_ALERT) fails.push(`client 2 last toast is ${a2.toast}, expected ${ROB_ALERT}`);
  return fails;
}

// A new account sees the registration form (R-89); fill and sign it.
export const ARRIVAL_NAMES = { 1: ["Ada", "Robber"], 2: ["Bob", "Officer"], 3: ["Cy", "Walker"] };
export function arrive(on, n, wait) {
  const ui = on(n, { verb: "ui" });
  if (ui.status !== "ok" || !ui.detail.includes("SignButton")) return;
  const [first, last] = ARRIVAL_NAMES[n] ?? ["Pat", "Player"];
  on(n, { verb: "type", widget: "RegFirstName", text: first });
  on(n, { verb: "type", widget: "RegLastName", text: last });
  on(n, { verb: "type", widget: "RegAge", text: "30" });
  wait(400);
  on(n, { verb: "click", widget: "SignButton" });
  wait(1500);
}

// The session identity of a player id (LF_Utils.BuildSessionIdentity).
export function sessionUid(playerId) {
  return `00bbbddd-5e55-0000-0000-${String(playerId).padStart(12, "0")}`;
}

// The police roster file the server reads on first use ($profile:LifeFramework/whitelist.txt,
// LF_WhitelistManager format: version, faction entries, job entries). Write it before `up`.
export function rosterText(playerIds) {
  const lines = ["1", String(playerIds.length)];
  for (const id of playerIds) lines.push(sessionUid(id), "POLICE");
  lines.push("0");
  return lines.join("\n") + "\n";
}

function writeRoster(playerIds) {
  const dir = join(sessionPaths(sessionDir()).server, "profile", "LifeFramework");
  mkdirSync(dir, { recursive: true });
  const file = join(dir, "whitelist.txt");
  writeFileSync(file, rosterText(playerIds));
  console.log(`roster: ${playerIds.map(sessionUid).join(", ")} -> ${file}`);
  return 0;
}

// Runs fn while one client records itself in-engine (tools/film/engine.mjs),
// then encodes the clip. Works on the hidden LF-Agents desktop.
async function filmed(label, rest, fn) {
  requireFullProfile("filming");
  const eng = await import("../tools/film/engine.mjs");
  const h = await import("./harness.mjs");
  const i = rest.indexOf("--film-client");
  const client = i === -1 ? 1 : Number(rest[i + 1]);
  const box = clientBox(sessionDir(), client);
  const send = (spec, t) => {
    h.setBoxes([box]);
    return h.sendCommand(spec, t);
  };
  const name = `${label.replace(/[^A-Za-z0-9]/g, "_")}_${Date.now() % 100000000}`;
  const start = eng.engineStart(send, name);
  if (!start.ok) console.log(`film: start on client ${client} failed: ${start.error}`);
  const code = await fn();
  if (!start.ok) return code;
  const clip = eng.engineStop(send, (alias) => {
    h.setBoxes([box]);
    return h.resolveCapture(alias);
  }, { label: `session-${label}` });
  console.log(`film: client ${client} ${eng.describe(clip)}`);
  return code;
}

// M2 #255: client 1 downs client 2 at the fuel station with one lethal hit. Client 2 is alive,
// downed and unconscious (life=INCAPACITATED) and client 1 is not wanted: the murder hook waits
// for the real death. Both clients read the other body as downed (`peers`), a client that joins
// AFTER the downing (client 3, started by `session join 3`) reads it too, and a real death
// (`die`) of the downed player is the murder, attributed to whoever downed them, and ends in the
// normal hospital respawn. Needs a session with 2 clients.
// The time left a downed client reads (#256): a number no larger than the configured bleed-out.
export function judgeBleed(read, bleedSeconds) {
  const n = Number(read);
  if (!Number.isFinite(n) || n <= 0) return [`the downed client reads no time left: bleed=${read}`];
  if (n > bleedSeconds) return [`bleed=${n} is above the configured ${bleedSeconds}`];
  return [];
}

export function judgeDowned(shot, victim, shooter, peers1, peers2) {
  const fails = [];
  if (!/refused=0 alive=1 downed=1/.test(shot)) fails.push(`the lethal hit did not down: ${shot}`);
  const v = parseState(victim);
  if (v.downed !== "1" || v.life !== "INCAPACITATED") fails.push(`victim state: downed=${v.downed} life=${v.life}, expected downed=1 life=INCAPACITATED`);
  const sh = parseState(shooter);
  if (sh.wanted !== "0") fails.push(`shooter wanted=${sh.wanted}: a downing must not be a murder`);
  const p1 = String(peers1 ?? "");
  const p2 = String(peers2 ?? "");
  if (!/player=2 downed=1 life=INCAPACITATED/.test(p1)) fails.push(`client 1 does not see player 2 downed: ${p1}`);
  if (!/player=2 downed=1 life=INCAPACITATED/.test(p2)) fails.push(`client 2 does not see itself downed: ${p2}`);
  return fails;
}

// M2 #261: client 1 downs client 2, drags the body to the hospital desk, drops it, carries it to
// the escort car and loads it into a seat. Clients 1 and 2 and a late joiner (client 3, joined
// after the drag began) read the body at the carrier's side, then in the seat. Needs 2 clients.
export function parsePeer(peers, id) {
  const m = new RegExp(`player=${id} downed=(\\d) life=(\\w+) carrier=(-?\\d+) seat=(\\d) at=(-?[\\d.]+),(-?[\\d.]+),(-?[\\d.]+)`).exec(String(peers ?? ""));
  if (!m) return null;
  return { downed: m[1], life: m[2], carrier: Number(m[3]), seat: m[4], at: [Number(m[5]), Number(m[6]), Number(m[7])] };
}

export function judgeCarry(peerLine, id, carrierId, carrierAt, maxGap) {
  const p = parsePeer(peerLine, id);
  const c = parsePeer(peerLine, carrierId);
  if (c) carrierAt = c.at;
  if (!p) return [`no peer line for player ${id}: ${peerLine}`];
  const fails = [];
  if (p.carrier !== carrierId) fails.push(`player ${id} carrier=${p.carrier}, expected ${carrierId}`);
  const gap = Math.hypot(p.at[0] - carrierAt[0], p.at[2] - carrierAt[2]);
  if (!(gap <= maxGap)) fails.push(`player ${id} is ${gap.toFixed(1)} m from the carrier, expected within ${maxGap}`);
  return fails;
}

// `seen [radius]` runs on the asking client and reports what ITS copy of the world says: the replicated
// account state, carried cash, vehicles (engine, lock, ignition, stolen), doors, loose bags, the chop
// yard percent, the last toast and the radio calls heard. A broken RplProp or a missed BumpMe shows here
// and nowhere on the server, which is why a proof of a player-visible result reads it on the OTHER client.
export function parseSeen(detail) {
  const text = String(detail ?? "");
  const m = /^me=(\w+)\/(\d)\/(\d+) cash=(-?\d+) downed=(\d) life=(\w+) bleed=(-?\d+) veh=(\S*) door=(\S*) bags=(\d+) chop=(-?\d+) toast=(\S*) dispatches=(\d+) lastdispatch=(\S*)(?: posters=(\S+))?(?: cases=(\S+))?/.exec(text);
  if (!m) return null;
  const list = (raw) => (raw === "none" ? [] : raw.split("|").map((e) => Object.fromEntries(e.split(":").map((x, i) => (i === 0 ? ["name", x] : x.split("=")))))); 
  return { faction: m[1], duty: m[2], wanted: Number(m[3]), cash: Number(m[4]), downed: m[5], life: m[6], bleed: Number(m[7]), veh: list(m[8]), door: list(m[9]), bags: Number(m[10]), chop: Number(m[11]), toast: m[12], dispatches: Number(m[13]), lastdispatch: m[14], posters: m[15] ?? "", cases: m[16] ?? "" };
}

// Read client n (`seen` by default) until judge(parsed) returns no failures or the budget runs out (a
// replicated value lands a moment after the server writes it). Returns { line, fails, seconds }; an
// unparsable line is a failure, never a pass. `verb: "peers"` with `parse` reads the other players' bodies.
export function clientSees(on, wait, n, judge, { radius = 12, budgetMs = 10000, label = "", verb = "seen", parse = parseSeen } = {}) {
  const t0 = Date.now();
  let line;
  let fails;
  do {
    line = on(n, { verb, radius }, 10000).detail;
    const read = parse(line);
    fails = read ? judge(read, line) : [`no ${verb} line: ${line}`];
    if (!fails.length) break;
    wait(1000);
  } while (Date.now() - t0 < budgetMs);
  const seconds = (Date.now() - t0) / 1000;
  const what = label ? `${label}, ` : "";
  return { line, fails: fails.map((f) => `${what}client ${n} reads: ${f}`), seconds };
}

// Client n reads player `id`'s body in `peers` (downed, life, carrier, seat, position) until judge(peer) is clean.
export const clientSeesPeer = (on, wait, n, id, judge, opts = {}) => clientSees(on, wait, n, (peer) => judge(peer), { ...opts, verb: "peers", parse: (line) => parsePeer(line, id) });

// M2 #252 on a running `up --clients 2` session: client 2 clocks in at the hospital desk and its
// own client reads a radio on the medic frequency (the replicated tuning); client 1, a civilian,
// carries no radio. A shot of client 2 after clock-in.
export const MEDIC_RADIO_KHZ = 42000;

export function judgeRadio(onDuty, civilian) {
  const fails = [];
  if (!/radios=[1-9]/.test(onDuty)) fails.push(`the clocked-in medic reads no radio: ${onDuty}`);
  if (!new RegExp(`f=(.*,)?${MEDIC_RADIO_KHZ}(,|$)`).test(onDuty)) fails.push(`the medic radio is not on ${MEDIC_RADIO_KHZ} kHz as the client reads it: ${onDuty}`);
  if (!/radios=0/.test(civilian)) fails.push(`the civilian reads a radio: ${civilian}`);
  return fails;
}

// The ticket deadlines the server will run with (Configs/Police/LF_Police.conf).
export function ticketDeadlines(confText) {
  const first = Number(/m_iTicketFirstDeadline\s+(\d+)/.exec(confText)?.[1] ?? 1800);
  const second = Number(/m_iTicketSecondDeadline\s+(\d+)/.exec(confText)?.[1] ?? 1800);
  return { first, second };
}

// --machine <name|any|local> (tools/pool.json). Game clients need a desktop
// session; ssh runs in session 0, which has none. A machine whose pool entry has
// the "clients" cap keeps a user signed in, and `up` there starts each client on
// that desktop through an interactive scheduled task (tools/lib/interactive-launch.mjs).
// Without the cap a remote machine runs server-only sessions (--clients 0) and
// clients join it over the LAN. Drive a remote session with `run <script>` and
// fetch its screenshots and films with `pull`. The chosen remote is remembered per worktree
// in tmp/session-remote.json, so down/status/state/restart from this worktree
// follow it (the local session dir is shared by every checkout on this PC).
function remotePointerFile() {
  return join(ROOT, "tmp", "session-remote.json");
}

export function readRemotePointer() {
  try {
    return JSON.parse(readFileSync(remotePointerFile(), "utf8"));
  } catch {
    return null;
  }
}

const SESSION_QUEUE_MIN = 90;

function pidIsAlive(pid) {
  if (!pid) return false;
  try {
    process.kill(pid, 0);
    return true;
  } catch (e) {
    return e.code === "EPERM";
  }
}

// Runs the local leg of `up` (tools/cli.mjs session up) and returns its exit code. LF_SESSION_ROUTED
// tells `tools\cli session up` that the machine is already chosen (it does not route again).
function runLocalUp(args) {
  const r = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "up", ...args], { windowsHide: true, stdio: "inherit", cwd: process.cwd(), env: { ...process.env, LF_SESSION_ROUTED: "1" } });
  return r.status ?? 1;
}

async function routeMachine(cmd, rest) {
  const i = rest.indexOf("--machine");
  const want = i === -1 ? null : String(rest[i + 1] ?? "");
  const args = i === -1 ? rest : rest.filter((_, k) => k !== i && k !== i + 1);
  const pool = await import("../tools/remote/pool.mjs");
  const machines = pool.loadMachines(ROOT);
  let ptr = readRemotePointer();
  if (cmd !== "up") {
    if (want === "local" || !ptr) return { local: true, args };
    const m = pool.findMachine(machines, ptr.machine);
    if (!m) throw new Error(`session is on ${ptr.machine}, which is not in the pool any more; delete ${remotePointerFile()}`);
    console.log(`session: on ${m.name} (${ptr.dir})`);
    const code = await pool.runIn(m, ptr.dir, "agent-box/session.mjs", [cmd, ...args]);
    if (cmd === "down" && code === 0) rmSync(remotePointerFile(), { force: true });
    // ssh failed (255) and the machine does not answer: it went away and took its session with it.
    // `down` clears the record; the rest say so.
    if (code === 255 && !pool.probe(ROOT, m, { fresh: true }).reachable) {
      pool.markDown(ROOT, m, "went away during a session");
      if (cmd === "down") {
        rmSync(remotePointerFile(), { force: true });
        console.log(`session: ${m.name} does not answer; its session went with it. Record cleared; the next 'up' picks another machine`);
        return { code: 0 };
      }
      console.log(`session: ${m.name} does not answer (the session there is gone with it); 'node agent-box/session.mjs down' clears the record, then 'up' again`);
      return { code: 1 };
    }
    return { code };
  }
  if (want === "local") return { local: true, args };
  const clients = parseClients(args);
  const profile = parseSessionProfile(args).profile;
  // A remote leg ships this worktree's resourceDatabase.rdb: refresh it here first, before the queue,
  // as `test` does (a fresh worktree sent a 0.1 MiB rdb and the box ran its own headless Workbench).
  spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "rdb"], { windowsHide: true, stdio: "inherit", cwd: ROOT });
  // A record whose machine went away is not a session any more: forget it.
  if (ptr) {
    const pm = pool.findMachine(machines, ptr.machine);
    if (pm && !pool.probe(ROOT, pm, { fresh: true }).reachable) {
      console.log(`session: the recorded session on ${ptr.machine} went away with that machine; record cleared`);
      rmSync(remotePointerFile(), { force: true });
      ptr = null;
    }
  }
  if (want && want !== "any") {
    const target = pool.findMachine(machines, want);
    if (!target) throw new Error(`--machine ${want}: not in the pool (${machines.map((m) => m.name).join(", ")})`);
    if (target.via === "local") return { local: true, args };
    const code = await upOnRemote(pool, target, args, clients, ptr);
    return { code: code === UP_MACHINE_GONE ? 1 : code };
  }
  // A session of this worktree already runs on this PC: `up` reports it.
  const live = readSession(sessionDir());
  if (live && !live.stoppedAt && [live.serverPid, ...(live.clients ?? []).map((c) => c.pid)].some((p) => pidIsAlive(p))) return { local: true, args };
  // The default: every machine that can run this session is considered (this PC first on a tie),
  // laptop included, for any client count and profile. With --wait the request queues for whichever
  // machine has a lane and the RAM first; the chosen machine's `up` runs without --wait, so a lost
  // race there (LANE_BUSY) or a machine that goes away puts the request back in the queue here
  // instead of pinning it to one machine.
  const waitMs = args.includes("--wait") ? SESSION_QUEUE_MIN * 60000 : 0;
  const prio = args.indexOf("--priority") === -1 ? undefined : args[args.indexOf("--priority") + 1];
  const legArgs = args.filter((a) => a !== "--wait");
  const deadline = Date.now() + waitMs;
  for (;;) {
    let target;
    try {
      target = await pool.chooseSessionMachine(ROOT, { clients, profile, waitMs: Math.max(0, deadline - Date.now()), priority: prio, meta: { clients, profile } });
    } catch (e) {
      if (!e.refusal) throw e;
      console.log(`session: ${e.message}`);
      return { code: 1 };
    }
    if (!target) {
      if (waitMs > 0) {
        console.log(`session: no session lane freed on any machine within ${SESSION_QUEUE_MIN} min`);
        return { code: 1 };
      }
      // No wait asked: one look at this PC, which says why it cannot start.
      return { local: true, args };
    }
    const code = target.via === "local" ? runLocalUp(legArgs) : await upOnRemote(pool, target, legArgs, clients, ptr);
    const again = code === pool.LANE_BUSY_CODE || code === UP_MACHINE_GONE;
    if (!again || waitMs <= 0 || Date.now() > deadline) return { code: code === UP_MACHINE_GONE ? 1 : code };
    console.log(`session: ${code === UP_MACHINE_GONE ? `${target.name} went away` : `lost the race for a lane on ${target.name}`}; queuing again for any machine`);
    // A lane that looked free and was not: give the holder a moment before asking again.
    Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, 3000);
  }
}

// Exit code of upOnRemote when the machine went away during the remote `up` (the request requeues).
const UP_MACHINE_GONE = 254;

// `up` on a pool machine: sync this worktree there and run its own `session up` (pinned to itself,
// no --wait: the queue is here). Returns the exit code; UP_MACHINE_GONE when the machine went away.
async function upOnRemote(pool, target, args, clients, ptr) {
  if (clients > 0 && !target.caps.includes("clients")) {
    console.log(`session: ${target.name} cannot start game clients over ssh (no desktop session); use --clients 0 for a server-only session there and join it from this PC`);
    return 2;
  }
  if (target.maxClients && clients > target.maxClients) {
    console.log(`session: ${target.name} runs at most ${target.maxClients} client(s) per session (maxClients in tools/pool.json: heat); ask for fewer or use --machine local`);
    return 2;
  }
  // A record on the same machine is not a refusal: the box's own `up` says whether that session still lives, and a stale one is replaced.
  if (ptr && ptr.machine !== target.name) {
    console.log(`session: one is recorded on ${ptr.machine}; 'node agent-box/session.mjs down' first`);
    return 1;
  }
  console.log(`session: on ${target.name}${clients > 0 ? ` (${clients} client(s) on its desktop)` : " (server only)"}`);
  // The pointer is written before the remote up runs: an up that is killed or times out mid-way can leave a live server on the box, and without a pointer `down` would say "none recorded" while the box's up refuses.
  const writePtr = (dir) => {
    mkdirSync(dirname(remotePointerFile()), { recursive: true });
    writeFileSync(remotePointerFile(), JSON.stringify({ machine: target.name, host: target.host, dir, at: new Date().toISOString(), root: ROOT }));
  };
  const r = await pool.syncAndRun(ROOT, target, { script: "agent-box/session.mjs", args: ["up", "--machine", "local", ...args], onSlot: (rDir) => writePtr(rDir) });
  if (r.code === 0) {
    writePtr(r.rDir);
    return 0;
  }
  // Nothing started there: the record goes, so the next machine is free to take the request.
  const gone = r.code === 255 || r.code === 3 ? !pool.probe(ROOT, target, { fresh: true }).reachable : false;
  if (r.code === pool.LANE_BUSY_CODE || gone) rmSync(remotePointerFile(), { force: true });
  if (gone) {
    pool.markDown(ROOT, target, "went away during session up");
    return UP_MACHINE_GONE;
  }
  return r.code;
}

// M1-20 on a running `up --clients 2` session with client 2 on the roster
// (`session.mjs roster 2` before up): client 2 clocks in, client 1 robs the
// till. Client 1 hears the normal alarm; client 2 gets the radio dispatch (place
// and description, no name) and reads it on the wanted board. Client 2 clocks
// out, the till cooldown runs out, client 1 robs again: nobody is on duty, so
// the alarm is the loud one and no dispatch goes out.
export const TILL_COOLDOWN_MS = 61000;

export function judgeDispatch(robbed1, robbed2, loud1, loud2) {
  const fails = [];
  const r1 = parseState(robbed1);
  const r2 = parseState(robbed2);
  const l1 = parseState(loud1);
  const l2 = parseState(loud2);
  if (r1.alarm !== "normal") fails.push(`client 1 hears alarm=${r1.alarm} with an officer on duty, expected normal`);
  if (r2.dispatch !== "1") fails.push(`client 2 dispatch=${r2.dispatch} after the first robbery, expected 1`);
  if (!String(r2.lastdispatch ?? "").startsWith("#LF-Offence_Robbery@")) fails.push(`client 2 lastdispatch=${r2.lastdispatch}, expected the robbery and its place`);
  // The robber's first name (the surname "Robber" is inside the offence key itself).
  for (const name of [ARRIVAL_NAMES[1][0], ARRIVAL_NAMES[1].join("_")]) {
    if (String(r2.lastdispatch ?? "").includes(name)) fails.push(`client 2's dispatch names the robber (${name})`);
  }
  if (r2.toast !== ROB_ALERT) fails.push(`client 2 last toast is ${r2.toast}, expected ${ROB_ALERT}`);
  if (l1.alarm !== "loud") fails.push(`client 1 hears alarm=${l1.alarm} with nobody on duty, expected loud`);
  if (l2.dispatch !== "1") fails.push(`client 2 dispatch=${l2.dispatch} off duty, expected still 1 (no dispatch with nobody on duty)`);
  if (l2.duty !== "0") fails.push(`client 2 duty=${l2.duty} for the loud robbery, expected 0`);
  return fails;
}

// `run <script> [args]`: runs node <script> where the recorded session runs. On a
// pool machine (session up --machine) the journey, harness, and proofs have to
// run there: they talk to the clients through files in that machine's session dir.
async function runWhereSession(rest) {
  const [script, ...args] = rest;
  if (!script) throw new Error("run <script> [args...]");
  const ptr = readRemotePointer();
  if (!ptr) {
    const r = spawnSync(process.execPath, [join(ROOT, script), ...args], { windowsHide: true, stdio: "inherit", cwd: ROOT });
    return r.status ?? 1;
  }
  const pool = await import("../tools/remote/pool.mjs");
  const m = pool.findMachine(pool.loadMachines(ROOT), ptr.machine);
  if (!m) throw new Error(`session is on ${ptr.machine}, which is not in the pool any more`);
  console.log(`session: running ${script} on ${m.name} (${ptr.dir})`);
  return pool.runIn(m, ptr.dir, script, args);
}

// `pull [dest]`: copies the remote session's evidence here: the run folder's
// agent-box/reports (journey.html, films) and each client's newest el-captures.
// Lands in tmp/session-pull/<machine>-<time>/ unless dest is given.
async function pullFromSession(rest) {
  const ptr = readRemotePointer();
  if (!ptr) {
    console.log("pull: no remote session recorded; the evidence is already on this PC");
    return 0;
  }
  const tb = await import("../tools/remote/testbox.mjs");
  const stamp = new Date().toISOString().replace(/[:.]/g, "-").slice(0, 19);
  const dest = resolve(rest[0] || join(ROOT, "tmp", "session-pull", `${ptr.machine}-${stamp}`));
  mkdirSync(dest, { recursive: true });
  const zip = `${ptr.dir}\\tmp\\session-pull.zip`;
  const ps = [
    `$stage = Join-Path ${tb.psQuote(ptr.dir)} 'tmp\\session-pull'`,
    "if (Test-Path $stage) { Get-ChildItem -LiteralPath $stage -Recurse -File | Remove-Item -Force; Get-ChildItem -LiteralPath $stage -Recurse -Directory | Sort-Object FullName -Descending | Remove-Item -Force }",
    "New-Item -ItemType Directory -Force $stage | Out-Null",
    `$reports = Join-Path ${tb.psQuote(ptr.dir)} 'agent-box\\reports'`,
    "if (Test-Path $reports) { Copy-Item -Recurse -Force $reports (Join-Path $stage 'reports') }",
    // The run folder's own session dir (sessionDir: one per checkout, named after its folder).
    `$session = Join-Path $env:LOCALAPPDATA ${tb.psQuote(join("LifeFramework", "session", basename(String(ptr.dir).replace(/[\\/]+$/, ""))))}`,
    "Get-ChildItem -LiteralPath $session -Directory -Filter 'client*' -ErrorAction SilentlyContinue | ForEach-Object {",
    "  $logs = Join-Path $_.FullName 'logs'",
    "  $newest = Get-ChildItem -LiteralPath $logs -Directory -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 1",
    "  if ($newest) { $to = Join-Path $stage $_.Name; New-Item -ItemType Directory -Force $to | Out-Null; Copy-Item -Force (Join-Path $newest.FullName 'console.log') $to -ErrorAction SilentlyContinue; $cap = Join-Path $newest.FullName 'el-captures'; if (Test-Path $cap) { Copy-Item -Recurse -Force $cap $to } }",
    "}",
    `if (Test-Path ${tb.psQuote(zip)}) { Remove-Item -Force ${tb.psQuote(zip)} }`,
    `Compress-Archive -Path (Join-Path $stage '*') -DestinationPath ${tb.psQuote(zip)}`,
    "exit 0",
  ].join("\n");
  const r = tb.sshPs(ptr.host, ps, { timeout: 900000 });
  if (r.status !== 0) {
    console.log(`FAIL  pull: staging on ${ptr.machine} failed: ${(r.stderr || r.stdout || "").trim().slice(-400)}`);
    return 1;
  }
  const local = join(dest, "session-pull.zip");
  const c = tb.scp([`${ptr.host}:${zip.replace(/\\/g, "/")}`, local]);
  if (c.status !== 0) {
    console.log(`FAIL  pull: scp failed: ${(c.stderr || "").trim()}`);
    return 1;
  }
  // Windows tar (bsdtar) reads zip; a Git Bash PATH finds GNU tar first, which takes "C:" for a host.
  const x = spawnSync(join(process.env.SystemRoot || "C:\\Windows", "System32", "tar.exe"), ["-xf", local, "-C", dest], { windowsHide: true, encoding: "utf8" });
  if (x.status !== 0) {
    console.log(`FAIL  pull: unpack failed: ${(x.stderr || "").trim()}`);
    return 1;
  }
  rmSync(local, { force: true });
  console.log(`pull: ${ptr.machine} evidence -> ${dest}`);
  return 0;
}

// Proof verbs live one per file in agent-box/proofs/<verb>.mjs, each default-exporting
// { name, usage, run(ctx), film? }. Adding a proof adds a file; nothing here lists them.
const PROOFS_DIR = join(dirname(fileURLToPath(import.meta.url)), "proofs");

export async function loadProofs() {
  const out = new Map();
  for (const f of readdirSync(PROOFS_DIR).filter((n) => n.endsWith(".mjs") && !n.startsWith("_")).sort()) {
    const mod = (await import(pathToFileURL(join(PROOFS_DIR, f)).href)).default;
    if (!mod || mod.name !== basename(f, ".mjs") || typeof mod.run !== "function") throw new Error(`proofs/${f}: default export must be { name: "${basename(f, ".mjs")}", usage, run(ctx) }`);
    out.set(mod.name, mod);
  }
  return out;
}

export async function usageText() {
  const proofs = [...(await loadProofs()).values()];
  const verbs = proofs.map((p) => p.usage + (p.film ? " [--film]" : ""));
  return [
    "usage: node agent-box/session.mjs up [--clients N (default 1)] [--hold min] [--mission debug|playtest] | wait [--timeout s] | join <n> | restart | down | status | reset [--strict] | state <client> | film <seconds> [--client N] | lease [minutes] | roster <id>... | run <script> [args] | pull [dest] | selftest",
    `proof verbs (agent-box/proofs/<verb>.mjs): ${verbs.join(" | ")}`,
  ].join("\n");
}

// Brings every running client back to a living civilian off duty with no wanted level (#496).
async function runReset(args = []) {
  const h = await import("./harness.mjs");
  const { resetClients } = await import("./proofs/_reset.mjs");
  const dir = sessionDir();
  const current = readSession(dir);
  if (!current || current.stoppedAt) {
    console.log("reset: no session; node agent-box/session.mjs up --clients 2 --wait");
    return 1;
  }
  const count = Math.max(1, (current.clients ?? []).length || 2);
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    return h.sendCommand(spec, timeout);
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  let dirty = false;
  for (const { n, notes } of resetClients(on, wait, count)) {
    console.log(`reset client ${n}: ${notes.length ? notes.join("; ") : "already clean"}`);
    if (notes.some((x) => x.startsWith("still dirty") || x.includes("not reset"))) dirty = true;
  }
  return dirty && args.includes("--strict") ? 1 : 0;
}

// Commands that act on the live session's clients: they must run where the session runs. With a
// remote session recorded (tmp/session-remote.json) they are relayed there, like harness.mjs does;
// --local keeps one on this PC. Proof verbs count too (a proof drives the harness in-process, so on
// this PC it found no session: `reset: no session`, `no_live_session`, 2026-11-14).
const SESSION_BOUND = new Set(["reset", "roster", "join", "film"]);

// Pure: maps the absolute paths inside root among args to the same place in the remote run folder
// rdir, so a proof's out dir lands in its run folder; `back` lists what to copy home afterwards.
export function remoteArgs(args, root, rdir) {
  const base = resolve(root).replaceAll("\\", "/").replace(/\/$/, "");
  const rbase = String(rdir).replaceAll("\\", "/").replace(/\/$/, "");
  const back = [];
  const out = args.map((a) => {
    if (typeof a !== "string" || !/^[A-Za-z]:[\\/]/.test(a)) return a;
    const p = resolve(a).replaceAll("\\", "/");
    if (p.toLowerCase() !== base.toLowerCase() && !p.toLowerCase().startsWith(`${base.toLowerCase()}/`)) return a;
    const remote = `${rbase}${p.slice(base.length)}`;
    back.push({ remote, local: resolve(a) });
    return remote;
  });
  return { args: out, back };
}

// Pure: whether cmd runs where the recorded remote session runs. --local keeps it here (and is
// dropped from the args either way for a session-bound command).
export function sessionRelay(cmd, rest, ptr, isProof) {
  if (!cmd || !(SESSION_BOUND.has(cmd) || isProof)) return { relay: false, rest };
  if (rest.includes("--local")) return { relay: false, rest: rest.filter((a) => a !== "--local") };
  return { relay: !!ptr, rest };
}

async function relayToSession(cmd, rest, ptr) {
  const pool = await import("../tools/remote/pool.mjs");
  const tb = await import("../tools/remote/testbox.mjs");
  const m = pool.findMachine(pool.loadMachines(ROOT), ptr.machine);
  if (!m) throw new Error(`session is on ${ptr.machine}, which is not in the pool any more`);
  const { args, back } = remoteArgs(rest, ROOT, ptr.dir);
  console.log(`session: ${cmd} runs where the session runs: ${m.name} (${ptr.dir})`);
  const code = await pool.runIn(m, ptr.dir, "agent-box/session.mjs", [cmd, ...args]);
  // What the command wrote into paths of this worktree comes home (a proof's pictures).
  for (const b of back) {
    mkdirSync(dirname(b.local), { recursive: true });
    const r = tb.scp(["-r", `${m.host}:${b.remote}`, dirname(b.local)]);
    if (r.status === 0) console.log(`session: copied ${b.remote} from ${m.name} to ${b.local}`);
  }
  return code;
}

async function main(argv) {
  let [cmd, ...rest] = argv;
  if (rest.includes("--help") || rest.includes("-h") || cmd === "help" || cmd === "--help") {
    console.log("usage: node agent-box/session.mjs up [--profile lowspec|full (default lowspec; shots, filming and --res/--visible/--sound mean full)] [--clients N (default 1)] [--hold min (max 45)] [--mission debug|playtest] [--res WxH] [--visible] [--sound] [--wait] [--machine any|local|<name>] | wait | restart | down | status | state <n> | run <script> [args] | pull [dest] | <proof>  (--help never starts a session)");
    return 0;
  }
  {
    const ptr = readRemotePointer();
    const r = sessionRelay(cmd, rest, ptr, ptr && cmd && !SESSION_BOUND.has(cmd) ? (await loadProofs()).has(cmd) : false);
    if (r.relay) return relayToSession(cmd, r.rest, ptr);
    rest = r.rest;
  }
  // Any session command is activity for the pool's lease (tools/remote/pool.mjs reapIdleSessions).
  if (cmd && !["down", "selftest", "lease"].includes(cmd)) {
    touchLease(sessionDir(), cmd);
    const warn = takeHoldWarning(sessionDir());
    if (warn) console.log(warn);
  }
  if (cmd === "lease") {
    const min = Number(rest[0] ?? 30);
    if (!Number.isFinite(min) || min <= 0) {
      console.log("usage: node agent-box/session.mjs lease [minutes]   keep this session's slot for that long even idle (default 30)");
      return 1;
    }
    console.log(`session: lease held until ${extendLease(sessionDir(), min)}`);
    return 0;
  }
  if (cmd === "reset") return runReset(rest);
  if (cmd === "run") return runWhereSession(rest);
  if (cmd === "pull") return pullFromSession(rest);
  if (["up", "down", "status", "restart", "state", "wait"].includes(cmd)) {
    const routed = await routeMachine(cmd, rest);
    if (!routed.local) return routed.code;
    rest = routed.args;
  }
  if (cmd === "film") {
    requireFullProfile("film");
    const r = spawnSync(process.execPath, [join(ROOT, "tools", "film", "film.mjs"), "engine", ...rest, ...(rest.includes("--client") ? [] : ["--client", "1"])], { windowsHide: true, stdio: "inherit" });
    return r.status ?? 1;
  }
  if (cmd === "selftest") {
    const f = selftestSession();
    const launch = await import("../tools/lib/interactive-launch.mjs");
    f.push(...launch.selftest().map((x) => `interactive launch: ${x}`));
    if (!clientArgs({ n: 1, addonsDir: "a", profile: "p" }).includes("-noSound")) f.push("a default client is not -noSound");
    if (clientArgs({ n: 1, addonsDir: "a", profile: "p", sound: true }).includes("-noSound")) f.push("--sound still passes -noSound");
    f.push(...(await import("./proofs/_reset.mjs")).selftestReset().map((x) => `reset: ${x}`));
    for (const x of f) console.log(`  ${x}`);
    console.log(f.length ? "session selftest: FAIL" : "session selftest: OK");
    return f.length ? 1 : 0;
  }
  if (cmd === "up" || cmd === "down" || cmd === "status" || cmd === "restart" || cmd === "wait" || cmd === "join") {
    const r = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", cmd, ...rest], { windowsHide: true, stdio: "inherit", cwd: process.cwd(), env: { ...process.env, LF_SESSION_ROUTED: "1" } });
    return r.status ?? 1;
  }
  const proofs = await loadProofs();
  const proof = proofs.get(cmd);
  if (proof) {
    // Each proof starts from a clean player: the last one's duty, wanted and downed state must not leak (#496).
    if (!rest.includes("--no-reset") && !proof.noReset) await runReset([]);
    rest = rest.filter((a) => a !== "--no-reset");
    if (proof.film && rest.includes("--film")) return filmed(cmd, rest, () => proof.run({ args: rest }));
    return proof.run({ args: rest });
  }
  if (cmd === "roster") {
    const ids = rest.map(Number).filter((n) => Number.isInteger(n) && n > 0);
    if (!ids.length) throw new Error("roster <player id> [<player id> ...]");
    return writeRoster(ids);
  }
  if (cmd === "state") {
    const n = Number(rest[0]);
    if (!Number.isInteger(n) || n < 1) throw new Error("state <client number>");
    const { sendCommand, setBoxes } = await import("./harness.mjs");
    setBoxes([clientBox(sessionDir(), n)]);
    const r = sendCommand({ verb: "state" }, 10000);
    if (r.status !== "ok") {
      console.log(`client ${n}: ${r.status} ${r.detail}`);
      return 1;
    }
    console.log(`client ${n}: ${r.detail}`);
    return 0;
  }
  if (cmd === "--help" || cmd === "help") {
    console.log(await usageText());
    return 0;
  }
  console.log(await usageText());
  return cmd ? 1 : 0;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  main(process.argv.slice(2)).then(
    (c) => process.exit(c),
    (e) => {
      console.error(e.message);
      process.exit(1);
    },
  );
}
