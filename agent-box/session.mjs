#!/usr/bin/env node
// Multi-client session: one dedicated server plus N game clients on this PC,
// each client with its own -profile, joined over 127.0.0.1:2001.
//
//   node agent-box/session.mjs up --clients 2     boot the server, launch the clients
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
//
// `up` and `down` run through `tools\cli session` because the server boot
// shares the CLI's machine-wide server-2001 lock and addon junction helper.
// The session dir lives outside every worktree (default
// %LOCALAPPDATA%\LifeFramework\session, override LF_SESSION_DIR) and holds no
// junctions.

import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..");

export const SERVER_WORLD = "Worlds/DebugWorld/DebugWorld.ent";
export const MAX_CLIENTS = 3;
export const WINDOW = { width: 640, height: 360, gap: 8 };
export const SERVER_READY = "Entered online game state.";
// The server refuses a client whose script checksum differs
// ("remote script source code checksum does not match"), so clients compile
// with the same define as the server.
export const SESSION_DEFINE = "LF_SESSION_IDENTITY";

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

export function serverArgs({ addonsDir, profile }) {
  return [
    "-server", SERVER_WORLD,
    "-addonsDir", addonsDir,
    "-addons", "LifeFramework",
    "-profile", profile,
    "-maxFPS", "30",
    "-logLevel", "normal",
    // -noBackend clients all carry the NULL identity and the same Steam name;
    // EL_Utils.GetPlayerUID gives each player id its own identity under this define.
    "-scrDefine", SESSION_DEFINE,
    "-disableCrashReporter",
    "-noBackend",
  ];
}

// Small windows in a row along the top of the screen, so the user's desk
// stays usable. -screenWidth/-screenHeight/-posX/-posY need -window.
export function windowFor(n) {
  return { x: (n - 1) * (WINDOW.width + WINDOW.gap), y: 0, width: WINDOW.width, height: WINDOW.height };
}

export function clientArgs({ n, addonsDir, profile, address = "127.0.0.1" }) {
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
    "-scrDefine", SESSION_DEFINE,
    "-disableCrashReporter",
    "-noBackend",
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
  const o1 = "faction=CIVILIAN duty=0 wanted=0 rank=0 cash=500 bank=0 job=UNEMPLOYED org=none player=1";
  const oa1 = o1.replace("cash=500", "cash=250").replace("org=none", `org=${ORG_NAME}`);
  const oa2 = o1.replace("player=1", "player=2").replace("org=none", `org=${ORG_NAME}`);
  if (judgeOrg(o1, oa1, oa2, "cannot_perform", GATE_OPENED).length) failures.push(`judgeOrg failed a good run: ${judgeOrg(o1, oa1, oa2, "cannot_perform", GATE_OPENED)}`);
  if (!judgeOrg(o1, oa1, o1, "cannot_perform", GATE_OPENED).length) failures.push("judgeOrg passed an invitee who never joined");
  if (!judgeOrg(o1, oa1, oa2, GATE_OPENED, GATE_OPENED).length) failures.push("judgeOrg passed a gate that opened for a stranger");
  if (!judgeOrg(o1, oa1.replace("cash=250", "cash=500"), oa2, "cannot_perform", GATE_OPENED).length) failures.push("judgeOrg passed a founding with no fee");
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
  steps.push(on(1, { verb: "use", className: "EL_OrganizationDeskAction", entityName: "Roadmap_Org_Desk" }, 10000));
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

  // Arrive: both pick civilian and spawn in town. The faction menu is gone once
  // a character exists, so a second proof on the same session skips this.
  for (const n of [1, 2]) {
    const ui = on(n, { verb: "ui" });
    if (ui.status === "ok" && ui.detail.includes("CivilianButton")) {
      on(n, { verb: "click", widget: "CivilianButton" });
      wait(1500);
      on(n, { verb: "click", widget: "TownButton" });
    }
  }
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

async function main(argv) {
  const [cmd, ...rest] = argv;
  if (cmd === "selftest") {
    const f = selftestSession();
    for (const x of f) console.log(`  ${x}`);
    console.log(f.length ? "session selftest: FAIL" : "session selftest: OK");
    return f.length ? 1 : 0;
  }
  if (cmd === "up" || cmd === "down" || cmd === "status") {
    const r = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", cmd, ...rest], { windowsHide: true, stdio: "inherit", cwd: process.cwd() });
    return r.status ?? 1;
  }
  if (cmd === "proof") return runProof();
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
  console.log("usage: node agent-box/session.mjs up [--clients N] | down | status | state <client> | proof | interaction | org [--shot] | selftest");
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
