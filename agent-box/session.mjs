#!/usr/bin/env node
// Multi-client session: one dedicated server plus N game clients on this PC,
// each client with its own -profile, joined over 127.0.0.1:2001.
//
//   node agent-box/session.mjs up --clients 2     boot the server, launch the clients
//   node agent-box/session.mjs up --clients 2 --mission playtest   LF_PlaytestTown, harness build
//   node agent-box/session.mjs restart            save, stop, load, rejoin (playtest mission)
//   node agent-box/session.mjs wait [--timeout 600]   block until a starting session is ready (exit 0), or say why not
//   node agent-box/session.mjs down               close every process the session started
//   node agent-box/session.mjs status             pids, profiles, logs
//   node agent-box/session.mjs state <client>     faction, duty, wanted, cash, bank, job, org
//   node agent-box/session.mjs film <seconds> [--client 1] [--fps 12]   in-engine clip of one client
//   node agent-box/session.mjs proof --film [--film-client 1]          the proof, filmed in-engine
//
// Any harness verb targets one client with --client <n>:
//   node agent-box/harness.mjs --client 2 look
//
// The server runs the bare DebugWorld (no mission header, no test define), so
// the world's game mode keeps dev tools open and the harness RPCs pass
// LF_DevGate. The playtest mission (LF_PlaytestTown.conf) closes that gate.
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

import { spawnSync } from "node:child_process";
import { randomBytes } from "node:crypto";
import { existsSync, mkdirSync, readdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { basename, dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { PLAYTEST_MISSION } from "../tools/host/host.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..");

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
export function sessionBase(env = process.env) {
  const base = env.LOCALAPPDATA || join(env.USERPROFILE || ".", "AppData", "Local");
  return join(base, "LifeFramework", "session");
}

export function sessionDir(env = process.env, root = ROOT) {
  if (env.LF_SESSION_DIR) return env.LF_SESSION_DIR;
  return join(sessionBase(env), basename(resolve(root)));
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
    if (s && !s.stoppedAt && (alive(s.serverPid) || (s.clients ?? []).some((c) => alive(c.pid)))) out.push({ dir: other, root: s.root });
  }
  return out;
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
  return sessionAddress(box, root, dir, readSession(dir));
}

export function serverArgs({ addonsDir, profile, mission = "debug", loadSave = false }) {
  const args = [
    "-server", mission === "playtest" ? PLAYTEST_MISSION : SERVER_WORLD,
    "-addonsDir", addonsDir,
    "-addons", "LifeFramework",
    "-profile", profile,
    "-maxFPS", "30",
    "-logLevel", "normal",
    // -noBackend clients all carry the NULL identity and the same Steam name;
    // LF_Utils.GetPlayerUID gives each player id its own identity under this define.
    ...defineArgs(mission),
    "-disableCrashReporter",
    "-noBackend",
  ];
  if (loadSave) args.push("-loadSessionSave");
  return args;
}

// Small windows in a row along the top of the screen, so the user's desk
// stays usable. -screenWidth/-screenHeight/-posX/-posY need -window.
export function windowFor(n, res = WINDOW) {
  return { x: (n - 1) * (res.width + WINDOW.gap), y: 0, width: res.width, height: res.height };
}

export function clientArgs({ n, addonsDir, profile, address = "127.0.0.1", mission = "debug", session = "", res = WINDOW, sound = false }) {
  const w = windowFor(n, res);
  return [
    "-client", address,
    "-addonsDir", addonsDir,
    "-addons", "LifeFramework",
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
    "-maxFPS", "30",
    ...defineArgs(mission),
    "-disableCrashReporter",
    "-noBackend",
    ...(session ? ["-lfSession", session] : []),
  ];
}

export function parseClients(args) {
  const i = args.indexOf("--clients");
  const n = i === -1 ? 2 : Number(args[i + 1]);
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

export function selftestSession() {
  const failures = [];
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
  if (s.some((x) => /LF_AUTOTEST|LF_PlaytestTown|-config/.test(x))) failures.push("server args carry a test define, the playtest mission, or -config");
  if (s[s.indexOf("-scrDefine") + 1] !== SESSION_DEFINE) failures.push("server does not define LF_SESSION_IDENTITY");
  if (a1[a1.indexOf("-scrDefine") + 1] !== SESSION_DEFINE) failures.push("client script define differs from the server (checksum refusal)");
  if (s.includes(HARNESS_DEFINE) || a1.includes(HARNESS_DEFINE)) failures.push("the bare DebugWorld session carries LF_HARNESS");
  if (s.includes("-loadSessionSave")) failures.push("a fresh session loads a save");
  const ps = serverArgs({ addonsDir: "A", profile: "SP", mission: "playtest" });
  const pc = clientArgs({ n: 1, addonsDir: "A", profile: "P1", mission: "playtest" });
  if (ps[1] !== PLAYTEST_MISSION) failures.push("--mission playtest does not run LF_PlaytestTown");
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
  if (sessionDir({ LF_SESSION_DIR: "X" }) !== "X") failures.push("LF_SESSION_DIR does not override");
  if (sessionDir({ LOCALAPPDATA: "L" }, "C:/x/wt-a") !== join("L", "LifeFramework", "session", "wt-a")) failures.push("the session dir is not per checkout");
  if (sessionDir({ LOCALAPPDATA: "L" }, "C:/x/wt-a") === sessionDir({ LOCALAPPDATA: "L" }, "C:/x/wt-b")) failures.push("two checkouts share a session dir");
  const st = parseState("faction=police duty=1 wanted=0 cash=250 bank=1000 job=none org=none");
  if (st.faction !== "police" || st.cash !== "250" || st.org !== "none") failures.push("parseState lost a field");
  try {
    parseClients(["--clients", "9"]);
    failures.push("--clients 9 was accepted");
  } catch {}
  if (parseClients([]) !== 2) failures.push("default client count is not 2");
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
function arriveCivilians(on, wait) {
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

async function runInteraction() {
  const h = await import("./harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = (n) => on(n, { verb: "state" }, 10000).detail;

  // Registration form R-89 (M1-08): each player signs a name (any faction screen first).
  arriveCivilians(on, wait);

  const before1 = state(1);
  const before2 = state(2);
  const other = `Player${parseState(before2).player}`;
  on(1, { verb: "stand", entityName: other });
  wait(2500);
  on(1, { verb: "look" });
  const steps = [];
  steps.push(on(1, { verb: "use", className: "LF_IntroduceAction", entityName: other }, 10000));
  wait(2500);
  steps.push(on(1, { verb: "use", className: "LF_ShowIDAction", entityName: other }, 10000));
  wait(2000);
  steps.push(on(1, { verb: "use", className: "LF_GiveCashAction", entityName: other }, 10000));
  wait(2000);
  steps.push(on(1, { verb: "type", widget: "GiveCashAmount", text: String(GIVE_AMOUNT) }));
  steps.push(on(1, { verb: "click", widget: "GiveCashButton" }));
  wait(3000);
  const after1 = state(1);
  const after2 = state(2);

  console.log("");
  console.log(`before  client 1: ${before1}`);
  console.log(`before  client 2: ${before2}`);
  console.log(`after   client 1: ${after1}`);
  console.log(`after   client 2: ${after2}`);
  const fails = judgeInteraction(before1, before2, after1, after2);
  for (const st of steps) if (st.status !== "ok") fails.unshift(`${st.line} returned ${st.status} ${st.detail}`);
  for (const x of fails) console.log(`FAIL  ${x}`);
  console.log(fails.length ? "interaction: FAIL" : `interaction: PASS (introduced, showed ID, handed over ${GIVE_AMOUNT} cash)`);
  return fails.length ? 1 : 0;
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

async function runOrg(args) {
  const h = await import("./harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = (n) => on(n, { verb: "state" }, 10000).detail;

  arriveCivilians(on, wait);
  const before1 = state(1);
  const before2 = state(2);
  const one = `Player${parseState(before1).player}`;
  const two = `Player${parseState(before2).player}`;

  // Client 1 founds the organization at the registry desk and takes the yard lease.
  const steps = [];
  on(1, { verb: "stand", entityName: "Roadmap_Org_Desk" });
  wait(2500);
  steps.push(on(1, { verb: "use", className: "LF_OrganizationDeskAction", entityName: "Roadmap_Org_Desk" }, 10000));
  wait(1500);
  steps.push(on(1, { verb: "type", widget: "OrgName", text: ORG_NAME }));
  steps.push(on(1, { verb: "click", widget: "FoundButton" }));
  wait(3000);
  steps.push(on(1, { verb: "click", widget: "OrgHideoutRow_Action" }));
  wait(2500);
  on(1, { verb: "ui" });
  if (args.includes("--shot")) {
    h.setBoxes([clientBox(dir, 1)]);
    const shot = h.sendCommand({ verb: "capture", mode: "ui", name: "org_ledger" }, 25000);
    console.log(`client 1: shot ui org_ledger -> ${shot.status} ${shot.detail}`);
    wait(3000);
    console.log(`client 1: ledger shot ${h.resolveCapture(shot.detail) || "not written"}`);
  }
  steps.push(on(1, { verb: "click", widget: "CloseButton" }));

  // The lease shut the gate. Client 2 is not a member yet: the gate refuses.
  on(2, { verb: "stand", entityName: ORG_GATE });
  wait(2500);
  const gateBefore = on(2, { verb: "use", className: "SCR_DoorUserAction", entityName: ORG_GATE }, 10000).detail;

  // Face to face: client 1 invites, client 2 joins.
  on(1, { verb: "stand", entityName: two });
  wait(2500);
  on(1, { verb: "look" });
  steps.push(on(1, { verb: "use", className: "LF_PlayerRoleAction", entityName: two }, 10000));
  wait(2000);
  on(2, { verb: "look" });
  steps.push(on(2, { verb: "use", className: "LF_PlayerRoleAction", entityName: one }, 10000));
  wait(2500);

  // Client 2 is a member now: the gate opens.
  on(2, { verb: "stand", entityName: ORG_GATE });
  wait(2500);
  const gateAfter = on(2, { verb: "use", className: "SCR_DoorUserAction", entityName: ORG_GATE }, 10000).detail;
  wait(1500);
  const after1 = state(1);
  const after2 = state(2);

  console.log("");
  console.log(`before  client 1: ${before1}`);
  console.log(`before  client 2: ${before2}`);
  console.log(`after   client 1: ${after1}`);
  console.log(`after   client 2: ${after2}`);
  console.log(`gate    client 2: before join ${gateBefore}, after join ${gateAfter}`);
  const fails = judgeOrg(before1, after1, after2, gateBefore, gateAfter);
  for (const st of steps) if (st.status !== "ok") fails.unshift(`${st.line} returned ${st.status} ${st.detail}`);
  for (const x of fails) console.log(`FAIL  ${x}`);
  console.log(fails.length ? "org: FAIL" : `org: PASS (founded ${ORG_NAME}, invited, joined, the gate opened for the member)`);
  return fails.length ? 1 : 0;
}

// The M1-05 card proof on a running `up --clients 2` session: client 1 is a
// civilian who robs Roadmap_Fuel_Till, client 2 is an on-duty officer (debug
// menu, dev gate open on the bare DebugWorld) who receives the alert. Prints
// `state` for both before and after, then judges the after line.
export const ROB_TARGET = "Roadmap_Fuel_Till";
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

async function runProof() {
  const h = await import("./harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = (n) => on(n, { verb: "state" }, 10000).detail;

  // Arrive: both register and spawn in town as civilians (there is no faction
  // screen). The form is gone once a character exists, so a second proof skips it.
  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  // Client 2 joins the police and goes on duty through the debug menu.
  on(2, { verb: "open", menu: "DebugMenu", mode: "show" });
  wait(800);
  on(2, { verb: "click", widget: "FactionPoliceButton" });
  on(2, { verb: "open", menu: "DebugMenu", mode: "hide" });
  wait(1500);

  const before1 = state(1);
  const before2 = state(2);
  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "look" });
  const rob = on(1, { verb: "use", className: "LF_RobAction", entityName: ROB_TARGET }, 10000);
  wait(2500);
  const after1 = state(1);
  const after2 = state(2);

  console.log("");
  console.log(`before  client 1: ${before1}`);
  console.log(`before  client 2: ${before2}`);
  console.log(`after   client 1: ${after1}`);
  console.log(`after   client 2: ${after2}`);
  const fails = judgeProof(before1, after1, after2);
  if (rob.status !== "ok") fails.unshift(`rob returned ${rob.status} ${rob.detail}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "proof: FAIL" : "proof: PASS (client 1 robbed, client 2 got the alert)");
  return fails.length ? 1 : 0;
}

// A new account sees the registration form (R-89); fill and sign it.
export const ARRIVAL_NAMES = { 1: ["Ada", "Robber"], 2: ["Bob", "Officer"], 3: ["Cy", "Walker"] };
function arrive(on, n, wait) {
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

// M1-11 on a running `up --clients 2` session: client 2 (on the roster) clocks in at
// the duty desk, client 1 robs the fuel till, client 2 reads the wanted board (police
// view), client 1 reads it too (public view), client 2 clocks out. Prints state lines;
// the poster lines land in each client's log (read them after `down`).
async function runPoliceProof() {
  const h = await import("./harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = (n) => on(n, { verb: "state" }, 10000).detail;
  const fails = [];

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  const start1 = state(1);
  const start2 = state(2);

  on(2, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  on(2, { verb: "look" });
  const clockIn = on(2, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000);
  wait(2500);
  const onDuty2 = state(2);
  if (clockIn.status !== "ok") fails.push(`clock in returned ${clockIn.status} ${clockIn.detail}`);
  if (!onDuty2.includes("faction=POLICE") || !onDuty2.includes("duty=1")) fails.push(`client 2 after clock-in: ${onDuty2}`);

  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "look" });
  const rob = on(1, { verb: "use", className: "LF_RobAction", entityName: ROB_TARGET }, 10000);
  wait(2500);
  const robbed1 = state(1);
  if (rob.status !== "ok") fails.push(`rob returned ${rob.status} ${rob.detail}`);
  if (!robbed1.includes("wanted=1")) fails.push(`client 1 after the robbery: ${robbed1}`);

  for (const n of [2, 1]) {
    on(n, { verb: "stand", entityName: "Roadmap_Police_WantedBoard" });
    wait(2500);
    on(n, { verb: "look" });
    const read = on(n, { verb: "use", className: "LF_WantedBoardReadAction", entityName: "Roadmap_Police_WantedBoard" }, 10000);
    if (read.status !== "ok") fails.push(`client ${n} board read returned ${read.status} ${read.detail}`);
    wait(2500);
    on(n, { verb: "capture", mode: "ui", name: `m1_11_board_client${n}` }, 25000);
    on(n, { verb: "ui" });
    on(n, { verb: "click", widget: "Close" });
    wait(800);
  }

  on(2, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  on(2, { verb: "look" });
  const clockOut = on(2, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000);
  wait(2500);
  const offDuty2 = state(2);
  if (clockOut.status !== "ok") fails.push(`clock out returned ${clockOut.status} ${clockOut.detail}`);
  if (!offDuty2.includes("faction=CIVILIAN") || !offDuty2.includes("duty=0")) fails.push(`client 2 after clock-out: ${offDuty2}`);

  console.log("");
  console.log(`start      client 1: ${start1}`);
  console.log(`start      client 2: ${start2}`);
  console.log(`clocked in client 2: ${onDuty2}`);
  console.log(`robbed     client 1: ${robbed1}`);
  console.log(`clock out  client 2: ${offDuty2}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "police proof: FAIL" : "police proof: PASS (clock in, robbery, board read by police and public, clock out)");
  return fails.length ? 1 : 0;
}

// Runs fn while one client records itself in-engine (tools/film/engine.mjs),
// then encodes the clip. Works on the hidden LF-Agents desktop.
async function filmed(label, rest, fn) {
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

// M1-27: client 1 shoots client 2 at the fuel station (murder, wanted), then on the town square
// (safe zone, refused). `shoot` is one lethal hit through the vanilla HandleDamage path.
async function runMurderProof() {
  const h = await import("./harness.mjs");
  const dir = sessionDir();
  // A lethal hit downs a player (M2 #255); the murder hook fires on the real death. This proof
  // shoots to kill, so it runs with the downed state off.
  const medicConf = readFileSync(join(ROOT, "addons", "LifeFramework", "Configs", "Medic", "LF_Medic.conf"), "utf8");
  if (!/m_bDownedEnabled\s+0/.test(medicConf)) {
    console.log("murder proof: lethal hits down a player now; set m_bDownedEnabled to 0 in LF_Medic.conf for the run");
    return 1;
  }
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = (n) => on(n, { verb: "state" }, 10000).detail;
  const fails = [];

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  const start1 = state(1);
  const start2 = state(2);

  // Outside the zone: the fuel station.
  on(2, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  const outside = on(1, { verb: "shoot", playerId: 2 }, 10000);
  wait(600);
  on(1, { verb: "capture", mode: "ui", name: "m1_27_murder_client1" }, 25000);
  const murder1 = state(1);
  if (!outside.detail.includes("refused=0 alive=0")) fails.push(`shot outside: ${outside.status} ${outside.detail}`);
  if (!murder1.includes("wanted=3")) fails.push(`client 1 after the kill: ${murder1}`);

  // Client 2 wakes in hospital.
  wait(3000);
  on(2, { verb: "click", widget: "RespawnButton" });
  wait(12000);
  const respawned2 = state(2);

  // The wanted board now carries a murder poster: a description, no name.
  on(2, { verb: "stand", entityName: "Roadmap_Police_WantedBoard" });
  wait(2500);
  const board = on(2, { verb: "use", className: "LF_WantedBoardReadAction", entityName: "Roadmap_Police_WantedBoard" }, 10000);
  wait(2500);
  on(2, { verb: "capture", mode: "ui", name: "m1_27_board_client2" }, 25000);
  on(2, { verb: "click", widget: "Close" });
  if (board.status !== "ok") fails.push(`board read: ${board.status} ${board.detail}`);

  // Inside the zone: the town square, 3.5 m apart.
  on(2, { verb: "stand", entityName: "Sign_SafeZone_Spawn" });
  wait(2500);
  on(1, { verb: "stand", entityName: "Sign_TownMap" });
  wait(2500);
  const inside = on(1, { verb: "shoot", playerId: 2 }, 10000);
  wait(600);
  on(1, { verb: "capture", mode: "ui", name: "m1_27_safezone_client1" }, 25000);
  const square1 = state(1);
  const square2 = state(2);
  if (!inside.detail.includes("refused=1 alive=1")) fails.push(`shot inside: ${inside.status} ${inside.detail}`);
  if (!square1.includes("wanted=3")) fails.push(`client 1 after the refused shot: ${square1}`);

  // The notice, as a stranger walking up to it reads it.
  on(1, { verb: "look" });
  const read = on(1, { verb: "use", className: "LF_SignReadAction", entityName: "Sign_SafeZone_Spawn" }, 10000);
  wait(2500);
  on(1, { verb: "capture", mode: "ui", name: "m1_27_sign_card_client1" }, 25000);
  on(1, { verb: "click", widget: "Close" });
  if (read.status !== "ok") fails.push(`sign read: ${read.status} ${read.detail}`);
  on(1, { verb: "capture", mode: "world", name: "m1_27_sign_spawn_west", x: 130.5, y: 1.2, z: 250, ox: -4, oy: 0.6, oz: 0 }, 25000);
  on(1, { verb: "capture", mode: "world", name: "m1_27_sign_east_out", x: 145.5, y: 1.2, z: 258, ox: 5, oy: 0.6, oz: 0 }, 25000);
  on(1, { verb: "capture", mode: "world", name: "m1_27_sign_east_in", x: 145.5, y: 1.2, z: 258, ox: -4, oy: 0.6, oz: 0 }, 25000);

  console.log("");
  console.log(`start     client 1: ${start1}`);
  console.log(`start     client 2: ${start2}`);
  console.log(`shot out  client 1: ${outside.detail}`);
  console.log(`murder    client 1: ${murder1}`);
  console.log(`respawn   client 2: ${respawned2}`);
  console.log(`shot in   client 1: ${inside.detail}`);
  console.log(`square    client 1: ${square1}`);
  console.log(`square    client 2: ${square2}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "murder proof: FAIL" : "murder proof: PASS (kill outside -> wanted 3; shot on the square refused)");
  return fails.length ? 1 : 0;
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

async function runCarryProof() {
  const h = await import("./harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = (n) => on(n, { verb: "state" }, 10000).detail;
  const peers = (n) => on(n, { verb: "peers" }, 10000).detail;
  const at = (n) => String(parseState(state(n)).pos ?? "0,0,0").split(",").map(Number);
  const fails = [];
  const need = (cond, text) => { if (!cond) fails.push(text); };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  on(2, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "shoot", playerId: 2 }, 10000);
  wait(2500);
  const patient = `Player${parseState(state(2)).player}`;
  need(parseState(state(2)).downed === "1", "client 2 was not downed");

  // Drag it to the hospital desk.
  on(1, { verb: "stand", entityName: patient });
  wait(2500);
  on(1, { verb: "look" });
  const drag = on(1, { verb: "use", className: "LF_DragAction", entityName: patient }, 10000);
  need(drag.status === "ok", `drag returned ${drag.status} ${drag.detail}`);
  wait(1500);
  const start1 = peers(1);
  need(parsePeer(start1, 2)?.carrier === 1, `the body is not held by client 1 after the drag: ${start1}`);
  on(1, { verb: "stand", entityName: "Roadmap_Hospital_Duty_Desk" });
  wait(4000);
  const carrierAt = at(1);
  const afterDrag = { 1: peers(1), 2: peers(2) };
  for (const n of [1, 2]) for (const f of judgeCarry(afterDrag[n], 2, 1, carrierAt, 4)) fails.push(`drag, client ${n}: ${f}`);
  const [cx, cy, cz] = carrierAt;
  on(1, { verb: "capture", mode: "world", name: "m2_261_drag_world", x: cx, y: cy + 0.8, z: cz, ox: 3.5, oy: 1.6, oz: 0 }, 25000);

  // A late joiner reads the hold and the body next to the carrier.
  let afterDrag3 = "";
  if (!process.argv.includes("--no-late-join")) {
    const joined = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "join", "3"], { windowsHide: true, stdio: "inherit" });
    if (joined.status !== 0) {
      fails.push("client 3 did not join late");
    } else {
      arrive(on, 3, wait);
      wait(8000);
      on(3, { verb: "stand", entityName: "Roadmap_Hospital_Duty_Desk" });
      wait(4000);
      afterDrag3 = peers(3);
      for (const f of judgeCarry(afterDrag3, 2, 1, carrierAt, 4)) fails.push(`drag, late joiner: ${f}`);
    }
  }

  // Drop, then carry to the escort car and load.
  const drop = on(1, { verb: "use", className: "LF_DropAction", entityName: patient }, 10000);
  need(drop.status === "ok", `drop returned ${drop.status} ${drop.detail}`);
  wait(1500);
  need(parsePeer(peers(2), 2)?.carrier === 0, "the body is still held after the drop");
  on(1, { verb: "use", className: "LF_CarryAction", entityName: patient }, 10000);
  wait(1500);
  need(parsePeer(peers(2), 2)?.carrier === 1, "the carry did not take hold");
  on(1, { verb: "stand", entityName: "Roadmap_Escort_Car" });
  wait(4000);
  const load = on(1, { verb: "use", className: "LF_LoadPatientAction", entityName: patient }, 10000);
  need(load.status === "ok", `load returned ${load.status} ${load.detail}`);
  wait(9000);
  const seated = { 1: peers(1), 2: peers(2) };
  if (afterDrag3) seated[3] = peers(3);
  for (const n of Object.keys(seated)) {
    const p = parsePeer(seated[n], 2);
    need(p && p.seat === "1" && p.carrier === 0, `seat, client ${n}: ${seated[n]}`);
  }
  on(1, { verb: "capture", mode: "world", name: "m2_261_loaded_world", x: cx, y: cy + 0.8, z: cz, ox: 3.5, oy: 1.6, oz: 0 }, 25000);
  console.log(`afterDrag ${JSON.stringify(afterDrag)} late ${afterDrag3} seated ${JSON.stringify(seated)}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "carry proof: FAIL" : "carry proof: PASS (drag, drop, carry, load; seen by both clients and a late joiner)");
  return fails.length ? 1 : 0;
}

async function runDownedProof() {
  const h = await import("./harness.mjs");
  const dir = sessionDir();
  const medicConf = readFileSync(join(ROOT, "addons", "LifeFramework", "Configs", "Medic", "LF_Medic.conf"), "utf8");
  if (/m_bDownedEnabled\s+0/.test(medicConf)) {
    console.log("downed proof: m_bDownedEnabled is 0 in LF_Medic.conf; set it to 1 for the run");
    return 1;
  }
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = (n) => on(n, { verb: "state" }, 10000).detail;
  const fails = [];

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  const start1 = state(1);
  const start2 = state(2);

  on(2, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  const shot = on(1, { verb: "shoot", playerId: 2 }, 10000);
  wait(2500);
  on(1, { verb: "capture", mode: "ui", name: "m2_255_downed_client1" }, 25000);
  on(2, { verb: "capture", mode: "ui", name: "m2_255_downed_client2" }, 25000);
  const victim = state(2);
  const shooter = state(1);
  const peers1 = on(1, { verb: "peers" }, 10000).detail;
  const peers2 = on(2, { verb: "peers" }, 10000).detail;
  // The body on the ground, seen from 2.5 m away (a world shot from the downed player's position).
  const [bx, by, bz] = String(parseState(victim).pos ?? "0,0,0").split(",").map(Number);
  if (Number.isFinite(bx)) on(1, { verb: "capture", mode: "world", name: "m2_255_downed_body", x: bx, y: by + 0.3, z: bz, ox: 2.5, oy: 1.2, oz: 0 }, 25000);
  for (const f of judgeDowned(shot.detail, victim, shooter, peers1, peers2)) fails.push(f);

  // A second lethal hit changes nothing: a downed player takes no further damage.
  const again = on(1, { verb: "shoot", playerId: 2 }, 10000);
  wait(1500);
  const victimAgain = parseState(state(2));
  if (victimAgain.downed !== "1" || victimAgain.life !== "INCAPACITATED") fails.push(`after a second hit: downed=${victimAgain.downed} life=${victimAgain.life}`);

  // Late joiner: client 3 connects after the downing and must read client 2 as downed.
  let peers3 = "";
  if (process.argv.includes("--no-late-join")) {
    console.log("late join skipped (--no-late-join)");
  } else {
    const joined = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "join", "3"], { windowsHide: true, stdio: "inherit" });
    if (joined.status !== 0) {
      fails.push("client 3 did not join late");
    } else {
      arrive(on, 3, wait);
      wait(8000);
      on(3, { verb: "stand", entityName: ROB_TARGET });
      wait(4000);
      peers3 = on(3, { verb: "peers" }, 10000).detail;
      on(3, { verb: "capture", mode: "ui", name: "m2_255_downed_latejoin_client3" }, 25000);
      if (!/player=2 downed=1 life=INCAPACITATED/.test(peers3)) fails.push(`the late joiner does not see player 2 downed: ${peers3}`);
    }
  }

  // #259: a medic revives. Client 1 clocks in at the hospital desk, walks to the downed client 2
  // and revives them: client 2 is standing with health above zero, keeps what they carried, and the
  // bleed-out clock is gone. A civilian is not offered the entry (client 1 before clocking in is
  // refused by the server). Then client 1 clocks out and downs client 2 again for the rest of the proof.
  const patientName = `Player${parseState(state(2)).player}`;
  const cashBefore = parseState(state(2)).cash;
  const civilianTry = on(1, { verb: "use", className: "LF_ReviveAction", entityName: patientName }, 10000);
  wait(1500);
  const afterCivilian = parseState(state(2));
  if (afterCivilian.downed !== "1") fails.push(`a civilian's revive changed the patient: downed=${afterCivilian.downed} (${civilianTry.status} ${civilianTry.detail})`);
  on(1, { verb: "stand", entityName: "Roadmap_Hospital_Duty_Desk" });
  wait(2500);
  on(1, { verb: "look" });
  const medicIn = on(1, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Hospital_Duty_Desk" }, 10000);
  wait(2500);
  if (!parseState(state(1)).faction?.includes("MEDIC")) fails.push(`client 1 did not clock in as a medic: ${medicIn.status} ${medicIn.detail}`);
  on(1, { verb: "stand", entityName: patientName });
  wait(2500);
  on(1, { verb: "look" });
  const [px, py, pz] = String(parseState(state(2)).pos ?? "0,0,0").split(",").map(Number);
  const seeBody = (name) => { if (Number.isFinite(px)) on(1, { verb: "capture", mode: "world", name, x: px, y: py + 0.3, z: pz, ox: 2.5, oy: 1.2, oz: 0 }, 25000); };
  seeBody("m2_259_before_revive_world");
  // The world capture moves the view: stand beside the patient again before the action.
  on(1, { verb: "stand", entityName: patientName });
  wait(2500);
  on(1, { verb: "look" });
  const revive = on(1, { verb: "use", className: "LF_ReviveAction", entityName: patientName }, 10000);
  // The vanilla wake takes a few seconds after the downed state ends.
  let revived = parseState(state(2));
  for (let i = 0; i < 8 && revived.life !== "ALIVE"; i++) {
    wait(2000);
    revived = parseState(state(2));
  }
  on(2, { verb: "capture", mode: "ui", name: "m2_259_revived_client2" }, 25000);
  seeBody("m2_259_after_revive_world");
  on(1, { verb: "stand", entityName: patientName });
  wait(1500);
  if (revive.status !== "ok") fails.push(`revive returned ${revive.status} ${revive.detail}`);
  if (revived.downed !== "0" || revived.life !== "ALIVE" || revived.bleed !== "-1") fails.push(`after the revive: downed=${revived.downed} life=${revived.life} bleed=${revived.bleed}`);
  if (revived.cash !== cashBefore) fails.push(`the revived player's cash changed: ${cashBefore} -> ${revived.cash}`);
  on(1, { verb: "stand", entityName: "Roadmap_Hospital_Duty_Desk" });
  wait(2500);
  on(1, { verb: "look" });
  on(1, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Hospital_Duty_Desk" }, 10000);
  wait(2500);
  on(2, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "shoot", playerId: 2 }, 10000);
  wait(2500);
  if (parseState(state(2)).downed !== "1") fails.push("client 2 was not downed again after the revive");
  console.log(`revive   client 2: ${JSON.stringify(revived)} (civilian try: ${civilianTry.detail})`);

  // The real death of a downed player (what the bleed-out will call): the death screen, the
  // respawn, and a fresh body that is not downed.
  // #256: the client reads the time left (a replicated, throttled copy); giving up is refused
  // before the delay and accepted after it, and then it is the real death.
  const bleedConf = medicConf;
  const bleedSeconds = Number(/m_fBleedOutSecondss+(d+)/.exec(bleedConf)?.[1] ?? 180);
  const giveUpDelay = Number(/m_fGiveUpDelaySecondss+(d+)/.exec(bleedConf)?.[1] ?? 30);
  const bleedRead = parseState(state(2)).bleed;
  for (const f of judgeBleed(bleedRead, bleedSeconds)) fails.push(f);
  const early = on(2, { verb: "giveup" }, 10000);
  wait(2000);
  const afterEarly = parseState(state(2));
  if (afterEarly.downed !== "1" || afterEarly.life !== "INCAPACITATED") fails.push(`an early give-up changed the state: downed=${afterEarly.downed} life=${afterEarly.life}`);
  wait((giveUpDelay + 2) * 1000);
  const late = on(2, { verb: "giveup" }, 10000);
  if (late.status !== "ok") fails.push(`giveup returned ${late.status} ${late.detail}`);
  wait(4000);
  on(2, { verb: "capture", mode: "ui", name: "m2_255_death_screen_client2" }, 25000);
  // Vanilla keeps the instigator of the knockout as the killer when the downed player dies, so the
  // murder is the downer's, on the real death only.
  const wantedAfterDeath = parseState(state(1)).wanted;
  if (wantedAfterDeath !== "3") fails.push(`shooter wanted=${wantedAfterDeath} after the downed player died, expected 3 (murder attributed to the downer)`);
  on(2, { verb: "click", widget: "RespawnButton" });
  wait(12000);
  const respawned = parseState(state(2));
  if (respawned.downed !== "0" || respawned.life !== "ALIVE") fails.push(`after the respawn: downed=${respawned.downed} life=${respawned.life}`);

  // #256: the bleed-out timer. Down client 2 again and leave them alone: the time runs out, the
  // death is the same real death, and the respawn is the normal one.
  on(2, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  const shot2 = on(1, { verb: "shoot", playerId: 2 }, 10000);
  wait(2500);
  const down2 = parseState(state(2));
  if (down2.downed !== "1") fails.push(`the second downing did not down: ${shot2.detail}`);
  wait((bleedSeconds + 8) * 1000);
  const bled = parseState(state(2));
  // A dead player has no body to answer `state` (no_session), which is the expected end.
  if (bled.downed === "1" || bled.life === "ALIVE" || bled.life === "INCAPACITATED") fails.push(`after the timer: downed=${bled.downed} life=${bled.life}, expected dead`);
  on(2, { verb: "capture", mode: "ui", name: "m2_256_bled_out_client2" }, 25000);
  on(2, { verb: "click", widget: "RespawnButton" });
  wait(12000);
  const woke = parseState(state(2));
  if (woke.downed !== "0" || woke.life !== "ALIVE") fails.push(`after the bleed-out respawn: downed=${woke.downed} life=${woke.life}`);

  console.log("");
  console.log(`start    client 1: ${start1}`);
  console.log(`start    client 2: ${start2}`);
  console.log(`shot     client 1: ${shot.detail}`);
  console.log(`bleed    client 2: ${bleedRead} (early give-up: ${early.detail}, then ${late.detail})`);
  console.log(`bled out client 2: ${JSON.stringify(bled)}; respawn ${JSON.stringify(woke)}`);
  console.log(`victim   client 2: ${victim}`);
  console.log(`shooter  client 1: ${shooter}`);
  console.log(`peers    client 1: ${peers1}`);
  console.log(`peers    client 2: ${peers2}`);
  console.log(`again    client 1: ${again.detail}`);
  console.log(`peers    client 3 (late joiner): ${peers3 || "skipped"}`);
  console.log(`respawn  client 2: ${JSON.stringify(respawned)}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "downed proof: FAIL" : "downed proof: PASS (downed, not dead, no murder; seen downed by both clients and a late joiner; a medic revives; real death is the murder and respawns)");
  return fails.length ? 1 : 0;
}

// M2 #258 on a running `up --clients 2` session: client 2 clocks in at the hospital desk and
// is a medic on duty (the medic kit, the service routing); the hospital board, closed to the
// public, opens for them; clock-out returns the civilian. Screenshots: the desk, the clock-in,
// the open board, the public's closed board.
async function runMedicProof() {
  const h = await import("./harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = (n) => on(n, { verb: "state" }, 10000).detail;
  const fails = [];
  const desk = (n) => {
    on(n, { verb: "stand", entityName: "Roadmap_Hospital_Duty_Desk" });
    wait(2500);
    on(n, { verb: "look" });
    const r = on(n, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Hospital_Duty_Desk" }, 10000);
    wait(2500);
    return r;
  };
  const board = (n, name) => {
    on(n, { verb: "stand", entityName: "Roadmap_Hospital_DispatchBoard" });
    wait(2500);
    on(n, { verb: "look" });
    const r = on(n, { verb: "use", className: "LF_WantedBoardReadAction", entityName: "Roadmap_Hospital_DispatchBoard" }, 10000);
    wait(2500);
    on(n, { verb: "capture", mode: "ui", name }, 25000);
    on(n, { verb: "ui" });
    on(n, { verb: "click", widget: "Close" });
    wait(800);
    return r;
  };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  const start2 = state(2);
  board(1, "m2_258_board_public_client1");

  on(2, { verb: "stand", entityName: "Roadmap_Hospital_Duty_Desk" });
  wait(2500);
  on(2, { verb: "capture", mode: "world", name: "m2_258_desk", x: 259.4, y: 1.0, z: 256.4, ox: 0, oy: 1.2, oz: -4.5 }, 25000);
  const clockIn = desk(2);
  const onDuty = state(2);
  on(2, { verb: "capture", mode: "ui", name: "m2_258_clocked_in_client2" }, 25000);
  if (clockIn.status !== "ok") fails.push(`clock in returned ${clockIn.status} ${clockIn.detail}`);
  if (!onDuty.includes("faction=MEDIC") || !onDuty.includes("duty=1")) fails.push(`client 2 after clock-in: ${onDuty}`);
  board(2, "m2_258_board_medic_client2");

  const clockOut = desk(2);
  const offDuty = state(2);
  if (clockOut.status !== "ok") fails.push(`clock out returned ${clockOut.status} ${clockOut.detail}`);
  if (!offDuty.includes("faction=CIVILIAN") || !offDuty.includes("duty=0")) fails.push(`client 2 after clock-out: ${offDuty}`);

  console.log("");
  console.log(`start     client 2: ${start2}`);
  console.log(`clocked in client 2: ${onDuty}`);
  console.log(`clock out client 2: ${offDuty}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "medic proof: FAIL" : "medic proof: PASS (clock in at the hospital desk, the medic board opens, clock out)");
  return fails.length ? 1 : 0;
}

// M2 #263 on a running `up --clients 2` session (debug mission: the dev gate is open): client 1 turns
// the hospital ambulance's siren on through the same rule the driver's key uses, client 2 (a second
// machine) must see and hear the change arrive by replication; then off. Screenshots: lights off,
// lights on from both clients. Replication is read from client 2's own log line.
async function runSirenProof() {
  const h = await import("./harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const vehicles = [
    { name: "Roadmap_Hospital_Ambulance", at: [266, 1.2, 250], label: "ambulance" },
    { name: "Roadmap_Police_Car", at: [240, 1.0, 246], label: "police" },
  ];
  const view = (n, v, tag) => on(n, { verb: "capture", mode: "world", name: `m2_263_${v.label}_${tag}_client${n}`, x: v.at[0], y: v.at[1], z: v.at[2], ox: 5, oy: 2.5, oz: -6 }, 25000);

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  for (const v of vehicles) {
    on(1, { verb: "stand", entityName: v.name });
    wait(2500);
    view(1, v, "off");
    const onR = on(1, { verb: "siren", entityName: v.name, mode: 1 }, 10000);
    if (!onR.detail.includes("changed=1 on=1 broadcasts=1")) fails.push(`${v.label} siren on: ${onR.status} ${onR.detail}`);
    wait(2500);
    view(1, v, "on_a");
    wait(400);
    view(2, v, "on_b");
    const again = on(1, { verb: "siren", entityName: v.name, mode: 1 }, 10000);
    if (!again.detail.includes("changed=0")) fails.push(`${v.label} the same mode asked twice changed again: ${again.detail}`);
    const offR = on(1, { verb: "siren", entityName: v.name, mode: 0 }, 10000);
    if (!offR.detail.includes("changed=1 on=0 broadcasts=2")) fails.push(`${v.label} siren off: ${offR.status} ${offR.detail}`);
    wait(1500);
  }

  // Replication: client 2 saw both vehicles change (its own log carries the mode line once per change).
  const logs = [];
  const clientDir = join(dir, "client2", "logs");
  if (existsSync(clientDir)) {
    for (const d of readdirSync(clientDir).sort()) {
      const p = join(clientDir, d, "console.log");
      if (existsSync(p)) logs.push(readFileSync(p, "utf8"));
    }
  }
  const text = logs.join("\n");
  const modeLines = (text.match(/Siren\]? mode -> /g) ?? []).length;
  console.log(`client 2 siren mode lines: ${modeLines}`);
  if (modeLines < 4) fails.push(`client 2 logged ${modeLines} siren mode changes, expected at least 4 (on and off for two vehicles)`);

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "siren proof: FAIL" : "siren proof: PASS (ambulance and police car: siren on and off replicated to client 2, a repeated request changes nothing)");
  return fails.length ? 1 : 0;
}

// M2 #260 on a running `up --clients 2 --mission playtest` session: client 1 downs client 2, clocks in
// as a medic at the hospital desk and revives them. Client 2 holds a paper treatment bill (the bank
// and carried cash untouched), pays it in cash at the hospital desk, and the medic's share arrives
// at clock-out. Then client 2 dies for real and wakes in hospital: a second paper bill, bank untouched,
// paid at the desk too. Screenshots: the revive toast, the desk payment, the wake bill card.
async function runBillProof() {
  const h = await import("./harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = (n) => parseState(on(n, { verb: "state" }, 10000).detail);
  const fails = [];
  const deskStep = (n, className) => {
    on(n, { verb: "stand", entityName: "Roadmap_Hospital_Duty_Desk" });
    wait(2500);
    on(n, { verb: "look" });
    const r = on(n, { verb: "use", className, entityName: "Roadmap_Hospital_Duty_Desk" }, 10000);
    wait(2500);
    return r;
  };
  const medicConf = readFileSync(join(ROOT, "addons", "LifeFramework", "Configs", "Medic", "LF_Medic.conf"), "utf8");
  const giveUpDelay = Number(/m_fGiveUpDelaySeconds\s+(\d+)/.exec(medicConf)?.[1] ?? 30);
  const billAmount = Number(/m_iReviveBillAmount\s+(\d+)/.exec(medicConf)?.[1] ?? 100);
  const share = Math.floor(billAmount * Number(/m_fBillMedicShare\s+([\d.]+)/.exec(medicConf)?.[1] ?? 0.5));
  const respawnConf = readFileSync(join(ROOT, "addons", "LifeFramework", "Configs", "Spawning", "LF_Respawn.conf"), "utf8");
  const wakeAmount = Number(/m_iBillAmount\s+(\d+)/.exec(respawnConf)?.[1] ?? 150);

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  const start2 = state(2);
  const medic0 = state(1);
  const patientName = `Player${start2.player}`;

  // Down the patient, then a medic revives.
  on(2, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "shoot", playerId: 2 }, 10000);
  wait(2500);
  if (state(2).downed !== "1") fails.push("client 2 was not downed");
  deskStep(1, "LF_DutyAction");
  if (!String(state(1).faction).includes("MEDIC")) fails.push("client 1 did not clock in as a medic");
  on(1, { verb: "stand", entityName: patientName });
  wait(2500);
  on(1, { verb: "look" });
  const revive = on(1, { verb: "use", className: "LF_ReviveAction", entityName: patientName }, 10000);
  let revived = state(2);
  for (let i = 0; i < 8 && revived.life !== "ALIVE"; i++) {
    wait(2000);
    revived = state(2);
  }
  wait(1500);
  on(2, { verb: "capture", mode: "ui", name: "m2_260_revive_bill_client2" }, 25000);
  revived = state(2);
  if (revive.status !== "ok") fails.push(`revive returned ${revive.status} ${revive.detail}`);
  if (revived.bills !== "1") fails.push(`the revived patient carries bills=${revived.bills}, expected 1`);
  if (revived.cash !== start2.cash) fails.push(`the bill took cash before the desk: ${start2.cash} -> ${revived.cash}`);
  if (revived.bank !== start2.bank) fails.push(`the bill touched the bank: ${start2.bank} -> ${revived.bank}`);

  // The patient pays at the hospital desk, in cash.
  on(2, { verb: "stand", entityName: "Roadmap_Hospital_Duty_Desk" });
  wait(2500);
  on(2, { verb: "look" });
  on(2, { verb: "capture", mode: "ui", name: "m2_260_desk_pay_prompt_client2" }, 25000);
  const pay = on(2, { verb: "use", className: "LF_TreatmentBillPayAction", entityName: "Roadmap_Hospital_Duty_Desk" }, 10000);
  wait(2500);
  const paid = state(2);
  on(2, { verb: "capture", mode: "ui", name: "m2_260_bill_paid_client2" }, 25000);
  if (pay.status !== "ok") fails.push(`the desk payment returned ${pay.status} ${pay.detail}`);
  if (Number(paid.cash) !== Number(start2.cash) - billAmount) fails.push(`paying took ${Number(start2.cash) - Number(paid.cash)} cash, expected ${billAmount}`);
  const payAgain = on(2, { verb: "use", className: "LF_TreatmentBillPayAction", entityName: "Roadmap_Hospital_Duty_Desk" }, 10000);
  wait(1500);
  if (state(2).cash !== paid.cash) fails.push(`a second desk payment changed cash: ${paid.cash} -> ${state(2).cash} (${payAgain.status})`);

  // The medic's share arrives once, at clock-out.
  const medicBefore = state(1);
  deskStep(1, "LF_DutyAction");
  const medicAfter = state(1);
  const gained = Number(medicAfter.cash) - Number(medicBefore.cash);
  if (gained < share) fails.push(`clock-out paid ${gained}, expected at least the ${share} share`);
  on(1, { verb: "capture", mode: "ui", name: "m2_260_medic_share_client1" }, 25000);

  // A real death: the wake writes a second paper bill, the bank stays whole.
  on(2, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "shoot", playerId: 2 }, 10000);
  wait(2500);
  wait((giveUpDelay + 2) * 1000);
  on(2, { verb: "giveup" }, 10000);
  wait(4000);
  on(2, { verb: "click", widget: "RespawnButton" });
  wait(14000);
  on(2, { verb: "capture", mode: "ui", name: "m2_260_wake_bill_card_client2" }, 25000);
  const woke = state(2);
  if (woke.bills !== "1") fails.push(`the wake left bills=${woke.bills}, expected the one wake bill`);
  if (woke.bank !== start2.bank) fails.push(`the wake touched the bank: ${start2.bank} -> ${woke.bank}`);
  on(2, { verb: "click", widget: "Close" });
  const cashWake = Number(woke.cash);
  deskStep(2, "LF_TreatmentBillPayAction");
  const wakePaid = state(2);
  if (Number(wakePaid.cash) !== cashWake - wakeAmount) fails.push(`the wake bill took ${cashWake - Number(wakePaid.cash)} cash, expected ${wakeAmount}`);
  on(2, { verb: "capture", mode: "ui", name: "m2_260_wake_bill_paid_client2" }, 25000);

  console.log("");
  console.log(`start    client 2: ${JSON.stringify(start2)}`);
  console.log(`revived  client 2: ${JSON.stringify(revived)}`);
  console.log(`paid     client 2: ${JSON.stringify(paid)}`);
  console.log(`medic    before ${medicBefore.cash} after clock-out ${medicAfter.cash} (share ${share})`);
  console.log(`woke     client 2: ${JSON.stringify(woke)}; paid the wake bill ${JSON.stringify(wakePaid)}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "bill proof: FAIL" : "bill proof: PASS (revive bill on paper, paid in cash at the hospital desk, medic share at clock-out, wake bill on paper, bank untouched)");
  return fails.length ? 1 : 0;
}

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

async function runRadioProof() {
  const h = await import("./harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  on(2, { verb: "stand", entityName: "Roadmap_Hospital_Duty_Desk" });
  wait(2500);
  on(2, { verb: "look" });
  const clockIn = on(2, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Hospital_Duty_Desk" }, 10000);
  wait(3500);
  const onDuty = on(2, { verb: "radio" }, 10000).detail;
  const civilian = on(1, { verb: "radio" }, 10000).detail;
  on(2, { verb: "capture", mode: "ui", name: "m2_252_radio_client2" }, 25000);
  const fails = judgeRadio(onDuty, civilian);
  if (clockIn.status !== "ok") fails.unshift(`clock in returned ${clockIn.status} ${clockIn.detail}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "radio proof: FAIL" : "radio proof: PASS (the medic's radio reads the service frequency on the client, the civilian has none)");
  return fails.length ? 1 : 0;
}

// M2 #253 on a running `up --clients 2` session with client 2 on the roster (`session.mjs roster 2`
// before up): client 2 clocks in (its radio reads the police frequency); client 1, a civilian, is
// handed a scanner (dev gate; the fence path is covered by tests) and robs the till; the dispatch
// reaches the officer's radio chirp and the civilian's scanner. The server log carries
// `[ELDebug:Scanner] ... heard on a scanner` for client 1; shots of both clients.
async function runScannerProof() {
  const h = await import("./harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  on(2, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  on(2, { verb: "look" });
  const clockIn = on(2, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000);
  wait(3000);
  const officerRadio = on(2, { verb: "radio" }, 10000).detail;
  const give = on(1, { verb: "scanner" }, 10000);
  wait(1500);
  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "look" });
  const rob = on(1, { verb: "use", className: "LF_RobAction", entityName: ROB_TARGET }, 10000);
  wait(1500);
  on(1, { verb: "capture", mode: "ui", name: "m2_253_scanner_client1" }, 25000);
  on(2, { verb: "capture", mode: "ui", name: "m2_253_dispatch_client2" }, 25000);
  if (clockIn.status !== "ok") fails.push(`clock in returned ${clockIn.status} ${clockIn.detail}`);
  if (give.status !== "ok" || !give.detail.includes("added=1")) fails.push(`scanner not given: ${give.status} ${give.detail}`);
  if (rob.status !== "ok") fails.push(`rob returned ${rob.status} ${rob.detail}`);
  if (!officerRadio.includes("f=41000")) fails.push(`the officer radio is not on 41000: ${officerRadio}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "scanner proof: FAIL" : "scanner proof: PASS (officer radio on 41000, scanner given, till robbed; confirm the 'heard on a scanner' server log line)");
  return fails.length ? 1 : 0;
}

// M2 #273 on a running `up --clients 2` session with client 2 on the roster (`session.mjs roster 2`
// before up): client 2 clocks in; client 1 is handed a stamped drill (dev gate, standing in for the
// fence), opens the bank vault with it, and the drill is left at the door. Client 2 bags it and logs
// the bag at the evidence locker, which identifies the buyer. Confirm `[LFDebug:Serials] ... bought by`
// in the server log.
async function runVaultProof() {
  const h = await import("./harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  on(2, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  on(2, { verb: "look" });
  const clockIn = on(2, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000);
  wait(3000);
  on(1, { verb: "stand", entityName: "Roadmap_Bank_Vault" });
  wait(2500);
  const noTool = on(1, { verb: "use", className: "LF_BankRobAction", entityName: "Roadmap_Bank_Vault" }, 10000);
  const give = on(1, { verb: "drill" }, 10000);
  wait(1500);
  const rob = on(1, { verb: "use", className: "LF_BankRobAction", entityName: "Roadmap_Bank_Vault" }, 10000);
  wait(3000);
  on(1, { verb: "capture", mode: "ui", name: "m2_273_vault_client1" }, 25000);
  on(2, { verb: "stand", entityName: "Roadmap_Bank_Vault" });
  wait(2500);
  const bag = on(2, { verb: "use", className: "LF_BagEvidenceAction" }, 10000);
  wait(1500);
  on(2, { verb: "stand", entityName: "Roadmap_Police_EvidenceLocker" });
  wait(2500);
  const log = on(2, { verb: "use", className: "LF_LogEvidenceAction", entityName: "Roadmap_Police_EvidenceLocker" }, 10000);
  wait(1500);
  on(2, { verb: "capture", mode: "ui", name: "m2_273_locker_client2" }, 25000);
  if (clockIn.status !== "ok") fails.push(`clock in returned ${clockIn.status} ${clockIn.detail}`);
  if (noTool.status === "ok") fails.push(`the vault opened without a tool: ${noTool.detail}`);
  if (give.status !== "ok" || !give.detail.includes("added=1")) fails.push(`drill not given: ${give.status} ${give.detail}`);
  if (rob.status !== "ok") fails.push(`rob returned ${rob.status} ${rob.detail}`);
  if (bag.status !== "ok") fails.push(`bagging returned ${bag.status} ${bag.detail}`);
  if (log.status !== "ok") fails.push(`logging returned ${log.status} ${log.detail}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "vault proof: FAIL" : "vault proof: PASS (refused without a tool, opened with the drill, drill bagged, buyer identified at the locker; confirm the server log)");
  return fails.length ? 1 : 0;
}

// The ticket deadlines the server will run with (Configs/Police/LF_Police.conf).
export function ticketDeadlines(confText) {
  const first = Number(/m_iTicketFirstDeadline\s+(\d+)/.exec(confText)?.[1] ?? 1800);
  const second = Number(/m_iTicketSecondDeadline\s+(\d+)/.exec(confText)?.[1] ?? 1800);
  return { first, second };
}

// M1-19 on a running `up --clients 2` session (roster: player 2): client 2 clocks in and
// writes client 1 a ticket face to face; client 1 reads it and pays at the desk; client 2
// writes a second one that client 1 leaves unpaid past both deadlines (doubled, then
// wanted +1). Needs short deadlines in LF_Police.conf (each 120 s or less) for the run.
// Screenshots: the ticket pad, the ticket card, the paid receipt, the doubled ticket.
async function runTicketProof() {
  const h = await import("./harness.mjs");
  const dir = sessionDir();
  const conf = readFileSync(join(ROOT, "addons", "LifeFramework", "Configs", "Police", "LF_Police.conf"), "utf8");
  const { first, second } = ticketDeadlines(conf);
  if (first > 120 || second > 120) {
    console.log(`ticket proof: the deadlines are ${first} s and ${second} s; set both to 120 or less in LF_Police.conf for the run`);
    return 1;
  }
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = (n) => parseState(on(n, { verb: "state" }, 10000).detail);
  const waitText = (n, text) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.waitForText(text, 10000);
    console.log(`client ${n}: wait-text ${text} -> ${r.status} ${r.found ? "found" : r.detail}`);
    return r;
  };
  const fails = [];
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what} returned ${r.status} ${r.detail}`);
    return r;
  };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  const suspect = `Player${state(1).player}`;

  on(2, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  must(on(2, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000), "clock in");
  wait(2500);

  const write = (offenceRow, shot) => {
    on(2, { verb: "stand", entityName: suspect });
    wait(2500);
    on(2, { verb: "look" });
    must(on(2, { verb: "use", className: "LF_PlayerRoleAction", entityName: suspect }, 10000), "Write ticket");
    must(waitText(2, "POLICE TICKET"), "the ticket pad");
    if (shot) on(2, { verb: "capture", mode: "ui", name: shot }, 25000);
    on(2, { verb: "ui", mode: "text" });
    must(on(2, { verb: "click", widget: `TicketOffence${offenceRow}` }), `offence row ${offenceRow}`);
    wait(3000);
  };

  // Ticket 1: written, read, paid at the desk.
  const before = state(1);
  write(0, "m1_19_ticket_pad");
  must(waitText(1, "Issued to"), "the ticket card on the holder's screen");
  on(1, { verb: "capture", mode: "ui", name: "m1_19_ticket_card" }, 25000);
  on(1, { verb: "ui", mode: "text" });
  on(1, { verb: "click", widget: "TicketClose" });
  const written = state(1);
  if (written.tickets !== "1") fails.push(`client 1 carries ${written.tickets} unpaid ticket(s) after the write, expected 1`);
  const owed = Number(written.owed);

  on(1, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  on(1, { verb: "look" });
  must(on(1, { verb: "use", className: "LF_FineTicketPayAction", entityName: "Roadmap_Police_Desk" }, 10000), "Pay ticket");
  must(waitText(1, "PAID"), "the stamped receipt");
  on(1, { verb: "capture", mode: "ui", name: "m1_19_ticket_receipt" }, 25000);
  on(1, { verb: "ui", mode: "text" });
  on(1, { verb: "click", widget: "TicketClose" });
  const paid = state(1);
  if (Number(paid.cash) !== Number(before.cash) - owed) fails.push(`client 1 cash ${before.cash} -> ${paid.cash}, expected -${owed}`);
  if (paid.tickets !== "0") fails.push(`client 1 still carries ${paid.tickets} unpaid ticket(s) after paying`);

  // Ticket 2: left unpaid past both deadlines.
  write(1, null);
  on(1, { verb: "click", widget: "TicketClose" });
  const second1 = state(1);
  const base = Number(second1.owed);
  console.log(`waiting ${first + 5} s of online time for the first deadline`);
  wait((first + 5) * 1000);
  const late = state(1);
  if (Number(late.owed) !== base * 2) fails.push(`after the first deadline the ticket owes ${late.owed}, expected ${base * 2}`);
  if (late.wanted !== second1.wanted) fails.push(`the first deadline changed wanted ${second1.wanted} -> ${late.wanted}`);
  on(1, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  on(1, { verb: "look" });
  on(1, { verb: "capture", mode: "ui", name: "m1_19_ticket_doubled_at_desk" }, 25000);
  console.log(`waiting ${second + 5} s of online time for the second deadline`);
  wait((second + 5) * 1000);
  const wanted = state(1);
  if (Number(wanted.wanted) !== Number(second1.wanted) + 1) fails.push(`after the second deadline wanted is ${wanted.wanted}, expected ${Number(second1.wanted) + 1}`);

  console.log("");
  console.log(`before         client 1: ${JSON.stringify(before)}`);
  console.log(`written        client 1: ${JSON.stringify(written)}`);
  console.log(`paid           client 1: ${JSON.stringify(paid)}`);
  console.log(`second written client 1: ${JSON.stringify(second1)}`);
  console.log(`first deadline client 1: ${JSON.stringify(late)}`);
  console.log(`second deadln  client 1: ${JSON.stringify(wanted)}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "ticket proof: FAIL" : "ticket proof: PASS (written face to face, paid at the desk in cash, unpaid one doubled then wanted +1)");
  return fails.length ? 1 : 0;
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

async function routeMachine(cmd, rest) {
  const i = rest.indexOf("--machine");
  const want = i === -1 ? null : String(rest[i + 1] ?? "");
  const args = i === -1 ? rest : rest.filter((_, k) => k !== i && k !== i + 1);
  const pool = await import("../tools/remote/pool.mjs");
  const machines = pool.loadMachines(ROOT);
  const ptr = readRemotePointer();
  if (cmd !== "up") {
    if (want === "local" || !ptr) return { local: true, args };
    const m = pool.findMachine(machines, ptr.machine);
    if (!m) throw new Error(`session is on ${ptr.machine}, which is not in the pool any more; delete ${remotePointerFile()}`);
    console.log(`session: on ${m.name} (${ptr.dir})`);
    const code = await pool.runIn(m, ptr.dir, "agent-box/session.mjs", [cmd, ...args]);
    if (cmd === "down" && code === 0) rmSync(remotePointerFile(), { force: true });
    return { code };
  }
  if (!want || want === "local") return { local: true, args };
  const clients = parseClients(args);
  let target = null;
  if (want === "any") {
    for (const m of machines) {
      if (clients > 0 && !m.caps.includes("clients")) continue;
      if (m.via === "ssh" && !pool.probe(ROOT, m).reachable) continue;
      if (pool.freeSlotOn(ROOT, m)) {
        target = m;
        break;
      }
    }
    if (!target) {
      console.log(`session: no machine with a free server slot${clients > 0 ? " that can run game clients" : ""}; queuing here (--wait) or retry`);
      return { local: true, args };
    }
  } else {
    target = pool.findMachine(machines, want);
    if (!target) throw new Error(`--machine ${want}: not in the pool (${machines.map((m) => m.name).join(", ")})`);
  }
  if (target.via === "local") return { local: true, args };
  if (clients > 0 && !target.caps.includes("clients")) {
    console.log(`session: ${target.name} cannot start game clients over ssh (no desktop session); use --clients 0 for a server-only session there and join it from this PC`);
    return { code: 2 };
  }
  if (ptr) {
    console.log(`session: one is recorded on ${ptr.machine}; 'node agent-box/session.mjs down' first`);
    return { code: 1 };
  }
  console.log(`session: server-only session on ${target.name}`);
  const r = await pool.syncAndRun(ROOT, target, { script: "agent-box/session.mjs", args: ["up", ...args] });
  mkdirSync(dirname(remotePointerFile()), { recursive: true });
  if (r.code === 0) writeFileSync(remotePointerFile(), JSON.stringify({ machine: target.name, host: target.host, dir: r.rDir, at: new Date().toISOString(), root: ROOT }));
  return { code: r.code };
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

async function runDispatchProof() {
  const h = await import("./harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = (n) => on(n, { verb: "state" }, 10000).detail;
  const duty = (n) => {
    on(n, { verb: "stand", entityName: "Roadmap_Police_Desk" });
    wait(2500);
    on(n, { verb: "look" });
    const r = on(n, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000);
    wait(2500);
    return r;
  };
  const rob = (n) => {
    on(n, { verb: "stand", entityName: ROB_TARGET });
    wait(2500);
    on(n, { verb: "look" });
    const r = on(n, { verb: "use", className: "LF_RobAction", entityName: ROB_TARGET }, 10000);
    wait(3000);
    return r;
  };
  const fails = [];

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  if (duty(2).status !== "ok") fails.push("client 2 clock in failed");
  // Client 2 waits at the station, out of the till's normal hear range.
  const first = rob(1);
  if (first.status !== "ok") fails.push(`rob returned ${first.status} ${first.detail}`);
  const robbed1 = state(1);
  on(1, { verb: "capture", mode: "ui", name: "m1_20_alarm_normal_client1" }, 25000);
  wait(1000);
  on(2, { verb: "capture", mode: "ui", name: "m1_20_dispatch_client2" }, 25000);
  const robbed2 = state(2);

  on(2, { verb: "stand", entityName: "Roadmap_Police_WantedBoard" });
  wait(2500);
  on(2, { verb: "look" });
  on(2, { verb: "use", className: "LF_WantedBoardReadAction", entityName: "Roadmap_Police_WantedBoard" }, 10000);
  wait(2500);
  on(2, { verb: "capture", mode: "ui", name: "m1_20_board_dispatch_client2" }, 25000);
  on(2, { verb: "ui" });
  on(2, { verb: "click", widget: "Close" });
  wait(800);

  if (duty(2).status !== "ok") fails.push("client 2 clock out failed");
  console.log(`waiting ${TILL_COOLDOWN_MS / 1000} s for the till cooldown`);
  wait(TILL_COOLDOWN_MS);
  const second = rob(1);
  if (second.status !== "ok") fails.push(`second rob returned ${second.status} ${second.detail}`);
  const loud1 = state(1);
  on(1, { verb: "capture", mode: "ui", name: "m1_20_alarm_loud_client1" }, 25000);
  const loud2 = state(2);

  console.log("");
  console.log(`robbed (officer on duty) client 1: ${robbed1}`);
  console.log(`robbed (officer on duty) client 2: ${robbed2}`);
  console.log(`robbed (nobody on duty)  client 1: ${loud1}`);
  console.log(`robbed (nobody on duty)  client 2: ${loud2}`);
  fails.push(...judgeDispatch(robbed1, robbed2, loud1, loud2));
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "dispatch proof: FAIL" : "dispatch proof: PASS (normal alarm + radio dispatch on duty; loud alarm and no dispatch off duty)");
  return fails.length ? 1 : 0;
}

async function runWitnessProof() {
  const h = await import("./harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what} returned ${r.status} ${r.detail}`);
    return r;
  };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  // Client 2 stands at the till (a witness within a few metres), client 1 robs it.
  on(2, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "look" });
  must(on(1, { verb: "use", className: "LF_RobAction", entityName: ROB_TARGET }, 10000), "rob");
  wait(3000);

  // The witness walks to the payphone and describes the suspect's clothing.
  on(2, { verb: "stand", entityName: "Village_Payphone" });
  wait(2500);
  on(2, { verb: "look" });
  must(on(2, { verb: "use", className: "LF_WitnessPayphoneAction", entityName: "Village_Payphone" }, 10000), "payphone description");
  wait(1000);
  on(2, { verb: "capture", mode: "ui", name: "m2_266_witness_payphone_client2" }, 25000);

  // An officer reads the board: the statement sits under the dispatch log.
  on(2, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  on(2, { verb: "look" });
  must(on(2, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000), "clock in");
  wait(2500);
  // #267: the officer asks the shopkeeper, who stood near the till (a town NPC), what it saw.
  on(2, { verb: "stand", entityName: "Roadmap_Fuel_Clerk" });
  wait(2500);
  on(2, { verb: "look" });
  must(on(2, { verb: "use", className: "LF_WitnessAskAction", entityName: "Roadmap_Fuel_Clerk" }, 10000), "ask the shopkeeper");
  wait(1000);
  on(2, { verb: "capture", mode: "ui", name: "m2_267_ask_shopkeeper_client2" }, 25000);
  on(2, { verb: "stand", entityName: "Roadmap_Police_WantedBoard" });
  wait(2500);
  on(2, { verb: "look" });
  on(2, { verb: "use", className: "LF_WantedBoardReadAction", entityName: "Roadmap_Police_WantedBoard" }, 10000);
  wait(2500);
  must(h.waitForText("WITNESS", 10000).found ? { status: "ok" } : { status: "error", detail: "no WITNESS line on the board" }, "board witness line");
  on(2, { verb: "capture", mode: "ui", name: "m2_266_witness_board_client2" }, 25000);
  on(2, { verb: "ui" });
  on(2, { verb: "click", widget: "Close" });

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "witness proof: FAIL" : "witness proof: PASS (a witness at the till described the suspect from the payphone; the officer's board shows it)");
  return fails.length ? 1 : 0;
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

// #243: the quarry is the one lawless area. One client stands at the warning sign (just outside), the quarry sign, then at the
// square sign; `state` must read zone=normal, lawless, safe. Runnable as a demo (docs/playtest.md).
async function runQuarryProof() {
  const h = await import("./harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const zone = () => parseState(on(1, { verb: "state" }, 10000).detail).zone;
  const fails = [];

  arrive(on, 1, wait);
  wait(8000);
  on(1, { verb: "stand", entityName: "Sign_Lawless_Entry" });
  wait(2500);
  const gate = zone();
  wait(6000); // the sign face paints on the client once the sign streams in
  on(1, { verb: "capture", mode: "world", name: "Sign_Lawless_Entry", x: 171.5, y: 1.5, z: 207, ox: 0, oy: 1, oz: 9 }, 25000);
  on(1, { verb: "capture", mode: "world", name: "Sign_Lawless_Close", x: 171.5, y: 1.5, z: 207, ox: 0, oy: 0.5, oz: 3 }, 25000);
  on(1, { verb: "stand", entityName: "Sign_Lawless_Pit" });
  wait(2500);
  const pit = zone();
  on(1, { verb: "stand", entityName: "Sign_SafeZone_Spawn" });
  wait(2500);
  const plaza = zone();
  if (gate !== "normal") fails.push(`warning sign: zone=${gate}, expected normal (it stands just outside)`);
  if (pit !== "lawless") fails.push(`quarry sign: zone=${pit}, expected lawless`);
  if (plaza !== "safe") fails.push(`plaza: zone=${plaza}, expected safe`);
  for (const f of fails) console.log(`  ${f}`);
  console.log(fails.length ? "quarry proof: FAIL" : "quarry proof: PASS (normal at the warning sign, lawless at the quarry sign, safe on the plaza)");
  return fails.length ? 1 : 0;
}

async function main(argv) {
  let [cmd, ...rest] = argv;
  if (cmd === "run") return runWhereSession(rest);
  if (cmd === "pull") return pullFromSession(rest);
  if (["up", "down", "status", "restart", "state", "wait"].includes(cmd)) {
    const routed = await routeMachine(cmd, rest);
    if (!routed.local) return routed.code;
    rest = routed.args;
  }
  if (rest.includes("--film") && ["proof", "police-proof", "murder-proof", "interaction"].includes(cmd)) {
    const run = { proof: runProof, "police-proof": runPoliceProof, "murder-proof": runMurderProof, interaction: runInteraction }[cmd];
    return filmed(cmd, rest, run);
  }
  if (cmd === "film") {
    const r = spawnSync(process.execPath, [join(ROOT, "tools", "film", "film.mjs"), "engine", ...rest, ...(rest.includes("--client") ? [] : ["--client", "1"])], { windowsHide: true, stdio: "inherit" });
    return r.status ?? 1;
  }
  if (cmd === "selftest") {
    const f = selftestSession();
    const launch = await import("../tools/lib/interactive-launch.mjs");
    f.push(...launch.selftest().map((x) => `interactive launch: ${x}`));
    if (!clientArgs({ n: 1, addonsDir: "a", profile: "p" }).includes("-noSound")) f.push("a default client is not -noSound");
    if (clientArgs({ n: 1, addonsDir: "a", profile: "p", sound: true }).includes("-noSound")) f.push("--sound still passes -noSound");
    for (const x of f) console.log(`  ${x}`);
    console.log(f.length ? "session selftest: FAIL" : "session selftest: OK");
    return f.length ? 1 : 0;
  }
  if (cmd === "up" || cmd === "down" || cmd === "status" || cmd === "restart" || cmd === "wait" || cmd === "join") {
    const r = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", cmd, ...rest], { windowsHide: true, stdio: "inherit", cwd: process.cwd() });
    return r.status ?? 1;
  }
  if (cmd === "proof") return runProof();
  if (cmd === "police-proof") return runPoliceProof();
  if (cmd === "dispatch-proof") return runDispatchProof();
  if (cmd === "witness-proof") return runWitnessProof();
  if (cmd === "ticket-proof") return runTicketProof();
  if (cmd === "murder-proof") return runMurderProof();
  if (cmd === "quarry-proof") return runQuarryProof();
  if (cmd === "downed-proof") return runDownedProof();
  if (cmd === "carry-proof") return runCarryProof();
  if (cmd === "medic-proof") return runMedicProof();
  if (cmd === "siren-proof") return runSirenProof();
  if (cmd === "bill-proof") return runBillProof();
  if (cmd === "radio-proof") return runRadioProof();
  if (cmd === "scanner-proof") return runScannerProof();
  if (cmd === "vault-proof") return runVaultProof();
  if (cmd === "roster") {
    const ids = rest.map(Number).filter((n) => Number.isInteger(n) && n > 0);
    if (!ids.length) throw new Error("roster <player id> [<player id> ...]");
    return writeRoster(ids);
  }
  if (cmd === "interaction") return runInteraction();
  if (cmd === "org") return runOrg(rest);
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
  console.log("usage: node agent-box/session.mjs up [--clients N] [--mission debug|playtest] | wait [--timeout s] | join <n> | restart | down | status | state <client> | film <seconds> [--client N] | proof|police-proof|murder-proof|quarry-proof|downed-proof|carry-proof|medic-proof|bill-proof|siren-proof|radio-proof|scanner-proof|vault-proof|dispatch-proof|witness-proof|interaction [--film] | org [--shot] | run <script> [args] | pull [dest] | selftest");
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
