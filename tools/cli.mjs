#!/usr/bin/env node
// tools/cli.mjs — unified Life Framework development CLI.
//
// Zero-dependency Node.js. Manages the MCP servers (clone / build / update /
// verify / enable / disable) and dispatches validation, lint and test scripts
// that live under tools/{validation,lint,test}/.
//
// Usage:
//   node tools/cli.mjs <command> [args]
//   tools\cli status                  (Windows: .cmd shim)
//
// Commands:
//   status                            Show toolchain + MCP server state
//   mcp install [name]                Clone (if missing), npm ci, build
//   mcp update [name]                 git pull --ff-only, npm ci, build
//   mcp verify [name]                 Boot the server briefly to prove it starts
//   mcp enable <name>                 Set enabled=true in opencode.json
//   mcp disable <name>                Set enabled=false in opencode.json
//   validate | lint | test            Run every script in tools/{dir}/
//
//   [name] is one of: enfusion-mcp, enfusion-workbench. Omitted = all.

import { spawn, spawnSync } from "node:child_process";
import { closeSync, createWriteStream, existsSync, fstatSync, mkdirSync, openSync, readdirSync, readFileSync, readSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const MCP_DIR = join(ROOT, "tools", "mcp");
const CONFIG_PATH = join(ROOT, "opencode.json");
const TOOL_DIRS = { validate: "validation", lint: "lint", test: "test" };

const SERVERS = {
  "enfusion-mcp": {
    repo: "steffenbk/enfusion-mcp-BK",
    dir: "enfusion-mcp-bk",
    label: "Enfusion MCP (primary, api/script/prefab + Workbench control)",
  },
  "enfusion-workbench": {
    repo: "Goldwep/enfusion-workbench-mcp",
    dir: "enfusion-workbench-mcp",
    label: "Enfusion Workbench MCP (112 tools, GUID/resource index + refactor)",
  },
};

const IS_WIN = process.platform === "win32";
const NPM = IS_WIN ? "npm.cmd" : "npm";
const PWSH = IS_WIN ? "powershell.exe" : "powershell";

// ---------------------------------------------------------------- helpers

function readConfig() {
  try {
    return JSON.parse(readFileSync(CONFIG_PATH, "utf8"));
  } catch {
    return { mcp: {} };
  }
}

function writeConfig(cfg) {
  writeFileSync(CONFIG_PATH, JSON.stringify(cfg, null, 2) + "\n", "utf8");
}

function serverEnv(name) {
  return readConfig().mcp?.[name]?.environment ?? {};
}

function sh(cmd, args, opts = {}) {
  const winCmd = IS_WIN && /\.(cmd|bat)$/i.test(cmd);
  const res = spawnSync(cmd, args, {
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
    ...(winCmd ? { shell: true } : {}),
    ...opts,
  });
  if (res.error) {
    res.status = res.status ?? 1;
    res.stdout = res.stdout ?? "";
    res.stderr = res.stderr ?? `failed to spawn ${cmd}: ${res.error.message}`;
  }
  res.stdout = res.stdout ?? "";
  res.stderr = res.stderr ?? "";
  return res;
}

function gitIn(dir, args) {
  return sh("git", ["-C", dir, ...args]);
}

function entryOf(server) {
  return join(MCP_DIR, server.dir, "dist", "index.js");
}

function targets(name) {
  if (name) {
    if (!SERVERS[name]) {
      console.log(`unknown MCP server: ${name} (expected one of: ${Object.keys(SERVERS).join(", ")})`);
      return null;
    }
    return [name];
  }
  return Object.keys(SERVERS);
}

// ---------------------------------------------------------------- commands

function cmdStatus() {
  const cfg = readConfig();
  console.log("toolchain:");
  console.log(`  node ${process.version}`);
  const npmv = sh(NPM, ["-v"]).stdout.trim();
  console.log(`  npm  ${npmv}`);
  console.log("");
  console.log("mcp servers (tools/mcp/):");
  for (const name of Object.keys(SERVERS)) {
    const s = SERVERS[name];
    const installed = existsSync(entryOf(s));
    const commit = installed ? (gitIn(join(MCP_DIR, s.dir), ["rev-parse", "--short", "HEAD"]).stdout || "").trim() : "-";
    const enabled = cfg.mcp?.[name]?.enabled;
    console.log(
      `  ${name.padEnd(20)} installed=${installed} enabled=${String(enabled).padEnd(5)} commit=${commit}`,
    );
    console.log(`      ${s.label}`);
  }
  console.log("");
  console.log("environment (from opencode.json):");
  const env = cfg.mcp?.["enfusion-mcp"]?.environment ?? {};
  for (const [k, v] of Object.entries(env)) {
    console.log(`  ${k} = ${v}`);
  }
  return 0;
}

function cmdInstall(name) {
  const names = targets(name);
  if (!names) return 1;
  let failed = 0;
  for (const n of names) {
    const s = SERVERS[n];
    const dir = join(MCP_DIR, s.dir);
    if (!existsSync(join(dir, ".git"))) {
      console.log(`cloning ${s.repo} ...`);
      const res = sh("git", ["clone", "--depth", "1", `https://github.com/${s.repo}.git`, dir]);
      if (res.status !== 0) {
        console.log(`FAIL  clone ${s.repo}: ${res.stderr || res.stdout}`);
        failed++;
        continue;
      }
    }
    const pin = (gitIn(dir, ["rev-parse", "--short", "HEAD"]).stdout || "").trim();
    console.log(`${n} @ ${pin}: npm ci + build ...`);
    const ci = sh(NPM, ["ci", "--no-audit", "--no-fund"], { cwd: dir });
    if (ci.status !== 0) {
      console.log(`FAIL  npm ci ${n}: ${ci.stderr || ci.stdout}`);
      failed++;
      continue;
    }
    const b = sh(NPM, ["run", "build"], { cwd: dir });
    if (b.status !== 0) {
      console.log(`FAIL  build ${n}: ${b.stderr || b.stdout}`);
      failed++;
      continue;
    }
    console.log(`OK    ${n} installed at ${entryOf(s)}`);
  }
  return failed ? 1 : 0;
}

function cmdUpdate(name) {
  const names = targets(name);
  if (!names) return 1;
  let failed = 0;
  for (const n of names) {
    const s = SERVERS[n];
    const dir = join(MCP_DIR, s.dir);
    if (!existsSync(join(dir, ".git"))) {
      console.log(`not installed: ${n} (run 'cli mcp install ${n}')`);
      failed++;
      continue;
    }
    const before = (gitIn(dir, ["rev-parse", "--short", "HEAD"]).stdout || "").trim();
    const status = gitIn(dir, ["status", "--porcelain"]);
    if (status.stdout.trim()) {
      console.log(`SKIP  ${n}: working tree dirty (${status.stdout.trim().split(/\r?\n/)[0]}); stash or reset first`);
      failed++;
      continue;
    }
    console.log(`${n}: updating ${before} -> latest ...`);
    const pull = gitIn(dir, ["pull", "--ff-only"]);
    if (pull.status !== 0) {
      console.log(`FAIL  pull ${n}: ${pull.stderr || pull.stdout}`);
      failed++;
      continue;
    }
    const after = (gitIn(dir, ["rev-parse", "--short", "HEAD"]).stdout || "").trim();
    const ci = sh(NPM, ["ci", "--no-audit", "--no-fund"], { cwd: dir });
    if (ci.status !== 0) {
      console.log(`FAIL  npm ci ${n}: ${ci.stderr || ci.stdout}`);
      failed++;
      continue;
    }
    const b = sh(NPM, ["run", "build"], { cwd: dir });
    if (b.status !== 0) {
      console.log(`FAIL  build ${n}: ${b.stderr || b.stdout}`);
      failed++;
      continue;
    }
    console.log(`OK    ${n}: ${before} -> ${after}`);
  }
  return failed ? 1 : 0;
}

function cmdVerify(name) {
  const names = targets(name);
  if (!names) return 1;
  return Promise.all(names.map(verifyOne)).then((codes) => (codes.some((c) => c !== 0) ? 1 : 0));
}

function verifyOne(name) {
  const s = SERVERS[name];
  const entry = entryOf(s);
  if (!existsSync(entry)) {
    console.log(`FAIL  ${name}: missing ${entry} (run 'cli mcp install ${name}')`);
    return Promise.resolve(1);
  }
  return new Promise((resolvePromise) => {
    const child = spawn(process.execPath, [entry], {
      env: { ...process.env, ...serverEnv(name) },
      stdio: ["pipe", "pipe", "pipe"],
    });
    child.stdout?.on("data", () => {});
    child.stderr?.on("data", () => {});
    let settled = false;
    const done = (ok, msg) => {
      if (settled) return;
      settled = true;
      if (child.exitCode === null) child.kill();
      console.log(`${ok ? "OK  " : "FAIL"}  ${name}: ${msg}`);
      resolvePromise(ok ? 0 : 1);
    };
    child.on("error", (e) => done(false, `spawn error: ${e.message}`));
    child.on("exit", (code) => done(code === 0, `exited unexpectedly (code ${code})`));
    setTimeout(() => done(true, "server started and stayed alive (killed after 4s)"), 4000);
  });
}

function cmdSetEnabled(name, enabled) {
  const names = targets(name);
  if (!names) return 1;
  const cfg = readConfig();
  cfg.mcp = cfg.mcp ?? {};
  for (const n of names) {
    if (!cfg.mcp[n]) {
      console.log(`no config entry for ${n} in opencode.json`);
      continue;
    }
    cfg.mcp[n].enabled = enabled;
    console.log(`${n} -> ${enabled ? "enabled" : "disabled"} (quit and restart opencode to apply)`);
  }
  writeConfig(cfg);
  return 0;
}

function invokeScript(full) {
  if (full.endsWith(".ps1")) return sh(PWSH, ["-NoProfile", "-ExecutionPolicy", "Bypass", "-File", full]);
  if (full.endsWith(".cmd") || full.endsWith(".bat")) return sh("cmd", ["/c", full]);
  if (full.endsWith(".sh")) return sh("bash", [full]);
  return sh(process.execPath, [full]); // .mjs/.js/.cjs and bare scripts
}

function cmdRunArea(area) {
  const dir = join(ROOT, "tools", TOOL_DIRS[area]);
  if (!existsSync(dir)) {
    console.log(`tools/${TOOL_DIRS[area]}/ does not exist yet`);
    return 0;
  }
  const scripts = readdirSync(dir).filter(
    (f) => /\.(ps1|cmd|bat|sh|mjs|js|cjs)$/.test(f) && !f.endsWith("README.md"),
  );
  if (scripts.length === 0) {
    console.log(`No ${area} checks defined. Add scripts to tools/${TOOL_DIRS[area]}/ and they run automatically.`);
    return 0;
  }
  let failed = 0;
  for (const s of scripts) {
    const res = invokeScript(join(dir, s));
    const ok = res.status === 0;
    console.log(`${ok ? "PASS" : "FAIL"}  ${area}/${s}`);
    if (res.stdout) process.stdout.write(res.stdout + (res.stdout.endsWith("\n") ? "" : "\n"));
    if (res.stderr) process.stderr.write(res.stderr + (res.stderr.endsWith("\n") ? "" : "\n"));
    if (!ok) failed++;
  }
  console.log(`${area}: ${scripts.length} check(s), ${failed} failed`);
  return failed ? 1 : 0;
}

// ----------------------------------------------------------- build / serve / test

function envOf() {
  return readConfig().mcp?.["enfusion-mcp"]?.environment ?? {};
}

function workbenchExe() {
  const env = envOf();
  const wb = env.ENFUSION_WORKBENCH_PATH || "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger Tools";
  return join(wb, "Workbench", "ArmaReforgerWorkbenchSteamDiag.exe");
}

function serverExe() {
  const env = envOf();
  return env.ENFUSION_SERVER_PATH || "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger Server/ArmaReforgerServer.exe";
}

function serverArgs() {
  // Addon discovery: -server loads the DebugWorld directly (no -config, which
  // conflicts with -addons). -addonsDir points at the repo addons plus a
  // junction to the game client's core/data ONLY - the server install's own
  // ./addons is skipped so the packed EPF/EDF paks (which fail to compile on
  // current Reforger) never load.
  const gameDir = envOf().ENFUSION_GAME_PATH || "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger";
  const profile = join(ROOT, "server", "profile", "test");
  const junction = join(profile, "game-addons");
  if (!existsSync(join(junction, "data"))) {
    mkdirSync(join(junction), { recursive: true });
    try {
      sh("cmd", ["/c", "mklink", "/J", join(junction, "core"), join(gameDir, "addons", "core")]);
      sh("cmd", ["/c", "mklink", "/J", join(junction, "data"), join(gameDir, "addons", "data")]);
    } catch {
      // junction may already exist or mklink unavailable; core/data are then resolved via gameDir below
    }
  }
  const addonsDir = [join(ROOT, "addons"), junction].join(",");
  return [
    "-server", "Worlds/DebugWorld/DebugWorld.ent",
    "-addonsDir", addonsDir,
    "-addons", "LifeFramework",
    "-profile", profile,
    "-maxFPS", "30",
    "-logLevel", "normal",
    "-disableCrashReporter",
    "-noBackend",
  ];
}

function latestWorkbenchLog(profile) {
  const logsDir = join(profile, "logs");
  if (!existsSync(logsDir)) return null;
  const dirs = readdirSync(logsDir)
    .filter((d) => d.startsWith("logs_"))
    .sort();
  if (!dirs.length) return null;
  const f = join(logsDir, dirs[dirs.length - 1], "console.log");
  return existsSync(f) ? f : null;
}

function cmdBuild() {
  const exe = workbenchExe();
  if (!existsSync(exe)) {
    console.log(`FAIL  workbench not found: ${exe}`);
    console.log("      set ENFUSION_WORKBENCH_PATH (Arma Reforger Tools) in opencode.json");
    return Promise.resolve(1);
  }
  const gproj = join(ROOT, "addons", "LifeFramework", "LifeFramework.gproj");
  const out = join(ROOT, "server", "build");
  const profile = join(ROOT, "server", "profile", "build");
  mkdirSync(out, { recursive: true });
  const env = envOf();
  const gameDir = env.ENFUSION_GAME_PATH || "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger";
  if (!existsSync(gameDir)) {
    console.log(`FAIL  game install not found: ${gameDir} (set ENFUSION_GAME_PATH)`);
    return Promise.resolve(1);
  }
  const addonDirs = [];
  if (env.ENFUSION_PROJECT_PATH) addonDirs.push(env.ENFUSION_PROJECT_PATH);
  // The base game (core/data, GUID 58D0FB3206B6F859) resolves reliably through
  // a junction to the game install's addons - ./addons relative to the game dir
  // is flaky in headless launches. Reuse the same junction the server harness
  // builds (server/profile/test/game-addons), or the top-level one if present.
  const junctions = [
    join(ROOT, "server", "profile", "test", "game-addons"),
    "C:/Users/jaspe/Documents/Reforger/GameAddonsLink",
  ];
  const junction = junctions.find((j) => existsSync(join(j, "data")));
  if (junction) addonDirs.push(junction);
  const args = [
    "-gproj", gproj,
    "-wbModule=ResourceManager",
    "-buildData", "PC", out,
    "-metaFiles",
    "-loadBuiltData",
    "-noSplash",
    "-run",
    "-exitAfterInit",
    "-profile", profile,
    ...addonDirs.flatMap((d) => ["-addonsDir", d]),
  ];
  console.log(`building addon (headless Workbench) -> ${out}`);
  console.log(`  cwd (game dir): ${gameDir}`);
  console.log(`  addon dirs: ${addonDirs.join(", ")}`);
  console.log("  (streaming Workbench console.log live)");

  return new Promise((resolvePromise) => {
    const child = spawn(exe, args, { cwd: gameDir });
    let buffer = "";
    const onData = (d) => {
      buffer += d.toString();
      const lines = buffer.split(/\r?\n/);
      buffer = lines.pop();
      for (const l of lines) {
        if (l.trim()) console.log(`  ${l}`);
      }
    };
    child.stdout?.on("data", onData);
    child.stderr?.on("data", onData);

    let lastLog = null;
    let offset = 0;
    const iv = setInterval(() => {
      const f = latestWorkbenchLog(profile);
      if (f && f !== lastLog) {
        lastLog = f;
        offset = 0;
      }
      if (!f) return;
      try {
        const fd = openSync(f, "r");
        const size = fstatSync(fd).size;
        if (size > offset) {
          const buf = Buffer.alloc(size - offset);
          readSync(fd, buf, 0, buf.length, offset);
          offset = size;
          for (const l of buf.toString("utf8").split(/\r?\n/)) {
            if (l && /SCRIPT|ENGINE|RESOURCES|DEFAULT|BUILD|PROFILING/.test(l)) console.log(`  ${l}`);
          }
        }
        closeSync(fd);
      } catch {}
    }, 1000);

    const buildTimeout = setTimeout(() => {
      clearInterval(iv);
      console.log("build TIMEOUT after 15 min - killing Workbench");
      try {
        child.kill();
      } catch {}
      resolvePromise(1);
    }, 900000);

    child.on("error", (e) => {
      clearTimeout(buildTimeout);
      clearInterval(iv);
      console.log(`FAIL  spawn error: ${e.message}`);
      resolvePromise(1);
    });
    child.on("exit", (code) => {
      clearTimeout(buildTimeout);
      clearInterval(iv);
      if (code === 0) {
        console.log("build OK (exit 0)");
        resolvePromise(0);
        return;
      }
      console.log(`build FAILED (exit ${code})`);
      const f = latestWorkbenchLog(profile);
      if (f) {
        const lines = readFileSync(f, "utf8").split(/\r?\n/);
        lines
          .filter((l) => /\( E \)|\bERROR\b|Exception|Can't compile|Unable to/i.test(l))
          .slice(-20)
          .forEach((l) => console.log(`  ${l}`));
      }
      resolvePromise(1);
    });
  });
}

function cmdServe() {
  const exe = serverExe();
  if (!existsSync(exe)) {
    console.log(`FAIL  dedicated server not found: ${exe}`);
    console.log("      install Arma Reforger Server (Steam app 1874900) or set ENFUSION_SERVER_PATH");
    return 1;
  }
  const args = serverArgs();
  console.log(`launching: ${exe}`);
  console.log(`  ${args.join(" ")}`);
  return new Promise((resolvePromise) => {
    const child = spawn(exe, args, { cwd: dirname(exe), stdio: "inherit" });
    child.on("error", (e) => resolvePromise((console.log(`FAIL  spawn error: ${e.message}`), 1)));
    child.on("exit", (code) => resolvePromise(code ?? 1));
  });
}

function runServerTest(exe, args, logFile, tier = "all") {
  return new Promise((resolvePromise) => {
    const log = createWriteStream(logFile, { flags: "a" });
    // Unpacked addon discovery is CWD-relative (./addons), so the server must
    // run from the repo root; the -addonsDir entries supply the packed
    // dependency addons (EPF/EDF) and the vanilla Game addon.
    const child = spawn(exe, args, { cwd: ROOT, stdio: ["ignore", "pipe", "pipe"] });
    const interesting = /\[ELTEST\]|\( E \)|\( W \)|ERROR|Unable to|scenario|mission|Initializing|Starting|loading addon|loaded addon|Game destroyed/i;
    child.stdout?.on("data", (d) => {
      log.write(d);
      for (const l of d.toString().split(/\r?\n/)) {
        if (l && interesting.test(l)) console.log(`  ${l}`);
      }
    });
    child.stderr?.on("data", (d) => {
      log.write(d);
      for (const l of d.toString().split(/\r?\n/)) {
        if (l && interesting.test(l)) console.log(`  ${l}`);
      }
    });
    let done = false;
    const finish = (ok, msg) => {
      if (done) return;
      done = true;
      if (child.exitCode === null) child.kill();
      log.end();
      console.log(msg);
      resolvePromise(ok ? 0 : 1);
    };
    child.on("error", (e) => finish(false, `FAIL  spawn error: ${e.message}`));
    child.on("exit", (code) => {
      if (!done) finish(false, `FAIL  server exited before tests completed (code ${code})`);
    });
    const started = Date.now();
    const iv = setInterval(() => {
      let tail = "";
      try {
        tail = readFileSync(logFile, "utf8");
      } catch {
        return;
      }
      const eltest = tail.match(/\[ELTEST\] SUMMARY (?:tier=(\w+) )?passed=(\d+) failed=(\d+) total=(\d+)/);
      const mgr = tail.match(/\[EL_Tests\] runtime tests done, failures=(\d+)/);
      const m = eltest || mgr;
      if (m) {
        clearInterval(iv);
        const passed = eltest ? +eltest[2] : null;
        const failed = eltest ? +eltest[3] : +mgr[1];
        const engineErrors = (tail.match(/\( E \)/g) || []).length;
        // A fast request answered by an all run (or vice versa) means the
        // -scrDefine plumbing failed; that is a no-verdict, not a pass.
        const tierSeen = eltest ? eltest[1] : null;
        const tierOk = !eltest || !tierSeen || tierSeen === tier;
        const ok = failed === 0 && engineErrors === 0 && tierOk;
        console.log(
          `tests: ${eltest ? `tier=${tierSeen} passed=${passed} failed=${failed}` : `failures=${failed} (EL_TestManager)`} engineErrors=${engineErrors} ${ok ? "OK" : "FAILED"}`,
        );
        if (!tierOk) console.log(`FAIL  requested tier=${tier} but suite reported tier=${tierSeen}`);
        finish(ok, `test result: ${ok ? "OK" : "FAILED"}`);
        return;
      }
      if (Date.now() - started > 300000) {
        clearInterval(iv);
        finish(false, "FAIL  timeout waiting for test summary (300s)");
      }
    }, 2000);
  });
}

async function cmdTest(noBuild, tier = "all") {
  if (tier !== "fast" && tier !== "all") {
    console.log(`unknown tier: ${tier} (expected fast or all)`);
    return 2;
  }
  if (!noBuild) {
    const b = await cmdBuild();
    if (b !== 0) return b;
  }
  // Delegate to the proven PowerShell harness (tools/test/test-e2e.ps1):
  // boots the dedicated server via -server + a neutral addonsDir, polls the
  // profile console.log every 2s for the [ELTEST] SUMMARY, self-terminates.
  const harness = join(ROOT, "tools", "test", "test-e2e.ps1");
  if (!existsSync(harness)) {
    console.log(`FAIL  harness not found: ${harness}`);
    return 1;
  }
  const res = sh(PWSH, ["-NoProfile", "-ExecutionPolicy", "Bypass", "-File", harness, "-Tier", tier]);
  process.stdout.write(res.stdout + res.stderr);
  return res.status;
}

async function cmdCi() {
  const steps = [
    ["validate", () => cmdRunArea("validate")],
    ["build", () => cmdBuild()],
    ["test", () => cmdTest(false)],
  ];
  let failed = 0;
  for (const [name, fn] of steps) {
    console.log(`\n=== ci: ${name} ===`);
    const code = await Promise.resolve(fn());
    if (code !== 0) failed++;
  }
  console.log(failed === 0 ? "\nci: ALL PASSED" : `\nci: ${failed} step(s) failed`);
  return failed === 0 ? 0 : 1;
}

function cmdHelp() {
  console.log(`Life Framework dev CLI

usage: tools\\cli <command> [args]

  status                        toolchain + MCP server state
  mcp install [name]            clone (if missing), npm ci, build
  mcp update [name]             git pull --ff-only, npm ci, build
  mcp verify [name]             boot server briefly to confirm it starts
  mcp enable <name>             enable a server in opencode.json
  mcp disable <name>            disable a server in opencode.json
  build                         headless Workbench build of the addon
  serve                         boot the headless test server (blocks)
  test [--no-build] [--tier fast|all]
                                build + boot server + parse ELTEST results
  ci                            validate + build + test (full gate)
  call <tool> '<json>'|@file    call an MCP tool directly (see tools/mcp-call.mjs)
  validate | lint               run every script in tools/{validation,lint}/
  run <area>                    run every script in tools/<area>/ (validate, lint, test, ...)

servers: ${Object.keys(SERVERS).join(", ")}
`);
  return 0;
}

// ------------------------------------------------------------------- main

const [cmd, ...rest] = process.argv.slice(2);

function argValue(flags, name) {
  const i = flags.indexOf(name);
  if (i === -1 || i + 1 >= flags.length) return null;
  return flags[i + 1];
}

// tools\cli call <tool> '<json>'|@file [server]  -  delegate to tools/mcp-call.mjs
function cmdMcpCall(callArgs) {
  const entry = join(ROOT, "tools", "mcp-call.mjs");
  if (!existsSync(entry)) {
    console.log("tools/mcp-call.mjs not found");
    return 1;
  }
  const res = sh(process.execPath, [entry, ...callArgs]);
  process.stdout.write(res.stdout + res.stderr);
  return res.status;
}

const cmds = {
  status: () => cmdStatus(),
  install: (n) => cmdInstall(n),
  update: (n) => cmdUpdate(n),
  verify: (n) => cmdVerify(n),
  enable: (n) => cmdSetEnabled(n, true),
  disable: (n) => cmdSetEnabled(n, false),
  build: () => cmdBuild(),
  serve: () => cmdServe(),
  test: (flags) => {
    const list = flags ?? [];
    const noBuild = list.includes("--no-build");
    return cmdTest(noBuild, argValue(list, "--tier") ?? "all");
  },
  ci: () => cmdCi(),
validate: () => cmdRunArea("validate"),
  lint: () => cmdRunArea("lint"),
  run: (n) =>
    TOOL_DIRS[n] ? cmdRunArea(n) : (console.log(`unknown area: ${n} (expected one of: ${Object.keys(TOOL_DIRS).join(", ")}`), 1),
  call: () => cmdMcpCall(rest),
  help: () => cmdHelp(),
};

const [a, b] = rest;

let code;
if (cmd === "mcp") {
  const sub = cmds[a];
  code = sub ? sub(b) : (console.log(`unknown mcp subcommand: ${a}\n`), cmdHelp());
} else if (cmd && cmds[cmd]) {
  code = cmds[cmd](cmd === "test" ? rest : a);
} else {
  if (cmd) console.log(`unknown command: ${cmd}\n`);
  code = cmdHelp();
}

Promise.resolve(code).then((c) => process.exit(c));