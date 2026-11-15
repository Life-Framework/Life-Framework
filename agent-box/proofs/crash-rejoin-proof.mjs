import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { join } from "node:path";
import { spawnSync } from "node:child_process";
import { hostPaths, newestConsoleLog, pidAlive, readHostState, READY_LINE } from "../../tools/host/host.mjs";
import { hostLogState, hostPid, joinPlainClient, registerClient, rejoinPlainClient, selftestScratchHost, startScratchHost } from "./_scratch-host.mjs";

// #1563: crash, then rejoin. A scratch host under `host watch` on a pool session lane (#1588), a plain
// client that registers, a kill of the host process by the pid in host.json, then the watch's restart and
// the same client rejoining. What survived is judged from the host's own console.log: the harness `state`
// verb stays refused (the dev gate is closed on a hosted header) and the log does not record cash, so the
// proof checks the bank, the save-load lines and the crash backup the watch writes.
//   crash-rejoin-proof [--no-restart]   --no-restart is the red run: the watch has --max-restarts 0, does
//   not start the host again, and the proof fails at "host back".
// LF_SCRATCH_KEEP=1 keeps the scratch dir for reading.

const CONFIG = { schemaVersion: 1, persistence: { enableAutosave: true, autosaveInterval: 30 } };
const sleep = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);

// ---------------------------------------------------------------- pure judges (selftested)

// True when the text from `offset` on holds a save that began and finished: "preparing to save" then
// "save completed". An autosave that began before the offset does not count: it may predate the account.
export function saveAfter(text, offset = 0) {
  return /preparing to save[\s\S]*save completed \(/.test(String(text ?? "").slice(offset));
}

// The last ledger balance on the bank account of `uid`, or "".
export function lastBankBalance(text, uid) {
  let bank = "";
  for (const l of String(text ?? "").split(/\r?\n/)) {
    const b = /\[LFDebug:Ledger\] #\d+ (\S+) bank -?\d+ \(([^)]*)\) balance (-?\d+)/.exec(l);
    if (b && b[1] === uid) bank = b[3];
  }
  return bank;
}

// Ledger lines that open or top up a bank account of `uid`: after a restart these mean the account was
// not there and the game paid it again.
export function reopenedBank(text, uid) {
  const out = [];
  for (const l of String(text ?? "").split(/\r?\n/)) {
    const b = /\[LFDebug:Ledger\] #\d+ (\S+) bank (-?\d+) \((starting-bank|debug-topup)\) balance (-?\d+)/.exec(l);
    if (b && b[1] === uid) out.push(`${b[3]} ${b[2]} -> ${b[4]}`);
  }
  return out;
}

// What a restarted host says about the load: the save-loaded line, the continue line and the record counts.
export function loadEvidence(text) {
  const t = String(text ?? "");
  const n = (re) => {
    const m = re.exec(t);
    return m ? Number(m[1]) : -1;
  };
  return {
    loaded: /\[LFDebug:Persistence\] save loaded: version/.test(t),
    continued: /session continued from a save point \(-loadSessionSave\)/.test(t),
    accounts: n(/applied (\d+) account record\(s\)/),
    banks: n(/applied (\d+) bank account record\(s\)/),
    damaged: /save load damaged|load skipped/.test(t),
  };
}

// What the log says about a player coming back: a returning account is `spawning player`, a new one is
// `registration form sent` (agent-box/README "Multi-client session").
export function returnEvidence(text) {
  const t = String(text ?? "");
  return { spawned: /spawning player \d+/.test(t), form: /registration form sent to player \d+/.test(t), quiz: /quiz sent to player \d+/.test(t) };
}

// The crash backups under <host dir>/backups: folders named *-crash, with whether each holds the crash-log folder.
export function crashBackups(backupsDir) {
  if (!existsSync(backupsDir)) return [];
  return readdirSync(backupsDir)
    .filter((d) => /-crash(-\d+)?$/.test(d) && statSync(join(backupsDir, d)).isDirectory())
    .map((d) => ({ name: d, dir: join(backupsDir, d), hasLog: existsSync(join(backupsDir, d, "crash-log")), hasSave: readdirSync(join(backupsDir, d)).some((x) => x !== "crash-log") }));
}

export function selftestCrashRejoin() {
  const f = [];
  const log = [
    "[LFDebug:Ledger] #1 U-1 bank 1000000 (starting-bank) balance 1000000 counterparty ''",
    "[LFDebug:Persistence] preparing to save (AUTO)",
    "[LFDebug:Persistence] save completed (AUTO)",
  ].join("\n");
  if (!saveAfter(log, 0)) f.push("saveAfter missed a save");
  if (saveAfter(log, log.length)) f.push("saveAfter counted a save before the offset");
  if (saveAfter("preparing to save (AUTO)\n", 0)) f.push("saveAfter counted a save that never completed");
  if (lastBankBalance(log, "U-1") !== "1000000" || lastBankBalance(log, "U-2") !== "") f.push("lastBankBalance wrong uid handling");
  if (reopenedBank(log, "U-1").length !== 1 || reopenedBank("", "U-1").length) f.push("reopenedBank");
  const e = loadEvidence("[LFDebug:Persistence] save loaded: version 40, game build x, running y\n[LFDebug:Persistence] applied 2 account record(s)\n[LFDebug:Persistence] applied 1 bank account record(s)\n[LFDebug:Persistence] session continued from a save point (-loadSessionSave)");
  if (!e.loaded || !e.continued || e.accounts !== 2 || e.banks !== 1 || e.damaged) f.push(`loadEvidence read ${JSON.stringify(e)}`);
  if (loadEvidence("").loaded || loadEvidence("").accounts !== -1) f.push("loadEvidence invents a load");
  const r = returnEvidence("[LFDebug:Spawn] spawning player 1\n");
  if (!r.spawned || r.form) f.push(`returnEvidence read ${JSON.stringify(r)}`);
  if (!returnEvidence("registration form sent to player 1").form) f.push("returnEvidence missed a registration form");
  return f;
}

// ---------------------------------------------------------------- the run

function hostLog(h) {
  const f = newestConsoleLog(hostPaths(h.hostDir).profile);
  return f ? { file: f, text: readFileSync(f, "utf8") } : { file: null, text: "" };
}

function waitFor(label, budgetS, fn, stepS = 2) {
  const end = Date.now() + budgetS * 1000;
  for (;;) {
    const v = fn();
    if (v) return v;
    if (Date.now() > end) return null;
    sleep(stepS * 1000);
  }
}

async function runCrashRejoin(args) {
  const noRestart = args.includes("--no-restart");
  const fails = [...selftestScratchHost(), ...selftestCrashRejoin()];
  if (fails.length) {
    for (const x of fails) console.log(`FAIL  selftest: ${x}`);
    return 1;
  }
  let failedAt = "";
  const step = (name, ok, detail = "") => {
    console.log(`${ok ? "PASS" : "FAIL"}  ${name}${detail ? `: ${detail}` : ""}`);
    if (!ok && !failedAt) failedAt = name;
    return ok;
  };
  let h = null;
  try {
    h = startScratchHost({ maxRestarts: noRestart ? 0 : 3, config: CONFIG });
    const oldPid = hostPid(h);
    console.log(`host: pid ${oldPid} under watch pid ${h.watchPid}, port ${h.port}, lane ${h.lock}`);
    joinPlainClient(h);
    const ui = registerClient(h);
    console.log(`registered: last ui = ${ui || "(none)"}`);

    // 1. who the player is and the bank, from the host log.
    const before = waitFor("bank", 90, () => {
      const t = hostLog(h).text;
      const s = hostLogState(t);
      const b = s.uid ? lastBankBalance(t, s.uid) : "";
      return s.uid && s.faction && b ? { ...s, bank: b, offset: t.length } : null;
    });
    if (!step("player registered, bank noted", !!before, before ? `uid ${before.uid} faction ${before.faction} bank ${before.bank}` : "the host log names no uid, faction and bank balance for player 1")) return 1;

    // 2. a save that began after the bank line: a killed process writes no shutdown save.
    const saved = waitFor("save", 150, () => saveAfter(hostLog(h).text, before.offset));
    if (!step("save written after the registration", !!saved, `autosave every ${CONFIG.persistence.autosaveInterval} s`)) return 1;

    // 3. kill the host by the pid in host.json, never by image name.
    const killPid = readHostState(h.hostDir)?.pid;
    if (!step("host pid read from host.json", !!killPid && pidAlive(killPid), `pid ${killPid}`)) return 1;
    const oldLog = hostLog(h).file;
    spawnSync("taskkill", ["/PID", String(killPid), "/T", "/F"], { windowsHide: true });
    const gone = waitFor("exit", 30, () => !pidAlive(killPid), 0.5);
    if (!step("host killed", !!gone, `pid ${killPid}`)) return 1;

    // 4. the watch records the crash.
    const { backups } = hostPaths(h.hostDir);
    const cb = waitFor("crash backup", 60, () => crashBackups(backups).find((c) => c.hasLog) ?? null);
    step("crash backup with the crash-log folder", !!cb, cb ? `${cb.name}${cb.hasSave ? " (saves copied)" : " (no saves in it)"}` : "no backups/*-crash with a crash-log folder");
    if (cb && !cb.hasSave) step("crash backup holds the saves", false, cb.dir);

    // 5. the watch starts the host again (the red run: --max-restarts 0, so it must not).
    const back = waitFor("host back", noRestart ? 45 : 420, () => {
      const hs = readHostState(h.hostDir);
      const log = newestConsoleLog(hostPaths(h.hostDir).profile);
      if (!hs?.pid || hs.pid === killPid || !pidAlive(hs.pid) || !log || log === oldLog) return null;
      return readFileSync(log, "utf8").includes(READY_LINE) ? { pid: hs.pid, log } : null;
    }, 3);
    if (!step("host back", !!back, back ? `new pid ${back.pid} (was ${killPid}), ready line in the new log` : `no new host process within ${noRestart ? 45 : 420} s`)) return 1;
    h.serverLog = back.log;

    // 6. the load, from the restarted host's log.
    const loadText = waitFor("save loaded", 60, () => (loadEvidence(readFileSync(back.log, "utf8")).loaded ? readFileSync(back.log, "utf8") : null));
    const ev = loadEvidence(loadText ?? readFileSync(back.log, "utf8"));
    step("save loaded", ev.loaded && !ev.damaged, `continued=${ev.continued} accounts=${ev.accounts} bank accounts=${ev.banks}`);
    step("accounts applied from the save", ev.accounts >= 1 && ev.banks >= 1, `applied ${ev.accounts} account record(s), ${ev.banks} bank account record(s)`);

    // 7. the same client comes back.
    rejoinPlainClient(h);
    const after = waitFor("returning player", 90, () => {
      const t = readFileSync(back.log, "utf8");
      const r = returnEvidence(t);
      return r.spawned || r.form || r.quiz ? { t, r } : null;
    });
    step("client rejoined as a returning player", !!after && after.r.spawned && !after.r.form, after ? `spawning player ${after.r.spawned}, registration form ${after.r.form}, quiz ${after.r.quiz}` : "the restarted host logged no spawn for the client");

    // 8. the bank, from the log. The log writes a ledger line only on a movement, so a restored account
    //    prints none: the proof then holds the balance when nothing re-opened or topped it up.
    const t2 = readFileSync(back.log, "utf8");
    const reopened = reopenedBank(t2, before.uid);
    const lastLedger = lastBankBalance(t2, before.uid);
    const same = reopened.length === 0 && (lastLedger === "" || lastLedger === before.bank);
    step("same bank after the rejoin", same, reopened.length ? `the restarted host paid the account again: ${reopened.join("; ")}` : `bank before ${before.bank}, after ${lastLedger || `${before.bank} (no ledger line: restored, not re-opened)`}`);
    return failedAt ? 1 : 0;
  } catch (e) {
    step("crash-rejoin-proof ran", false, e.message);
    return 1;
  } finally {
    if (h ?? null) h.teardown();
    console.log(`crash-rejoin-proof: ${failedAt ? `FAIL at "${failedAt}"` : "PASS"}${noRestart ? " (red run, --no-restart)" : ""}`);
  }
}

export default {
  name: "crash-rejoin-proof",
  noReset: true,
  usage: "crash-rejoin-proof [--no-restart]   (no session needed: boots its own scratch host under host watch on a pool session lane, kills it, judges the restart and the rejoin from the host log; --no-restart is the red run)",
  run: (ctx) => runCrashRejoin(ctx.args ?? []),
};
