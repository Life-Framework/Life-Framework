import { spawn } from "node:child_process";
import { cpSync, existsSync, mkdirSync, renameSync } from "node:fs";
import { dirname, join } from "node:path";
import { backupSaveRoot, findSavePoints, freshDir, hostPaths, pickBackup, pidAlive, saveRoot, stamp } from "../../tools/host/host.mjs";
import { ROOT, arrive, clientBox, parseState, readSession, sessionDir } from "../session.mjs";

// #1561, the agent-runnable half of #306's restart acceptance, on a running
// `up --mission playtest` session (one client):
//  1. client 1 registers and reads its bank and cash;
//  2. `session restart` writes the server's SHUTDOWN save (the engine has no earlier one to copy; the
//     autosave runs every 600 s) and the client rejoins: that save is "the backup point";
//  3. the backup: the same copy `host backup` makes (host.backupSaveRoot), into a scratch host folder
//     under tmp/. The live host profile is never touched;
//  4. the client moves money (a 100 withdrawal at the bank teller): bank and cash now differ;
//  5. the server stops (`session restart` again) and the restore runs in the gap between the old
//     server's exit and the new boot: the newest backup goes back as the save folder and the save the
//     stop wrote is kept beside it, the way `host restore` does it (tools/host/host.mjs restoreBackup;
//     the CLI verb only knows a host folder's layout, a session's server profile has another);
//  6. the server boots from the restored save, the client rejoins and must read the OLD bank and cash.
// `--skip-restore` leaves step 5's restore out: the proof must then fail at "old balance back".
// Logs are the proof: the restore line, the server's save-load line and the client's state lines.

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// host.restoreBackup for any profile: the current save folder moves to <backups>/<stamp>-replaced-by-restore
// (never deleted) and the backup is copied in its place.
function restoreInto(profile, backups, backup, now = new Date()) {
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

async function runRestoreRoundtripProof(args) {
  const skipRestore = args.includes("--skip-restore");
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
  const fails = [];

  const s0 = readSession(dir);
  if (!s0 || s0.stoppedAt) {
    console.log("FAIL  no running session: 'tools\\cli session up --mission playtest' first");
    return 1;
  }
  if (s0.mission !== "playtest") {
    console.log("FAIL  the proof needs 'session up --mission playtest' (the bare DebugWorld has no stop file, so no shutdown save)");
    return 1;
  }
  const serverProfile = s0.serverProfile;
  // A scratch host folder under tmp/: only its backups/ is used, never a real host.
  const scratch = join(ROOT, "tmp", `restore-roundtrip-host-${process.pid}`);
  const { backups } = hostPaths(scratch);
  console.log(`scratch host folder: ${scratch}`);

  // `session restart`, the real command; `between` runs once the old server has exited and before the
  // new one can read the save.
  const restart = async (label, between) => {
    const pid = readSession(dir)?.serverPid;
    let out = "";
    const child = spawn(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "restart"], { cwd: ROOT, windowsHide: true, env: { ...process.env, LF_SESSION_ROUTED: "1" } });
    const sink = (d) => {
      out += d;
      process.stdout.write(d);
    };
    child.stdout.on("data", sink);
    child.stderr.on("data", sink);
    let code = null;
    const closed = new Promise((res) => child.on("close", (c) => res((code = c ?? 1))));
    const deadline = Date.now() + 200000;
    let gone = false;
    while (code === null && !gone && Date.now() < deadline) {
      if (pid && !pidAlive(pid)) gone = true;
      else await sleep(50);
    }
    if (gone && between) {
      try {
        between();
      } catch (e) {
        fails.push(`${label}: ${e.message}`);
      }
    }
    await closed;
    if (code !== 0) fails.push(`${label}: session restart exited ${code}`);
    return { code, out };
  };
  // The returning account spawns after the restart; give it a moment, then read.
  const stateAfterRestart = () => {
    wait(8000);
    let s = state();
    for (let i = 0; i < 3 && s.bank === undefined; i++) {
      wait(5000);
      s = state();
    }
    return s;
  };

  // 1. arrive and read the opening balance.
  arrive(on, 1, wait);
  wait(8000);
  const opening = state();
  console.log(`opening: bank=${opening.bank} cash=${opening.cash}`);
  if (opening.bank === undefined || opening.cash === undefined) fails.push("the opening state has no bank or cash");

  // 2. the save the backup copies.
  const first = await restart("first restart", null);
  if (first.code !== 0) return finish(fails, skipRestore, null);
  const backupPoint = stateAfterRestart();
  console.log(`backup point (after the first restart): bank=${backupPoint.bank} cash=${backupPoint.cash}`);
  if (backupPoint.bank !== opening.bank || backupPoint.cash !== opening.cash) fails.push(`the first restart already changed the account: bank ${opening.bank} -> ${backupPoint.bank}, cash ${opening.cash} -> ${backupPoint.cash}`);

  // 3. the backup.
  const backup = backupSaveRoot(serverProfile, backups, "roundtrip");
  if (!backup) {
    fails.push(`no save to back up under ${serverProfile}: the first restart wrote none`);
    return finish(fails, skipRestore, null);
  }
  console.log(`backup: ${backup}`);

  // 4. the balance moves.
  on(1, { verb: "stand", entityName: "Bank_Teller", side: "s" });
  wait(2500);
  on(1, { verb: "look" });
  const use = on(1, { verb: "use", className: "LF_ATMAction", entityName: "Bank_Teller" }, 10000);
  if (use.status !== "ok") fails.push(`the teller returned ${use.status} ${use.detail}`);
  wait(1500);
  for (const key of ["Key1Button", "Key0Button", "Key0Button"]) on(1, { verb: "click", widget: key });
  wait(300);
  on(1, { verb: "click", widget: "WithdrawButton" });
  wait(2000);
  on(1, { verb: "click", widget: "CloseButton" });
  const moved = state();
  console.log(`moved: bank=${moved.bank} cash=${moved.cash}`);
  if (Number(moved.bank) === Number(backupPoint.bank)) fails.push(`the balance did not move (bank ${backupPoint.bank} -> ${moved.bank}): the proof has nothing to restore`);

  // 5. stop, restore (unless skipped), boot.
  const second = await restart("second restart", () => {
    if (skipRestore) {
      console.log("restore: SKIPPED (--skip-restore)");
      return;
    }
    const r = restoreInto(serverProfile, backups, pickBackup(scratch, null));
    console.log(`restore: ${backup} -> ${r.target}`);
    if (r.kept) console.log(`restore: the save the stop wrote is kept in ${r.kept}`);
  });
  if (second.code !== 0) return finish(fails, skipRestore, null);

  // 6. the old balance back.
  const back = stateAfterRestart();
  console.log(`after the restart: bank=${back.bank} cash=${back.cash} (backup point bank=${backupPoint.bank} cash=${backupPoint.cash}; before the restart bank=${moved.bank} cash=${moved.cash})`);
  if (back.bank !== backupPoint.bank || back.cash !== backupPoint.cash) fails.push(`old balance back: the rejoined client reads bank=${back.bank} cash=${back.cash}, the backup held bank=${backupPoint.bank} cash=${backupPoint.cash} (before the restart: bank=${moved.bank} cash=${moved.cash})`);
  return finish(fails, skipRestore, back);
}

function finish(fails, skipRestore, back) {
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "restore roundtrip proof: FAIL" : `restore roundtrip proof: PASS (the restored backup brought the old balance back: bank=${back?.bank} cash=${back?.cash})`);
  if (skipRestore && !fails.length) console.log("FAIL  --skip-restore passed: the proof does not check the restore");
  return fails.length || skipRestore ? 1 : 0;
}

export default {
  name: "restore-roundtrip-proof",
  usage: "restore-roundtrip-proof [--skip-restore]   (session up --mission playtest)",
  run: ({ args = [] } = {}) => runRestoreRoundtripProof(args),
};
