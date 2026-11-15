// opening-run: host=tmp timeout=2700
// Opening-day beat B7, the warnings and the close (#1603; docs/playtest/m4-opening-day.md "B7").
//
//   node tools/playtest/opening/b7-restart.mjs --host <dir under tmp/> [--in 6] [--warnings 3,2,1] [--timeout 900] [--queue 600]
//   node tools/playtest/opening/b7-restart.mjs --judge <dir with run1.log, watch.log, run2.log>
//
// Live: on a SCRATCH host folder (`--host` must resolve inside <repo>/tmp/, as for B9) it sets a test restart
// `--in` minutes ahead with b7-restart-setup.mjs in the scratch owner config (the committed server/flagship/config.json
// is never touched), starts `tools\cli host watch`, waits for the watchdog to bring the server back, stops the host
// and judges the logs. `--warnings` narrows the warning steps so a short run still sees each of them (the shipped
// 15, 5 and 1 need a 16 minute wait); leave it off to keep what the setup writes. Port 2001 is one lock shared with every
// other session here: while it is held the beat waits up to --queue seconds, setting the time again each try. The setup is
// undone at the end.
//
// The beat stops what it started on every path (#1692): the watcher it records and the host, by pid, in a finally
// (tools/playtest/opening/_scratch.mjs reapHost), so a failed boot leaves no server for the next beat to trip over.
//
// Judge: the console log of the boot that closes (run1), what `host watch` printed (watch) and the console log of the
// boot the watchdog starts (run2). One `BEAT B7 <id> PASS|FAIL <detail>` line per check, then the SUMMARY line.
// A warning step is expected only when it fits the time left when the ticker started (the ticker prints it).
// Exit 1 on a FAIL, 2 on a bad call (a refused --host, no logs).
import { copyFileSync, existsSync, mkdirSync, readFileSync, readdirSync, realpathSync, writeFileSync } from "node:fs";
import { dirname, isAbsolute, join, relative, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { spawn, spawnSync } from "node:child_process";
import { reapHost, recordPid } from "./_scratch.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const SETUP = join(ROOT, "tools", "playtest", "opening", "b7-restart-setup.mjs");

const row = (id, ok, detail) => ({ id, status: ok ? "PASS" : "FAIL", detail });
const has = (text, re) => re.test(text);
const lineIndex = (text, re) => {
  const m = re.exec(text);
  return m ? m.index : -1;
};

/** Judge one restart cycle from its logs. Pure: { run1, watch, run2 } texts in, rows { id, status, detail } out. */
export function judgeRestart({ run1 = "", watch = "", run2 = "" }) {
  const rows = [];
  const tick = /\[LFDebug:Ops\] restart warner: ticking once a second, restarts at (\S+), warnings ([\d, ]*) min before, next in (\d+) s/.exec(run1);
  if (!tick) {
    rows.push(row("ticker", false, "no 'restart warner: ticking' line: the schedule was not read (the setup wrote the wrong config, or the server never started the ticker)"));
    return rows;
  }
  const leads = tick[2].split(",").map((s) => Number(s.trim())).filter((n) => n >= 1);
  const window = Number(tick[3]);
  const fit = leads.filter((l) => l * 60 <= window);
  const skipped = leads.filter((l) => !fit.includes(l));
  rows.push(row("ticker", fit.length > 0, `restart at ${tick[1]}, warnings ${leads.join(", ")}, ${window} s left at the start${skipped.length ? `; ${skipped.join(", ")} do not fit and are not expected` : ""}${fit.length ? "" : "; no warning fits: raise --in"}`));

  let last = -1;
  for (const lead of fit) {
    const warn = new RegExp(`\\[LFDebug:Ops\\] restart warning: ${lead} minute\\(s\\) to the restart`);
    const toast = new RegExp(`\\[LFDebug:Ops\\] restart toast: told every player the server restarts in ${lead} minute\\(s\\)`);
    const wi = lineIndex(run1, warn);
    if (wi < 0) rows.push(row(`warning-${lead}`, false, `no 'restart warning: ${lead} minute(s)' line: the ${lead} minute warning was never published`));
    else if (!has(run1, toast)) rows.push(row(`warning-${lead}`, false, `published, but no 'restart toast: told every player ... ${lead} minute(s)' line: no toast reached the players`));
    else {
      rows.push(row(`warning-${lead}`, true, "published and toasted"));
      last = Math.max(last, wi);
    }
  }

  // Toast only: no siren is sounded and no error names the siren prop (the town has none placed).
  const siren = run1.split(/\r?\n/).find((l) => /\[LFDebug:Ops\].*siren/i.test(l) || (/SCRIPT\s+\(E\)|\bERROR\b/.test(l) && /restart siren/i.test(l)));
  rows.push(row("toast-only", !siren, siren ? `siren line in the log: ${siren.trim().slice(0, 160)}` : "the warnings are toasts, no siren line, no missing-prop error"));

  const close = lineIndex(run1, /\[LFDebug:Ops\] restart due: wrote \S+, closing the game/);
  const closeOk = close >= 0 && close >= last;
  rows.push(row("close", closeOk, close < 0 ? "no 'restart due: wrote <marker>, closing the game' line: the server never closed itself" : close < last ? "the close came before the last warning" : "restart marker written, normal close requested"));

  const save = lineIndex(run1.slice(Math.max(close, 0)), /\[LFDebug:Persistence\] save completed \(SHUTDOWN\)/);
  const noSave = has(run1, /session ended without a shutdown save/);
  rows.push(row("save", save >= 0 && !noSave, noSave ? "'session ended without a shutdown save' in the log" : save < 0 ? "no 'save completed (SHUTDOWN)' after the close: the shutdown save never ran" : "shutdown save completed after the close request"));

  const exited = has(watch, /host exited: scheduled restart/);
  const restarting = has(watch, /restarting the host \(scheduled restart\)/);
  const crash = /crash \d+:|host exited: exit without a stop/.exec(watch);
  const up = /host up \(pid \d+\)/g;
  const ups = (watch.match(up) || []).length;
  const watchdogOk = exited && restarting && !crash;
  rows.push(row("watchdog", watchdogOk, crash ? `the watchdog took the close for a crash: ${crash[0]}` : !exited ? "watch never printed 'host exited: scheduled restart'" : !restarting ? "watch never printed 'restarting the host (scheduled restart)'" : `scheduled restart seen, not a crash${ups > 1 ? `, host up ${ups} times` : ""}`));

  const online = has(run2, /Entered online game state\./);
  const loaded = has(run2, /\[LFDebug:Persistence\] save loaded:/);
  rows.push(row("back-up", online && loaded, !online ? "the second boot never reached 'Entered online game state.'" : !loaded ? "the second boot has no 'save loaded:' line: it came back without the save" : "server back, world online, save loaded"));
  return rows;
}

function printRows(rows) {
  for (const r of rows) console.log(`BEAT B7 ${r.id} ${r.status} ${r.detail}`);
  const fail = rows.filter((r) => r.status === "FAIL").length;
  console.log(`SUMMARY b7-restart ${fail ? "fail" : "ok"} pass=${rows.length - fail} fail=${fail}`);
  return fail ? 1 : 0;
}

// A --host must resolve (links followed) to a folder strictly inside <repo>/tmp/. Same rule as B9.
export function checkScratchHost(host, root = ROOT) {
  const tmp = join(root, "tmp");
  let real;
  try {
    real = realpathSync(resolve(host));
  } catch {
    return `--host ${host} does not exist`;
  }
  const realTmp = existsSync(tmp) ? realpathSync(tmp) : tmp;
  const rel = relative(realTmp, real);
  if (!rel || rel.startsWith("..") || isAbsolute(rel)) return `--host ${host} is outside tmp/ (${realTmp}): B7 only runs on a scratch host, never the real one`;
  return null;
}

const val = (args, name) => {
  const i = args.indexOf(name);
  return i === -1 || i + 1 >= args.length ? null : args[i + 1];
};

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const logDirs = (profile) => {
  const d = join(profile, "logs");
  return existsSync(d) ? readdirSync(d).filter((n) => n.startsWith("logs_")).sort() : [];
};
const readLog = (profile, name) => {
  const f = join(profile, "logs", name, "console.log");
  return existsSync(f) ? readFileSync(f, "utf8") : "";
};

function cli(host, args, timeout = 240000) {
  const r = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), ...args], { cwd: ROOT, encoding: "utf8", timeout, windowsHide: true, env: { ...process.env, LF_HOST_DIR: host } });
  return { status: r.status, out: `${r.stdout ?? ""}${r.stderr ?? ""}` };
}

async function live(host, args) {
  const profile = join(host, "profile");
  const owner = join(profile, "profile", "LifeFramework");
  const minutes = val(args, "--in") ?? "6";
  const warnings = val(args, "--warnings") ?? "3,2,1";
  const timeoutMs = Number(val(args, "--timeout") ?? 900) * 1000;
  const fail = (id, detail) => printRows([row(id, false, detail)]);

  if (!existsSync(join(owner, "config.json"))) {
    // `host config apply` refuses a host folder inside the repo (it adds secrets), and a scratch host must be inside
    // tmp/. The committed config holds no secret, so the scratch copy is a plain read of it; the original stays as it is.
    const source = join(ROOT, "server", "flagship", "config.json");
    if (!existsSync(source)) return fail("setup", `no ${source} to seed the scratch owner config from`);
    mkdirSync(owner, { recursive: true });
    copyFileSync(source, join(owner, "config.json"));
  }
  let undone = false;
  const undo = () => {
    if (!undone) spawnSync(process.execPath, [SETUP, "--profile", owner, "--undo"], { encoding: "utf8", windowsHide: true });
    undone = true;
  };
  // The restart time and the warning steps in the scratch owner config; run again after a wait so the time stays ahead.
  const applySetup = () => {
    const r = spawnSync(process.execPath, [SETUP, "--profile", owner, "--in", minutes], { encoding: "utf8", windowsHide: true });
    if (r.status !== 0) return { ok: false, out: r.stdout.trim().slice(0, 240) };
    if (warnings !== "none" && /^\d+(,\d+)*$/.test(warnings)) {
      const cfgFile = join(owner, "config.json");
      const cfg = JSON.parse(readFileSync(cfgFile, "utf8"));
      cfg.restarts.warningMinutes = warnings.split(",").map(Number);
      writeFileSync(cfgFile, JSON.stringify(cfg, null, 2) + "\n");
    }
    return { ok: true, out: r.stdout.trim() };
  };
  try {
    const before = new Set(logDirs(profile));
    // Port 2001 is one lock shared with every other session on this machine: when it is held, wait for it and set the
    // restart time again, so the warnings still have their lead. `host start` returns once the world is up.
    const queueUntil = Date.now() + Number(val(args, "--queue") ?? 600) * 1000;
    for (;;) {
      const s0 = applySetup();
      if (!s0.ok) return fail("setup", s0.out);
      console.log(`[b7] ${s0.out}; warnings ${warnings}`);
      const start = cli(host, ["host", "start"], 480000);
      if (start.status === 0) break;
      if (/lock 'server-2001' is held/.test(start.out) && Date.now() < queueUntil) {
        console.log("[b7] port 2001 is held by another session; waiting 30 s");
        await sleep(30000);
        continue;
      }
      return fail("boot", `host start exited ${start.status}: ${start.out.trim().slice(-300)}`);
    }
    // `host watch` adopts the host that `host start` left running.
    const child = spawn(process.execPath, [join(ROOT, "tools", "cli.mjs"), "host", "watch"], { cwd: ROOT, env: { ...process.env, LF_HOST_DIR: host }, windowsHide: true, stdio: ["ignore", "pipe", "pipe"] });
    recordPid(host, "host watch", child.pid);
    let watch = "";
    child.stdout.on("data", (d) => (watch += d));
    child.stderr.on("data", (d) => (watch += d));
    const started = Date.now();
    let fresh = [];
    let run2Loaded = false;
    while (Date.now() - started < timeoutMs) {
      await sleep(5000);
      fresh = logDirs(profile).filter((n) => !before.has(n));
      run2Loaded = fresh.length >= 2 && /\[LFDebug:Persistence\] save loaded:/.test(readLog(profile, fresh[1]));
      if (run2Loaded || child.exitCode !== null) break;
    }
    cli(host, ["host", "stop"], 240000);
    await sleep(2000);
    child.kill();
    fresh = logDirs(profile).filter((n) => !before.has(n));
    if (fresh.length < 1) return fail("boot", `the host never wrote a console log; watch said: ${watch.trim().slice(-300)}`);
    writeFileSync(join(host, "b7-watch.log"), watch);
    return printRows(judgeRestart({ run1: readLog(profile, fresh[0]), watch, run2: fresh[1] ? readLog(profile, fresh[1]) : "" }));
  } finally {
    undo();
    reapHost({ host, say: (m) => console.error(`[b7] ${m}`) });
  }
}

async function main(args) {
  const dir = val(args, "--judge");
  if (dir) {
    const f = (n) => (existsSync(join(dir, n)) ? readFileSync(join(dir, n), "utf8") : "");
    if (!f("run1.log")) {
      console.error(`[b7] no run1.log in ${dir}`);
      return 2;
    }
    return printRows(judgeRestart({ run1: f("run1.log"), watch: f("watch.log"), run2: f("run2.log") }));
  }
  const host = val(args, "--host");
  const err = host ? checkScratchHost(host) : "--host <dir under tmp/> is required (or --judge <dir>)";
  if (err) {
    console.error(`[b7] refused: ${err}`);
    return 2;
  }
  return live(resolve(host), args);
}

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) process.exit(await main(process.argv.slice(2)));
