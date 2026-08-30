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
import * as wt from "./wt.mjs";

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
  console.log("environment (effective; OS env wins over opencode.json):");
  const env = envOf();
  for (const [k, v] of Object.entries(env)) {
    console.log(`  ${k} = ${v}`);
  }
  if (Object.keys(env).length === 0) console.log("  (defaults: standard Steam install locations)");
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

function invokeScript(full, opts = {}) {
  if (full.endsWith(".ps1")) return sh(PWSH, ["-NoProfile", "-ExecutionPolicy", "Bypass", "-File", full], opts);
  if (full.endsWith(".cmd") || full.endsWith(".bat")) return sh("cmd", ["/c", full], opts);
  if (full.endsWith(".sh")) return sh("bash", [full], opts);
  return sh(process.execPath, [full], opts); // .mjs/.js/.cjs and bare scripts
}

// Effective env for spawned tool scripts: OS env overlaid with the configured
// ENFUSION_* values so tools/validation/* and tools/test/* agree with the CLI.
function toolEnv() {
  return { ...process.env, ...envOf() };
}

function cmdRunArea(area, root = ROOT) {
  // Worktree checks must use the target checkout's scripts. Using the main
  // checkout here makes a branch's generated registries look stale when it
  // has added a test or validator.
  const dir = join(root, "tools", TOOL_DIRS[area]);
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
    const res = invokeScript(join(dir, s), { cwd: root, env: toolEnv() });
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

// Effective ENFUSION_* environment: process.env wins over opencode.json, so
// contributors on non-default installs can set OS env vars instead of editing
// the (committed, portable) config.
function envOf() {
  const cfg = readConfig().mcp?.["enfusion-mcp"]?.environment ?? {};
  const env = { ...cfg };
  for (const k of ["ENFUSION_WORKBENCH_PATH", "ENFUSION_GAME_PATH", "ENFUSION_SERVER_PATH", "ENFUSION_PROJECT_PATH"]) {
    if (process.env[k]) env[k] = process.env[k];
  }
  return env;
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

// The main checkout is the world-editor copy. Heavy commands (build/test/dev/
// serve/ci) refuse to run there so agent activity never breaks the editor.
function guardHeavy(root, command, force) {
  if (!wt.isMainCheckout(root) || force) return;
  console.log(`REFUSE  '${command}' runs in the main checkout, which is the world-editor copy.`);
  console.log("        create a worktree first:  tools\\cli wt new <feature>");
  console.log(`        then run from the worktree, or 'cli wt ${command} <slug>', or pass --force to override.`);
  throw new Error("main-checkout guard");
}

// Junction to the base game's core/data addons under <root>/server/profile/
// test/game-addons. Self-healing: created on demand from the configured game
// install so builds and test servers resolve the vanilla Game addon on any
// machine. Returns the junction path if usable, else null (callers then rely
// on the engine's own addon discovery).
function gameAddonsJunction(root) {
  const gameDir = envOf().ENFUSION_GAME_PATH || "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger";
  const junction = join(root, "server", "profile", "test", "game-addons");
  if (!existsSync(join(junction, "data"))) {
    mkdirSync(join(junction), { recursive: true });
    try {
      sh("cmd", ["/c", "mklink", "/J", join(junction, "core"), join(gameDir, "addons", "core")]);
      sh("cmd", ["/c", "mklink", "/J", join(junction, "data"), join(gameDir, "addons", "data")]);
    } catch {
      // junction may already exist or mklink unavailable; callers fall back to engine discovery
    }
  }
  const coreSource = join(gameDir, "addons", "core");
  const coreTarget = join(junction, "core");
  const coreSourceHasProject = existsSync(join(coreSource, "core.gproj"));
  if (!coreSourceHasProject && existsSync(coreSource) && !existsSync(join(coreTarget, "core.gproj"))) {
    // Some server/client installs ship core data.pak without core.gproj. Do
    // not write through the junction into the shared install; make a local
    // core package and add only the project metadata required by the engine.
    try {
      rmSync(coreTarget, { recursive: true, force: true });
      mkdirSync(coreTarget, { recursive: true });
      for (const file of readdirSync(coreSource)) {
        copyFileSync(join(coreSource, file), join(coreTarget, file));
      }
      writeFileSync(join(coreTarget, "core.gproj"), [
        "GameProject {",
        " ID \"core\"",
        " GUID \"5614BBCCBB55ED1C\"",
        " TITLE \"core\"",
        " Configurations {",
        "  GameProjectConfig PC {",
        "  }",
        "  GameProjectConfig HEADLESS : PC {",
        "  }",
        " }",
        "}",
        "",
      ].join("\n"));
    } catch {
      // The normal junction remains the fallback if local provisioning fails.
    }
  }
  return existsSync(join(junction, "data")) ? junction : null;
}

function serverArgs(root, ports) {
  // Addon discovery: -server loads the DebugWorld directly (no -config, which
  // conflicts with -addons). -addonsDir points at the repo addons plus a
  // junction to the game client's core/data ONLY - the server install's own
  // ./addons is skipped so the packed EPF/EDF paks (which fail to compile on
  // current Reforger) never load. -bindPort/-a2sPort give every worktree its
  // own port pair so parallel test runs never collide.
  const profile = join(root, "server", "profile", "test");
  const junction = gameAddonsJunction(root) ?? "";
  const addonsDir = [join(root, "addons"), junction].filter(Boolean).join(",");
  return [
    "-server", "Worlds/DebugWorld/DebugWorld.ent",
    "-addonsDir", addonsDir,
    "-addons", "LifeFramework",
    "-profile", profile,
    "-maxFPS", "30",
    "-logLevel", "normal",
    "-scrDefine", "EL_AUTOTEST",
    "-disableCrashReporter",
    "-noBackend",
    "-bindPort", String(ports.gamePort),
    "-a2sPort", String(ports.a2sPort),
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

function cmdBuild(root = ROOT, opts = {}) {
  guardHeavy(root, "build", opts.force);
  const exe = workbenchExe();
  if (!existsSync(exe)) {
    console.log(`FAIL  workbench not found: ${exe}`);
    console.log("      set ENFUSION_WORKBENCH_PATH (Arma Reforger Tools) in opencode.json");
    return Promise.resolve(1);
  }
  // One Workbench build at a time across ALL worktrees: concurrent instances
  // fight over the shared shader cache and can corrupt the user's editor
  // session. The fast dev loop (cli dev) skips the build, so this rarely blocks.
  let lock;
  try {
    lock = wt.acquireLock(root, "build", { waitMs: opts.wait ? 900000 : 0 });
  } catch (e) {
    console.log(`FAIL  ${e.message} (a headless Workbench build is running in another worktree)`);
    return Promise.resolve(1);
  }
  const gproj = join(root, "addons", "LifeFramework", "LifeFramework.gproj");
  const out = join(root, "server", "build");
  const profile = join(root, "server", "profile", "build");
  mkdirSync(out, { recursive: true });
  const env = envOf();
  const gameDir = env.ENFUSION_GAME_PATH || "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger";
  if (!existsSync(gameDir)) {
    console.log(`FAIL  game install not found: ${gameDir} (set ENFUSION_GAME_PATH)`);
    wt.releaseLock(root, "build");
    return Promise.resolve(1);
  }
  const addonDirs = [join(root, "addons")];
  // The base game (core/data, GUID 58D0FB3206B6F859) resolves reliably through
  // a junction to the game install's addons - ./addons relative to the game dir
  // is flaky in headless launches. The junction is created on demand under
  // server/profile/test/game-addons (see gameAddonsJunction).
  const junction = gameAddonsJunction(root);
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

  const runOnce = () => new Promise((resolvePromise) => {
    const done = (code) => {
      resolvePromise(code);
    };
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
      done(1);
    }, 900000);

    child.on("error", (e) => {
      clearTimeout(buildTimeout);
      clearInterval(iv);
      console.log(`FAIL  spawn error: ${e.message}`);
      done(1);
    });
    child.on("exit", (code) => {
      clearTimeout(buildTimeout);
      clearInterval(iv);
      if (code === 0) {
        console.log("build OK (exit 0)");
        done(0);
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
      done(1);
    });
  });

  // NVIDIA driver 616.56 (installed 2026-08-30) makes Workbench's nvtt texture
  // worker exit with STATUS_ACCESS_VIOLATION (0xC0000005) at engine init on
  // roughly every other headless launch, usually after the log line
  // "Adapter ... failed to provide some output". The crash happens before any
  // project work starts, so rerunning the exact command is safe. Retry twice;
  // a compile or data error still fails the gate on the first attempt.
  const ACCESS_VIOLATION = 3221225477;
  const attempt = (n) => runOnce().then((code) => {
    if (code === ACCESS_VIOLATION && n < 3) {
      console.log(`  known flaky Workbench init crash (0xC0000005, GPU/nvtt) - retry ${n}/2`);
      return attempt(n + 1);
    }
    wt.releaseLock(root, "build");
    return code;
  });
  return attempt(1);
}

function cmdServe(root = ROOT, opts = {}) {
  guardHeavy(root, "serve", opts.force);
  const exe = serverExe();
  if (!existsSync(exe)) {
    console.log(`FAIL  dedicated server not found: ${exe}`);
    console.log("      install Arma Reforger Server (Steam app 1874900) or set ENFUSION_SERVER_PATH");
    return 1;
  }
  const args = serverArgs(root, wt.portsForRoot(root));
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

async function cmdTest(noBuild, tier = "all", root = ROOT, opts = {}) {
  if (tier !== "fast" && tier !== "all" && tier !== "persistence") {
    console.log(`unknown tier: ${tier} (expected fast, all, or persistence)`);
    return 2;
  }
  guardHeavy(root, "test", opts.force);
  if (!noBuild) {
    const b = await cmdBuild(root, { wait: opts.wait });
    if (b !== 0) return b;
  }
  // Delegate to the proven PowerShell harness (tools/test/test-e2e.ps1):
  // boots the dedicated server via -server + a neutral addonsDir, polls the
  // profile console.log every 2s for the [ELTEST] SUMMARY, self-terminates.
  // The harness is loaded from the CLI's own checkout but launched with cwd in
  // the target worktree, so it builds its profile/junction/logs there.
  const harness = join(ROOT, "tools", "test", "test-e2e.ps1");
  if (!existsSync(harness)) {
    console.log(`FAIL  harness not found: ${harness}`);
    return 1;
  }
  const ports = wt.portsForRoot(root);
  console.log(`test: worktree ports game=${ports.gamePort} a2s=${ports.a2sPort}`);
  const res = sh(PWSH, [
    "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", harness,
    "-Tier", tier,
    "-BindPort", String(ports.gamePort),
    "-A2sPort", String(ports.a2sPort),
  ], { cwd: root, env: toolEnv() });
  process.stdout.write(res.stdout + res.stderr);
  return res.status;
}

async function cmdDev(tier = "all", root = ROOT, opts = {}) {
  // Fast dev loop: skip the headless Workbench build (the dedicated server
  // compiles scripts at boot from the unpacked addons), run the in-game suite,
  // then dump the per-feature [ELDebug:*] lines from the same log.
  guardHeavy(root, "dev", opts.force);
  console.log("=== dev: validate ===");
  const v = await cmdRunArea("validate", root);
  if (v !== 0) return v;
  console.log("\n=== dev: test (no build) ===");
  return cmdTest(true, tier, root, opts);
}

async function cmdCi(root = ROOT, opts = {}) {
  guardHeavy(root, "ci", opts.force);
  const steps = [
    ["validate", () => cmdRunArea("validate", root)],
    ["build", () => cmdBuild(root, { wait: opts.wait })],
    ["test", () => cmdTest(false, "all", root, opts)],
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

Heavy commands (build/test/dev/serve/ci) refuse to run in the main checkout -
the world-editor copy. Create a worktree first: tools\\cli wt new <feature>.

  status                        toolchain + MCP server state
  mcp install [name]            clone (if missing), npm ci, build
  mcp update [name]             git pull --ff-only, npm ci, build
  mcp verify [name]             boot server briefly to confirm it starts
  mcp enable <name>             enable a server in opencode.json
  mcp disable <name>            disable a server in opencode.json
  build                         headless Workbench build of the addon (worktree only)
  serve                         boot the headless test server (blocks, worktree only)
  test [--no-build] [--tier fast|all|persistence]
                                build + boot server + parse ELTEST results (worktree only)
  dev [--tier fast|all|persistence]  fast dev loop: validate + test --no-build +
                                dump [ELDebug:*] feature log lines (worktree only)
  ci                            validate + build + test (full gate, worktree only)
  wt <command>                  worktree isolation + auto-merge (see: cli wt help)
  regen-tests                   regenerate the test registry from the test files
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

// ---------------------------------------------------------------- worktrees

function wtSlug(args) {
  const slug = args[0];
  if (!slug || slug.startsWith("-")) {
    console.log("usage: tools\\cli wt <command> <slug> [flags]");
    return null;
  }
  return slug;
}

function wtHelp() {
  console.log(`worktree commands (parallel agent isolation)

  tools\\cli wt new <slug> [--wait]    create Life-Framework-ws-<slug> at ws/<slug>, allocate ports
  tools\\cli wt list                   worktrees, ports, dirty/merged state
  tools\\cli wt test <slug> [--tier fast|all|persistence] [--no-build] [--wait]
                                       run the ELTEST suite in that worktree (from anywhere)
  tools\\cli wt e2e <slug>
                                       two-boot save-state E2E: persistence save boot, then
                                       reload same profile with -loadSessionSave (latest)
  tools\\cli wt build <slug> [--wait]  headless Workbench build in that worktree (serialized)
  tools\\cli wt dev <slug> [--tier X]  fast loop (validate + test --no-build) in that worktree
  tools\\cli wt gate <slug> [--wait]   validate + build + test (tier=all) in that worktree
  tools\\cli wt sync <slug>            merge origin/main into the worktree branch
  tools\\cli wt ship <slug> [--pr-only] [--skip-gate] [--skip-sync]
                                       sync -> gate -> push -> PR -> auto-merge into main
  tools\\cli wt prune <slug> [--force] remove a merged worktree + its branch
  tools\\cli wt open <slug>            open a terminal in the worktree
  tools\\cli wt open <slug> --workbench launch Workbench on that worktree
  tools\\cli wt main                   print the main checkout path

flags:
  --wait       queue behind the build/worktree lock instead of failing fast
  --pr-only    open the PR but leave it for review (no auto-merge)
  --skip-gate  / --skip-sync   skip a ship stage (debug only)
  --force      prune without merge/dirty checks
`);
  return 0;
}

function wtNewCmd(args) {
  const slug = wtSlug(args);
  if (!slug) return 1;
  try {
    const r = wt.wtNew(ROOT, slug, { waitMs: args.includes("--wait") ? 60000 : 0 });
    console.log(`created worktree ${r.slug}`);
    console.log(`  path:   ${r.path}`);
    console.log(`  branch: ${r.branch}`);
    console.log(`  ports:  game ${r.ports.gamePort}   a2s ${r.ports.a2sPort}`);
    console.log("");
    console.log(`  cd ${r.path}`);
    console.log(`  tools\\cli dev --tier fast        # iterate (no build)`);
    console.log(`  tools\\cli wt gate ${r.slug}      # validate + build + test`);
    console.log(`  tools\\cli wt ship ${r.slug}      # gate, PR, auto-merge`);
    return 0;
  } catch (e) {
    console.log(`FAIL  ${e.message}`);
    return 1;
  }
}

function wtListCmd() {
  const state = wt.reconcileState(ROOT);
  const rows = Object.values(state.worktrees ?? {});
  if (rows.length === 0) {
    console.log("no worktrees registered. create one:  tools\\cli wt new <feature>");
    return 0;
  }
  console.log("worktrees  (main checkout is reserved for the world editor)");
  console.log(`  ${"slug".padEnd(20)}${"branch".padEnd(22)}${"ports".padEnd(14)}state`);
  for (const e of rows) {
    if (!existsSync(e.path)) {
      console.log(`  ${e.slug.padEnd(20)}${e.branch.padEnd(22)}${`${e.ports.gamePort}/${e.ports.a2sPort}`.padEnd(14)}MISSING`);
      continue;
    }
    const merged = wt.gitOk(ROOT, ["merge-base", "--is-ancestor", e.branch, "origin/main"]);
    const ahead = parseInt(wt.git(ROOT, ["rev-list", "--count", "origin/main.." + e.branch]).stdout.trim() || "0", 10);
    const behind = parseInt(wt.git(ROOT, ["rev-list", "--count", e.branch + "..origin/main"]).stdout.trim() || "0", 10);
    const dirty = wt.git(e.path, ["status", "--porcelain"]).stdout.trim() ? "dirty" : "clean";
    const cur = wt.currentBranch(e.path);
    const stateText = merged && ahead === 0
      ? (behind > 0 ? "merged (prune me)" : "fresh (no commits yet)")
      : `${dirty}${cur !== e.branch ? ` on ${cur}` : ""}${ahead > 0 ? ` +${ahead}` : ""}`;
    console.log(`  ${e.slug.padEnd(20)}${e.branch.padEnd(22)}${`${e.ports.gamePort}/${e.ports.a2sPort}`.padEnd(14)}${stateText}`);
  }
  return 0;
}

function wtSyncCmd(args) {
  const slug = wtSlug(args);
  if (!slug) return 1;
  try {
    const r = wt.wtSync(ROOT, slug);
    console.log(`synced ${r.slug}: merged origin/main into ${r.branch}`);
    return 0;
  } catch (e) {
    console.log(`FAIL  ${e.message}`);
    return 1;
  }
}

function wtPruneCmd(args) {
  const slug = wtSlug(args);
  if (!slug) return 1;
  try {
    const r = wt.wtPrune(ROOT, slug, { force: args.includes("--force") });
    console.log(`pruned ${r.slug}: removed ${r.path} and branch ${r.branch}`);
    return 0;
  } catch (e) {
    console.log(`FAIL  ${e.message}`);
    return 1;
  }
}

function wtOpenCmd(args) {
  const slug = wtSlug(args);
  if (!slug) return 1;
  try {
    const e = wt.requireWorktree(ROOT, slug);
    if (args.includes("--workbench")) {
      const gproj = join(e.path, "addons", "LifeFramework", "LifeFramework.gproj");
      if (!existsSync(gproj)) {
        console.log(`FAIL  LifeFramework.gproj not found in ${e.path}`);
        return 1;
      }
      const profile = join(e.path, "server", "profile", "workbench");
      mkdirSync(profile, { recursive: true });
      const child = spawn(workbenchExe(), ["-gproj", gproj, "-profile", profile], {
        detached: true,
        stdio: "ignore",
      });
      child.unref();
      console.log(`launched Workbench for ${slug}`);
      console.log(`project: ${gproj}`);
      console.log(`profile: ${profile}`);
      console.log("open one worktree at a time; Workbench instances share a shader cache");
      return 0;
    }
    if (process.platform === "win32") {
      sh(PWSH, ["-NoProfile", "-Command", `Start-Process cmd -ArgumentList '/k', 'cd /d "${e.path}"'`]);
    }
    console.log(`opened a terminal in ${e.path}`);
    return 0;
  } catch (err) {
    console.log(`FAIL  ${err.message}`);
    return 1;
  }
}

async function wtTestCmd(args) {
  const slug = wtSlug(args);
  if (!slug) return 1;
  const flags = args.slice(1);
  try {
    const e = wt.requireWorktree(ROOT, slug);
    return cmdTest(
      flags.includes("--no-build"),
      argValue(flags, "--tier") ?? "all",
      e.path,
      { wait: flags.includes("--wait") },
    );
  } catch (err) {
    console.log(`FAIL  ${err.message}`);
    return 1;
  }
}

// One boot of the test-e2e.ps1 harness in the target worktree. Mirrors the
// harness invocation inside cmdTest (same ports, same cwd/env) but exposes the
// profile-keep and -loadSessionSave flags the two-boot save-state E2E needs.
function runE2ePass(root, tier, opts = {}) {
  const harness = join(ROOT, "tools", "test", "test-e2e.ps1");
  if (!existsSync(harness)) {
    console.log(`FAIL  harness not found: ${harness}`);
    return 1;
  }
  const ports = wt.portsForRoot(root);
  const psArgs = [
    "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", harness,
    "-Tier", tier,
    "-BindPort", String(ports.gamePort),
    "-A2sPort", String(ports.a2sPort),
  ];
  if (opts.keepProfile) psArgs.push("-KeepProfile");
  if (opts.loadLatestSave) psArgs.push("-LoadLatestSave");
  else if (opts.loadSessionSave !== undefined) psArgs.push("-LoadSessionSave", opts.loadSessionSave);
  if (opts.gracefulClose) psArgs.push("-GracefulClose");
  if (opts.extraDefine) psArgs.push("-ExtraDefine", opts.extraDefine);
  const res = sh(PWSH, psArgs, { cwd: root, env: toolEnv() });
  process.stdout.write(res.stdout + res.stderr);
  return res.status;
}

async function wtE2eCmd(args) {
  const slug = wtSlug(args);
  if (!slug) return 1;
  let e;
  try {
    e = wt.requireWorktree(ROOT, slug);
  } catch (err) {
    console.log(`FAIL  ${err.message}`);
    return 1;
  }
  const profile = join(e.path, "server", "profile", "test");
  console.log(`e2e: pass 1/2 - save boot (tier=persistence, fresh profile, graceful close so the shutdown save lands) in ${e.path}`);
  const pass1 = runE2ePass(e.path, "persistence", { gracefulClose: true });
  if (pass1 !== 0) {
    console.log(`e2e: FAILED in pass 1 (no save produced); not running pass 2`);
    console.log(`e2e: profile ${profile}`);
    return pass1;
  }
  console.log(`e2e: pass 2/2 - reload boot (tier=persistence, KeepProfile, -loadSessionSave latest, EL_E2E_LOAD_BOOT)`);
  const pass2 = runE2ePass(e.path, "persistence", { keepProfile: true, loadLatestSave: true, extraDefine: "EL_E2E_LOAD_BOOT" });
  console.log(`e2e: profile ${profile}`);
  console.log(`e2e: pass1 exit=${pass1} pass2 exit=${pass2} ${pass1 === 0 && pass2 === 0 ? "OK" : "FAILED"}`);
  return pass1 === 0 && pass2 === 0 ? 0 : (pass1 === 0 ? pass2 : pass1);
}

async function wtDevCmd(args) {
  const slug = wtSlug(args);
  if (!slug) return 1;
  const flags = args.slice(1);
  try {
    const e = wt.requireWorktree(ROOT, slug);
    return cmdDev(argValue(flags, "--tier") ?? "all", e.path, { wait: flags.includes("--wait") });
  } catch (err) {
    console.log(`FAIL  ${err.message}`);
    return 1;
  }
}

async function wtBuildCmd(args) {
  const slug = wtSlug(args);
  if (!slug) return 1;
  const flags = args.slice(1);
  try {
    const e = wt.requireWorktree(ROOT, slug);
    return cmdBuild(e.path, { wait: flags.includes("--wait") });
  } catch (err) {
    console.log(`FAIL  ${err.message}`);
    return 1;
  }
}

async function wtGateCmd(args) {
  const slug = wtSlug(args);
  if (!slug) return 1;
  const flags = args.slice(1);
  try {
    const e = wt.requireWorktree(ROOT, slug);
    return cmdCi(e.path, { wait: flags.includes("--wait") });
  } catch (err) {
    console.log(`FAIL  ${err.message}`);
    return 1;
  }
}

async function wtShipCmd(args) {
  const slug = wtSlug(args);
  if (!slug) return 1;
  const flags = args.slice(1);
  const prOnly = flags.includes("--pr-only");
  const skipGate = flags.includes("--skip-gate");
  const skipSync = flags.includes("--skip-sync");
  let e;
  try {
    e = wt.requireWorktree(ROOT, slug);
  } catch (err) {
    console.log(`FAIL  ${err.message}`);
    return 1;
  }
  const wtRoot = e.path;
  const cur = wt.currentBranch(wtRoot);
  if (cur !== e.branch) {
    console.log(`FAIL  worktree ${slug} is on '${cur}', expected ${e.branch}`);
    return 1;
  }
  const dirty = wt.git(wtRoot, ["status", "--porcelain"]).stdout.trim();
  if (dirty) {
    console.log(`FAIL  worktree ${slug} has uncommitted changes; commit before shipping`);
    return 1;
  }
  const nCommits = parseInt(wt.git(wtRoot, ["rev-list", "--count", "origin/main..HEAD"]).stdout.trim() || "0", 10);
  if (nCommits === 0) {
    console.log(`FAIL  nothing to ship: ${e.branch} has no commits beyond origin/main`);
    return 1;
  }
  if (!skipSync) {
    console.log("=== ship: sync origin/main ===");
    try {
      wt.wtSync(ROOT, slug);
    } catch (err) {
      console.log(`FAIL  ${err.message}`);
      return 1;
    }
  }
  if (!skipGate) {
    console.log("=== ship: gate (validate + build + test tier=all) ===");
    const code = await cmdCi(wtRoot, { wait: true });
    if (code !== 0) {
      console.log(`FAIL  gate failed; not shipping. Fix, commit, re-run 'cli wt ship ${slug}'.`);
      return 1;
    }
  }
  console.log("=== ship: push ===");
  const push = wt.git(wtRoot, ["push", "-u", "origin", e.branch]);
  if (push.status !== 0) {
    console.log(`FAIL  push: ${push.stderr || push.stdout}`);
    return 1;
  }
  const subjects = wt.commitSubjects(wtRoot, "origin/main..HEAD");
  const title = subjects[0] ?? e.branch;
  const body = [
    `Automated ship from worktree \`${slug}\`.`,
    "",
    "Gate passed locally: `validate` + headless `build` + `test` (tier=all).",
    "",
    "## Commits",
    ...subjects.map((s) => `- ${s}`),
  ].join("\n");
  let pr;
  try {
    if (wt.ghAvailable()) {
      pr = wt.createPrGh(wtRoot, { head: e.branch, title, body });
    } else {
      const token = process.env.GITHUB_TOKEN;
      if (!token) {
        console.log("FAIL  no PR path: install the GitHub CLI ('winget install GitHub.cli') or set GITHUB_TOKEN");
        return 1;
      }
      pr = await wt.createPrRest(wtRoot, { head: e.branch, title, body, token });
    }
  } catch (err) {
    console.log(`FAIL  PR creation: ${err.message}`);
    return 1;
  }
  console.log(`PR: ${pr.url}`);
  if (prOnly) {
    console.log("ship: PR left open for review (--pr-only)");
    return 0;
  }
  console.log("=== ship: merge ===");
  try {
    if (wt.ghAvailable()) {
      const m = wt.mergePrGh(wtRoot, pr.number, e.branch);
      if (m.already) console.log(`ship: PR already merged or no-op: ${m.output.trim()}`);
      else console.log("ship: merged into main");
    } else {
      const m = await wt.mergePrRest(wtRoot, pr.number, e.branch, process.env.GITHUB_TOKEN);
      if (m.already) console.log("ship: PR already merged or no-op");
      else console.log("ship: merged into main");
    }
  } catch (err) {
    console.log(`FAIL  merge: ${err.message} (PR is at ${pr.url})`);
    return 1;
  }
  console.log("ship: done. cleanup with 'cli wt prune " + slug + "'");
  return 0;
}

function cmdWt(rest) {
  const [sub, ...subArgs] = rest;
  const table = {
    new: () => wtNewCmd(subArgs),
    list: () => wtListCmd(),
    test: () => wtTestCmd(subArgs),
    e2e: () => wtE2eCmd(subArgs),
    dev: () => wtDevCmd(subArgs),
    build: () => wtBuildCmd(subArgs),
    gate: () => wtGateCmd(subArgs),
    ci: () => wtGateCmd(subArgs),
    sync: () => wtSyncCmd(subArgs),
    ship: () => wtShipCmd(subArgs),
    prune: () => wtPruneCmd(subArgs),
    open: () => wtOpenCmd(subArgs),
    main: () => {
      console.log(wt.mainRootOf(ROOT));
      return 0;
    },
    help: () => wtHelp(),
  };
  if (!sub || !table[sub]) {
    wtHelp();
    return sub ? 1 : 0;
  }
  return Promise.resolve(table[sub]());
}

const cmds = {
  status: () => cmdStatus(),
  install: (n) => cmdInstall(n),
  update: (n) => cmdUpdate(n),
  verify: (n) => cmdVerify(n),
  enable: (n) => cmdSetEnabled(n, true),
  disable: (n) => cmdSetEnabled(n, false),
  build: (flags) => cmdBuild(ROOT, { force: (flags ?? []).includes("--force"), wait: (flags ?? []).includes("--wait") }),
  serve: (flags) => cmdServe(ROOT, { force: (flags ?? []).includes("--force") }),
  dev: (flags) => {
    const list = flags ?? [];
    return cmdDev(argValue(list, "--tier") ?? "all", ROOT, { force: list.includes("--force"), wait: list.includes("--wait") });
  },
  test: (flags) => {
    const list = flags ?? [];
    const noBuild = list.includes("--no-build");
    return cmdTest(noBuild, argValue(list, "--tier") ?? "all", ROOT, { force: list.includes("--force"), wait: list.includes("--wait") });
  },
  ci: (flags) => cmdCi(ROOT, { force: (flags ?? []).includes("--force"), wait: (flags ?? []).includes("--wait") }),
  validate: () => cmdRunArea("validate"),
  lint: () => cmdRunArea("lint"),
  "regen-tests": () => {
    const gen = join(ROOT, "tools", "validation", "gen-test-registry.ps1");
    if (!existsSync(gen)) {
      console.log(`FAIL  generator not found: ${gen}`);
      return 1;
    }
    const res = sh(PWSH, ["-NoProfile", "-ExecutionPolicy", "Bypass", "-File", gen, "-Write"]);
    process.stdout.write(res.stdout + res.stderr);
    return res.status;
  },
  run: (n) =>
    TOOL_DIRS[n] ? cmdRunArea(n) : (console.log(`unknown area: ${n} (expected one of: ${Object.keys(TOOL_DIRS).join(", ")}`), 1),
  call: () => cmdMcpCall(rest),
  help: () => cmdHelp(),
};

const [a, b] = rest;
const HEAVY = ["build", "serve", "dev", "test", "ci"];

let code;
try {
  if (cmd === "mcp") {
    const sub = cmds[a];
    code = sub ? sub(b) : (console.log(`unknown mcp subcommand: ${a}\n`), cmdHelp());
  } else if (cmd === "wt") {
    code = cmdWt(rest);
  } else if (cmd && cmds[cmd]) {
    code = cmds[cmd](HEAVY.includes(cmd) ? rest : a);
  } else {
    if (cmd) console.log(`unknown command: ${cmd}\n`);
    code = cmdHelp();
  }
} catch (e) {
  code = Promise.resolve(1);
  if (e?.message && e.message !== "main-checkout guard") console.log(`FAIL  ${e.message}`);
}

Promise.resolve(code)
  .then((c) => process.exit(typeof c === "number" ? c : 1))
  .catch((e) => {
    console.log(e?.message && e.message !== "main-checkout guard" ? `FAIL  ${e.message}` : "");
    process.exit(1);
  });
