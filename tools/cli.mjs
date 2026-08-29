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
import { existsSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
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

function cmdHelp() {
  console.log(`Life Framework dev CLI

usage: tools\\cli <command> [args]

  status                        toolchain + MCP server state
  mcp install [name]            clone (if missing), npm ci, build
  mcp update [name]             git pull --ff-only, npm ci, build
  mcp verify [name]             boot server briefly to confirm it starts
  mcp enable <name>             enable a server in opencode.json
  mcp disable <name>            disable a server in opencode.json
  validate | lint | test        run every script in tools/{validation,lint,test}/

servers: ${Object.keys(SERVERS).join(", ")}
`);
  return 0;
}

// ------------------------------------------------------------------- main

const [cmd, a, b] = process.argv.slice(2);

const cmds = {
  status: () => cmdStatus(),
  install: (n) => cmdInstall(n),
  update: (n) => cmdUpdate(n),
  verify: (n) => cmdVerify(n),
  enable: (n) => cmdSetEnabled(n, true),
  disable: (n) => cmdSetEnabled(n, false),
  validate: () => cmdRunArea("validate"),
  lint: () => cmdRunArea("lint"),
  test: () => cmdRunArea("test"),
  help: () => cmdHelp(),
};

let code;
if (cmd === "mcp") {
  const sub = cmds[a];
  code = sub ? sub(b) : (console.log(`unknown mcp subcommand: ${a}\n`), cmdHelp());
} else if (cmd && cmds[cmd]) {
  code = cmds[cmd](a);
} else {
  if (cmd) console.log(`unknown command: ${cmd}\n`);
  code = cmdHelp();
}

Promise.resolve(code).then((c) => process.exit(c));