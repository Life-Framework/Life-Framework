#!/usr/bin/env node
// Multi-client session: one dedicated server plus N game clients on this PC,
// each client with its own -profile, joined over 127.0.0.1:2001.
//
//   node agent-box/session.mjs up --clients 2     boot the server, launch the clients
//   node agent-box/session.mjs up --clients 2 --mission playtest   LF_PlaytestTown, harness build
//   node agent-box/session.mjs restart            save, stop, load, rejoin (playtest mission)
//   node agent-box/session.mjs down               close every process the session started
//   node agent-box/session.mjs status             pids, profiles, logs
//   node agent-box/session.mjs state <client>     faction, duty, wanted, cash, bank, job, org
//
// Any harness verb targets one client with --client <n>:
//   node agent-box/harness.mjs --client 2 look
//
// The server runs the bare DebugWorld (no mission header, no test define), so
// the world's game mode keeps dev tools open and the harness RPCs pass
// EL_DevGate. The playtest mission (LF_PlaytestTown.conf) closes that gate.
// `up --mission playtest` runs that mission the way `toolscli host start` does,
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
// The session dir lives outside every worktree (default
// %LOCALAPPDATA%\LifeFramework\session, override LF_SESSION_DIR) and holds no
// junctions.

import { spawnSync } from "node:child_process";
import { randomBytes } from "node:crypto";
import { existsSync, mkdirSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { PLAYTEST_MISSION } from "../tools/host/host.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..");

export const SERVER_WORLD = "Worlds/DebugWorld/DebugWorld.ent";
export const MAX_CLIENTS = 3;
export const WINDOW = { width: 640, height: 360, gap: 8 };
export const SERVER_READY = "Entered online game state.";
// The server refuses a client whose script checksum differs
// ("remote script source code checksum does not match"), so clients compile
// with the same define as the server.
export const SESSION_DEFINE = "LF_SESSION_IDENTITY";
// Opens the harness gate on the playtest mission (EL_DevGate.HarnessBuild).
// Only a session passes it; toolscli host start never does.
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

export function sessionDir(env = process.env) {
  if (env.LF_SESSION_DIR) return env.LF_SESSION_DIR;
  const base = env.LOCALAPPDATA || join(env.USERPROFILE || ".", "AppData", "Local");
  return join(base, "LifeFramework", "session");
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
// commands (EL_AgentCommand.SessionAccepts).
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
    // EL_Utils.GetPlayerUID gives each player id its own identity under this define.
    ...defineArgs(mission),
    "-disableCrashReporter",
    "-noBackend",
  ];
  if (loadSave) args.push("-loadSessionSave");
  return args;
}

// Small windows in a row along the top of the screen, so the user's desk
// stays usable. -screenWidth/-screenHeight/-posX/-posY need -window.
export function windowFor(n) {
  return { x: (n - 1) * (WINDOW.width + WINDOW.gap), y: 0, width: WINDOW.width, height: WINDOW.height };
}

export function clientArgs({ n, addonsDir, profile, address = "127.0.0.1", mission = "debug", session = "" }) {
  const w = windowFor(n);
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
    "-noSound",
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
  if (!Number.isInteger(n) || n < 1 || n > MAX_CLIENTS) throw new Error(`--clients must be 1..${MAX_CLIENTS}`);
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
  const s = serverArgs({ addonsDir: "A", profile: "SP" });
  if (s[0] !== "-server" || s[1] !== SERVER_WORLD) failures.push("server does not run the bare DebugWorld");
  if (s.some((x) => /EL_AUTOTEST|LF_PlaytestTown|-config/.test(x))) failures.push("server args carry a test define, the playtest mission, or -config");
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
  if (sessionDir({ LOCALAPPDATA: "L" }) !== join("L", "LifeFramework", "session")) failures.push("default session dir changed");
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

  // Registration form R-89 (M1-08): sign a name so each player has a character.
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

  const before1 = state(1);
  const before2 = state(2);
  const other = `Player${parseState(before2).player}`;
  on(1, { verb: "stand", entityName: other });
  wait(2500);
  on(1, { verb: "look" });
  const steps = [];
  steps.push(on(1, { verb: "use", className: "LF_IntroduceAction", entityName: other }, 10000));
  wait(2500);
  steps.push(on(1, { verb: "use", className: "EL_ShowIDAction", entityName: other }, 10000));
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

// The M1-05 card proof on a running `up --clients 2` session: client 1 is a
// civilian who robs Roadmap_Fuel_Till, client 2 is an on-duty officer (debug
// menu, dev gate open on the bare DebugWorld) who receives the alert. Prints
// `state` for both before and after, then judges the after line.
export const ROB_TARGET = "Roadmap_Fuel_Till";
export const ROB_ALERT = "#EL-Police_Alert_Robbery";

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
  const rob = on(1, { verb: "use", className: "EL_RobAction", entityName: ROB_TARGET }, 10000);
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

// The session identity of a player id (EL_Utils.BuildSessionIdentity).
export function sessionUid(playerId) {
  return `00bbbddd-5e55-0000-0000-${String(playerId).padStart(12, "0")}`;
}

// The police roster file the server reads on first use ($profile:LifeFramework/whitelist.txt,
// EL_WhitelistManager format: version, faction entries, job entries). Write it before `up`.
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
  const clockIn = on(2, { verb: "use", className: "EL_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000);
  wait(2500);
  const onDuty2 = state(2);
  if (clockIn.status !== "ok") fails.push(`clock in returned ${clockIn.status} ${clockIn.detail}`);
  if (!onDuty2.includes("faction=POLICE") || !onDuty2.includes("duty=1")) fails.push(`client 2 after clock-in: ${onDuty2}`);

  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "look" });
  const rob = on(1, { verb: "use", className: "EL_RobAction", entityName: ROB_TARGET }, 10000);
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
  const clockOut = on(2, { verb: "use", className: "EL_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000);
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

async function main(argv) {
  const [cmd, ...rest] = argv;
  if (cmd === "selftest") {
    const f = selftestSession();
    for (const x of f) console.log(`  ${x}`);
    console.log(f.length ? "session selftest: FAIL" : "session selftest: OK");
    return f.length ? 1 : 0;
  }
  if (cmd === "up" || cmd === "down" || cmd === "status" || cmd === "restart") {
    const r = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", cmd, ...rest], { windowsHide: true, stdio: "inherit", cwd: process.cwd() });
    return r.status ?? 1;
  }
  if (cmd === "proof") return runProof();
  if (cmd === "police-proof") return runPoliceProof();
  if (cmd === "roster") {
    const ids = rest.map(Number).filter((n) => Number.isInteger(n) && n > 0);
    if (!ids.length) throw new Error("roster <player id> [<player id> ...]");
    return writeRoster(ids);
  }
  if (cmd === "interaction") return runInteraction();
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
  console.log("usage: node agent-box/session.mjs up [--clients N] [--mission debug|playtest] | restart | down | status | state <client> | proof | interaction | selftest");
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
