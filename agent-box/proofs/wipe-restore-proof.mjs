import { spawnSync } from "node:child_process";
import { copyFileSync, existsSync, mkdirSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { basename, join } from "node:path";
import { fileURLToPath } from "node:url";
import { safeRemoveTree } from "../../tools/link-safety.mjs";
import { READY_LINE, STOP_FILE, findSavePoints, hostPaths, newestConsoleLog, pidAlive, saveRoot } from "../../tools/host/host.mjs";
import { prepareScratch } from "../../tools/host/drill.mjs";
import { ageNotice, simulatePost } from "../../tools/playtest/opening/b9-wipe.mjs";
import { ROOT, arrive, newestLog, readSession, sessionDir } from "../session.mjs";

// Chapter #1006 slice #1562: #556's wipe dry run, with no person at the keyboard.
//   node agent-box/session.mjs up --mission playtest --clients 1 --wait     (the flagship pack is fine too)
//   tools\cli session wipe-restore-proof [--drop-backup] [--keep]
// Client 1 registers one account and the session server stops through its host stop file (the
// engine writes the SHUTDOWN save); the session is then closed. Its saves are copied into a SCRATCH
// host folder under tmp/ (never a real host: the proof only ever sets LF_HOST_DIR to that copy), and
// the real host commands run on it: `host wipe` with the wipe notice aged 25 h in a scratch
// sessions.json (the notice-ageing of tools/playtest/opening/b9-wipe.mjs, #1527/#1549) backs up and
// clears; `host start` boots clean and the server loads no account; `host restore` of the wipe's
// backup and one more `host start` loads the account back. The server console log is the judge:
// `[LFDebug:Persistence] applied N account record(s)`.
//
// The session ends with this proof (the host boots need port 2001). `--drop-backup` is the red
// run: the wipe's backup is deleted before the restore, so nothing comes back and the proof fails at
// "account back". `--selftest` runs the judge alone on canned logs (instant, no game).
export const STEPS = ["account registered", "wipe backed up", "wipe cleared", "clean boot", "no accounts", "account back"];
const ACCOUNTS = /\[LFDebug:Persistence\] applied (\d+) account record\(s\)/g;
const REGISTERED = /\[LFDebug:CharacterCreation\] character registered for player (\d+)/;
const SHUTDOWN_SAVE = /\[LFDebug:Persistence\] save completed \(SHUTDOWN\)/;

// The most account records one boot's log says it applied (0 when it says none).
export function accountsApplied(text) {
  let most = 0;
  for (const m of String(text ?? "").matchAll(ACCOUNTS)) most = Math.max(most, Number(m[1]));
  return most;
}

// Pure. r = { sessionLog, wipe: { code, backup, aside, left, rows }, clean: { started, text }, back: { started, text, restoreCode } }.
// Returns [{ id, ok, detail }] in STEPS order; a step whose precondition failed is not run again
// as a pass (it reports the missing input).
export function judgeWipeRestore(r) {
  const out = [];
  const step = (id, ok, detail) => out.push({ id, ok: !!ok, detail });
  const reg = REGISTERED.exec(String(r.sessionLog ?? ""));
  step("account registered", reg && SHUTDOWN_SAVE.test(r.sessionLog), reg ? `player ${reg[1]} registered, SHUTDOWN save written` : "the session server log has no 'character registered' line before its shutdown save");
  const w = r.wipe ?? {};
  step("wipe backed up", w.code === 0 && w.backup && w.rows === 1, `host wipe exit ${w.code}, backup=${w.backup ?? "none"} with a save point, wipes.log rows=${w.rows}`);
  step("wipe cleared", w.code === 0 && w.aside && w.left === 0, `moved aside to ${w.aside ?? "nowhere"}, save points left under the profile=${w.left}`);
  const c = r.clean ?? {};
  step("clean boot", c.started && String(c.text ?? "").includes(READY_LINE) && SHUTDOWN_SAVE.test(c.text ?? ""), c.started ? "the wiped host reached the world and saved on stop" : "the wiped host did not start");
  const n = accountsApplied(c.text);
  step("no accounts", c.started && n === 0, `the clean boot applied ${n} account record(s)`);
  const b = r.back ?? {};
  const m = accountsApplied(b.text);
  step("account back", b.started && m >= 1, `host restore exit ${b.restoreCode}; the boot after it applied ${m} account record(s)`);
  return out;
}

export function firstFailure(steps) {
  return steps.find((s) => !s.ok) ?? null;
}

export function selftestWipeRestore() {
  const f = [];
  const sessionLog = ["[LFDebug:CharacterCreation] character registered for player 1, age 30, slot 1", "[LFDebug:Persistence] save completed (SHUTDOWN)"].join("\n");
  const wipe = { code: 0, backup: "2026-11-15_10-00-00-wipe", aside: "2026-11-15_10-00-00-wiped-saves", left: 0, rows: 1 };
  const boot = (n) => ({ started: true, text: [READY_LINE, ...(n ? [`[LFDebug:Persistence] applied ${n} account record(s)`] : []), "[LFDebug:Persistence] save completed (SHUTDOWN)"].join("\n") });
  const good = { sessionLog, wipe, clean: boot(0), back: { ...boot(1), restoreCode: 0 } };
  const goodSteps = judgeWipeRestore(good);
  if (firstFailure(goodSteps)) f.push(`a good wipe and restore failed at "${firstFailure(goodSteps).id}"`);
  if (goodSteps.map((s) => s.id).join() !== STEPS.join()) f.push("the judge's steps are not STEPS, in order");
  // red-proof 2026-11-15: with `m >= 1` weakened to `b.started` the backup-deleted run below passed.
  const dropped = firstFailure(judgeWipeRestore({ ...good, back: { ...boot(0), restoreCode: 1 } }));
  if (dropped?.id !== "account back") f.push(`a restore with the backup deleted did not fail at "account back" (failed at ${dropped?.id})`);
  const kept = firstFailure(judgeWipeRestore({ ...good, clean: boot(1) }));
  if (kept?.id !== "no accounts") f.push(`a wipe that left the account in did not fail at "no accounts" (failed at ${kept?.id})`);
  const unregistered = firstFailure(judgeWipeRestore({ ...good, sessionLog: SHUTDOWN_SAVE.source }));
  if (unregistered?.id !== "account registered") f.push(`a session with no registered account did not fail at "account registered" (failed at ${unregistered?.id})`);
  const unwiped = firstFailure(judgeWipeRestore({ ...good, wipe: { ...wipe, left: 3, aside: null } }));
  if (unwiped?.id !== "wipe cleared") f.push(`a wipe that cleared nothing did not fail at "wipe cleared" (failed at ${unwiped?.id})`);
  if (!judgeWipeRestore({ ...good, clean: { started: false, text: "" } }).some((s) => s.id === "clean boot" && !s.ok)) f.push("a host that never started passed the clean boot");
  return f;
}

function hostCli(scratch, args, env, timeoutMs) {
  const r = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "host", ...args], {
    cwd: ROOT, encoding: "utf8", timeout: timeoutMs, windowsHide: true,
    env: { ...process.env, LF_HOST_DIR: scratch, LF_QUIET_CHILD: "1", ...env },
  });
  return { code: r.status ?? 1, out: `${r.stdout ?? ""}${r.stderr ?? ""}` };
}

// One `host start` on the scratch host, then `host stop`; the console log it wrote is returned. Kept in tmp/logs.
// Port 2001 goes to the next queued run the moment the session closes, so start queues for it (--wait, up to 30 min).
function bootScratch(scratch, label, env, stamp) {
  const started = hostCli(scratch, ["start", "--force", "--wait", "--timeout", "300"], env, 2400000);
  const log = newestConsoleLog(hostPaths(scratch).profile);
  const stopped = hostCli(scratch, ["stop"], env, 240000);
  if (stopped.code !== 0) console.log(`wipe-restore: ${label} boot: host stop exited ${stopped.code}`);
  const text = log && existsSync(log) ? readFileSync(log, "utf8") : "";
  let kept = log;
  if (log && existsSync(log)) {
    mkdirSync(join(ROOT, "tmp", "logs"), { recursive: true });
    kept = join(ROOT, "tmp", "logs", `wipe-restore-${stamp}-${label}.log`);
    copyFileSync(log, kept);
  }
  console.log(`wipe-restore: ${label} boot: host start exit ${started.code}; log ${kept ?? "none"}`);
  if (started.code !== 0) console.log(`  ${started.out.trim().split(/\r?\n/).slice(-4).join("\n  ")}`);
  return { started: started.code === 0, text, log: kept };
}

async function runWipeRestoreProof(ctx) {
  const args = ctx?.args ?? [];
  if (args.includes("--selftest")) {
    const f = selftestWipeRestore();
    for (const x of f) console.log(`  ${x}`);
    console.log(f.length ? "wipe-restore selftest: FAIL" : "wipe-restore selftest: OK");
    return f.length ? 1 : 0;
  }
  const dropBackup = args.includes("--drop-backup");
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const s = readSession(dir);
  if (!s || s.stoppedAt || !pidAlive(s.serverPid)) {
    console.log("FAIL  no running session: node agent-box/session.mjs up --mission playtest --clients 1 --wait first");
    return 1;
  }
  if (s.mission !== "playtest") {
    console.log("FAIL  the session must be --mission playtest: only it has the host stop file, so only it writes a shutdown save");
    return 1;
  }
  const on = (n, spec, timeout) => {
    h.setBoxes([join(s.clients.find((c) => c.n === n).box)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 200)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);

  // 1. One registered account, then a graceful stop: the engine writes the SHUTDOWN save.
  arrive(on, 1, wait);
  wait(8000);
  const sessionLog = newestLog(join(s.serverProfile, "logs"));
  // The log is read after the stop, so it holds the SHUTDOWN save line.
  writeFileSync(join(s.serverProfile, ...STOP_FILE), new Date().toISOString());
  for (let i = 0; i < 180 && pidAlive(s.serverPid); i++) wait(1000);
  if (pidAlive(s.serverPid)) {
    console.log("FAIL  the session server did not stop within 180 s; close it with 'tools\\cli session down'");
    return 1;
  }
  const sessionText = sessionLog && existsSync(sessionLog) ? readFileSync(sessionLog, "utf8") : "";
  const sessionSaves = saveRoot(s.serverProfile);
  const down = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "down"], { encoding: "utf8", windowsHide: true, env: { ...process.env, LF_SESSION_ROUTED: "1" }, timeout: 180000 });
  process.stdout.write(down.stdout ?? "");
  if (!sessionSaves) {
    console.log(`FAIL  the session server left no save point under ${s.serverProfile}`);
    return 1;
  }

  // 2. A scratch host under tmp/ takes a copy of those saves; every host command below runs on it.
  const stamp = new Date().toISOString().replace(/[-:T]/g, "").slice(0, 14) + "-" + process.pid;
  const runDir = join(ROOT, "tmp", "wipe-restore", `run-${stamp}`);
  const scratch = join(runDir, "host-wr");
  const name = basename(scratch);
  const sessions = join(runDir, "sessions.json");
  const env = { LF_ANNOUNCE_SESSIONS: sessions };
  mkdirSync(runDir, { recursive: true });
  const r = { sessionLog: sessionText, wipe: {}, clean: {}, back: {} };
  try {
    prepareScratch(sessionSaves, scratch);
    // The wipe notice: posted now, then aged 25 h in the scratch file only (b9-wipe.mjs).
    writeFileSync(sessions, JSON.stringify({ next: { at: new Date(Date.now() + 24 * 3600000).toISOString(), build: "0.0.0", wipe: true } }));
    simulatePost(sessions);
    const aged = ageNotice(sessions, 25);
    if (!aged.aged) throw new Error("the scratch wipe notice could not be aged");

    const { backups, profile } = hostPaths(scratch);
    const wipe = hostCli(scratch, ["wipe", "--confirm", name], env, 120000);
    const dirs = existsSync(backups) ? readdirSync(backups, { withFileTypes: true }).filter((e) => e.isDirectory()).map((e) => e.name) : [];
    const backupName = dirs.find((d) => /-wipe(-\d+)?$/.test(d));
    const asideName = dirs.find((d) => /-wiped-saves/.test(d));
    const log = join(backups, "wipes.log");
    const rows = existsSync(log) ? readFileSync(log, "utf8").split("\n").filter(Boolean).length : 0;
    r.wipe = {
      code: wipe.code,
      backup: backupName && findSavePoints(join(backups, backupName)).length ? backupName : null,
      aside: asideName && findSavePoints(join(backups, asideName)).length ? asideName : null,
      left: findSavePoints(profile).length,
      rows,
    };
    console.log(`wipe-restore: host wipe exit ${wipe.code}: ${wipe.out.trim().split(/\r?\n/).slice(-3).join(" | ").slice(0, 300)}`);

    // 3. The boot after the wipe is clean.
    r.clean = bootScratch(scratch, "clean", env, stamp);

    // 4. Restore the wipe's backup (the red run deletes it first), then boot again.
    if (dropBackup && backupName) {
      safeRemoveTree(join(backups, backupName));
      console.log(`wipe-restore: --drop-backup: deleted ${backupName} before the restore`);
    }
    const restore = hostCli(scratch, ["restore", backupName ?? "no-wipe-backup"], env, 120000);
    console.log(`wipe-restore: host restore exit ${restore.code}: ${restore.out.trim().split(/\r?\n/).slice(-2).join(" | ").slice(0, 240)}`);
    r.back = { ...bootScratch(scratch, "restored", env, stamp), restoreCode: restore.code };
  } catch (e) {
    console.log(`FAIL  ${e.message}`);
    return 1;
  } finally {
    if (!args.includes("--keep")) safeRemoveTree(runDir);
    else console.log(`wipe-restore: scratch host kept at ${runDir}`);
  }

  const steps = judgeWipeRestore(r);
  for (const x of steps) console.log(`CHECK ${x.id} ${x.ok ? "PASS" : "FAIL"} ${x.detail}`);
  const first = firstFailure(steps);
  console.log(first ? `wipe-restore proof: FAIL at "${first.id}"` : "wipe-restore proof: PASS (one account wiped away on a scratch host, and back after the restore)");
  return first ? 1 : 0;
}

export default {
  name: "wipe-restore-proof",
  usage: "wipe-restore-proof [--drop-backup] [--keep] [--selftest]   (session up --mission playtest --clients 1 first; ends the session; scratch host under tmp/ only)",
  run: (ctx) => runWipeRestoreProof(ctx),
};

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1] && process.argv.includes("--selftest")) {
  runWipeRestoreProof({ args: ["--selftest"] }).then((c) => process.exit(c));
}
