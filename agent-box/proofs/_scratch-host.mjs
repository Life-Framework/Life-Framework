// Scratch host under the watch plus a plain client the harness drives (#1588). The setup a
// crash-then-rejoin proof (#1563) runs on top of; `crash-host-setup` is its own proof verb.
//
// Why it is built this way:
// - `host start` never passes a script define (tools/host/host.mjs selftest), and a client with a
//   define is refused by such a server ("remote script source code checksum does not match"). So the
//   client here carries no -scrDefine: the same checksum a player's client has.
// - The harness poll on a client (LF_AgentHarness.EnsureLoop) needs no define: it runs for any local
//   player and reads $profile:agent-box/command.txt. The client gets its own -profile (its own
//   channel) and -lfSession <id>, a CLI parameter, not a define, so commands are addressed to it.
//   `ui`, `type` and `click` act on the client alone. `state` asks the server, whose dev gate
//   (LF_DevGate) is closed on the playtest header for a non-admin, so it may answer dev_gate_closed;
//   the faction is then read from what the player sees.
// - Port and lock: the scratch host takes a pool session lane (pool.acquireSessionLaneSync, the queue
//   `session up` uses) and hands it to `host watch` -> `host start` as LF_SERVER_SLOT_HELD=<lock>:<token>.
//   `host start` then binds that lane's port (-addr 127.0.0.1:<port>) and leaves the lock with this
//   process, so a crash restart needs no new lock and never takes server-2001 beside a live session.
//   The lease of this checkout's session dir is extended while the scratch host runs, so the idle
//   reaper does not close it.
// - Everything lives in a scratch dir under %LOCALAPPDATA%\LifeFramework\scratch-host\; teardown is by
//   the pids this module started or read from host.json, never by image name.
import { spawn, spawnSync } from "node:child_process";
import { randomBytes } from "node:crypto";
import { closeSync, existsSync, mkdirSync, openSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import * as pool from "../../tools/remote/pool.mjs";
import * as wt from "../../tools/wt.mjs";
import { hostPaths, newestConsoleLog, pidAlive, readHostState, READY_LINE } from "../../tools/host/host.mjs";
import { launchAddons } from "../../tools/lib/addons.mjs";
import { quietSpawnDetached } from "../../tools/lib/quiet-launch.mjs";
import { interactiveNeeded, interactiveSpawnDetached } from "../../tools/lib/interactive-launch.mjs";
import { extendLease, sessionDirOf } from "../../tools/lib/session-lease.mjs";
import { safeRemoveTree } from "../../tools/link-safety.mjs";
import { applyClientSettings, arrive, clientArgs, newSessionId, parseState, passQuiz, resolveProfile } from "../session.mjs";
import { sendCommand, setBoxes } from "../harness.mjs";

export const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const GAME_DIR = process.env.ENFUSION_GAME_PATH || "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Arma Reforger";
// The server log lines that say a client reached the game (cli.mjs cmdSessionJoin uses the same).
export const JOIN_MARKER = /(?:registration form sent to player|spawning player|quiz sent to player) \d+/g;

const sleep = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);

// Pure: a plain client's launch args: the session client's args with every -scrDefine pair removed.
export function plainClientArgs(args) {
  const out = [];
  for (let i = 0; i < args.length; i++) {
    if (args[i] === "-scrDefine") {
      i++;
      continue;
    }
    out.push(args[i]);
  }
  return out;
}

export function scratchBase(env = process.env) {
  const base = env.LOCALAPPDATA || join(env.USERPROFILE || ".", "AppData", "Local");
  return join(base, "LifeFramework", "scratch-host");
}

function joinsIn(log) {
  if (!log || !existsSync(log)) return 0;
  return (readFileSync(log, "utf8").match(JOIN_MARKER) ?? []).length;
}

// Takes a session lane, starts `host watch` on a scratch LF_HOST_DIR (unless noHost: the red run) and
// waits for the world. Returns a handle; call handle.teardown() in a finally.
// maxRestarts is the watch's --max-restarts (0 is the red run of #1563: the watch does not restart a
// crash). config, when given, is written as the host's $profile:LifeFramework/config.json before boot
// (the autosave interval of a crash proof).
export function startScratchHost({ root = ROOT, waitMin = 10, noHost = false, log = console.log, readyS = 420, maxRestarts = 3, config = null } = {}) {
  const stamp = new Date().toISOString().replace(/[:.]/g, "-");
  const dir = join(scratchBase(), `${pool.slugFor(root)}-${stamp}`);
  const hostDir = join(dir, "host");
  mkdirSync(hostDir, { recursive: true });
  const token = randomBytes(8).toString("hex");
  const lane = pool.acquireSessionLaneSync(root, { waitMs: waitMin * 60000, meta: { purpose: "session", root, clients: 1, profile: "lowspec", token, scratchHost: dir } });
  const lock = lane?.lockName ?? lane?.name;
  const port = lane?.port ?? pool.portForLock(lock);
  log(`scratch: lane ${lock} (port ${port}), dir ${dir}`);
  const sdir = sessionDirOf(root);
  mkdirSync(sdir, { recursive: true });
  extendLease(sdir, 30);
  const h = { root, dir, hostDir, lock, port, token, lane, watchPid: 0, client: null, id: newSessionId(), log };
  h.teardown = () => teardown(h);
  if (noHost) return h;
  if (config) {
    const cfgDir = join(hostPaths(hostDir).profile, "profile", "LifeFramework");
    mkdirSync(cfgDir, { recursive: true });
    writeFileSync(join(cfgDir, "config.json"), `${JSON.stringify(config, null, 2)}\n`);
  }
  const out = openSync(join(dir, "watch.log"), "a");
  const env = { ...process.env, LF_HOST_DIR: hostDir, [pool.SLOT_ENV]: `${lock}:${token}` };
  const watch = spawn(process.execPath, [join(root, "tools", "cli.mjs"), "host", "watch", "--max-restarts", String(maxRestarts), "--crash-limit", "0"], { cwd: root, env, stdio: ["ignore", out, out], windowsHide: true });
  closeSync(out);
  h.watchPid = watch.pid;
  watch.unref();
  log(`scratch: host watch pid ${h.watchPid} (log ${join(dir, "watch.log")})`);
  const deadline = Date.now() + readyS * 1000;
  while (Date.now() < deadline) {
    const pid = readHostState(hostDir)?.pid;
    const f = newestConsoleLog(hostPaths(hostDir).profile);
    if (pid && pidAlive(pid) && f && readFileSync(f, "utf8").includes(READY_LINE)) {
      h.serverLog = f;
      log(`scratch: host up (pid ${pid}) on 127.0.0.1:${port}`);
      return h;
    }
    if (!pidAlive(h.watchPid)) break;
    sleep(3000);
  }
  throw Object.assign(new Error(`host did not come up under the watch within ${readyS}s; see ${join(dir, "watch.log")}`), { handle: h });
}

export function hostPid(h) {
  const pid = readHostState(h.hostDir)?.pid;
  return pid && pidAlive(pid) ? pid : 0;
}

// Starts the plain client (no -scrDefine) on its own scratch profile and waits for its join line in the
// host's console.log. Returns the client record, or throws "client joined: ..." on a timeout.
export function joinPlainClient(h, { joinS = 240 } = {}) {
  const profile = join(h.dir, "client1");
  const box = join(profile, "profile", "agent-box");
  mkdirSync(box, { recursive: true });
  const rp = resolveProfile("lowspec");
  applyClientSettings(profile, rp);
  const args = plainClientArgs(clientArgs({ n: 1, addonsDir: join(h.root, "addons"), profile, port: h.port, mission: "playtest", session: h.id, rp, addons: launchAddons(h.root) }));
  if (args.includes("-scrDefine")) throw new Error("plain client args still carry -scrDefine");
  const game = join(GAME_DIR, "ArmaReforgerSteam.exe");
  const log = h.serverLog ?? newestConsoleLog(hostPaths(h.hostDir).profile);
  const before = joinsIn(log);
  const pid = interactiveNeeded() ? interactiveSpawnDetached(game, args, { cwd: GAME_DIR, marker: profile }) : quietSpawnDetached(game, args, { cwd: GAME_DIR });
  h.client = { pid, profile, box, args };
  h.log(`scratch: plain client pid ${pid} -> 127.0.0.1:${h.port} (session id ${h.id}, no -scrDefine)`);
  const deadline = Date.now() + joinS * 1000;
  while (Date.now() < deadline) {
    const cur = h.serverLog ?? newestConsoleLog(hostPaths(h.hostDir).profile);
    if (cur && joinsIn(cur) > (cur === log ? before : 0)) {
      h.log("scratch: client joined (join line in the host log)");
      return h.client;
    }
    if (!pidAlive(pid)) throw new Error(`client joined: no, the client (pid ${pid}) exited`);
    sleep(2000);
  }
  throw new Error(`client joined: no join line in the host log within ${joinS}s`);
}

// A client that lost its server: closes it by pid and starts the plain client again on the same scratch
// profile, against the host's newest console.log (`serverLog` is re-pointed first, so only a join line in
// the restarted host's log counts).
export function rejoinPlainClient(h, { joinS = 240 } = {}) {
  if (h.client?.pid && pidAlive(h.client.pid)) {
    spawnSync("taskkill", ["/PID", String(h.client.pid), "/T", "/F"], { windowsHide: true });
    for (let i = 0; i < 20 && pidAlive(h.client.pid); i++) sleep(500);
  }
  h.serverLog = newestConsoleLog(hostPaths(h.hostDir).profile) ?? h.serverLog;
  return joinPlainClient(h, { joinS });
}

// Sends one harness command to the scratch client.
export function on(h, spec, timeoutMs = 8000) {
  setBoxes([h.client.box], h.id);
  // The game holds result.txt open for a moment after it writes it: the harness's unlink can hit EBUSY or EPERM.
  for (let i = 0; ; i++) {
    try {
      return sendCommand(spec, timeoutMs);
    } catch (e) {
      if (i >= 4 || !/EBUSY|EPERM|EACCES/.test(String(e?.code ?? e?.message))) throw e;
      sleep(1500);
    }
  }
}

// Walks the client through what a new player sees (quiz, faction, spawn, registration) with ui, click
// and type only, until no arrival screen is up. Returns the last `ui` detail.
export function registerClient(h, { rounds = 12 } = {}) {
  const send = (_n, spec, t) => on(h, spec, t);
  let last = "";
  for (let r = 0; r < rounds; r++) {
    const ui = on(h, { verb: "ui" });
    last = ui.status === "ok" ? ui.detail : `${ui.status} ${ui.detail}`;
    if (ui.status !== "ok") {
      sleep(3000);
      continue;
    }
    if (ui.detail.includes("QuizOption0Button")) passQuiz(send, 1, sleep);
    else if (ui.detail.includes("SignButton")) arrive(send, 1, sleep);
    else if (ui.detail.includes("CivilianButton")) on(h, { verb: "click", widget: "CivilianButton" });
    else if (ui.detail.includes("TownButton")) on(h, { verb: "click", widget: "TownButton" });
    else if (r > 0) return last;
    sleep(3000);
  }
  return last;
}

// Pure: what the scratch host's console.log says about one player: identity, faction (the role kit
// the spawn applied), bank balance (the last ledger line) and the cash top-ups it paid. This proof
// owns that server, so its log is fair evidence; it is what `state` would say when the dev gate refuses.
export function hostLogState(text, playerId = 1) {
  const lines = String(text ?? "").split(/\r?\n/);
  let uid = "";
  let faction = "";
  let bank = "";
  const clock = `[LFDebug:Playtime] clock started for player ${playerId} (`;
  for (const l of lines) {
    const i = l.indexOf(clock);
    if (i >= 0) uid = l.slice(i + clock.length).split(")")[0];
    const k = /\[LFDebug:Spawn\] applied role kit (\w+)/.exec(l);
    if (k) faction = k[1];
  }
  let cash = 0;
  let topups = 0;
  if (uid) {
    for (const l of lines) {
      const b = /\[LFDebug:Ledger\] #\d+ (\S+) bank -?\d+ \([^)]*\) balance (-?\d+)/.exec(l);
      if (b && b[1] === uid) bank = b[2];
      const c = /\[LFDebug:Money\] debug-topup cash \+(\d+) \(([^)]+)\)/.exec(l);
      if (c && c[2] === uid) {
        cash += Number(c[1]);
        topups++;
      }
    }
  }
  return { uid, faction, bank, cashTopup: topups ? String(cash) : "" };
}

// The client's state: `state` when the server's gate answers. On a hosted header (dev tools off)
// LF_DevGate refuses it for a non-admin (dev_gate_closed, #693b) and no define may open it; then the
// host log answers (hostLogState).
export function readState(h) {
  const st = on(h, { verb: "state" }, 10000);
  if (st.status === "ok") return { state: st, parsed: parseState(st.detail), fromLog: null };
  // The playtime line that names the player's uid follows the spawn by a few seconds.
  let fromLog = null;
  for (let i = 0; i < 15; i++) {
    const log = newestConsoleLog(hostPaths(h.hostDir).profile);
    fromLog = log ? hostLogState(readFileSync(log, "utf8")) : null;
    if (fromLog?.uid && fromLog.faction) break;
    sleep(2000);
  }
  return { state: st, parsed: null, fromLog };
}

export function teardown(h) {
  const said = [];
  if (h.client?.pid && pidAlive(h.client.pid)) {
    spawnSync("taskkill", ["/PID", String(h.client.pid), "/T", "/F"], { windowsHide: true });
    said.push(`client ${h.client.pid} closed`);
  }
  // Watch first (so it does not restart the host), then the host by the pid in host.json.
  if (h.watchPid && pidAlive(h.watchPid)) {
    spawnSync("taskkill", ["/PID", String(h.watchPid), "/T", "/F"], { windowsHide: true });
    said.push(`watch ${h.watchPid} closed`);
  }
  const hp = readHostState(h.hostDir)?.pid;
  if (hp && pidAlive(hp)) {
    spawnSync("taskkill", ["/PID", String(hp), "/T", "/F"], { windowsHide: true });
    said.push(`host ${hp} closed`);
  }
  for (let i = 0; i < 20 && hp && pidAlive(hp); i++) sleep(500);
  try {
    if (h.lane?.release) h.lane.release();
    else if (h.lock) wt.releaseLock(h.root, h.lock);
  } catch {}
  said.push(`lane ${h.lock} released`);
  // A closed game keeps its log open for a moment (EBUSY): wait for the pids, then retry the removal.
  for (let i = 0; i < 20 && h.client?.pid && pidAlive(h.client.pid); i++) sleep(500);
  if (!process.env.LF_SCRATCH_KEEP) {
    let err = null;
    for (let i = 0; i < 5; i++) {
      try {
        safeRemoveTree(h.dir);
        err = null;
        break;
      } catch (e) {
        err = e;
        sleep(2000);
      }
    }
    if (err) said.push(`scratch dir kept (${err.message})`);
  } else said.push(`scratch dir kept: ${h.dir}`);
  h.log(`scratch: teardown: ${said.join(", ")}`);
}

export function selftestScratchHost() {
  const f = [];
  const a = plainClientArgs(["-client", "127.0.0.1:2011", "-scrDefine", "LF_SESSION_IDENTITY", "-scrDefine", "LF_HARNESS", "-lfSession", "x"]);
  if (a.includes("-scrDefine") || a.includes("LF_HARNESS") || a.includes("LF_SESSION_IDENTITY")) f.push("plain client args keep a define");
  if (a.join(" ") !== "-client 127.0.0.1:2011 -lfSession x") f.push(`plain client args dropped more than the defines: ${a.join(" ")}`);
  const log = [
    "[LFDebug:Ledger] #1 U-1 bank 1000000 (starting-bank) balance 1000000 counterparty ''",
    "[LFDebug:Spawn] applied role kit CIVILIAN",
    "[LFDebug:Money] debug-topup cash +19500 (U-1)",
    "[LFDebug:Money] debug-topup cash +7 (U-2)",
    "[LFDebug:Playtime] clock started for player 1 (U-1)",
  ].join("\n");
  const st = hostLogState(log, 1);
  if (st.uid !== "U-1" || st.faction !== "CIVILIAN" || st.bank !== "1000000" || st.cashTopup !== "19500") f.push(`hostLogState read ${JSON.stringify(st)}`);
  if (hostLogState("", 1).faction !== "") f.push("hostLogState invents a faction from an empty log");
  return f;
}
