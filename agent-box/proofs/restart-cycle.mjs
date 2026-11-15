import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { pidAlive, STOP_FILE } from "../../tools/host/host.mjs";
import { decideExit, RESTART_MARKER } from "../../tools/host/watchdog.mjs";
import { safeRemoveTree } from "../../tools/link-safety.mjs";
import { extendLease, touchLease } from "../../tools/lib/session-lease.mjs";
import { adoptLock, transferLock } from "../../tools/wt.mjs";
import { ROOT, arrive, clientBox, newestLog, parseState, readSession, sessionDir } from "../session.mjs";

// #1268, the scheduled-restart cycle end to end, on `session up --mission playtest` (one client):
//  1. client 1 arrives and reads its bank and cash; the session records its identity;
//  2. the restart is set N minutes out in the session server's own owner config
//     (tools/playtest/opening/b7-restart-setup.mjs, warnings 15, 5, 1) and `session restart` boots
//     the server on it: the server logs "restart warner: ticking";
//  3. "warnings reach a client": the server logs the 1 minute warning (ticker, #1348) and the toast
//     to every player (#1265), and client 1's own log has "toast received: #LF-Ops_RestartToast";
//  4. "the server closes with the restart marker": the server process exits by itself, its log has
//     the O8 close line (#1267) and the SHUTDOWN save, and the watchdog's own judgement
//     (tools/host/watchdog.mjs decideExit, #1262) says "scheduled restart", not a crash;
//  5. "it comes back": the proof does the watch's restart (clears the marker, boots the same lane
//     from the save with `session restart --after-close`) and the new server is up with the client;
//  6. "the save loads": the new server logs "save loaded:" and client 1 is the same identity with
//     the same bank and cash.
// `--no-restart` leaves step 5's restart out (the watch is off): the proof must fail at "comes back".
// `--in N` minutes (default 4) sets how far out the restart is from the config write.

const SETUP = join(ROOT, "tools", "playtest", "opening", "b7-restart-setup.mjs");
const TOAST_KEY = "#LF-Ops_RestartToast";

const val = (args, name, dflt) => {
  const i = args.indexOf(name);
  return i === -1 || i + 1 >= args.length ? dflt : args[i + 1];
};
const read = (f) => (f && existsSync(f) ? readFileSync(f, "utf8") : "");

async function runRestartCycle(args) {
  const noRestart = args.includes("--no-restart");
  const inMin = Number(val(args, "--in", "4"));
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = () => parseState(on(1, { verb: "state" }, 10000).detail);
  const serverLog = () => newestLog(join(readSession(dir).serverProfile, "logs"));
  const clientLog = () => newestLog(join(dir, "client1", "logs"));
  const fails = [];
  const step = (name, ok, detail) => {
    console.log(`${ok ? "ok  " : "FAIL"}  ${name}: ${detail}`);
    if (!ok) fails.push(`${name}: ${detail}`);
    return ok;
  };
  const verdict = () => {
    console.log(fails.length ? `restart-cycle: FAIL at ${fails[0]}` : "restart-cycle: PASS (warnings reached the client, the server closed with the restart marker, came back and loaded the save)");
    return fails.length ? 1 : 0;
  };

  const s0 = readSession(dir);
  if (!s0 || s0.stoppedAt || !pidAlive(s0.serverPid)) {
    console.log("FAIL  no running session: 'tools\\cli session up --mission playtest' first");
    return 1;
  }
  if (s0.mission !== "playtest") {
    console.log("FAIL  the proof needs 'session up --mission playtest' (session restart and the shutdown save)");
    return 1;
  }
  if (!(inMin >= 2 && inMin <= 30)) {
    console.log(`FAIL  --in must be 2 to 30 minutes (got ${val(args, "--in", "")})`);
    return 1;
  }
  // The whole cycle is longer than the idle close; hold the session for it.
  extendLease(dir, inMin + 12);
  const profile = join(s0.serverProfile, "profile");
  const lock = s0.lane?.lock ?? "server-2001";

  // 1. arrive and read the account.
  arrive(on, 1, wait);
  wait(8000);
  const opening = state();
  const uidBefore = readSession(dir)?.clients?.find((c) => c.n === 1)?.identity?.uid;
  console.log(`opening: uid=${uidBefore} bank=${opening.bank} cash=${opening.cash}`);
  if (!step("arrive", opening.bank !== undefined && opening.cash !== undefined, `bank=${opening.bank} cash=${opening.cash}`)) return verdict();

  // 2. the schedule: the session server's own owner config, then a boot that reads it.
  const cfgDir = join(profile, "LifeFramework");
  const cfg = join(cfgDir, "config.json");
  if (!existsSync(cfg)) {
    mkdirSync(cfgDir, { recursive: true });
    writeFileSync(cfg, JSON.stringify({ schemaVersion: 1 }, null, 2) + "\n");
  }
  const setup = spawnSync(process.execPath, [SETUP, "--profile", cfgDir, "--in", String(inMin)], { encoding: "utf8", windowsHide: true });
  process.stdout.write(setup.stdout ?? "");
  const at = /PASS (\d\d:\d\d)/.exec(setup.stdout ?? "")?.[1];
  if (!step("schedule written", setup.status === 0 && !!at, at ? `restart at ${at} in ${cfg}` : `setup exited ${setup.status}`)) return verdict();
  const boot = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "restart"], { encoding: "utf8", windowsHide: true, env: { ...process.env, LF_SESSION_ROUTED: "1" }, timeout: 420000 });
  process.stdout.write(boot.stdout ?? "");
  if (!step("boot on the schedule", boot.status === 0, `session restart exited ${boot.status}`)) return verdict();
  const ticking = /restart warner: ticking[^\r\n]*/.exec(read(serverLog()))?.[0];
  if (!step("schedule read", !!ticking, ticking ?? `no "restart warner: ticking" line: the owner config did not schedule ${at}`)) return verdict();

  // 3. warnings reach the client; 4. the server closes by itself. One pass until the server exits.
  const s1 = readSession(dir);
  const pid = s1.serverPid;
  const until = /next in (\d+) s/.exec(ticking);
  const deadline = Date.now() + ((until ? Number(until[1]) : inMin * 60) + 180) * 1000;
  let held = false;
  let before = null;
  while (Date.now() < deadline && pidAlive(pid)) {
    touchLease(dir, "restart-cycle");
    // From the 1 minute warning on, read the account once and hold the lane across the close, as
    // `session restart` does, so a queued run cannot take it between the exit and the new boot.
    if (!held && /restart warning: 1 minute/.test(read(serverLog()))) {
      before = state();
      try {
        adoptLock(ROOT, lock, pid);
        held = true;
      } catch (e) {
        console.log(`WARN  lane ${lock} not held across the close: ${e.message}`);
        held = true;
      }
    }
    wait(1000);
  }
  const text = read(serverLog());
  const ctext = read(clientLog());
  const warned = (text.match(/restart warning: \d+ minute\(s\)[^\r\n]*/g) ?? []);
  const toasted = (text.match(/restart toast: told every player[^\r\n]*/g) ?? []);
  const received = (ctext.match(new RegExp(`toast received[^:]*: ${TOAST_KEY}`, "g")) ?? []).length;
  for (const l of [...warned, ...toasted]) console.log(`server: ${l}`);
  step("warnings reach a client", warned.length > 0 && toasted.length > 0 && received > 0, `${warned.length} warning(s), ${toasted.length} toast(s) sent, ${received} received in client 1's log`);

  const exited = !pidAlive(pid);
  const closeLine = /restart due: wrote [^\r\n]*/.exec(text)?.[0];
  const shutdownSave = /\(SHUTDOWN\)/.test(text);
  const marker = join(profile, RESTART_MARKER[1]);
  const signals = { stopFilePresent: existsSync(join(profile, STOP_FILE[1])), stopSeen: false, logStop: /\[LFDebug:Host\] stop requested/.test(text), restartMarker: existsSync(marker), logRestart: !!closeLine };
  const judged = decideExit(signals);
  console.log(`watchdog judgement: ${judged.action} (${judged.why}); marker ${signals.restartMarker ? "present" : "absent"}`);
  if (!step("closes with the restart marker", exited && !!closeLine && signals.restartMarker && shutdownSave && judged.action === "restart" && !judged.crash, `exited=${exited} close line=${!!closeLine} marker=${signals.restartMarker} SHUTDOWN save=${shutdownSave} judged=${judged.why}`)) {
    if (held && !exited) transferLock(ROOT, lock, pid);
    return verdict();
  }

  // 5. the watch's restart: clear the marker it consumed, boot the lane again from the save.
  if (existsSync(marker)) safeRemoveTree(marker);
  // Hand the lane back to the closed server's pid: `session restart --after-close` adopts it from there.
  if (held) {
    try {
      transferLock(ROOT, lock, pid);
    } catch {}
  }
  if (!noRestart) {
    const back = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "restart", "--after-close"], { encoding: "utf8", windowsHide: true, env: { ...process.env, LF_SESSION_ROUTED: "1" }, timeout: 420000 });
    process.stdout.write(back.stdout ?? "");
  } else {
    console.log("--no-restart: the watch is off, nothing boots the server again; waiting 60 s");
    wait(60000);
  }
  const s2 = readSession(dir);
  const newPid = s2?.serverPid;
  const cameBack = !!newPid && newPid !== pid && pidAlive(newPid) && !!s2.readyAt;
  if (!step("comes back", cameBack, cameBack ? `server pid ${pid} -> ${newPid}, client rejoined` : `no new server after the close (session server pid ${newPid ?? "-"})`)) return verdict();

  // 6. the save loads: the line every store read ends with, and the same account.
  const loaded = /save loaded:[^\r\n]*/.exec(read(serverLog()))?.[0];
  wait(8000);
  let after = state();
  for (let i = 0; i < 3 && after.bank === undefined; i++) {
    wait(5000);
    after = state();
  }
  const ref = before && before.bank !== undefined ? before : opening;
  const uidAfter = readSession(dir)?.clients?.find((c) => c.n === 1)?.identity?.uid;
  const same = after.bank === ref.bank && after.cash === ref.cash && (!uidBefore || !uidAfter || uidAfter === uidBefore);
  step("the save loads", !!loaded && same, `${loaded ?? "no \"save loaded:\" line"}; uid ${uidBefore} -> ${uidAfter}, bank ${ref.bank} -> ${after.bank}, cash ${ref.cash} -> ${after.cash}`);

  // Leave the session's config as it was (the next restart at the same time tomorrow is not wanted).
  spawnSync(process.execPath, [SETUP, "--profile", cfgDir, "--undo"], { windowsHide: true });
  return verdict();
}

export default {
  name: "restart-cycle",
  usage: "restart-cycle [--in N (minutes, default 4)] [--no-restart (red: the watch is off)]   (session up --mission playtest)",
  run: (ctx) => runRestartCycle(ctx?.args ?? []),
};
