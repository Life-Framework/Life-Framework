// tools/warm-spike/driver.mjs - warm-runner spike driver (2026-11-14). Not shipped.
// Runs on a pool machine in its own run folder (node tools/warm-spike/driver.mjs <scenario>):
// takes a Workbench lock, starts one Workbench with LF_WarmProbePlugin, edits a probe game script
// between rounds, asks the plugin to reload the game scripts, and checks which code runs.
// Every wait is bounded. Results: tmp/warm-spike/results.jsonl.

import { appendFileSync, copyFileSync, existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const HERE = join(ROOT, "tools", "warm-spike");
const OUT = join(ROOT, "tmp", "warm-spike");
const PROFILE = join(OUT, "wbprofile");
const RAW = join(PROFILE, "profile", "lf-warm");
const PLUGIN_DIR = join(ROOT, "addons", "LifeFramework", "Scripts", "WorkbenchGame", "LFWarm");
const PROBE_DIR = join(ROOT, "addons", "LifeFramework", "Scripts", "Game", "LFWarmProbe");
const PROBE = join(PROBE_DIR, "LF_WarmProbe.c");
const TOOLS = process.env.LF_WB_TOOLS || "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger Tools/Workbench";
const EXE = join(TOOLS, "ArmaReforgerWorkbenchSteamDiag.exe");
const GAME_ADDONS = process.env.LF_WB_GAME_ADDONS || "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger/addons";
const WORLD = "{F3AA388E8466788C}Worlds/DebugWorld/DebugWorld.ent";
const scenario = process.argv[2] || "reload";
const t00 = Date.now();

mkdirSync(RAW, { recursive: true });
mkdirSync(PLUGIN_DIR, { recursive: true });
mkdirSync(PROBE_DIR, { recursive: true });

function log(rec) {
  const line = JSON.stringify({ t: ((Date.now() - t00) / 1000).toFixed(1), ...rec });
  console.log(line);
  appendFileSync(join(OUT, "results.jsonl"), line + "\n");
}

const sleep = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);

function writeProbe(token, broken = false) {
  let src = readFileSync(join(HERE, "LF_WarmProbe.c.tmpl"), "utf8").replaceAll("__TOKEN__", token);
  if (broken) src += "\nvoid LF_WarmProbeBroken() { int x = ; }\n";
  writeFileSync(PROBE, src);
}

function wbAlive(pid) {
  const r = spawnSync("tasklist", ["/FI", `PID eq ${pid}`, "/FO", "CSV", "/NH"], { encoding: "utf8", timeout: 10000 });
  return /ArmaReforgerWorkbench/i.test(r.stdout || "");
}

let seq = 0;
let wbPid = 0;
function send(verb, arg = "", timeoutS = 180) {
  const id = `${Date.now()}-${++seq}`;
  rmSync(join(RAW, "done.txt"), { force: true });
  writeFileSync(join(RAW, "go.txt"), `${id}\n${verb}\n${arg}\n`);
  const t0 = Date.now();
  while (Date.now() - t0 < timeoutS * 1000) {
    sleep(200);
    const f = join(RAW, "done.txt");
    if (existsSync(f)) {
      const lines = readFileSync(f, "utf8").split(/\r?\n/);
      if (lines[0] === id) return { ok: true, result: lines.slice(1).join(" "), wallMs: Date.now() - t0 };
    }
    if (wbPid && !wbAlive(wbPid)) return { ok: false, result: "WORKBENCH GONE", wallMs: Date.now() - t0 };
  }
  return { ok: false, result: "TIMEOUT", wallMs: Date.now() - t0 };
}

function readToken() {
  const f = join(RAW, "play-token.txt");
  return existsSync(f) ? readFileSync(f, "utf8").trim() : "<none>";
}

function play(ms) {
  rmSync(join(RAW, "play-token.txt"), { force: true });
  const r = send("play", String(ms), 240);
  return { ...r, playToken: readToken() };
}

async function main() {
  const { acquireWorkbench } = await import("../lib/wb-lock.mjs");
  const lock = await acquireWorkbench(ROOT, { purpose: "warm-runner-spike", waitMs: 900000 });
  try {
    copyFileSync(join(HERE, "LF_WarmProbePlugin.c"), join(PLUGIN_DIR, "LF_WarmProbePlugin.c"));
    for (const f of ["go.txt", "done.txt", "alive.txt", "play-token.txt"]) rmSync(join(RAW, f), { force: true });
    writeProbe("T0");
    const gproj = join(ROOT, "addons", "LF_Everon1989", "LF_Everon1989.gproj");
    const args = ["-gproj", gproj, "-wbModule=WorldEditor", "-plugin=LF_WarmProbePlugin", "-noSplash", "-noSound", "-profile", PROFILE, "-addonsDir", [GAME_ADDONS, join(ROOT, "addons")].join(",")];
    const il = await import("../lib/interactive-launch.mjs");
    const tLaunch = Date.now();
    wbPid = il.interactiveNeeded() ? il.interactiveSpawnDetached(EXE, args, { cwd: TOOLS, marker: PROFILE }) : null;
    log({ step: "launch", pid: wbPid });
    while (!existsSync(join(RAW, "alive.txt")) && Date.now() - tLaunch < 300000) sleep(500);
    log({ step: "plugin-alive", sinceLaunchS: ((Date.now() - tLaunch) / 1000).toFixed(1) });
    let r = send("open", WORLD, 300);
    log({ step: "open", ...r, sinceLaunchS: ((Date.now() - tLaunch) / 1000).toFixed(1) });
    r = send("call", "LF_WarmProbeToken");
    log({ step: "call", expect: "T0", ...r });
    r = play(8000);
    log({ step: "play", expect: "T0", ...r, sinceLaunchS: ((Date.now() - tLaunch) / 1000).toFixed(1) });

    const menus = (process.env.LF_WARM_MENU || "Build|Reload Scripts;&Build|&Reload Scripts;Build|Validate and Reload Scripts;Script|Reload Scripts").split(";");
    let menu = null;
    const rounds = [["T1"], ["T2"], ["T3", true], ["T4"], ["T5"]];
    for (const [tok, broken] of rounds) {
      writeProbe(tok, !!broken);
      sleep(1000);
      let rr;
      for (const m of menu ? [menu] : menus) {
        rr = send("reload", m, 300);
        log({ step: "reload", menu: m, token: tok, broken: !!broken, ...rr });
        if (!rr.ok) break;
        if (/execute=1|execute=true/.test(rr.result)) { menu = m; break; }
      }
      if (!rr.ok) break;
      if (!menu) { log({ step: "no-menu-path-worked" }); break; }
      r = send("call", "LF_WarmProbeToken");
      log({ step: "call", expect: tok, ...r });
      if (!r.ok) break;
      r = play(8000);
      log({ step: "play", expect: tok, ...r });
      if (!r.ok) break;
    }
    if (wbPid && wbAlive(wbPid)) {
      r = send("quit", "", 60);
      log({ step: "quit", ...r });
    }
  } finally {
    if (wbPid && wbAlive(wbPid)) spawnSync("taskkill", ["/PID", String(wbPid), "/T", "/F"], { timeout: 15000 });
    rmSync(PROBE_DIR, { recursive: true, force: true });
    lock.release();
    log({ step: "end", scenario });
  }
}

main().catch((e) => {
  log({ step: "crash", error: String(e && e.stack || e) });
  process.exit(1);
});
