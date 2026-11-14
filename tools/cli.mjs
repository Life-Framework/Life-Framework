#!/usr/bin/env node
// tools/cli.mjs — unified Life Framework development CLI.
//
// Zero-dependency Node.js. Manages the MCP servers (clone / build / update /
// verify / enable / disable), runs the repo checks in tools/validation/, and
// drives the worktree build / test / ship flow.
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
//   validate [--full|--all]           Run the checks in tools/validation/ whose inputs changed since their last green run (--all: every check; --full: every check plus the Workbench compile)
//   test [--no-build] [--tier T]      Boot DebugWorld and run the LFTEST suite
//   ui-image <svg|png> <name> [--size N]  Import an illustration as .edds + imageset, headless (tools/ui-image)
//   rdb                               Refresh a stale worktree resourceDatabase.rdb (tools/lib/rdb-fresh.mjs)
//   session up [--clients N (default 1)] [--hold min] [--mission debug|playtest]|wait [--timeout s]|restart|down|status  Dedicated server + N game clients (agent-box/session.mjs)
//   shots baseline [--areas a,b] | area <name> --baseline   Every area in one session, shared; or find the freshest
//   shots area <name|x,z> | crop <shot> <x,y,w,h>   One-call screenshot rig for the area polish pass (agent-box/shots.mjs)
//   audit placement [--top N] [--area a] [--layer L]  Ranked placement audit of every DebugWorld layer entity, on the test server (agent-box/place-audit.mjs)
//   place on|against|ground|arrange|bounds|probe|verify  Place a world prop from real bounds and probe rays, then audit it (tools/place/place.mjs)
//   look <area|x,z|all> [--edit] [--close N]   Area pictures from a hidden Workbench, no session (tools/visual/look.mjs, docs/visual-testing.md)
//   logs <pattern> [flags]            Grep the newest server console.log
//   suite-stamp <tier>                Newest saved LFTEST summary for a tier
//   suite-valid <sha>                 Whether changes since <sha> still ride that summary
//   scope <name> <dir>                Write the test stub, features row, keys, and street row
//   scope --selftest                  Check those four outputs in a temp directory
//   prs status|heal                   Open ws/* PRs; heal conflicting or stuck ones (tools/prs.mjs)
//   discord relay start|stop|status   Background Discord welcome relay (tools/discord-mcp/); discord selftest
//
//   [name] is one of: enfusion-mcp, enfusion-workbench. Omitted = all.

import "./lib/quiet-gate.mjs";
import { spawn, spawnSync } from "node:child_process";
import { appendFileSync, closeSync, copyFileSync, cpSync, existsSync, fstatSync, lstatSync, mkdirSync, openSync, readdirSync, readFileSync, readSync, rmdirSync, rmSync, statSync, writeFileSync } from "node:fs";
import os from "node:os";
import { dirname, join, resolve, sep } from "node:path";
import { incrementalOff, loadState, plan, saveState, snapshotFiles } from "./lib/validate-incremental.mjs";
import { fileURLToPath } from "node:url";
import * as wt from "./wt.mjs";
import * as host from "./host/host.mjs";
import * as playtest from "./host/playtest.mjs";
import { cmdPlaytestServer } from "./release/playtest-server.mjs";
import { cmdPlaytestRelease } from "./release/release-playtest.mjs";
import * as session from "../agent-box/session.mjs";
import * as shots from "../agent-box/shots.mjs";
import { safeRemoveTree } from "./link-safety.mjs";
import { rdbFreshness, describeMissing, addonResources } from "./lib/rdb-fresh.mjs";
import { addonsDirArgs, addonDir, gprojOf, rdbAddons, resolveAddon, defaultPackOut, CORE } from "./lib/addons.mjs";
import { runPerf } from "./perf/perf-report.mjs";
import { newestSummaryLine, suiteValidCode, isSuiteSafePath, selftestSuiteStamp } from "./suite-stamp.mjs";
import { runScopeCli } from "./scope/scaffold.mjs";
import { cmdPrs } from "./prs.mjs";
import { cmdPromote } from "./promote.mjs";
import { runRelease } from "./release/release.mjs";
import { runDiscordCli } from "./discord-mcp/relay-ctl.mjs";
import { refsBody } from "./lib/release-gates.mjs";
import { listHidden, quietSpawn, quietSpawnDetached, quietSpawnSync } from "./lib/quiet-launch.mjs";
import { interactiveNeeded, interactiveSpawnDetached } from "./lib/interactive-launch.mjs";
import { localEnv } from "./lib/local-config.mjs";
import * as testbox from "./remote/testbox.mjs";
import * as pool from "./remote/pool.mjs";
import * as e2eShutdown from "./test/e2e-shutdown.mjs";
import { readReaped, resetLease } from "./lib/session-lease.mjs";
import * as measure from "./lib/session-measure.mjs";
import { trimProcs } from "./lib/ws-trim.mjs";

function mcpDirFor(root) {
  const own = join(root, "tools", "mcp");
  if (existsSync(join(own, "enfusion-mcp-bk")) || existsSync(join(own, "enfusion-workbench-mcp"))) return own;
  const shared = join(wt.mainRootOf(root), "tools", "mcp");
  return existsSync(shared) ? shared : own;
}

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..");
// tools/mcp/ is git-ignored, so a new worktree has no clones. It shares the
// main checkout's clones until it installs its own.
const MCP_DIR = mcpDirFor(ROOT);
const CONFIG_PATH = join(ROOT, "opencode.json");
const TOOL_DIRS = { validate: "validation" };

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
    windowsHide: true,
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

// ---------------------------------------------------------------- handler bundles

// Both MCP servers inject their EnforceScript NET API handlers into the same
// addon folder (Scripts/WorkbenchGame/EnfusionMCP), and Workbench compiles
// whatever is there. The two servers' bundled handler sets must stay a UNION:
// if one ships a handler the other lacks (e.g. enfusion-mcp-bk ships
// EMCP_WB_ReadProps, enfusion-workbench ships EMCP_WB_Capture), whichever
// injected last wins and the other server's NET API calls fail with
// "Failed to call not existing Net API function". These helpers reconcile the
// two bundles so no server ever overwrites a handler the other provides.
const HANDLER_SUBDIR = join("mod", "Scripts", "WorkbenchGame", "EnfusionMCP");

function handlerDirOf(server) {
  return join(MCP_DIR, server.dir, HANDLER_SUBDIR);
}

/** Copy a single .c handler from one server's bundle to the other's. */
function copyHandler(fromName, toName, file) {
  const from = join(handlerDirOf(SERVERS[fromName]), file);
  const to = join(handlerDirOf(SERVERS[toName]), file);
  mkdirSync(dirname(to), { recursive: true });
  copyFileSync(from, to);
}

/**
 * Reconcile both servers' bundled handler sets to the union. Copies any
 * handler present in one bundle but missing from the other, so a later
 * injection can never drop a Net API function the other server depends on.
 * Returns the list of copied files (empty when already in sync).
 */
function syncHandlerBundles() {
  const names = Object.keys(SERVERS);
  const copied = [];
  for (const name of names) {
    const dir = handlerDirOf(SERVERS[name]);
    if (!existsSync(dir)) continue;
    for (const f of readdirSync(dir)) {
      if (!f.endsWith(".c")) continue;
      for (const other of names) {
        if (other === name) continue;
        const otherFile = join(handlerDirOf(SERVERS[other]), f);
        if (!existsSync(otherFile)) {
          copyHandler(name, other, f);
          copied.push(`${other}: +${f}`);
        }
      }
    }
  }
  return copied;
}

// ---------------------------------------------------------------- local tool patches
//
// The `texture_recolor` MCP tool is a local addition to the upstream
// enfusion-workbench clone (tracked source: tools/mcp-patches/, never in the
// ignored clone). `cli mcp install/update` pulls the upstream repo, which would
// discard the tool — so after every clone/pull these helpers re-copy the patch
// bundle back into the clone. Mirrors syncHandlerBundles: the patch bundle is
// the source of truth, the clone is a build artifact.

const PATCH_DIR = join(ROOT, "tools", "mcp-patches");

/** Map a clone's src/ tree into the tracked patch bundle for that server. */
function patchDirOf(server) {
  return join(PATCH_DIR, server.dir, "src");
}

/** Copy every file in a server's tracked patch bundle back into the clone.
 *  Returns the list of files copied (empty when the bundle is absent). */
function syncLocalPatches() {
  const copied = [];
  for (const name of Object.keys(SERVERS)) {
    // src/ is the tool source; mod/ is the Workbench handler bundle. A handler
    // that names LF_ classes is patched here, or the clone's copy fails to
    // compile in Workbench after the EL_ to LF_ rename.
    for (const sub of ["src", "mod"]) {
      const patchSrc = join(PATCH_DIR, SERVERS[name].dir, sub);
      if (!existsSync(patchSrc)) continue;
      const cloneSub = join(MCP_DIR, SERVERS[name].dir, sub);
      if (!existsSync(cloneSub)) continue;
      for (const f of walkFiles(patchSrc)) {
        const rel = f.slice(patchSrc.length).replace(/^[\\/]+/, "");
        const dest = join(cloneSub, rel);
        mkdirSync(dirname(dest), { recursive: true });
        copyFileSync(f, dest);
        copied.push(`${name}: +${sub}/${rel}`);
      }
    }
  }
  return copied;
}

/** Recursively list files under a directory. */
function walkFiles(dir) {
  const out = [];
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    const full = join(dir, entry.name);
    if (entry.isDirectory()) out.push(...walkFiles(full));
    else out.push(full);
  }
  return out;
}

/** True when the clone carries every file in the tracked patch bundle. */
function localPatchesApplied() {
  for (const name of Object.keys(SERVERS)) {
    const patchSrc = patchDirOf(SERVERS[name]);
    if (!existsSync(patchSrc)) continue;
    const cloneSrc = join(MCP_DIR, SERVERS[name].dir, "src");
    if (!existsSync(cloneSrc)) return false;
    for (const f of walkFiles(patchSrc)) {
      const rel = f.slice(patchSrc.length).replace(/^[\\/]+/, "");
      if (!existsSync(join(cloneSrc, rel))) return false;
    }
  }
  return true;
}

/** True when both installed bundles carry the same set of .c handlers. */
function handlerBundlesInSync() {
  const setOf = (name) => {
    const dir = handlerDirOf(SERVERS[name]);
    if (!existsSync(dir)) return null;
    return new Set(readdirSync(dir).filter((f) => f.endsWith(".c")));
  };
  const a = setOf(Object.keys(SERVERS)[0]);
  const b = setOf(Object.keys(SERVERS)[1]);
  if (a === null || b === null) return true; // not both installed yet
  if (a.size !== b.size) return false;
  for (const f of a) if (!b.has(f)) return false;
  return true;
}

// ---------------------------------------------------------------- native addons

// A package with a binding.gyp (better-sqlite3 in enfusion-workbench) compiles
// a .node binary against one Node ABI. Upgrading Node leaves the old binary in
// place, and the server dies at startup with ERR_DLOPEN_FAILED "was compiled
// against a different Node.js version". Loading each binary in a child process
// with this Node finds that before a tool call does.
function nativeAddons(dir) {
  const nm = join(dir, "node_modules");
  if (!existsSync(nm)) return [];
  const names = [];
  for (const top of readdirSync(nm)) {
    if (top.startsWith(".")) continue;
    const scoped = top.startsWith("@") ? readdirSync(join(nm, top)).map((n) => `${top}/${n}`) : [top];
    for (const name of scoped) if (existsSync(join(nm, name, "binding.gyp"))) names.push(name);
  }
  return names.map((name) => {
    const release = join(nm, name, "build", "Release");
    const binaries = existsSync(release) ? readdirSync(release).filter((f) => f.endsWith(".node")) : [];
    if (binaries.length === 0) return { name, ok: false, reason: "not built (no build/Release/*.node)" };
    for (const bin of binaries) {
      const res = spawnSync(process.execPath, ["-e", `require(${JSON.stringify(join(release, bin))})`], { windowsHide: true, encoding: "utf8" });
      if (res.status !== 0) {
        const text = `${res.stderr || ""}${res.stdout || ""}`;
        const abi = text.match(/using\s+NODE_MODULE_VERSION (\d+)[\s\S]*?requires\s+NODE_MODULE_VERSION (\d+)/);
        const reason = abi
          ? `built for Node ABI ${abi[1]}, this Node (${process.version}) needs ABI ${abi[2]}`
          : (text.split(/\r?\n/).find((l) => /Error/.test(l)) || `load failed (exit ${res.status})`).trim();
        return { name, ok: false, reason };
      }
    }
    return { name, ok: true, reason: "" };
  });
}

/** Rebuild stale native addons with this Node. Returns the names still broken. */
function rebuildNativeAddons(n, dir) {
  const stale = nativeAddons(dir).filter((a) => !a.ok);
  if (stale.length === 0) return [];
  const names = stale.map((a) => a.name);
  console.log(`${n}: rebuilding native addons for ${process.version}: ${names.join(", ")}`);
  const res = sh(NPM, ["rebuild", ...names], { cwd: dir });
  if (res.status !== 0) console.log(`FAIL  npm rebuild ${n}: ${res.stderr || res.stdout}`);
  const still = nativeAddons(dir).filter((a) => !a.ok);
  for (const a of still) console.log(`FAIL  ${n}: ${a.name} ${a.reason}`);
  return still.map((a) => a.name);
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
  let nativeBroken = false;
  for (const name of Object.keys(SERVERS)) {
    const s = SERVERS[name];
    const installed = existsSync(entryOf(s));
    const commit = installed ? (gitIn(join(MCP_DIR, s.dir), ["rev-parse", "--short", "HEAD"]).stdout || "").trim() : "-";
    const enabled = cfg.mcp?.[name]?.enabled;
    console.log(
      `  ${name.padEnd(20)} installed=${installed} enabled=${String(enabled).padEnd(5)} commit=${commit}`,
    );
    console.log(`      ${s.label}`);
    if (installed) {
      for (const a of nativeAddons(join(MCP_DIR, s.dir)).filter((x) => !x.ok)) {
        nativeBroken = true;
        console.log(`      FAIL  native addon ${a.name}: ${a.reason}`);
        console.log(`            the server cannot start. FIX: tools\\cli mcp install ${name}  (runs npm rebuild)`);
      }
    }
  }
  if (!handlerBundlesInSync()) {
    console.log("");
    console.log(
      "  WARN  MCP handler bundles diverged — one server would overwrite the other's Net API",
    );
    console.log(
      "        handlers, breaking calls like EMCP_WB_ReadProps/EMCP_WB_Capture. Run 'cli mcp",
    );
    console.log("        install' (or 'update') to reconcile them to a union.");
  }
  if (!localPatchesApplied()) {
    console.log("");
    console.log(
      "  WARN  local MCP tool patches not applied to the clone (texture_recolor etc). Run",
    );
    console.log("        'cli mcp install' (or 'update') to re-copy tools/mcp-patches/.");
  }
  console.log("");
  console.log("environment (effective; OS env wins over opencode.json):");
  const env = envOf();
  for (const [k, v] of Object.entries(env)) {
    console.log(`  ${k} = ${v}`);
  }
  if (Object.keys(env).length === 0) console.log("  (defaults: standard Steam install locations)");
  console.log("");
  console.log("base game project (the engine refuses to boot without these):");
  let healthy = true;
  for (const c of baseGameIntegrity(env)) {
    const ok = existsSync(c.path);
    if (!ok) healthy = false;
    console.log(`  ${ok ? "OK  " : "FAIL"}  ${c.label}  ${c.path}`);
  }
  if (!healthy) {
    console.log("");
    console.log("  FIX  Steam > Arma Reforger (the GAME, app 1874880) > Properties > Installed Files >");
    console.log("       'Verify integrity of game files'. Verifying only 'Arma Reforger Tools' does");
    console.log("       NOT restore the game data project. Then relaunch the game once.");
  }
  return healthy && !nativeBroken ? 0 : 1;
}

// The identity files the engine uses to discover the vanilla Game addon
// (58D0FB3206B6F859) and the core addon (5614BBCCBB55ED1C). When these go
// missing (partial Steam update, moved install) both the game and Workbench
// fail with "Game addon ... not found" / "Arma Reforger base game project not
// found". cli status is the tripwire: it names the exact missing file and the
// Steam-verify step that restores it.
function baseGameIntegrity(env = envOf()) {
  const workbench = env.ENFUSION_WORKBENCH_PATH || "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger Tools";
  const game = env.ENFUSION_GAME_PATH || "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger";
  return [
    { label: "game data project:", path: join(game, "addons", "data", "ArmaReforger.gproj") },
    { label: "game core project: ", path: join(game, "addons", "core", "core.gproj") },
    { label: "workbench core:    ", path: join(workbench, "Workbench", "addons", "core", "core.gproj") },
  ];
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
    if (rebuildNativeAddons(n, dir).length) {
      failed++;
      continue;
    }
    console.log(`OK    ${n} installed at ${entryOf(s)}`);
  }
  const synced = syncHandlerBundles();
  if (synced.length) {
    console.log(`synced handler bundles to union (${synced.length}):`);
    for (const s of synced) console.log(`  ${s}`);
  }
  const patched = syncLocalPatches();
  if (patched.length) {
    console.log(`re-applied local tool patches (${patched.length}):`);
    for (const p of patched) console.log(`  ${p}`);
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
    if (rebuildNativeAddons(n, dir).length) {
      failed++;
      continue;
    }
    console.log(`OK    ${n}: ${before} -> ${after}`);
  }
  const synced = syncHandlerBundles();
  if (synced.length) {
    console.log(`synced handler bundles to union (${synced.length}):`);
    for (const s of synced) console.log(`  ${s}`);
  }
  const patched = syncLocalPatches();
  if (patched.length) {
    console.log(`re-applied local tool patches (${patched.length}):`);
    for (const p of patched) console.log(`  ${p}`);
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
      windowsHide: true,
      env: { ...process.env, ...serverEnv(name) },
      stdio: ["pipe", "pipe", "pipe"],
    });
    child.stdout?.on("data", () => {});
    let stderr = "";
    child.stderr?.on("data", (d) => { stderr += d; });
    let settled = false;
    const done = (ok, msg) => {
      if (settled) return;
      settled = true;
      if (child.exitCode === null) child.kill();
      console.log(`${ok ? "OK  " : "FAIL"}  ${name}: ${msg}`);
      resolvePromise(ok ? 0 : 1);
    };
    child.on("error", (e) => done(false, `spawn error: ${e.message}`));
    child.on("exit", (code) => {
      const why = stderr.split(/\r?\n/).find((l) => /Error|NODE_MODULE_VERSION/.test(l));
      done(code === 0, `exited unexpectedly (code ${code})${why ? `: ${why.trim()}` : ""}`);
    });
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

// Effective env for spawned tool scripts: OS env overlaid with the configured
// ENFUSION_* values so tools/validation/* and tools/test/test-e2e.ps1 agree with the CLI.
function toolEnv() {
  return { ...process.env, ...envOf() };
}

// Checks that launch Workbench (~20-40 s and a window). The default `validate`
// skips them because agents run it constantly and the test server compiles
// every script at boot anyway (test-e2e fails on `Can't compile`). They run in
// `validate --full`, `ci`, `wt gate`, and `wt ship`.
const FULL_ONLY = new Set(["validate-scripts.ps1"]);

// Checks are independent read-only scripts, so they run concurrently (most of
// their time is PowerShell start-up). Output is printed in file order.
const AREA_CONCURRENCY = Math.max(8, Math.min(12, os.cpus().length));

function invokeScriptAsync(full, opts = {}) {
  let cmd = process.execPath;
  let args = [full]; // .mjs/.js/.cjs and bare scripts
  if (full.endsWith(".ps1")) [cmd, args] = [PWSH, ["-NoProfile", "-ExecutionPolicy", "Bypass", "-File", full]];
  else if (full.endsWith(".cmd") || full.endsWith(".bat")) [cmd, args] = ["cmd", ["/c", full]];
  else if (full.endsWith(".sh")) [cmd, args] = ["bash", [full]];
  return new Promise((done) => {
    let stdout = "";
    let stderr = "";
    const child = spawn(cmd, args, { windowsHide: true, stdio: ["ignore", "pipe", "pipe"], ...opts });
    child.stdout.on("data", (d) => { stdout += d; });
    child.stderr.on("data", (d) => { stderr += d; });
    child.on("error", (e) => done({ status: 1, stdout, stderr: stderr + `failed to spawn ${cmd}: ${e.message}` }));
    child.on("close", (code) => done({ status: code ?? 1, stdout, stderr }));
  });
}

async function cmdRunArea(area, root = ROOT, opts = {}) {
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
  // LF_VALIDATE_SKIP=a.ps1,b.mjs skips checks a machine cannot run (CI has no
  // base game or MCP build). Each skip is printed so a run never hides one.
  const skip = new Set((process.env.LF_VALIDATE_SKIP || "").split(",").map((s) => s.trim()).filter(Boolean));
  const areaStarted = Date.now();
  // Incremental mode (plain `validate` and `dev` only): a check whose declared inputs are
  // unchanged since its last green run is skipped and counted. Full runs never use it.
  const isValidate = area === "validate";
  const state = isValidate ? loadState(root) : null;
  let inc = null;
  if (isValidate && opts.incremental && !opts.full && !incrementalOff()) {
    inc = plan(root, dir, scripts, state, snapshotFiles(root));
  }
  const jobs = scripts.map((s) => {
    if (skip.has(s)) return { s, skipped: "LF_VALIDATE_SKIP" };
    if (isValidate && !opts.full && FULL_ONLY.has(s)) {
      return { s, skipped: "Workbench compile: runs in 'validate --full', ci, wt gate, wt ship" };
    }
    if (inc?.get(s)?.unchanged) return { s, skipped: inc.get(s).why, incremental: true };
    return { s };
  });
  // Longest first (from the last recorded times) so one slow check never starts last.
  const lastSecs = (j) => state?.checks[j.s]?.secs ?? 0;
  const queue = jobs.filter((j) => !j.skipped).sort((a, b) => lastSecs(b) - lastSecs(a));
  const env = toolEnv();
  const worker = async () => {
    for (let job = queue.shift(); job; job = queue.shift()) {
      const started = Date.now();
      job.res = await invokeScriptAsync(join(dir, job.s), { cwd: root, env });
      job.secs = (Date.now() - started) / 1000;
    }
  };
  await Promise.all(Array.from({ length: AREA_CONCURRENCY }, worker));
  let failed = 0;
  let skippedIncr = 0;
  for (const job of jobs) {
    if (job.skipped) {
      if (job.incremental) skippedIncr++;
      else console.log(`SKIP  ${area}/${job.s} (${job.skipped})`);
      continue;
    }
    const { res } = job;
    const ok = res.status === 0;
    console.log(`${ok ? "PASS" : "FAIL"}  ${area}/${job.s} (${job.secs.toFixed(1)} s)`);
    if (res.stdout) process.stdout.write(res.stdout + (res.stdout.endsWith("\n") ? "" : "\n"));
    if (res.stderr) process.stderr.write(res.stderr + (res.stderr.endsWith("\n") ? "" : "\n"));
    if (!ok) failed++;
  }
  const ran = jobs.filter((j) => j.res);
  if (isValidate) {
    // Record a green run (fingerprint + time) only when the check passed; a full run records the
    // time and a fingerprint too, since it proves the same inputs. Failing checks are forgotten.
    const fps = plan(root, dir, scripts, state, snapshotFiles(root)); // after the run: generators rewrite their own inputs
    for (const j of ran) {
      if (j.res.status === 0) state.checks[j.s] = { fp: fps.get(j.s)?.fp ?? null, secs: j.secs, at: Date.now() };
      else delete state.checks[j.s];
    }
    saveState(root, state);
    if (ran.length > 1) {
      const slow = [...ran].sort((a, b) => b.secs - a.secs).slice(0, 3).map((j) => `${j.s} ${j.secs.toFixed(1)} s`).join(", ");
      console.log(`slowest: ${slow}`);
    }
    if (inc) console.log(`incremental: ran ${ran.length}, skipped ${skippedIncr} (inputs unchanged since their last green run; 'validate --all' runs every check)`);
  }
  console.log(`${area}: ${ran.length} check(s) run, ${failed} failed (${((Date.now() - areaStarted) / 1000).toFixed(1)} s)`);
  return failed ? 1 : 0;
}

// ----------------------------------------------------------- build / serve / test

// Effective ENFUSION_* environment: process.env wins over opencode.json, so
// contributors on non-default installs can set OS env vars instead of editing
// the (committed, portable) config.
// The gitignored lf.local.json (tools/lib/local-config.mjs) sits between the
// two: OS env > lf.local.json > opencode.json.
function envOf() {
  const cfg = readConfig().mcp?.["enfusion-mcp"]?.environment ?? {};
  const env = { ...cfg, ...localEnv(ROOT) };
  for (const k of ["ENFUSION_WORKBENCH_PATH", "ENFUSION_GAME_PATH", "ENFUSION_SERVER_PATH", "ENFUSION_PROJECT_PATH", "LF_GAME_ADDONS", ...Object.keys(env)]) {
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
  const configured = env.ENFUSION_SERVER_PATH || "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger Server";
  if (/\.exe$/i.test(configured)) return configured;
  return join(configured, "ArmaReforgerServer.exe");
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

// Workbench needs its own canonical core plus the game client's complete data
// package. Do not share this directory with dedicated-server tests: they use a
// smaller server-specific data set, and mixing the two crashes Workbench while
// it creates the scripted game.
function workbenchGameAddons(root, scope) {
  const gameDir = envOf().ENFUSION_GAME_PATH || "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger";
  const coreSource = join(dirname(workbenchExe()), "addons", "core");
  const dataSource = join(gameDir, "addons", "data");
  if (!existsSync(join(coreSource, "core.gproj"))) throw new Error(`Workbench core project not found: ${coreSource} (fix: Steam > Arma Reforger Tools > Properties > Installed Files > Verify integrity of game files)`);
  if (!existsSync(join(dataSource, "ArmaReforger.gproj"))) throw new Error(`game data project not found: ${dataSource} (fix: Steam > Arma Reforger (the GAME, app 1874880) > Properties > Installed Files > Verify integrity of game files; verifying only 'Arma Reforger Tools' does not restore this)`);

  const junction = join(root, "server", "profile", scope, "game-addons");
  removeGeneratedAddonRoot(root, junction);
  mkdirSync(junction, { recursive: true });
  createJunction(join(junction, "core"), coreSource);
  createJunction(join(junction, "data"), dataSource);
  return junction;
}

function createJunction(target, source) {
  const result = sh("cmd", ["/c", "mklink", "/J", target, source]);
  if (result.status !== 0 || !existsSync(target)) {
    throw new Error(`failed to create addon junction ${target} -> ${source}: ${result.stderr || result.stdout}`);
  }
}

// Never recursively remove a generated addon root. Its core/data children are
// junctions into Steam installs, and recursive removal can follow those links
// on Windows and delete the real base-game files.
function removeGeneratedAddonRoot(repoRoot, target) {
  const targetPath = resolve(target);
  const allowedRoot = resolve(repoRoot, "server", "profile");
  if (targetPath !== allowedRoot && !targetPath.startsWith(`${allowedRoot}${sep}`)) {
    throw new Error(`refusing to remove addon root outside repository profile: ${targetPath}`);
  }
  if (!existsSync(targetPath)) return;

  for (const entry of readdirSync(targetPath, { withFileTypes: true })) {
    if (entry.name !== "core" && entry.name !== "data") {
      throw new Error(`refusing to remove unexpected generated addon entry: ${join(targetPath, entry.name)}`);
    }
    const child = join(targetPath, entry.name);
    safeRemoveTree(child);
  }
  // The children are gone, so the root is an empty directory. rmSync without
  // recursive throws EISDIR on a directory in current Node.
  rmdirSync(targetPath);
}

function serverGameAddons(root) {
  const env = envOf();
  const configured = env.ENFUSION_SERVER_PATH || "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger Server";
  const serverDir = /\.exe$/i.test(configured) ? dirname(configured) : configured;
  const gameDir = env.ENFUSION_GAME_PATH || "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger";
  // LF_GAME_ADDONS (OS env or lf.local.json) names the core/data source outright.
  let source = env.LF_GAME_ADDONS || join(serverDir, "addons");
  if (env.LF_GAME_ADDONS) {
    if (!existsSync(join(source, "core")) || !existsSync(join(source, "data"))) throw new Error(`LF_GAME_ADDONS=${source} has no core/ and data/ folders`);
  } else if (!existsSync(join(source, "core")) || !existsSync(join(source, "data"))) {
    source = join(gameDir, "addons");
  }

  const target = join(root, "server", "profile", "serve", "game-addons");
  removeGeneratedAddonRoot(root, target);
  mkdirSync(target, { recursive: true });
  createJunction(join(target, "data"), join(source, "data"));

  const coreSource = join(source, "core");
  if (existsSync(join(coreSource, "core.gproj"))) {
    createJunction(join(target, "core"), coreSource);
  } else {
    const coreTarget = join(target, "core");
    cpSync(coreSource, coreTarget, { recursive: true });
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
  }
  return target;
}

// -addr <ip>:<port> is the only way a -server boot leaves UDP 2001 (-bindPort and
// -bindIP are ignored; observed 2026-11-10 on 1.9.0.37). A test lane's server
// listens on 127.0.0.1:2001+k so it runs beside the session server on 2001.
// Run and boot budget of the run in progress (the pool learned them from its history):
// test-e2e.ps1 kills a run past them and exits 124 with a TIMEOUT line.
let activeBudget = null;

function budgetArgs() {
  if (!activeBudget?.runS) return [];
  return ["-TimeoutSeconds", String(activeBudget.runS), ...(activeBudget.bootS ? ["-BootTimeoutSeconds", String(activeBudget.bootS)] : [])];
}

function laneListenAddr() {
  const port = pool.currentLanePort();
  return port && port !== 2001 ? `127.0.0.1:${port}` : null;
}

function serverArgs(root, ports) {
  // Addon discovery: -server loads the DebugWorld directly (no -config, which
  // conflicts with -addons). -addonsDir points at the repo addons plus a
  // junction to the game client's core/data ONLY - the server install's own
  // ./addons is skipped so stray downloaded or packed addons there never load
  // (LifeFramework.gproj depends only on the vanilla Game addon
  // 58D0FB3206B6F859). In -server mode the engine ignores -bindPort and
  // always listens on 2001 (-bindPort only overrides a -config JSON, and
  // -config cannot be combined with -addons). withServerSlot() serializes
  // every server boot on this machine instead.
  const profile = join(root, "server", "profile", "test");
  const junction = serverGameAddons(root);
  const addonsDir = [join(root, "addons"), junction].filter(Boolean).join(",");
  return [
    "-server", "Worlds/DebugWorld/DebugWorld.ent",
    "-addonsDir", addonsDir,
    "-addons", "LifeFramework",
    "-profile", profile,
    "-maxFPS", "30",
    "-logLevel", "normal",
    "-scrDefine", "LF_AUTOTEST",
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
  const addonInfo = opts.addon ? resolveAddon(opts.addon) : resolveAddon(CORE);
  if (!addonInfo) {
    console.log(`FAIL  unknown addon "${opts.addon}"; known: LifeFramework, LF_Everon1989`);
    wt.releaseLock(root, "build");
    return Promise.resolve(1);
  }
  const gproj = gprojOf(root, addonInfo.name);
  const out = join(root, "server", addonInfo.name === CORE ? "build" : `build-${addonInfo.name}`);
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
  // server/profile/build/game-addons (see workbenchGameAddons).
  addonDirs.push(workbenchGameAddons(root, "build"));
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
    ...addonsDirArgs(addonDirs),
  ];
  console.log(`building addon (headless Workbench) -> ${out}`);
  console.log(`  cwd (game dir): ${gameDir}`);
  console.log(`  addon dirs: ${addonDirs.join(", ")}`);
  console.log("  (streaming Workbench console.log live)");

  return new Promise((resolvePromise) => {
    const done = (code) => {
      wt.releaseLock(root, "build");
      resolvePromise(code);
    };
    // LF-Agents desktop: no Workbench window on the user's screen (tools/lib/quiet-launch.mjs).
    const child = quietSpawn(exe, args, { cwd: gameDir });
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

}

// Pack the addon the way Workbench > Publish Project bundles it (compressed, encrypted .pak files,
// .meta and source files left out), without uploading anything: -wbModule=ResourceManager
// -packAddon -packAddonDir (BIKI Arma_Reforger:Startup_Parameters#packAddon). The output folder
// loads like a Workshop download: -addonsDir <out-parent> -addons <GUID> (docs/playtest/workshop-host.md).
function cmdPack(root = ROOT, opts = {}) {
  guardHeavy(root, "pack", opts.force);
  const exe = workbenchExe();
  if (!existsSync(exe)) {
    console.log(`FAIL  workbench not found: ${exe}`);
    return 1;
  }
  const addonInfo = resolveAddon(opts.addon || CORE);
  if (!addonInfo) {
    console.log(`FAIL  unknown addon "${opts.addon}"; known: LifeFramework, LF_Everon1989 (name or GUID)`);
    return 1;
  }
  const out = resolve(opts.out || defaultPackOut(root, addonInfo.name));
  const addon = addonDir(root, addonInfo.name);
  if (out.toLowerCase().startsWith(addon.toLowerCase())) {
    console.log(`FAIL  pack output ${out} is inside the addon folder; Workbench would pack its own output`);
    return 1;
  }
  // A previous pack leaves its .pak files; packing over them is what Workbench does, so no delete.
  mkdirSync(out, { recursive: true });
  const gameDir = envOf().ENFUSION_GAME_PATH || "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger";
  const profile = join(root, "server", "profile", "pack");
  mkdirSync(profile, { recursive: true });
  let locked = false;
  try {
    wt.acquireLock(root, "build", { waitMs: opts.wait ? 900000 : 0 });
    locked = true;
    const args = [
      "-gproj", gprojOf(root, addonInfo.name),
      "-wbModule=ResourceManager",
      "-packAddon",
      "-packAddonDir", out,
      "-noSplash",
      "-profile", profile,
      ...addonsDirArgs([join(root, "addons"), workbenchGameAddons(root, "pack")]),
    ];
    console.log(`packing ${addonInfo.name} (headless Workbench) -> ${out}`);
    const started = Date.now();
    const res = quietSpawnSync(exe, args, { cwd: gameDir, timeoutMs: 900000 });
    const secs = ((Date.now() - started) / 1000).toFixed(0);
    const log = latestWorkbenchLog(profile);
    const lines = log ? readFileSync(log, "utf8").split(/\r?\n/) : [];
    for (const l of lines.filter((x) => /Packaging|\(E\).*(pack|bundle|cannot copy|metafile)/i.test(x)).slice(-12)) console.log(`  ${l}`);
    const paks = existsSync(out) ? readdirSync(out).filter((f) => /\.pak$/i.test(f)) : [];
    const manifest = ["addon.gproj", `${addonInfo.name}.gproj`].find((f) => existsSync(join(out, f)));
    const mb = paks.reduce((s, f) => s + statSync(join(out, f)).size, 0) / 1048576;
    // A skeleton addon with no resources packs to a project file and an rdb but no .pak.
    const empty = !paks.length && !!manifest && addonResources(addon).length === 0 && lines.some((x) => /Packaging project successful/.test(x));
    if (empty) {
      console.log(`pack OK in ${secs}s: ${addonInfo.name} declares no resources, so no .pak file; ${manifest} in ${out}`);
      return 0;
    }
    if (!paks.length || !manifest) {
      console.log(`pack FAILED after ${secs}s (Workbench exit ${res.status ?? res.error?.code ?? "?"}): ${paks.length} .pak file(s), project file ${manifest ? "present" : "missing"} in ${out}`);
      if (log) console.log(`  log: ${log}`);
      return 1;
    }
    console.log(`pack OK in ${secs}s: ${paks.length} .pak file(s), ${mb.toFixed(1)} MB, ${manifest} in ${out}`);
    return 0;
  } catch (e) {
    console.log(`FAIL  pack: ${e.message}`);
    return 1;
  } finally {
    if (locked) wt.releaseLock(root, "build");
  }
}

// Before every server boot from a worktree: copy main's resourceDatabase.rdb
// when it is the better copy, then make sure it registers every .meta in the
// addon. A resource that landed on main after anyone last opened Workbench is
// missing from every copy, and the server then refuses it (`Wrong GUID/name`,
// inventory operations that never complete). When stale, a headless Workbench
// opens the project, which registers the new metas and rewrites the file.
// The main checkout's rdb is the human's editor copy: only warn there.
// LF_RDB_REFRESH=0 turns the refresh off (warn only).
const RDB_REFRESH_TYPICAL_S = 15;
function ensureResourceDatabase(root) {
  wt.copyResourceDatabase(root);
  // The core always; another addon once it declares a resource (tools/lib/addons.mjs rdbAddons).
  let worst = 0;
  for (const name of rdbAddons(root)) worst = Math.max(worst, ensureAddonResourceDatabase(root, name));
  return worst;
}

function ensureAddonResourceDatabase(root, name) {
  const addon = addonDir(root, name);
  const tag = name === CORE ? "" : ` [${name}]`;
  const before = rdbFreshness(addon);
  if (before.missing.length === 0) return 0;
  const what = `${before.missing.length} unregistered resource(s): ${describeMissing(before.missing)}`;
  if (wt.isMainCheckout(root) || process.env.LF_RDB_REFRESH === "0") {
    console.log(`WARN  resource database stale${tag}: ${what}`);
    console.log("      open Workbench on this checkout once (it rewrites resourceDatabase.rdb), or run from a worktree");
    return 1;
  }
  const exe = workbenchExe();
  if (!existsSync(exe)) {
    console.log(`WARN  resource database stale${tag}: ${what}; Workbench not found (${exe}), cannot refresh`);
    return 1;
  }
  console.log(`resource database stale${tag}: ${what} -> refreshing (headless Workbench, takes ~${RDB_REFRESH_TYPICAL_S}s)`);
  let locked = false;
  try {
    wt.acquireLock(root, "build", { waitMs: 900000 });
    locked = true;
    const started = Date.now();
    const gameDir = envOf().ENFUSION_GAME_PATH || "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger";
    const profile = join(root, "server", "profile", "rdb");
    mkdirSync(profile, { recursive: true });
    const args = [
      "-gproj", gprojOf(root, name),
      "-wbModule=ResourceManager",
      "-noSplash",
      "-run",
      "-exitAfterInit",
      "-profile", profile,
      ...addonsDirArgs([join(root, "addons"), workbenchGameAddons(root, "rdb")]),
    ];
    const res = quietSpawnSync(exe, args, { cwd: gameDir, timeoutMs: 600000 });
    const after = rdbFreshness(addon);
    const secs = ((Date.now() - started) / 1000).toFixed(0);
    if (after.missing.length === 0) {
      console.log(`resource database${tag} refreshed in ${secs}s (${after.total} resources registered)`);
      return 0;
    }
    console.log(`WARN  resource database still stale${tag} after ${secs}s (Workbench exit ${res.status ?? res.error?.code ?? "?"}): ${after.missing.length} unregistered: ${describeMissing(after.missing)}`);
    return 1;
  } catch (e) {
    console.log(`WARN  resource database refresh failed: ${e.message}`);
    return 1;
  } finally {
    if (locked) wt.releaseLock(root, "build");
  }
}


// One headless Workbench run of a ResourceManager plugin under the build lock, bounded by
// timeoutMs (a hung Workbench is killed with its job). Used by tools\cli asset (stage import).
// `addon` (name, default the core) picks the project Workbench opens: the pack's project loads the
// core as its dependency, so a pack asset's .meta and .xob resolve beside it (#971).
function runWorkbenchPlugin(root, plugin, pluginArgs, { timeoutMs, logFile, addon = CORE }) {
  const addonInfo = resolveAddon(addon);
  if (!addonInfo) throw new Error(`unknown addon "${addon}"; known: LifeFramework, LF_Everon1989`);
  const exe = workbenchExe();
  if (!existsSync(exe)) throw new Error("Workbench not found: " + exe);
  const gameDir = envOf().ENFUSION_GAME_PATH || "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger";
  const profile = join(root, "server", "profile", "rdb");
  mkdirSync(profile, { recursive: true });
  wt.acquireLock(root, "build", { waitMs: 900000 });
  try {
    const res = quietSpawnSync(exe, [
      "-gproj", gprojOf(root, addonInfo.name),
      "-wbModule=ResourceManager", "-noSplash", "-run", "-exitAfterInit",
      "-plugin=" + plugin, ...pluginArgs,
      "-profile", profile,
      ...addonsDirArgs([join(root, "addons"), workbenchGameAddons(root, "rdb")]),
    ], { cwd: gameDir, timeoutMs, stdout: logFile, stderr: logFile });
    return { status: res.status, profile };
  } finally {
    wt.releaseLock(root, "build");
  }
}

// tools/cli ui-image <svg|png> <name>: see tools/ui-image/ui-image.mjs and
// docs/research/ui-illustration-pipeline.md.
async function cmdUiImage(flags) {
  const root = ROOT;
  const pos = (flags ?? []).filter((f, i, a) => !f.startsWith("--") && a[i - 1] !== "--size");
  const [src, name] = pos;
  const sizeIdx = (flags ?? []).indexOf("--size");
  const size = sizeIdx >= 0 ? Number(flags[sizeIdx + 1]) : 0;
  if (!src || !name || !/^[A-Za-z0-9_]+$/.test(name)) {
    console.log("usage: tools\cli ui-image <file.svg|file.png> <name> [--size N] [--no-imageset]");
    return 2;
  }
  if (wt.isMainCheckout(root)) { console.log("FAIL  ui-image runs only in a worktree"); return 1; }
  const ui = await import("./ui-image/ui-image.mjs");
  const png = await ui.writeSource(root, resolve(src), name, size);
  const dir = join(root, "addons", "LifeFramework", ui.ILLUSTRATION_DIR);
  const edds = join(dir, name + ".edds");
  const exe = workbenchExe();
  if (!existsSync(exe)) { console.log("FAIL  Workbench not found: " + exe); return 1; }
  let locked = false;
  try {
    wt.acquireLock(root, "build", { waitMs: 900000 });
    locked = true;
    const gameDir = envOf().ENFUSION_GAME_PATH || "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger";
    const profile = join(root, "server", "profile", "rdb");
    mkdirSync(profile, { recursive: true });
    const logFile = join(root, "tmp", "ui-image-" + name + ".log");
    mkdirSync(join(root, "tmp"), { recursive: true });
    const started = Date.now();
    if (!existsSync(edds + ".meta")) ui.writeTextureMeta(edds + ".meta", name);
    ui.installPlugin(root);
    quietSpawnSync(exe, [
      "-gproj", join(root, "addons", "LifeFramework", "LifeFramework.gproj"),
      "-wbModule=ResourceManager", "-noSplash", "-run", "-exitAfterInit",
      "-plugin=LF_ImageImportPlugin", "-lfImage=" + png,
      "-profile", profile,
      ...addonsDirArgs([join(root, "addons"), workbenchGameAddons(root, "rdb")]),
    ], { cwd: gameDir, timeoutMs: Number(process.env.LF_UI_IMAGE_TIMEOUT_S || 150) * 1000, stdout: logFile, stderr: logFile });
    const secs = ((Date.now() - started) / 1000).toFixed(0);
    const meta = edds + ".meta";
    if (!existsSync(edds) || !existsSync(meta)) {
      console.log("FAIL  import produced no .edds/.meta after " + secs + "s; the Workbench was killed if it hung (see " + logFile + ")");
      return 1;
    }
    const guid = ui.readMetaGuid(meta);
    const rel = ui.ILLUSTRATION_DIR + "/" + name;
    console.log("imported in " + secs + "s: {" + guid + "}" + rel + ".edds");
    if (!(flags ?? []).includes("--no-imageset")) {
      const { w, h } = await ui.imageSize(root, png);
      const ig = ui.writeImageset(root, name, guid, w, h);
      console.log("imageset: {" + ig + "}" + rel + ".imageset  (image name: full)");
    }
    return 0;
  } finally {
    if (locked) wt.releaseLock(root, "build");
  }
}

// Every -server boot listens on 0.0.0.0:2001 whatever -bindPort says
// (observed 2026-11-08 on 1.9.0.37). Two boots at once fail with "Unable to
// start replication", so one machine-wide lock covers every server boot.
const SERVER_SLOT = "server-2001";

function withServerSlot(root, waitMs, fn) {
  // The pool already holds the slot: this process took it (pool.acquireSlot on
  // this machine), or a pool holder owns it for this remote run (LF_SERVER_SLOT_HELD).
  if (process.env[pool.SLOT_ENV]) {
    try {
      pool.heldSlotFromEnv(root);
    } catch (e) {
      console.log(`FAIL  ${e.message}`);
      return 1;
    }
    return fn();
  }
  if (pool.heldLocalSlot() || testbox.serverSlotOwner(root)?.pid === process.pid) return fn();
  try {
    wt.acquireLock(root, SERVER_SLOT, { waitMs });
  } catch (e) {
    console.log(`FAIL  ${e.message}`);
    console.log("      another worktree's server holds port 2001; it frees when that run ends");
    return 1;
  }
  try {
    return fn();
  } finally {
    wt.releaseLock(root, SERVER_SLOT);
  }
}

function cmdServe(root = ROOT, opts = {}) {
  guardHeavy(root, "serve", opts.force);
  const exe = serverExe();
  if (!existsSync(exe)) {
    console.log(`FAIL  dedicated server not found: ${exe}`);
    console.log("      install Arma Reforger Server (Steam app 1874900) or set ENFUSION_SERVER_PATH");
    return 1;
  }
  ensureResourceDatabase(root);
  const args = serverArgs(root, wt.portsForRoot(root));
  try {
    wt.acquireLock(root, SERVER_SLOT, { waitMs: 0 });
  } catch (e) {
    console.log(`FAIL  ${e.message}`);
    return 1;
  }
  console.log(`launching: ${exe}`);
  console.log(`  ${args.join(" ")}`);
  return new Promise((resolvePromise) => {
    const done = (code) => {
      wt.releaseLock(root, SERVER_SLOT);
      resolvePromise(code);
    };
    const child = spawn(exe, args, { windowsHide: true, cwd: dirname(exe), stdio: "inherit" });
    child.on("error", (e) => done((console.log(`FAIL  spawn error: ${e.message}`), 1)));
    child.on("exit", (code) => done(code ?? 1));
  });
}

// ----------------------------------------------------------------- playtest host
// `host start|stop|status|backup`: the playtest dedicated server on
// Missions/LF_PlaytestTown.conf with a stable profile outside every worktree
// (tools/host/host.mjs). The server runs detached and owns the server-2001
// lock while it lives; `host stop` asks for a graceful close so the engine
// writes its SHUTDOWN save, then backs the saves up.

function sleepMs(ms) {
  Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
}

function hostPrintLines(log, from = 0, limit = 40) {
  if (!log || !existsSync(log)) return;
  const lines = host.hostLines(readFileSync(log, "utf8")).slice(from);
  for (const l of lines.slice(-limit)) console.log(`  ${l.trim()}`);
}

function hostJoinInfo() {
  const lan = host.lanAddresses();
  console.log(`join: direct connect to <this PC's address>:${host.GAME_PORT} (UDP)`);
  for (const a of lan) console.log(`  LAN  ${a}:${host.GAME_PORT}`);
  console.log(`  internet: your public IP:${host.GAME_PORT}, with UDP ${host.GAME_PORT} forwarded to this PC (docs/playtest.md)`);
}

function cmdHostStatus(dir) {
  const p = host.hostPaths(dir);
  const s = host.readHostState(dir);
  const alive = !!s && host.pidAlive(s.pid);
  console.log(`host: ${alive ? `running (pid ${s.pid})` : "stopped"}`);
  console.log(`  host dir: ${dir}`);
  if (s) {
    console.log(`  addon:    ${s.root}`);
    console.log(`  mission:  ${s.mission}`);
    console.log(`  started:  ${s.startedAt}${s.loadSave ? " (continued the latest save)" : " (fresh: no save existed)"}`);
  }
  const saves = host.findSavePoints(p.profile);
  console.log(`  saves:    ${saves.length ? `${saves.length} save point(s), newest ${saves[0].dir}` : "none yet"}`);
  if (saves.length) {
    const gate = host.buildGate({ saveBuild: host.newestSaveBuild(p.profile), gameBuild: host.exeBuild(serverExe()) });
    console.log(`  build:    ${gate.reason}${gate.changed ? " -> run 'tools\\cli host preflight' before opening" : ""}`);
  }
  if (s?.preflight) console.log(`  preflight: ${s.preflight.ok ? "PASS" : "FAIL"} ${s.preflight.saveBuild ?? "?"} -> ${s.preflight.gameBuild ?? "?"} at ${s.preflight.at}`);
  const backups = existsSync(p.backups) ? readdirSync(p.backups).sort() : [];
  console.log(`  backups:  ${backups.length ? `${backups.length}, newest ${join(p.backups, backups[backups.length - 1])}` : "none yet"}`);
  const log = host.newestConsoleLog(p.profile);
  if (log) console.log(`  log:      ${log}`);
  if (alive) hostJoinInfo();
  return 0;
}

function cmdHostStart(root, dir, opts) {
  guardHeavy(root, "host start", opts.force);
  const p = host.hostPaths(dir);
  const s = host.readHostState(dir);
  if (s && host.pidAlive(s.pid)) {
    console.log(`host: already running (pid ${s.pid}); 'tools\\cli host stop' first`);
    return 1;
  }
  const exe = serverExe();
  if (!existsSync(exe)) {
    console.log(`FAIL  dedicated server not found: ${exe}`);
    return 1;
  }
  // #980: the first boot on a new game build goes through `host preflight`.
  if (!opts.preflight && host.findSavePoints(p.profile).length) {
    const gate = host.buildGate({ saveBuild: host.newestSaveBuild(p.profile), gameBuild: host.exeBuild(exe) });
    if (gate.changed && !opts.force) {
      console.log(`FAIL  game update: ${gate.reason}`);
      console.log("      run 'tools\\cli host preflight' first (backs the save up, proves it loads on this build, restores it if not)");
      console.log("      --force starts anyway (docs/ops/game-updates.md)");
      return 1;
    }
    if (gate.changed) console.log(`WARN  game update: ${gate.reason}; started with --force, no preflight`);
  }
  try {
    wt.acquireLock(root, SERVER_SLOT, { waitMs: opts.wait ? 1800000 : 0 });
  } catch (e) {
    console.log(`FAIL  ${e.message}`);
    console.log("      a test or server run holds port 2001; retry when it ends, or pass --wait");
    return 1;
  }
  let pid = 0;
  try {
    ensureResourceDatabase(root);
    const junction = serverGameAddons(root);
    mkdirSync(p.profile, { recursive: true });
    const saves = host.findSavePoints(p.profile);
    const loadSave = saves.length > 0;
    if (loadSave) {
      const b = host.backupSaves(dir, "start");
      console.log(`host: backed up saves -> ${b}`);
    }
    const args = host.hostArgs({ addonsDir: [join(root, "addons"), junction].join(","), profile: p.profile, loadSave });
    console.log(`host: ${loadSave ? `continuing the latest save (${saves.length} save point(s))` : "no save yet, starting a fresh session"}`);
    console.log(`launching: ${exe}`);
    console.log(`  ${args.join(" ")}`);
    const before = new Set(existsSync(join(p.profile, "logs")) ? readdirSync(join(p.profile, "logs")) : []);
    const child = spawn(exe, args, { cwd: dirname(exe), detached: true, stdio: "ignore", windowsHide: true });
    pid = child.pid;
    child.unref();
    if (!pid) throw new Error("server did not start");
    wt.transferLock(root, SERVER_SLOT, pid);
    host.writeHostState(dir, { pid, root, mission: host.PLAYTEST_MISSION, loadSave, startedAt: new Date().toISOString(), exe });

    // Wait for the world: "Entered online game state." in the new console.log.
    const deadline = Date.now() + (opts.timeoutSec ?? 300) * 1000;
    let log = null;
    while (Date.now() < deadline) {
      sleepMs(2000);
      if (!host.pidAlive(pid)) break;
      const f = host.newestConsoleLog(p.profile);
      if (f && !before.has(basenameOfLogDir(f))) log = f;
      if (log && readFileSync(log, "utf8").includes(host.READY_LINE)) break;
    }
    if (!host.pidAlive(pid)) {
      console.log("FAIL  the server exited during startup");
      hostPrintLines(log);
      return 1;
    }
    const ready = log && readFileSync(log, "utf8").includes(host.READY_LINE);
    if (!ready) {
      console.log(`WARN  no '${host.READY_LINE}' within the timeout; the server is still running (pid ${pid})`);
      if (log) console.log(`      log: ${log}`);
      return 1;
    }
    sleepMs(3000); // the session-start check logs just after the game state.
    console.log(`host: up (pid ${pid}); log ${log}`);
    hostPrintLines(log);
    hostJoinInfo();
    console.log("stop with: tools\\cli host stop");
    return 0;
  } catch (e) {
    console.log(`FAIL  ${e.message}`);
    return 1;
  } finally {
    // Before the transfer this process owns the lock; after it the server
    // does, and releaseLock leaves a lock it does not own alone.
    wt.releaseLock(root, SERVER_SLOT);
  }
}

function basenameOfLogDir(consoleLog) {
  return dirname(consoleLog).split(/[\\/]/).pop();
}

function cmdHostStop(dir, opts) {
  const p = host.hostPaths(dir);
  const s = host.readHostState(dir);
  if (!s || !host.pidAlive(s.pid)) {
    console.log("host: not running");
    if (s) wt.releaseLock(ROOT, SERVER_SLOT, s.pid);
    return 0;
  }
  const log = host.newestConsoleLog(p.profile);
  const seen = log ? host.hostLines(readFileSync(log, "utf8")).length : 0;
  const stopFile = join(p.profile, ...host.STOP_FILE);
  mkdirSync(dirname(stopFile), { recursive: true });
  writeFileSync(stopFile, new Date().toISOString());
  console.log(`host: stop requested (pid ${s.pid}); waiting for the shutdown save`);
  const deadline = Date.now() + (opts.timeoutSec ?? 180) * 1000;
  while (Date.now() < deadline && host.pidAlive(s.pid)) sleepMs(1000);
  let code = 0;
  if (host.pidAlive(s.pid)) {
    console.log(`FAIL  the server did not close within ${opts.timeoutSec ?? 180}s; force-killing pid ${s.pid} (no shutdown save)`);
    sh("taskkill", ["/PID", String(s.pid), "/F"]);
    code = 1;
  }
  hostPrintLines(log, seen);
  wt.releaseLock(ROOT, SERVER_SLOT, s.pid);
  const b = host.backupSaves(dir, "stop");
  console.log(b ? `host: backed up saves -> ${b}` : "host: no save to back up");
  return code;
}

// `host preflight` (#980): after a game update, before opening the server.
// Back the save up, boot the host on the installed build with the save, wait
// for the LF store's load line, stop it (shutdown save) and judge the log. On a
// failure the backup goes back in place and the replaced saves are kept.
function cmdHostPreflight(root, dir, opts) {
  const p = host.hostPaths(dir);
  const s = host.readHostState(dir);
  if (s && host.pidAlive(s.pid)) {
    console.log(`host: running (pid ${s.pid}); 'tools\\cli host stop' first`);
    return 1;
  }
  if (!host.findSavePoints(p.profile).length) {
    console.log("preflight: no save yet; nothing to prove. 'tools\\cli host start' starts a fresh world");
    return 0;
  }
  const saveBuild = host.newestSaveBuild(p.profile);
  const gameBuild = host.exeBuild(serverExe());
  const gate = host.buildGate({ saveBuild, gameBuild });
  console.log(`preflight: save build ${saveBuild ?? "unknown"}, installed game ${gameBuild ?? "unknown"} (${gate.reason})`);
  const backup = host.backupSaves(dir, `preflight-${saveBuild ?? "unknown"}-to-${gameBuild ?? "unknown"}`);
  console.log(`preflight: backed up saves -> ${backup}`);
  const started = cmdHostStart(root, dir, { ...opts, preflight: true });
  const log = host.newestConsoleLog(p.profile);
  let stopped = 1;
  if (started === 0) stopped = cmdHostStop(dir, opts);
  else {
    const st = host.readHostState(dir);
    if (st && host.pidAlive(st.pid)) stopped = cmdHostStop(dir, opts);
  }
  const verdict = started === 0 && stopped === 0 ? host.preflightVerdict(log ? readFileSync(log, "utf8") : "") : { ok: false, reason: started !== 0 ? "the server did not come up" : "the server did not stop cleanly" };
  const st = host.readHostState(dir) ?? {};
  host.writeHostState(dir, { ...st, preflight: { ok: verdict.ok, saveBuild, gameBuild, backup, log, at: new Date().toISOString(), reason: verdict.reason } });
  if (verdict.ok) {
    console.log(`preflight: PASS ${verdict.reason}`);
    console.log("preflight: the save loads on this build; open the server with 'tools\\cli host start'");
    return 0;
  }
  console.log(`preflight: FAIL ${verdict.reason}`);
  try {
    const r = host.restoreBackup(dir, backup);
    console.log(`preflight: restored the backup -> ${r.target}`);
    if (r.kept) console.log(`preflight: the saves the failed boot left are kept in ${r.kept}`);
  } catch (e) {
    console.log(`preflight: restore FAILED (${e.message}); the backup is untouched at ${backup}`);
  }
  console.log(`preflight: log ${log ?? "(none)"}`);
  console.log("preflight: do not open the server. Diagnose with -persistenceDiag logs (docs/ops/game-updates.md)");
  return 1;
}

// `host restore [<backup>]`: put the newest (or the named) backup back.
function cmdHostRestore(dir, name) {
  const s = host.readHostState(dir);
  if (s && host.pidAlive(s.pid)) {
    console.log(`host: running (pid ${s.pid}); 'tools\\cli host stop' first`);
    return 1;
  }
  const backup = host.pickBackup(dir, name);
  if (!backup) {
    console.log(name ? `host: no backup named ${name}` : "host: no backup yet");
    return 1;
  }
  try {
    const r = host.restoreBackup(dir, backup);
    console.log(`host: restored ${backup} -> ${r.target}`);
    if (r.kept) console.log(`host: the replaced saves are kept in ${r.kept}`);
    return 0;
  } catch (e) {
    console.log(`FAIL  ${e.message}`);
    return 1;
  }
}

function cmdHost(args) {
  const list = args ?? [];
  const sub = list[0];
  const dir = host.hostDir();
  const timeout = argValue(list, "--timeout");
  const opts = { force: list.includes("--force"), wait: list.includes("--wait"), timeoutSec: timeout ? Number(timeout) : undefined };
  if (sub === "start") return cmdHostStart(ROOT, dir, opts);
  if (sub === "stop") return cmdHostStop(dir, opts);
  if (sub === "status") return cmdHostStatus(dir);
  if (sub === "preflight") return cmdHostPreflight(ROOT, dir, opts);
  if (sub === "restore") return cmdHostRestore(dir, list[1] && !list[1].startsWith("--") ? list[1] : null);
  if (sub === "backup") {
    const b = host.backupSaves(dir, "manual");
    console.log(b ? `host: backed up saves -> ${b}` : "host: no save to back up");
    return 0;
  }
  console.log("usage: tools\\cli host start|stop|status|backup|preflight|restore [<backup>] [--wait] [--timeout <s>] [--force]");
  return sub ? 1 : 0;
}

// ----------------------------------------------------------------- playtest
// `playtest [start]|stop|status|shortcut`: the maintainer's one command
// (tools/host/playtest.mjs). Start reuses a running host or boots one from the
// pinned origin/main checkout, then opens the real game client on this
// desktop, joined to it. Stop closes the client, then `host stop` (shutdown save).

// Clone or fast-forward <parent>\Life-Framework-playtest to origin/main.
// Returns the checkout root, or null when it cannot be made.
function playtestCheckout(mainRoot) {
  const pt = playtest.playtestRoot(mainRoot);
  const fetch = wt.git(mainRoot, ["fetch", "--quiet", "origin", "main"]);
  if (fetch.status !== 0) console.log(`WARN  git fetch failed (${fetch.stderr.trim()}); using the last fetched main`);
  if (!existsSync(join(pt, ".git"))) {
    console.log(`playtest: creating the playtest checkout ${pt} (origin/main, detached)`);
    const add = wt.git(mainRoot, ["worktree", "add", "--detach", pt, "origin/main"]);
    if (add.status !== 0) {
      console.log(`FAIL  git worktree add: ${add.stderr.trim()}`);
      return null;
    }
    return pt;
  }
  const dirty = wt.git(pt, ["status", "--porcelain", "--untracked-files=no"]).stdout.trim();
  if (dirty) {
    console.log(`WARN  ${pt} has local edits; playing it as it is (it should only ever track origin/main):`);
    for (const l of dirty.split(/\r?\n/).slice(0, 5)) console.log(`        ${l}`);
    return pt;
  }
  const ff = wt.git(pt, ["merge", "--ff-only", "--quiet", "origin/main"]);
  if (ff.status !== 0) console.log(`WARN  ${pt} cannot fast-forward to origin/main (${ff.stderr.trim()}); playing it as it is`);
  return pt;
}

function playtestServerSlotHolder(mainRoot) {
  try {
    const info = JSON.parse(readFileSync(join(wt.hubDirOf(mainRoot), "locks", `${SERVER_SLOT}.lock`), "utf8"));
    return info.pid && host.pidAlive(info.pid) ? info : null;
  } catch {
    return null;
  }
}

function focusWindowOf(pid) {
  sh(PWSH, ["-NoProfile", "-Command", `$null = (New-Object -ComObject WScript.Shell).AppActivate(${Number(pid)})`]);
}

function cmdPlaytestStart(mainRoot, hostDir, playerDir) {
  let s = host.readHostState(hostDir);
  let root;
  if (s && host.pidAlive(s.pid)) {
    root = s.root;
    console.log(`playtest: the host is already running (pid ${s.pid}, ${root}); joining it`);
  } else {
    root = playtestCheckout(mainRoot);
    if (!root) return 1;
    const head = wt.git(root, ["log", "-1", "--format=%h %s"]).stdout.trim();
    console.log(`playtest: build ${head}`);
    const holder = playtestServerSlotHolder(mainRoot);
    if (holder) {
      console.log(`playtest: an agent's test or session is using the game server right now (pid ${holder.pid}, since ${holder.at}).`);
      console.log("          Waiting for it to finish (up to 30 minutes). Once you are in, agent tests move to the test box.");
    }
    const res = spawnSync(process.execPath, [join(root, "tools", "cli.mjs"), "host", "start", "--wait"], { cwd: root, stdio: "inherit" });
    s = host.readHostState(hostDir);
    if (res.status !== 0 || !s || !host.pidAlive(s.pid)) {
      console.log("FAIL  the playtest server did not come up (see above); nothing else was started");
      return 1;
    }
  }

  const pp = playtest.playerPaths(playerDir);
  const prev = playtest.readClientState(playerDir);
  if (prev && host.pidAlive(prev.pid)) {
    console.log(`playtest: your game is already open (pid ${prev.pid})`);
    focusWindowOf(prev.pid);
    return 0;
  }
  const game = gameClientExe();
  if (!existsSync(game)) {
    console.log(`FAIL  game not found: ${game} (set ENFUSION_GAME_PATH)`);
    return 1;
  }
  const hostLog = host.newestConsoleLog(host.hostPaths(hostDir).profile);
  const joinsBefore = hostLog ? playtest.countJoins(readFileSync(hostLog, "utf8")) : 0;
  mkdirSync(pp.profile, { recursive: true });
  const args = playtest.playerClientArgs({ addonsDir: join(s.root, "addons"), profile: pp.profile });
  console.log(`playtest: opening the game: ${game}\n  ${args.join(" ")}`);
  const child = spawn(game, args, { cwd: dirname(game), detached: true, stdio: "ignore" });
  child.unref();
  if (!child.pid) {
    console.log("FAIL  the game did not start");
    return 1;
  }
  playtest.writeClientState(playerDir, { pid: child.pid, root: s.root, startedAt: new Date().toISOString() });

  // Wait for the server to let this player in, then bring the window forward.
  console.log("playtest: the game is loading and joins the server by itself (about a minute)");
  const deadline = Date.now() + 300000;
  let joined = false;
  while (Date.now() < deadline && host.pidAlive(child.pid)) {
    sleepMs(3000);
    const text = hostLog && existsSync(hostLog) ? readFileSync(hostLog, "utf8") : "";
    if (playtest.countJoins(text) > joinsBefore) {
      joined = true;
      break;
    }
  }
  if (!host.pidAlive(child.pid)) {
    console.log(`FAIL  the game closed before it joined (client logs: ${join(pp.profile, "logs")})`);
    return 1;
  }
  focusWindowOf(child.pid);
  if (joined) console.log("playtest: you are in. Register at the form, then play. Stop with: tools\\cli playtest stop");
  else console.log(`WARN  no join seen in the server log within 5 minutes; the game is still open (host log ${hostLog})`);
  const commit = wt.git(s.root, ["rev-parse", "HEAD"]).stdout.trim();
  console.log("");
  for (const l of playtest.friendLines({ lan: host.lanAddresses(), port: host.GAME_PORT, commit })) console.log(l);
  return joined ? 0 : 1;
}

function cmdPlaytestStop(hostDir, playerDir) {
  const c = playtest.readClientState(playerDir);
  if (c && host.pidAlive(c.pid)) {
    console.log(`playtest: closing your game (pid ${c.pid})`);
    sh("taskkill", ["/PID", String(c.pid)]);
    for (let i = 0; i < 20 && host.pidAlive(c.pid); i++) sleepMs(1000);
    if (host.pidAlive(c.pid)) killTree(c.pid);
    // Give the server a moment to handle the disconnect before the shutdown save.
    sleepMs(5000);
  } else {
    console.log("playtest: your game is not open");
  }
  if (c) playtest.writeClientState(playerDir, { ...c, stoppedAt: new Date().toISOString() });
  return cmdHostStop(hostDir, {});
}

function cmdPlaytestStatus(hostDir, playerDir) {
  const c = playtest.readClientState(playerDir);
  const open = !!c && host.pidAlive(c.pid);
  console.log(`game: ${open ? `open (pid ${c.pid}, since ${c.startedAt})` : "closed"}`);
  console.log(`  profile:  ${playtest.playerPaths(playerDir).profile}`);
  return cmdHostStatus(hostDir);
}

// A desktop shortcut to the repo-root launcher.
function cmdPlaytestShortcut(mainRoot) {
  const target = join(mainRoot, "Play Life Framework.cmd");
  const ps = [
    "$s = New-Object -ComObject WScript.Shell",
    "$l = $s.CreateShortcut((Join-Path ([Environment]::GetFolderPath('Desktop')) 'Play Life Framework.lnk'))",
    `$l.TargetPath = '${target.replaceAll("'", "''")}'`,
    `$l.WorkingDirectory = '${mainRoot.replaceAll("'", "''")}'`,
    "$l.Save()",
    "$l.FullName",
  ].join("; ");
  const res = sh(PWSH, ["-NoProfile", "-Command", ps]);
  if (res.status !== 0) {
    console.log(`FAIL  ${res.stderr.trim()}`);
    return 1;
  }
  console.log(`playtest: shortcut ${res.stdout.trim()} -> ${target}`);
  return 0;
}

function cmdPlaytest(args) {
  const list = args ?? [];
  const sub = list[0] ?? "start";
  const mainRoot = wt.mainRootOf(ROOT);
  const hostDir = host.hostDir();
  const playerDir = playtest.playerDir();
  if (sub === "start") return cmdPlaytestStart(mainRoot, hostDir, playerDir);
  if (sub === "stop") return cmdPlaytestStop(hostDir, playerDir);
  if (sub === "status") return cmdPlaytestStatus(hostDir, playerDir);
  if (sub === "shortcut") return cmdPlaytestShortcut(mainRoot);
  if (sub === "server") return cmdPlaytestServer(list.slice(1));
  if (sub === "release") return cmdPlaytestRelease(ROOT, list.slice(1), { ensureRdb: ensureResourceDatabase, isMain: wt.isMainCheckout(ROOT) });
  console.log("usage: tools\\cli playtest [start]|stop|status|shortcut|server <address>|off|release [--tag T]");
  return 1;
}

// --only names: a test class (LF_Test_X), a GetName() string, or a test file
// name (LF_Test_Money or LF_Test_Money.c), which expands to every test class
// declared in that file. Names the suite cannot match fail the run there.
function resolveOnlyNames(root, spec) {
  const testsDir = join(root, "addons", "LifeFramework", "Scripts", "Game", "Tests");
  const names = [];
  for (const raw of String(spec).split(",").map((x) => x.trim()).filter(Boolean)) {
    const file = join(testsDir, raw.endsWith(".c") ? raw : `${raw}.c`);
    if (/^(EL|LF)_Test_\w+(\.c)?$/.test(raw) && existsSync(file)) {
      const classes = [...readFileSync(file, "utf8").matchAll(/class\s+(LF_Test_\w+)\s*:\s*LF_Test\b/g)].map((m) => m[1]);
      if (classes.length) {
        names.push(...classes);
        continue;
      }
    }
    names.push(raw.replace(/\.c$/, ""));
  }
  return [...new Set(names)];
}

// ------------------------------------------------------------ multi-client session
// `session up|down|status` (agent-box/session.mjs): one dedicated server on the
// bare DebugWorld plus N windowed game clients, each with its own -profile,
// joined over 127.0.0.1:2001. The server owns the server-2001 lock while it
// lives; `down` kills every process the session started and frees the lock.

function gameClientExe() {
  const gameDir = envOf().ENFUSION_GAME_PATH || "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger";
  return join(gameDir, "ArmaReforgerSteam.exe");
}

function killTree(pid) {
  if (!pid || !host.pidAlive(pid)) return false;
  sh("taskkill", ["/PID", String(pid), "/T", "/F"]);
  return true;
}

// Under ssh (a session on a remote pool machine) every child dies with the ssh
// session (observed 2026-11-09: a detached spawn was gone seconds after the
// connection closed). A process WMI creates is not in that session's job, so
// the session server outlives the command that started it.
function spawnOutsideSsh(exe, args, cwd) {
  const q = (a) => (/[\s"]/.test(a) ? `"${a.replace(/"/g, '\\"')}"` : a);
  const cmdline = [exe, ...args].map(q).join(" ");
  const ps = `$r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = '${cmdline.replace(/'/g, "''")}'; CurrentDirectory = '${cwd.replace(/'/g, "''")}' }; Write-Output "$($r.ReturnValue) $($r.ProcessId)"`;
  const r = sh(PWSH, ["-NoProfile", "-NonInteractive", "-Command", ps]);
  const m = /^(\d+) (\d+)/m.exec(r.stdout ?? "");
  if (!m || m[1] !== "0") {
    console.log(`FAIL  Win32_Process.Create: ${(r.stdout + r.stderr).trim()}`);
    return { pid: 0 };
  }
  return { pid: Number(m[2]) };
}

// The lane a session runs on: recorded in session.json (a session of before the lanes ran on server-2001).
function laneLockOf(s) {
  return s?.lane?.lock ?? SERVER_SLOT;
}

// Verbose session lines go to <session dir>/up.log; the console gets the verdict.
let sessionLog = null;
function slog(msg) {
  if (sessionLog) {
    try {
      appendFileSync(sessionLog, `${new Date().toISOString()} ${msg}
`);
      return;
    } catch {}
  }
  console.log(msg);
}

function cmdSessionDown(dir) {
  const s = session.readSession(dir);
  if (!s) {
    console.log("session: none recorded");
    return 0;
  }
  for (const c of s.clients ?? []) {
    console.log(`session: client ${c.n} pid ${c.pid} ${killTree(c.pid) ? "closed" : "already gone"}`);
  }
  console.log(`session: server pid ${s.serverPid} ${killTree(s.serverPid) ? "closed" : "already gone"}`);
  for (let i = 0; i < 20 && host.pidAlive(s.serverPid); i++) sleepMs(500);
  wt.releaseLock(ROOT, laneLockOf(s), s.serverPid);
  const stoppedAt = new Date().toISOString();
  session.writeSession(dir, { ...s, stoppedAt });
  recordSessionEnd(dir, s, stoppedAt);
  return 0;
}

// An owner's `session down` writes the session's line in runs.jsonl. A close by the pool (idle or
// hold cap) was recorded by the closer, which left reaped.json just before running this.
function recordSessionEnd(dir, s, stoppedAt) {
  try {
    if (s.stoppedAt || !s.heldSince) return;
    const reaped = readReaped(dir);
    if (reaped && Date.now() - Date.parse(reaped.at) < 180000) return;
    pool.recordSession(ROOT, { slot: laneLockOf(s), slug: wt.slugOfRoot(ROOT) ?? undefined, priority: s.priority, waitS: s.waitS, heldSinceMs: Date.parse(s.heldSince), endMs: Date.parse(stoppedAt), boots: s.boots, bootS: s.bootS, clients: s.expectClients, mission: s.mission, closedBy: "owner" });
  } catch {}
}

// `session join <n>`: starts client n on a running session that was brought up without it, so a
// proof can show a LATE JOINER (a client that connects after the state it must see was already
// set). The server log marker counts joins, so this waits for one more than before.
function cmdSessionJoin(root, dir, flags) {
  guardHeavy(root, "session join", flags.includes("--force"));
  const n = Number(flags[0]);
  if (!Number.isInteger(n) || n < 1 || n > session.MAX_CLIENTS) {
    console.log("usage: tools\\cli session join <client 1.." + session.MAX_CLIENTS + ">");
    return 2;
  }
  const s = session.readSession(dir);
  if (!s || s.stoppedAt || !host.pidAlive(s.serverPid)) {
    console.log("session: no running session; 'node agent-box/session.mjs up' first");
    return 1;
  }
  if ((s.clients ?? []).some((c) => c.n === n && host.pidAlive(c.pid))) {
    console.log(`session: client ${n} is already running`);
    return 1;
  }
  const game = gameClientExe();
  if (!existsSync(game)) {
    console.log(`FAIL  not found: ${game}`);
    return 1;
  }
  const c = session.sessionPaths(dir, session.MAX_CLIENTS).clients.find((x) => x.n === n);
  // RAM gate like `up` (#563): trim idle sessions first, refuse if the floor would still be breached.
  const ps = pool.poolSettings(root);
  const gate = pool.gateJoin(root, ps.sessionReserve?.[s.profile]?.clientMb ?? ps.sessionReserveClientMb);
  if (!gate.ok) {
    console.log(`session: join refused, ${gate.reason}${gate.trimmed ? " (after trimming idle sessions)" : ""}; close a session or retry later`);
    return 1;
  }
  const log = session.newestLog(join(s.serverProfile, "logs"));
  const marker = /(?:registration form sent to player|spawning player) \d+/g;
  const before = (readFileSync(log, "utf8").match(marker) ?? []).length;
  const state = { ...s, clients: (s.clients ?? []).filter((x) => x.n !== n) };
  sessionLog = join(dir, "up.log");
  const fail = (msg) => {
    console.log(`FAIL  ${msg}`);
    return 1;
  };
  const failed = sessionLaunchClient(root, dir, state, c, { game, mission: s.mission, res: s.res, visible: s.visible, fail });
  if (failed !== null) return failed;
  const joined = sessionAwaitJoin(log, before + 1, marker, fail, state);
  if (joined < 0) return 1;
  console.log(`session: client ${n} joined late (${joined} join(s) in the server log)`);
  return 0;
}

function cmdSessionStatus(dir) {
  const s = session.readSession(dir);
  if (!s) {
    console.log("session: none recorded");
    return 0;
  }
  const up = (pid) => (host.pidAlive(pid) ? "up" : "gone");
  console.log(`session dir: ${dir}  profile ${s.profile ?? "full"}`);
  console.log(`  server   pid ${s.serverPid} ${up(s.serverPid)}  log ${session.newestLog(join(s.serverProfile, "logs")) ?? "-"}`);
  for (const c of s.clients ?? []) {
    console.log(`  client ${c.n} pid ${c.pid} ${up(c.pid)}  box ${c.box}  log ${session.newestLog(c.logs) ?? "-"}`);
  }
  return 0;
}

// How long `session up --wait` queues (an aged request reaches the front at 40 min, so it needs more than 30).
const SESSION_QUEUE_MIN = 90;

// --hold <min>: raises this session's hold cap for a known-long job, up to sessionHoldMaxMin.
function sessionHoldFlag(flags, settings) {
  const v = argValue(flags, "--hold");
  if (v === null) return { min: settings.sessionHoldMin, note: "" };
  const want = Number(v);
  if (!Number.isFinite(want) || want <= 0) throw new Error(`--hold needs minutes (got '${v}')`);
  const min = Math.min(settings.sessionHoldMaxMin, Math.max(settings.sessionHoldMin, Math.round(want)));
  const cut = want > settings.sessionHoldMaxMin ? ` (asked ${want}; the most is ${settings.sessionHoldMaxMin})` : "";
  return { min, note: `session: hold cap raised to ${min} min${cut}` };
}

// `session up` prints the cap and, when other session requests are queued, how many wait.
function sessionCapNote(root, settings, holdMin) {
  const waiting = pool.sessionQueue(root).length;
  const cap = `hold cap ${holdMin} min (enforced only while a session request queues; then a ${settings.sessionGraceMin} min warning)`;
  if (waiting) console.log(`session: ${waiting} other session request(s) are queued behind you; ${cap}. Do your proof and shots in one go, then 'node agent-box/session.mjs down'`);
  else console.log(`session: nobody is queued; ${cap}`);
}

function cmdSessionUp(root, dir, flags) {
  guardHeavy(root, "session up", flags.includes("--force"));
  const clients = session.parseClients(flags);
  const mission = session.parseMission(flags);
  const rp = session.parseSessionProfile(flags);
  const res = session.sessionRes(flags, rp);
  const visible = flags.includes("--visible");
  // --sound keeps game audio on (the default session is -noSound); for a filmed audio pass.
  const sound = flags.includes("--sound");
  const prev = session.readSession(dir);
  if (prev && !prev.stoppedAt && (host.pidAlive(prev.serverPid) || (prev.clients ?? []).some((c) => host.pidAlive(c.pid)))) {
    console.log("session: one is already running; 'node agent-box/session.mjs down' first");
    return 1;
  }
  // Another checkout's live session blocks only through the lane lock (and the RAM floor) that
  // acquireSessionLaneSync below checks: a free lane is ours. Clients left by a session whose server
  // is gone hold no lane; they are reaped further down by their recorded pids.
  const exe = serverExe();
  const game = gameClientExe();
  for (const f of clients > 0 ? [exe, game] : [exe]) {
    if (!existsSync(f)) {
      console.log(`FAIL  not found: ${f}`);
      return 1;
    }
  }
  const holderOf = () => {
    try {
      const holders = [];
      for (const l of pool.sessionLanes(root)) {
        try {
          const h = JSON.parse(readFileSync(join(wt.hubDirOf(root), "locks", `${l.lock}.lock`), "utf8"));
          if (h.pid && host.pidAlive(h.pid)) holders.push(`${h.purpose ?? "a run"} of ${h.slug ?? h.root ?? "another checkout"} (pid ${h.pid})`);
        } catch {}
      }
      return holders.length ? holders.join(" and ") : "another run";
    } catch {
      return "another run";
    }
  };
  // The resource database refresh (a headless Workbench, minutes when stale) runs BEFORE the
  // queue: it used to run with the lane held, keeping everyone else out while it worked.
  ensureResourceDatabase(root);
  const settings = pool.poolSettings(root);
  const hold = sessionHoldFlag(flags, settings);
  const queuedAt = Date.now();
  let lane = null;
  try {
    if (flags.includes("--wait")) console.log(`session: the session lane is held by ${holderOf()}; queued (up to ${SESSION_QUEUE_MIN} min)`);
    lane = pool.acquireSessionLaneSync(root, { waitMs: flags.includes("--wait") ? SESSION_QUEUE_MIN * 60000 : 0, meta: { slug: wt.slugOfRoot(root) ?? undefined, purpose: "session", root, clients, profile: rp.profile }, priority: priorityFlag(flags), callerPids: [process.ppid, Number(process.env.LF_CALLER_PID)].filter((p) => Number.isInteger(p) && p > 0) });
  } catch (e) {
    if (e.refusal) {
      console.log(`FAIL  ${e.message}`);
      return 1;
    }
    // LANE_BUSY: the router (agent-box/session.mjs) queues the request again for any machine.
    if (process.env.LF_SESSION_ROUTED === "1" && !flags.includes("--wait")) console.log(`session: no lane here after all (${e.message})`);
    else console.log(`FAIL  the session lane is held by ${holderOf()} (${e.message}); retry when it ends, or pass --wait`);
    return pool.LANE_BUSY_CODE;
  }
  const laneLock = lane?.lockName ?? lane?.name ?? SERVER_SLOT;
  const lanePort = lane?.port ?? 2001;
  console.log(`session: lane ${laneLock} (port ${lanePort})`);
  // Another lane's healthy session is not a leftover. Clients of a session whose server is gone are
  // orphans: close them by the pids in that session's own record (never by process name).
  for (const d of session.reapOrphanSessions(dir, host.pidAlive, killTree)) console.log(`session: closed orphaned clients of ${d} (their server was gone)`);
  const heldSince = new Date().toISOString();
  const waitS = Math.round((lane?.queuedMs ?? Date.now() - queuedAt) / 100) / 10;
  resetLease(dir);
  const paths = session.sessionPaths(dir, clients);
  sessionLog = join(dir, "up.log");
  try {
    writeFileSync(sessionLog, "");
  } catch {}
  // id names this session's clients (-lfSession); harness commands from other checkouts are refused.
  const state = { id: session.newSessionId(), lane: { lock: laneLock, port: lanePort }, root, mission, res, profile: rp.profile, levers: rp.levers, visible, sound, startedAt: new Date().toISOString(), serverPid: 0, serverProfile: paths.server, clients: [], expectClients: clients,
    // The hold cap judges only a session that carries holdCap (pool.mjs reapIdleSessions).
    holdCap: true, holdMin: hold.min, heldSince, waitS, priority: pool.resolvePriority(root, priorityFlag(flags)), boots: 1, bootS: [] };
  if (hold.note) console.log(hold.note);
  const bootT0 = Date.now();
  const fail = (msg) => {
    console.log(`FAIL  ${msg}`);
    console.log(`      full log ${sessionLog}`);
    session.writeSession(dir, state);
    cmdSessionDown(dir);
    wt.releaseLock(root, laneLock);
    return 1;
  };
  try {
    // A fresh session starts from no save: an old playtest save would be loaded by restart.
    const log = sessionLaunch(root, dir, state, paths, { exe, game, mission, res, visible, loadSave: false, fail, marker: /(?:registration form sent to player|spawning player) \d+/g });
    if (typeof log === "number") return log;
    // Joined means the server sent a new account the registration form or spawned a returning
    // one. There is no faction menu (M1-11).
    const joined = sessionAwaitJoin(log, clients, /(?:registration form sent to player|spawning player) \d+/g, fail, state);
    if (joined < 0) return 1;
    const bootS = Math.round((Date.now() - bootT0) / 100) / 10;
    session.writeSession(dir, { ...state, readyAt: new Date().toISOString(), bootS: [bootS] });
    console.log(`session: ready in ${bootS} s; ${joined} client(s) joined${mission === "playtest" ? " (LF_PlaytestTown, LF_HARNESS)" : ""}; profile ${rp.profile}${rp.profile === "lowspec" ? " (640x360 clients, lowest textures, no post-processing; shots need --profile full)" : ""}. Drive: harness.mjs --client <n> <verb>; close: session.mjs down`);
    sessionCapNote(root, settings, hold.min);
    console.log(`session: full log ${sessionLog}`);
    return 0;
  } catch (e) {
    return fail(e.message);
  } finally {
    // Before the transfer this process owns the lock; after it the server does.
    wt.releaseLock(root, laneLock);
  }
}

// `session measure --variants full,lowspec,tex+res --clients 1,2 [--settle s] [--proof name] [--out file]`:
// RAM of the server and each client per profile or lever set (tools/lib/session-measure.mjs). One
// queue wait, then every variant boots and closes in turn while this process keeps the lane.
function cmdSessionMeasure(root, dir, flags) {
  guardHeavy(root, "session measure", flags.includes("--force"));
  const variants = (argValue(flags, "--variants") ?? "full,lowspec").split(",").map((v) => measure.parseVariant(v));
  const counts = (argValue(flags, "--clients") ?? "1").split(",").map(Number);
  if (counts.some((n) => !Number.isInteger(n) || n < 0 || n > session.MAX_CLIENTS)) throw new Error(`--clients is a list of 0..${session.MAX_CLIENTS}`);
  const settleS = Number(argValue(flags, "--settle") ?? 30);
  // --proof quarry-proof (every count) or --proof 1:quarry-proof,2:scanner-proof (by client count).
  const proofFor = (n) => {
    const list = String(argValue(flags, "--proof") ?? "").split(",").filter(Boolean);
    const hit = list.find((p) => p.startsWith(`${n}:`)) ?? list.find((p) => !/^\d:/.test(p));
    return hit ? hit.replace(/^\d:/, "") : "";
  };
  const mission = argValue(flags, "--mission") ?? "debug";
  const prev = session.readSession(dir);
  if (prev && !prev.stoppedAt && (host.pidAlive(prev.serverPid) || (prev.clients ?? []).some((c) => host.pidAlive(c.pid)))) {
    console.log("session: one is already running; 'node agent-box/session.mjs down' first");
    return 1;
  }
  const exe = serverExe();
  const game = gameClientExe();
  ensureResourceDatabase(root);
  let lane = null;
  try {
    if (flags.includes("--wait")) console.log(`measure: queued for a session lane (up to ${SESSION_QUEUE_MIN} min)`);
    lane = pool.acquireSessionLaneSync(root, { waitMs: flags.includes("--wait") ? SESSION_QUEUE_MIN * 60000 : 0, meta: { slug: wt.slugOfRoot(root) ?? undefined, purpose: "session", root, clients: Math.max(...counts), profile: "full" }, priority: priorityFlag(flags), callerPids: [process.ppid, Number(process.env.LF_CALLER_PID)].filter((p) => Number.isInteger(p) && p > 0) });
  } catch (e) {
    console.log(e.refusal ? `FAIL  ${e.message}` : `FAIL  no session lane (${e.message}); pass --wait`);
    return 1;
  }
  const laneLock = lane?.lockName ?? lane?.name ?? SERVER_SLOT;
  const lanePort = lane?.port ?? 2001;
  console.log(`measure: lane ${laneLock} (port ${lanePort}); free RAM ${measure.freeMb()} MB`);
  sessionLog = join(dir, "up.log");
  const rows = [];
  let state = null;
  const closeAll = () => {
    if (!state) return;
    try {
      wt.adoptLock(root, laneLock, state.serverPid);
    } catch {}
    for (const c of state.clients ?? []) killTree(c.pid);
    killTree(state.serverPid);
    for (let i = 0; i < 20 && host.pidAlive(state.serverPid); i++) sleepMs(500);
    session.writeSession(dir, { ...state, stoppedAt: new Date().toISOString() });
    sleepMs(4000);
  };
  const marker = /(?:registration form sent to player|spawning player) \d+/g;
  try {
    for (const v of variants) {
      for (const n of v.only === null ? counts : [v.only]) {
        const rp = session.resolveProfile(v.profile, v.levers);
        const res = session.sessionRes([], rp);
        const paths = session.sessionPaths(dir, n);
        resetLease(dir);
        state = { id: session.newSessionId(), lane: { lock: laneLock, port: lanePort }, root, mission, res, profile: v.profile, levers: v.levers, visible: false, sound: false, startedAt: new Date().toISOString(), serverPid: 0, serverProfile: paths.server, clients: [], expectClients: n, holdCap: true, holdMin: 45, heldSince: new Date().toISOString(), boots: 1, bootS: [] };
        const freeBefore = measure.freeMb();
        const t0 = Date.now();
        const fail = (msg) => {
          throw new Error(msg);
        };
        const log = sessionLaunch(root, dir, state, paths, { exe, game, mission, res, visible: false, loadSave: false, fail, marker });
        if (typeof log === "number") throw new Error(`launch failed (${log})`);
        if (n > 0 && sessionAwaitJoin(log, n, marker, fail, state) < 0) throw new Error("clients did not join");
        const bootS = Math.round((Date.now() - t0) / 100) / 10;
        session.writeSession(dir, { ...state, readyAt: new Date().toISOString(), bootS: [bootS] });
        let proofNote = "";
        const proof = proofFor(n);
        if (proof && n > 0) {
          const r = spawnSync(process.execPath, [join(root, "agent-box", "session.mjs"), proof], { windowsHide: true, encoding: "utf8", cwd: root, timeout: 300000 });
          proofNote = `${proof} ${r.status === 0 ? "PASS" : "exit " + r.status}`;
          console.log(`measure: ${v.name} x${n}: ${proofNote}`);
        }
        sleepMs(settleS * 1000);
        const got = measure.sampleProcs([state.serverPid, ...state.clients.map((c) => c.pid)], PWSH);
        const srv = got.find((g) => g.pid === state.serverPid);
        const cls = state.clients.map((c) => got.find((g) => g.pid === c.pid)).filter(Boolean);
        // What a trim leaves: the pool trims session processes when it would otherwise refuse a session for RAM.
        const before = got.map((g) => g.wsMb);
        trimProcs([state.serverPid, ...state.clients.map((c) => c.pid)], PWSH);
        sleepMs(3000);
        const after = measure.sampleProcs([state.serverPid, ...state.clients.map((c) => c.pid)], PWSH);
        sleepMs(15000);
        const later = measure.sampleProcs([state.serverPid, ...state.clients.map((c) => c.pid)], PWSH);
        const row = { trimmedWs: after.map((g) => g.wsMb), idleAfterTrimWs: later.map((g) => g.wsMb), beforeTrimWs: before, variant: v.name, levers: v.levers, clients: n, bootS, serverWs: srv?.wsMb ?? 0, serverPriv: srv?.privMb ?? 0, serverPeak: srv?.peakMb ?? 0, clientWs: cls.map((c) => c.wsMb), clientPriv: cls.map((c) => c.privMb), clientPeak: cls.map((c) => c.peakMb), serverCpuS: srv?.cpuS ?? 0, clientCpuS: cls.map((c) => c.cpuS), freeBefore, freeMb: measure.freeMb(), proof: proofNote };
        row.totalWs = row.serverWs + row.clientWs.reduce((a, b) => a + b, 0);
        rows.push(row);
        console.log(`measure: ${v.name} x${n}: ws before trim ${row.beforeTrimWs.join("/")}, 3 s after ${row.trimmedWs.join("/")}, 18 s after ${row.idleAfterTrimWs.join("/")} (server first)`);
        console.log(`measure: ${v.name} x${n}: server ${row.serverWs} MB, clients ${row.clientWs.join("/") || "-"} MB (ws), total ${row.totalWs} MB, free ${row.freeMb} MB`);
        closeAll();
        state = null;
      }
    }
  } catch (e) {
    console.log(`FAIL  ${e.message}`);
    closeAll();
    return 1;
  } finally {
    wt.releaseLock(root, laneLock);
    const out = argValue(flags, "--out");
    if (out && rows.length) measure.saveJson(out, rows);
    if (rows.length) console.log(`\n${measure.formatTable(rows)}`);
  }
  return 0;
}

// Boots the server (fresh, or with -loadSessionSave), then each client in
// order with its own profile. Returns the server console.log path, or the exit
// code fail() returned.
function sessionLaunch(root, dir, state, paths, { exe, game, mission, res, visible, loadSave, fail, marker }) {
  const junction = serverGameAddons(root);
  mkdirSync(paths.server, { recursive: true });
  const sArgs = session.serverArgs({ addonsDir: [join(root, "addons"), junction].join(","), profile: paths.server, mission, loadSave, port: state.lane?.port ?? 2001, rp: session.profileOf(state) });
  slog(`launching server: ${exe}\n  ${sArgs.join(" ")}`);
  const before = new Set(existsSync(join(paths.server, "logs")) ? readdirSync(join(paths.server, "logs")) : []);
  const srv = process.env.SSH_CONNECTION ? spawnOutsideSsh(exe, sArgs, dirname(exe)) : spawn(exe, sArgs, { cwd: dirname(exe), detached: true, stdio: "ignore", windowsHide: true });
  srv.unref?.();
  if (!srv.pid) return fail("server did not start");
  state.serverPid = srv.pid;
  wt.transferLock(root, laneLockOf(state), srv.pid);
  session.writeSession(dir, state);

  let log = null;
  const deadline = Date.now() + 300000;
  while (Date.now() < deadline && host.pidAlive(srv.pid)) {
    sleepMs(2000);
    const f = session.newestLog(join(paths.server, "logs"));
    if (f && !before.has(basenameOfLogDir(f))) log = f;
    if (log && readFileSync(log, "utf8").includes(session.SERVER_READY)) break;
  }
  if (!log || !readFileSync(log, "utf8").includes(session.SERVER_READY)) return fail(`server not ready (log ${log ?? "none"})`);
  slog(`server up (pid ${srv.pid}); log ${log}`);

  for (const c of paths.clients) {
    const failed = sessionLaunchClient(root, dir, state, c, { game, mission, res, visible, fail });
    if (failed !== null) return failed;
    // Client n+1 starts once client n has joined, so player ids follow client numbers and two clients
    // do not load at once. This replaced a fixed 15 s sleep: a client joins in about 7 s (measured
    // 2026-11-10: 6.7 to 8.2 s from launch to the server's join line).
    if (marker && c.n < paths.clients.length) {
      const joinBy = Date.now() + 40000;
      while (Date.now() < joinBy && host.pidAlive(state.serverPid) && (readFileSync(log, "utf8").match(marker) ?? []).length < c.n) sleepMs(500);
      slog(`client ${c.n} joined ${Math.round((Date.now() - Date.parse(state.clients.at(-1).launchedAt)) / 100) / 10} s after its launch`);
    }
  }
  return log;
}

// Launches one session client (its own profile) and records it in the session state. Returns
// null, or the code fail() returned.
function sessionLaunchClient(root, dir, state, c, { game, mission, res, visible, fail }) {
  mkdirSync(c.box, { recursive: true });
  const rp = session.profileOf(state);
  // The profile's engine settings go into the client profile before every launch (full clears them).
  session.applyClientSettings(c.profile, rp);
  const cArgs = session.clientArgs({ n: c.n, addonsDir: join(root, "addons"), profile: c.profile, mission, port: state.lane?.port ?? 2001, session: state.id, res, sound: !!state.sound, rp });
  slog(`launching client ${c.n}: ${game}\n  ${cArgs.join(" ")}`);
  // Clients render on the hidden LF-Agents desktop; harness screenshots still work.
  // --visible puts them on the user's desktop (with -noFocus) so tools/film can record.
  // Over ssh (a pool machine such as lf-testbox) there is no desktop: the client
  // goes to the signed-in user's desktop through an interactive scheduled task.
  let child = { pid: 0 };
  if (interactiveNeeded()) {
    try {
      child = { pid: interactiveSpawnDetached(game, cArgs, { cwd: dirname(game), marker: c.profile }) };
    } catch (e) {
      return fail(`client ${c.n}: ${e.message}`);
    }
  } else if (visible) {
    const p = spawn(game, cArgs, { cwd: dirname(game), detached: true, stdio: "ignore" });
    p.unref();
    child = { pid: p.pid };
  } else {
    // quiet-launch reports a client that dies in its first 3s with its exit code. Seen
    // once in a while on the hidden desktop with no log and no cause; one retry clears it.
    try {
      child = { pid: quietSpawnDetached(game, cArgs, { cwd: dirname(game) }) };
    } catch (e) {
      slog(`client ${c.n} first launch failed (${e.message}); retrying once`);
      sleepMs(5000);
      try {
        child = { pid: quietSpawnDetached(game, cArgs, { cwd: dirname(game) }) };
      } catch (e2) {
        return fail(`client ${c.n} exited right after launch twice: ${e2.message}`);
      }
    }
  }
  if (!child.pid) return fail(`client ${c.n} did not start`);
  state.clients.push({ n: c.n, pid: child.pid, profile: c.profile, box: c.box, logs: c.logs, launchedAt: new Date().toISOString() });
  session.writeSession(dir, state);
  // The next client waits for this one's join line (sessionLaunch); here only a client that dies at once is caught.
  sleepMs(3000);
  if (!host.pidAlive(child.pid)) return fail(`client ${c.n} exited right after launch (log ${session.newestLog(c.logs) ?? "none"})`);
  return null;
}

// One look at a starting session: { joined } while it is still coming up, or
// { fail } with a one-line reason as soon as it cannot get there (a refused
// client, a client or the server exited, Steam not running).
function sessionVerdict(state, log, marker) {
  const text = readFileSync(log, "utf8");
  const refused = text.split(/\r?\n/).find((l) => /ValidationError|Disconnecting identity/.test(l));
  if (refused) return { fail: `a client was refused: ${refused.trim()}` };
  const joined = (text.match(marker) ?? []).length;
  if (state) {
    if (!host.pidAlive(state.serverPid)) return { fail: `the server exited (log ${log})` };
    for (const c of state.clients ?? []) {
      const cl = session.newestLog(c.logs);
      if (!host.pidAlive(c.pid)) return { fail: `client ${c.n} exited (log ${cl ?? "none"})` };
      if (cl) {
        const steam = readFileSync(cl, "utf8").split(/\r?\n/).find((l) => /steam.*(not running|must be running|failed to init|SteamAPI_Init)/i.test(l));
        if (steam) return { fail: `client ${c.n}: Steam refused it: ${steam.trim().slice(0, 160)}` };
      }
    }
  }
  return { joined };
}

// Waits until marker matches the server log once per client. Returns the
// count, or -1 after fail().
function sessionAwaitJoin(log, clients, marker, fail, state) {
  const joinDeadline = Date.now() + 240000;
  let joined = 0;
  while (Date.now() < joinDeadline) {
    const v = sessionVerdict(state, log, marker);
    if (v.fail) {
      fail(v.fail);
      return -1;
    }
    joined = v.joined;
    if (joined >= clients) return joined;
    sleepMs(2000);
  }
  fail(`${joined}/${clients} client(s) joined within 240s (server log ${log})`);
  return -1;
}

// `session wait [--timeout s]`: blocks on a session that is already starting
// (an up running in the background or in another terminal) until the server and
// every client are ready. Exit 0 ready; exit 1 with a one-line reason otherwise.
function cmdSessionWait(dir, flags) {
  const i = flags.indexOf("--timeout");
  const timeoutS = i === -1 ? 600 : Number(flags[i + 1]);
  if (!Number.isFinite(timeoutS) || timeoutS <= 0) {
    console.log("FAIL  --timeout needs seconds");
    return 2;
  }
  const t0 = Date.now();
  const deadline = t0 + timeoutS * 1000;
  let sawSession = false;
  for (;;) {
    const s = session.readSession(dir);
    if (s && !s.stoppedAt) {
      sawSession = true;
      if (s.readyAt && host.pidAlive(s.serverPid)) {
        console.log(`session: ready; server pid ${s.serverPid}, ${(s.clients ?? []).length} client(s)`);
        return 0;
      }
      const log = s.serverPid ? session.newestLog(join(s.serverProfile, "logs")) : null;
      if (s.serverPid && !host.pidAlive(s.serverPid)) {
        console.log(`FAIL  the server exited before it was ready (log ${log ?? "none"})`);
        return 1;
      }
      if (log && s.expectClients !== undefined && (s.clients ?? []).length >= s.expectClients) {
        const marker = s.restartedAt ? /flow start for player \d+/g : /(?:registration form sent to player|spawning player) \d+/g;
        const v = sessionVerdict(s, log, marker);
        if (v.fail) {
          console.log(`FAIL  ${v.fail}`);
          return 1;
        }
        if (readFileSync(log, "utf8").includes(session.SERVER_READY) && v.joined >= s.expectClients) {
          console.log(`session: ready; server pid ${s.serverPid}, ${v.joined} client(s) joined`);
          return 0;
        }
      }
    } else if (s?.stoppedAt && sawSession) {
      console.log("FAIL  the session was closed while waiting");
      return 1;
    } else if (!sawSession && Date.now() - t0 > 30000) {
      console.log("FAIL  no session starting; run 'node agent-box/session.mjs up' first");
      return 1;
    }
    if (Date.now() > deadline) {
      console.log(`FAIL  session not ready within ${timeoutS} s (state ${session.sessionPaths(dir).state})`);
      return 1;
    }
    sleepMs(2000);
  }
}

// Playtest only: the host stop file closes the server gracefully (the engine
// writes its SHUTDOWN save), then the same client profiles rejoin, in the same
// order, a server that loaded that save.
function cmdSessionRestart(root, dir, flags) {
  guardHeavy(root, "session restart", flags.includes("--force"));
  const prev = session.readSession(dir);
  if (!prev || prev.stoppedAt || !host.pidAlive(prev.serverPid)) {
    console.log("session: no running session to restart; 'node agent-box/session.mjs up --mission playtest' first");
    return 1;
  }
  if (prev.mission !== "playtest") {
    console.log("session: restart needs --mission playtest (the bare DebugWorld has no stop file, so no shutdown save)");
    return 1;
  }
  const exe = serverExe();
  const game = gameClientExe();
  const oldLog = session.newestLog(join(prev.serverProfile, "logs"));
  // Hold port 2001 across the restart: a queued test would otherwise take it
  // between the old server's exit and the new boot.
  try {
    wt.adoptLock(root, laneLockOf(prev), prev.serverPid);
  } catch (e) {
    console.log(`FAIL  ${e.message}`);
    return 1;
  }
  const stopFile = join(prev.serverProfile, ...host.STOP_FILE);
  mkdirSync(dirname(stopFile), { recursive: true });
  writeFileSync(stopFile, new Date().toISOString());
  console.log(`session: stop requested (server pid ${prev.serverPid}); waiting for the shutdown save`);
  const deadline = Date.now() + 180000;
  while (Date.now() < deadline && host.pidAlive(prev.serverPid)) sleepMs(1000);
  if (host.pidAlive(prev.serverPid)) {
    wt.transferLock(root, laneLockOf(prev), prev.serverPid);
    console.log("FAIL  the server did not close within 180s; close it with 'node agent-box/session.mjs down'");
    return 1;
  }
  const saved = !!oldLog && /\(SHUTDOWN\)/.test(readFileSync(oldLog, "utf8"));
  console.log(`session: server closed${saved ? " after the SHUTDOWN save" : "; WARN no SHUTDOWN save line in its log"}`);
  for (const c of prev.clients ?? []) console.log(`session: client ${c.n} pid ${c.pid} ${killTree(c.pid) ? "closed" : "already gone"}`);
  const clients = (prev.clients ?? []).length;
  const paths = session.sessionPaths(dir, clients);
  sessionLog = join(dir, "up.log");
  const state = { ...prev, restartedAt: new Date().toISOString(), readyAt: undefined, serverPid: 0, clients: [], boots: (prev.boots ?? 1) + 1 };
  const bootT0 = Date.now();
  const fail = (msg) => {
    console.log(`FAIL  ${msg}`);
    session.writeSession(dir, state);
    cmdSessionDown(dir);
    wt.releaseLock(root, laneLockOf(prev));
    return 1;
  };
  try {
    const log = sessionLaunch(root, dir, state, paths, { exe, game, mission: prev.mission, res: prev.res, visible: prev.visible, loadSave: true, fail, marker: /flow start for player \d+/g });
    if (typeof log === "number") return log;
    const joined = sessionAwaitJoin(log, clients, /flow start for player \d+/g, fail, state);
    if (joined < 0) return 1;
    const loaded = /session continued from a save point|loading the latest save point/.test(readFileSync(log, "utf8"));
    const bootS = Math.round((Date.now() - bootT0) / 100) / 10;
    session.writeSession(dir, { ...state, readyAt: new Date().toISOString(), bootS: [...(prev.bootS ?? []), bootS] });
    console.log(`session: restarted in ${bootS} s${loaded ? " from the save" : "; WARN the server log does not say it loaded a save"}; ${joined} client(s) rejoined`);
    return 0;
  } catch (e) {
    return fail(e.message);
  } finally {
    wt.releaseLock(root, laneLockOf(prev));
  }
}

const SESSION_USAGE = "usage: tools\\cli session up [--profile lowspec|full (default lowspec; shots and filming need full)] [--levers a,b] [--clients N (default 1)] [--hold min (max 45)] [--mission debug|playtest] [--res WxH] [--visible] [--sound] [--wait] [--machine any|local|<name>] | wait [--timeout s] | join <n> | restart | down | status";

function cmdSession(args) {
  const list = args ?? [];
  const dir = session.sessionDir();
  if (list.includes("--help") || list.includes("-h") || list[0] === "help") {
    console.log(SESSION_USAGE);
    return 0;
  }
  // `toolscli session up` goes through the router (agent-box/session.mjs), which picks the machine:
  // this PC or a pool machine with a lane and the RAM. The router's own local leg sets LF_SESSION_ROUTED.
  if (list[0] === "up" && process.env.LF_SESSION_ROUTED !== "1") {
    const r = spawnSync(process.execPath, [join(ROOT, "agent-box", "session.mjs"), "up", ...list.slice(1)], { windowsHide: true, stdio: "inherit", cwd: ROOT, env: { ...process.env, LF_CALLER_PID: process.env.LF_CALLER_PID || String(process.ppid) } });
    return r.status ?? 1;
  }
  if (list[0] === "up") return cmdSessionUp(ROOT, dir, list.slice(1));
  // The session dir is shared by every checkout: only the one that started a
  // live session may restart or close it (--force overrides).
  if ((list[0] === "restart" || list[0] === "down") && !list.includes("--force")) {
    const s = session.readSession(dir);
    const live = s && !s.stoppedAt && (host.pidAlive(s.serverPid) || (s.clients ?? []).some((c) => host.pidAlive(c.pid)));
    if (live && s.root && resolve(s.root).toLowerCase() !== resolve(ROOT).toLowerCase()) {
      console.log(`session: the live session belongs to ${s.root}; refusing ${list[0]} from ${ROOT} (pass --force to override)`);
      return 1;
    }
  }
  if (list[0] === "join") return cmdSessionJoin(ROOT, dir, list.slice(1));
  if (list[0] === "measure") return cmdSessionMeasure(ROOT, dir, list.slice(1));
  if (list[0] === "restart") return cmdSessionRestart(ROOT, dir, list.slice(1));
  if (list[0] === "down") return cmdSessionDown(dir);
  if (list[0] === "status") return cmdSessionStatus(dir);
  if (list[0] === "wait") return cmdSessionWait(dir, list.slice(1));
  console.log(SESSION_USAGE);
  return list[0] ? 1 : 0;
}

async function cmdTest(noBuild, tier = "all", root = ROOT, opts = {}) {
  if (tier !== "fast" && tier !== "all" && tier !== "persistence") {
    console.log(`unknown tier: ${tier} (expected fast, all, or persistence)`);
    return 2;
  }
  let only = [];
  if (opts.only !== undefined) {
    only = resolveOnlyNames(root, opts.only);
    if (only.length === 0) {
      console.log("usage: tools\\cli test --no-build --only <LF_Test_Class|test-name|LF_Test_File>[,...]");
      return 2;
    }
    console.log(`test: --only ${only.join(",")}`);
  }
  guardHeavy(root, "test", opts.force);
  if (!noBuild) {
    const b = await cmdBuild(root, { wait: opts.wait });
    if (b !== 0) return b;
  }
  // The test pool (tools/remote/pool.mjs) picks the machine: the first free
  // slot on this PC or any reachable remote (--local / --remote [name] narrow
  // it); when all are busy the run queues FIFO for whichever frees first.
  // A remote run ships this worktree's rdb, so refresh it before the choice.
  ensureResourceDatabase(root);
  const purpose = only.length ? "test only" : `test ${tier}`;
  // A pool machine that disappears mid-run (the laptop shuts down) is a normal case: it is marked
  // down and the run queues again for another machine (pool.machineGone).
  for (let attempt = 1; ; attempt++) {
    const r = await testOnce(root, { purpose, tier, only, opts, lastTry: attempt >= pool.FAILOVER_ATTEMPTS });
    if (!r.requeue) return r.code;
  }
}

async function testOnce(root, { purpose, tier, only, opts, lastTry }) {
  let slot;
  try {
    slot = await pool.acquireSlot(root, { ...poolMode(root, opts), purpose, priority: opts.priority, budget: opts.budget });
  } catch (e) {
    console.log(`FAIL  ${e.message}`);
    return { code: 3 };
  }
  let code = 3;
  let extra = {};
  try {
    if (!slot.local) {
      const args = ["test", "--no-build", "--local", "--tier", tier, ...(only.length ? ["--only", only.join(",")] : []), ...(opts.world ? ["--world", opts.world] : []), ...(slot.budgetS ? ["--budget", String(slot.budgetS)] : [])];
      const r = await testbox.runRemote(root, slot, { args, writeSyncState: pool.writeSyncState });
      code = r.code;
      if (r.summary) extra = { passed: r.summary.passed, total: r.summary.total };
      if (r.sync) extra.syncS = Math.round(r.sync.ms / 100) / 10;
      if (!lastTry && pool.machineGone(root, slot, code)) {
        extra.lost = true;
        return { code, requeue: true };
      }
      return { code };
    }
    const before = Date.now();
    activeBudget = { runS: slot.budgetS ?? opts.budget, bootS: slot.bootS };
    code = runLocalTest(root, tier, only, opts.world);
    const stamp = newestStamp(root, before);
    if (stamp) extra = { passed: stamp.passed, total: stamp.total };
    return { code };
  } finally {
    slot.release();
    if (!slot.inherited) pool.recordRun(root, slot, { purpose, code, extra });
  }
}

// The summary this run stamped (tools/test/test-e2e.ps1 appends one line).
function newestStamp(root, since) {
  const f = join(root, "tmp", "test-results", "summaries.jsonl");
  try {
    if (statSync(f).mtimeMs < since) return null;
    const lines = readFileSync(f, "utf8").trim().split(/\r?\n/);
    return JSON.parse(lines[lines.length - 1]);
  } catch {
    return null;
  }
}

// --local / --remote [name] / --machine <name|any> / --any (default).
// LF_TESTBOX_AUTO=0 keeps the default on this machine.
function poolMode(root, opts) {
  if (opts.local) return { mode: "local" };
  if (opts.machine) return opts.machine === "any" ? { mode: "any" } : { mode: "any", machine: opts.machine };
  if (opts.remote !== undefined) return opts.remote ? { mode: "remote", machine: opts.remote } : { mode: "remote" };
  if (opts.any) return { mode: "any" };
  if (envOf().LF_TESTBOX_AUTO === "0") return { mode: "local" };
  return { mode: "any" };
}

// world: another world or mission header to boot instead of DebugWorld (Worlds/CoreOnEveron/CoreOnEveron.ent
// boots the core alone on vanilla Everon, #1047). Its layers must place an LF_TestRunner.
function runLocalTest(root, tier, only, world) {
  // Delegate to the proven PowerShell harness (tools/test/test-e2e.ps1):
  // boots the dedicated server via -server + a neutral addonsDir, polls the
  // profile console.log every 2s for the [LFTEST] SUMMARY, self-terminates.
  // The harness is loaded from the CLI's own checkout but launched with cwd in
  // the target worktree, so it builds its profile/junction/logs there.
  const harness = join(ROOT, "tools", "test", "test-e2e.ps1");
  if (!existsSync(harness)) {
    console.log(`FAIL  harness not found: ${harness}`);
    return 1;
  }
  const ports = wt.portsForRoot(root);
  console.log(`test: worktree ports game=${ports.gamePort} a2s=${ports.a2sPort}`);
  return withServerSlot(root, 0, () => {
    const res = sh(PWSH, [
      "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", harness,
      "-Tier", tier,
      "-BindPort", String(ports.gamePort),
      "-A2sPort", String(ports.a2sPort),
      ...(laneListenAddr() ? ["-ListenAddr", laneListenAddr()] : []),
      ...budgetArgs(),
      ...(only.length ? ["-Only", only.join(",")] : []),
      ...(world ? ["-Mission", world] : []),
    ], { cwd: root, env: toolEnv() });
    process.stdout.write(res.stdout + res.stderr);
    return res.status;
  });
}

async function cmdDev(tier = "all", root = ROOT, opts = {}) {
  // Fast dev loop: skip the headless Workbench build (the dedicated server
  // compiles scripts at boot from the unpacked addons), run the in-game suite,
  // then dump the per-feature [LFDebug:*] lines from the same log.
  guardHeavy(root, "dev", opts.force);
  console.log("=== dev: validate ===");
  const v = await cmdRunArea("validate", root, { incremental: true });
  if (v !== 0) return v;
  console.log("\n=== dev: test (no build) ===");
  process.env.LF_TEST_DEBUG_LINES = "1"; // dev is the verb that dumps the feature lines
  return cmdTest(true, tier, root, opts);
}

async function cmdCi(root = ROOT, opts = {}) {
  guardHeavy(root, "ci", opts.force);
  const steps = [
    ["validate --full", () => cmdRunArea("validate", root, { full: true })],
    ["build", () => cmdBuild(root, { wait: opts.wait })],
    ["test", () => cmdTest(true, "all", root, opts)],
    // After the suite: a branch that moved or added world props must pass the placement audit. It runs
    // through the same test lane as "test" (queues, never collides with a session).
    ["placement audit", async () => (await import("./lib/placement-gate.mjs")).runPlacementGate(changedFiles(root), async (a) => (await import("../agent-box/place-audit.mjs")).main(["placement", ...a]))],
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

// Tool runs live on the private LF-Agents desktop (tools/lib/quiet-launch.mjs),
// so nobody can see or close them by hand. This lists them; --close ends them.
function cmdHidden(flags) {
  const procs = listHidden();
  if (procs.length === 0) {
    console.log("hidden: no windows on the LF-Agents desktop");
    return 0;
  }
  for (const p of procs) console.log(`  pid ${p.pid}  ${p.name}  started ${p.started}`);
  if (!flags.includes("--close")) {
    console.log("close them with: tools\\cli hidden --close");
    return 0;
  }
  for (const p of procs) {
    const res = sh("taskkill", ["/PID", String(p.pid), "/T", "/F"]);
    console.log(`${res.status === 0 ? "closed" : "FAIL  could not close"} pid ${p.pid} ${p.name}`);
  }
  return 0;
}

// Default help is the command names; `help --verbose` is the full usage (docs/agent-budget.md).
function compactNames(runFull, header, strip, extra = []) {
  if (process.env.LF_OUTPUT === "full") return runFull();
  const lines = [];
  const orig = console.log;
  console.log = (...a) => lines.push(a.join(" "));
  let code;
  try { code = runFull(); } finally { console.log = orig; }
  const names = [];
  for (const l of lines.join("\n").split("\n")) {
    const m = /^ {2}(?! )(.*?)(?: {2,}|$)/.exec(l);
    if (!m) continue;
    const w = m[1].replace(strip, "").split(/\s+/)[0];
    if (/^[a-z][a-z0-9-]*$/.test(w) && !names.includes(w)) names.push(w);
  }
  for (const n of extra) if (/^[a-z][a-z0-9-]*$/.test(n) && !names.includes(n)) names.push(n);
  console.log(header);
  let row = " ";
  for (const n of names) {
    if (row.length + n.length > 100) { console.log(row); row = " "; }
    row += ` ${n}`;
  }
  if (row.trim()) console.log(row);
  console.log("full usage: add --verbose; a command run with no arguments prints its own usage");
  return code;
}
const CLI_NAME = "tools" + String.fromCharCode(92) + "cli";
function cmdHelp() { return compactNames(cmdHelpFull, `${CLI_NAME} <command> [args]; commands:`, /^$/, [...Object.keys(cmds), ...(existsSync(join(ROOT, "tools", "commands")) ? readdirSync(join(ROOT, "tools", "commands")).filter((n) => n.endsWith(".mjs")).map((n) => n.slice(0, -4)) : [])]); }
function wtHelp() { return compactNames(wtHelpFull, `${CLI_NAME} wt <command>; commands:`, /^tools.cli wt\s+/); }

function cmdHelpFull() {
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
  build [addon]                 headless Workbench build of LifeFramework (default) or LF_Everon1989 (worktree only)
  pack [addon] [--out dir] [--wait]  pack LifeFramework (default) or LF_Everon1989 (name or GUID), no upload
                                (default tmp/workshop-pack/<addon>; docs/playtest/workshop-host.md)
  serve                         boot the headless test server (blocks, worktree only)
  playtest [stop|status|shortcut]  the maintainer's one command: host from the pinned origin/main
                                checkout (..\\Life-Framework-playtest), then open the game joined to it;
                                stop closes the game and saves; shortcut adds a desktop icon;
                                server <address>|off points the download's launcher at a host (PR to
                                website/public/playtest-server.json, tools/release/playtest-server.mjs)
                                release [--tag playtest-m2-N]: refresh the rdb, attach it to a pre-release,
                                start the playtest-release workflow (worktree at origin/main)
  host start|stop|status|backup  playtest dedicated server on Missions/LF_PlaytestTown.conf;
                                start continues the latest save, stop saves and backs up
                                (docs/playtest.md; start refuses in the main checkout).
                                start refuses a save from another game build until a preflight passes
  host preflight                after a game update, before opening: back up, load the save on
                                the installed build, stop; restores the backup on any failure
                                (docs/ops/game-updates.md)
  host restore [<backup>]       put the newest (or named) backup back; the replaced saves are kept
  test [--no-build] [--tier fast|all|persistence] [--only <name>[,<name>...]] [--world <path.ent>] [--any|--local|--remote [name]|--machine <name>]
                                build + boot server + parse LFTEST results (worktree only).
                                --only runs just those tests (class, GetName(), or test file
                                name like LF_Test_Money); SUMMARY tier=only, any tier allowed.
                                The test pool runs it on the first free slot of any machine
                                (--any, default); --local / --remote [name] / --machine narrow it.
                                Remote runs carry uncommitted state too (docs/worktrees.md)
  e2e [--local|--remote [name]] [--legacy-save] [--old-build]
                                two-boot save, restart, and reload proof in this worktree;
                                --legacy-save writes the save before #331 (version 23) in boot 1;
                                --old-build stamps boot 1's save as game build 1.8.0.13 (#980)
  --budget <s>                  run-time budget for a known-long test/e2e run (default: learned from the pool's history)
  --priority high|normal|low    on test, e2e, dev, ci, session up, wt gate/ship: queue order in the pool
                                (default normal; LF_PRIORITY or 'wt new --priority' set a worktree default)
  pool status|stats|prune       test pool: machines, lanes and holders, queue, last runs;
                                prune removes remote run folders of gone worktrees
  testbox provision|status|prune|screenshot|record|start|vdd  set up / inspect / clean / film the second test machine
  dev [--tier fast|all|persistence]  fast dev loop: validate + test --no-build +
                                dump [LFDebug:*] feature log lines (worktree only)
  ci                            validate --full + build + test (full gate, worktree only)
  wt <command>                  worktree isolation + auto-merge (see: cli wt help)
  release <x.y.z> [--dry-run|--confirm [--push]]  HUMAN step: dry run by default; --confirm stamps both .gproj and kits,
                                runs the gates, tags vX.Y.Z locally; never publishes (docs/worktrees.md "Release gates"; agents dry-run only)
  promote [--to sha] [--confirm]  stable branch: list PRs merged since stable and write the checklist;
                                --confirm fast-forwards stable, verifies and closes their issues (cli promote --help)
  prs status|heal [--dry-run] [<n>...]
                                open ws/* PRs; heal merges main into idle DIRTY/BEHIND PRs
                                (generated conflicts only), gates, pushes, re-queues auto-merge,
                                and merges green PRs stuck open (see: cli prs help)
  film-make <storyboard.json> [--out f.mp4] [--keep] [--no-vo] [--shot ids] [--group name] [--preview] [--client n] [--no-shoot] | --setup | --selftest
                                post-production: cards, crossfades, captions + .srt, offline voice-over,
                                ducked music bed (docs/film-pipeline.md "Post-production")
  config-check <config.json> [--strict] [--no-loops] [--json]
                                validate a server owner's config.json offline, as the server would at boot:
                                JSON syntax (line:col), unknown keys, types, ranges, schemaVersion, record keys,
                                profit loops; exit 1 on errors (docs/config/owner-config.md)
  regen-tests                   regenerate the test registry from the test files
  regen-localization            regenerate the runtime string tables from the .st source
  rdb                           register new .meta resources in this worktree's resourceDatabase.rdb
                                (headless Workbench, ~10 s; test/dev/serve/session/e2e run it when stale)
  asset new <slug> --kind weapon|gadget|deployable --prompt "..." | run <slug> [--from s] [--only s] [--force] | status | doctor | selftest
                                roadmap item to asset: Meshy generation (paid, capped per asset), headless Blender clean-up,
                                Workbench import, prefab scaffold; resumable per stage in tmp/assets/<slug>/ (docs/asset-pipeline/README.md)
  look <area | x,z | all> [--edit] [--close N] [--hour h] [--measure] [--wait] | item <prefab|slug> [--states ground,inventory] | selftest
                                area pictures with no session: a hidden Workbench plays DebugWorld, shoots a wide,
                                three obliques, a top view and close-ups, and writes tmp/look/<area>/contact.jpg
                                (about 20 s; docs/visual-testing.md)
  logs <pattern> [--before N] [--after N] [--limit N] [--tail] [--scope test|serve|build|workbench]
                                grep the newest server console.log (default after=3 limit=8)
  perf [--log <file>] [test flags]   run the 128-character cost benchmark (LF_Test_PerfBudget), print the report vs Configs/Perf/LF_PerfBudget.conf; exit 1 over budget
  suite-stamp <tier>            print the newest tmp/test-results/summaries.jsonl line for a tier
  suite-valid <sha>             exit 0 when every file since <sha> is localization, the generated
                                test registry, or docs/features.md; exit 1 for gameplay, a
                                persistence serializer, a prefab, or a world layer
  scope <name> <dir>            write a test stub, a features-row snippet, localization keys
                                to append, and one street-manifest row into <dir>
  scope --selftest              run that write in a temp directory and check the four outputs
  discord relay start|stop|status  Discord welcome relay as a background process (pid + log in tmp/);
                                discord selftest checks its event filter (docs/community/onboarding.md)
  call <tool> '<json>'|@file    call an MCP tool directly (see tools/mcp-call.mjs)
  hidden [--close]              list (or close) tool processes on the private LF-Agents desktop
                                (Workbench/server runs started there never show a window)
  validate [--full|--all]       run the checks in tools/validation/ whose declared inputs changed since their last green
                                run in this worktree (it names how many it skipped); --all runs every check; --full also adds the
                                Workbench compile (validate-scripts.ps1), which ci/gate/ship run, always with every check

servers: ${Object.keys(SERVERS).join(", ")}
`);
  return 0;
}

// ------------------------------------------------------------------- main

const [cmd, ...rest] = process.argv.slice(2);

// First argument that is neither a flag nor the value of one of `valueFlags`.
function firstPositional(flags, valueFlags = []) {
  for (let i = 0; i < flags.length; i++) {
    if (valueFlags.includes(flags[i])) { i++; continue; }
    if (!flags[i].startsWith("--")) return flags[i];
  }
  return null;
}

function argValue(flags, name) {
  const i = flags.indexOf(name);
  if (i === -1 || i + 1 >= flags.length) return null;
  return flags[i + 1];
}

// --local pins a test run to this machine; --remote [host] sends it to the
// test box (host defaults to LF_TESTBOX). See tools/remote/testbox.mjs.
function priorityFlag(list) {
  const v = argValue(list, "--priority");
  if (v === null) return undefined;
  const p = pool.normalizePriority(v);
  if (!p) throw new Error(`--priority must be high, normal or low (got '${v}')`);
  return p;
}

function remoteFlags(list) {
  const out = { local: list.includes("--local"), any: list.includes("--any"), priority: priorityFlag(list) };
  const b = argValue(list, "--budget");
  if (b !== null) {
    const n = Number(b);
    if (!(n > 0)) throw new Error(`--budget must be a number of seconds (got '${b}')`);
    out.budget = n;
  }
  if (list.includes("--remote")) {
    const v = argValue(list, "--remote");
    out.remote = v && !v.startsWith("--") ? v : "";
  }
  const m = argValue(list, "--machine");
  if (m && !m.startsWith("--")) out.machine = m;
  return out;
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

// ---------------------------------------------------------------- log grep

// newest console.log under server/profile/<scope>/logs (the test server writes
// one logs_<timestamp>/ dir per boot; the newest file is the last run).
function newestConsoleLog(root, scope = "test") {
  const logsDir = join(root, "server", "profile", scope, "logs");
  if (!existsSync(logsDir)) return null;
  let newest = null;
  const walk = (dir) => {
    for (const e of readdirSync(dir, { withFileTypes: true })) {
      const p = join(dir, e.name);
      if (e.isDirectory()) walk(p);
      else if (e.isFile() && e.name === "console.log") {
        const m = statSync(p).mtimeMs;
        if (!newest || m > newest.mtime) newest = { path: p, mtime: m };
      }
    }
  };
  walk(logsDir);
  return newest?.path ?? null;
}

// Grep the newest test-server console.log. Mirrors the old hand-rolled
// Select-String one-liner agents used to type: newest log, regex match with
// after-context, capped results. Output is agent-greppable (file + line nums).
//
//   tools\cli logs <pattern> [--before N] [--after N] [--limit N] [--tail]
//                            [--scope test|serve|build|workbench]
function cmdLogs(flags, root = ROOT) {
  const list = flags ?? [];
  const pattern = list.find((a) => !a.startsWith("--") && !/^@/.test(a));
  if (!pattern) {
    console.log("usage: tools\\cli logs <pattern> [--before N] [--after N] [--limit N] [--tail] [--scope test|serve|build|workbench]");
    console.log("       grep the newest server console.log (default after=3 limit=8), regex match");
    return 1;
  }
  const scope = argValue(list, "--scope") ?? "test";
  const before = parseInt(argValue(list, "--before") ?? "0", 10) || 0;
  const after = parseInt(argValue(list, "--after") ?? "3", 10) || 0;
  const limit = parseInt(argValue(list, "--limit") ?? "8", 10) || 0;
  const tail = list.includes("--tail");
  let re;
  try {
    re = new RegExp(pattern);
  } catch (e) {
    console.log(`invalid pattern: ${e.message}`);
    return 1;
  }
  const file = newestConsoleLog(root, scope);
  if (!file) {
    console.log(`no console.log found under server/profile/${scope}/logs in ${root}`);
    console.log("       run 'cli test' or 'cli serve' first, or point --scope at the profile you used");
    return 1;
  }
  const lines = readFileSync(file, "utf8").split(/\r?\n/);
  const hits = [];
  for (let i = 0; i < lines.length; i++) {
    if (re.test(lines[i])) hits.push(i);
  }
  if (hits.length === 0) {
    console.log(`no matches of /${pattern}/ in ${file}`);
    return 0;
  }
  const shown = tail ? hits.slice(-limit) : hits.slice(0, limit);
  const full = list.includes("--full") || process.env.LF_OUTPUT === "full";
  console.log(`=== ${file}`);
  console.log(`${hits.length} match(es) of /${pattern}/ (before=${before} after=${after})${hits.length > limit ? `, showing ${tail ? "last" : "first"} ${limit}` : ""}`);
  // Merge overlapping context windows so context lines print once.
  const blocks = [];
  for (const i of shown) {
    const s = Math.max(0, i - before);
    const e = Math.min(lines.length - 1, i + after);
    const last = blocks[blocks.length - 1];
    if (last && s <= last.end + 1) last.end = Math.max(last.end, e);
    else blocks.push({ start: s, end: e, hits: new Set([i]) });
  }
  for (const b of blocks) {
    for (let i = b.start; i <= b.end; i++) {
      const marker = b.hits.has(i) ? ">" : " ";
      const text = full || lines[i].length <= 300 ? lines[i] : `${lines[i].slice(0, 299)}... (+${lines[i].length - 299} chars; --full)`;
      console.log(`${marker} ${i + 1}: ${text}`);
    }
    console.log("  ---");
  }
  return 0;
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

function wtHelpFull() {
  console.log(`worktree commands (parallel agent isolation)

  tools\\cli wt new <slug> [--wait] [--base B] [--priority low]  create Life-Framework-ws-<slug> at ws/<slug>, allocate ports;
                                       --priority records the pool priority for that worktree (spikes: low)
  tools\\cli wt list                   worktrees, ports, dirty/merged state
  tools\\cli wt test <slug> [--tier fast|all|persistence] [--only <names>] [--no-build] [--wait]
                                       run the LFTEST suite in that worktree (from anywhere)
  tools\\cli wt e2e <slug> [--legacy-save] [--old-build]
                                       two-boot save-state E2E: persistence save boot ended by the
                                       host stop file (SHUTDOWN save), then reload same profile
                                       with -loadSessionSave (latest = the shutdown save);
                                       --legacy-save: boot 1 writes the pre-#331 format (version 23)
                                       --old-build: boot 1 stamps game build 1.8.0.13; the build gate
                                       backs the save up and boot 2 must load it (#980)
  tools\\cli wt build <slug> [--wait]  headless Workbench build in that worktree (serialized)
  tools\\cli wt dev <slug> [--tier X]  fast loop (validate + test --no-build) in that worktree
  tools\\cli wt logs <slug> <pattern> [--before N] [--after N] [--limit N] [--tail] [--scope X]
                                       grep that worktree's newest console.log (from anywhere)
  tools\\cli wt gate <slug> [--wait]   validate --full + build + test (tier=all) in that worktree
  tools\\cli wt sync <slug>            merge the worktree's base (origin/main by default) into the branch
  tools\\cli wt sync-base <base> [--source main]
                                       PR that merges origin/<source> into origin/<base> (merge commit on a
                                       sync/ branch, never a force push); auto-merges when green
  tools\\cli wt ship <slug> [--refs N] [--body-file F] [--pr-only] [--skip-gate] [--skip-sync] [--full-gate] [--base B]
                                       clean-tree check -> merge origin/<base> only if it conflicts ->
                                       gate -> push -> PR (or reuse the open one) -> gh auto-merge
  tools\\cli wt prune <slug> [--force] remove a merged worktree + its branch (a squash-merged PR counts as merged)
  tools\\cli wt prune --merged [--yes]  list every merged, clean, idle (30 min), unlocked worktree; --yes prunes them
  tools\\cli wt open <slug>            open a terminal in the worktree
  tools\\cli wt open <slug> --workbench launch Workbench on that worktree
  tools\\cli wt main                   print the main checkout path

flags:
  --base B     branch to cut from and ship to (default main; 'wt new' records it for 'wt ship')
  --wait       queue behind the build/worktree lock instead of failing fast
  --pr-only    open the PR but leave it for review (no auto-merge)
  --refs N     start the PR body with 'Refs #N' (N >= 1; leave it out when there is no issue); --closes N is the same. Merging never closes an issue:
               it moves to in-dev / needs-playtest and 'promote --confirm' verifies and closes it
  --body-file F  PR body from F (e.g. tmp/pr-body.md in the worktree; end it with the attribution line)
  --skip-gate  / --skip-sync   skip a ship stage (debug only)
  --full-gate                  run the in-game gate even when addons/ is unchanged
                               (a branch without addons/ changes runs validate only)
  --force      prune without merge/dirty checks
`);
  return 0;
}

function wtNewCmd(args) {
  const slug = wtSlug(args);
  if (!slug) return 1;
  try {
    const r = wt.wtNew(ROOT, slug, { waitMs: args.includes("--wait") ? 60000 : 0, base: argValue(args, "--base") ?? "main", priority: priorityFlag(args) });
    console.log(`created worktree ${r.slug}${r.priority ? ` (pool priority ${r.priority})` : ""}`);
    console.log(`  base:   origin/${r.base}`);
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

async function wtListCmd() {
  const state = wt.reconcileState(ROOT);
  const rows = Object.values(state.worktrees ?? {});
  if (rows.length === 0) {
    console.log("no worktrees registered. create one:  tools\\cli wt new <feature>");
    return 0;
  }
  console.log("worktrees  (main checkout is reserved for the world editor)");
  const slugW = Math.max(4, ...rows.map((e) => e.slug.length)) + 2;
  const branchW = Math.max(6, ...rows.map((e) => e.branch.length)) + 2;
  const line = (e, text) => `  ${e.slug.padEnd(slugW)}${(e.branch + (wt.baseOf(e) !== "main" ? ` (->${wt.baseOf(e)})` : "")).padEnd(branchW + 10)}${`${e.ports.gamePort}/${e.ports.a2sPort}`.padEnd(14)}${text}`;
  console.log(`  ${"slug".padEnd(slugW)}${"branch".padEnd(branchW)}${"ports".padEnd(14)}state`);
  // One git call for every branch's ahead/behind, one for every checkout's
  // current branch, and the per-worktree status calls in parallel: a serial
  // five-calls-per-row loop took ~3 s with a dozen worktrees.
  const counts = new Map();
  const refs = wt.git(ROOT, ["for-each-ref", "refs/heads", "--format=%(refname:short) %(ahead-behind:origin/main)"]);
  for (const l of refs.stdout.split(/\r?\n/)) {
    const [name, a, b] = l.trim().split(" ");
    if (name && a !== undefined) counts.set(name, { ahead: parseInt(a, 10) || 0, behind: parseInt(b, 10) || 0 });
  }
  const checkedOut = new Map(wt.listWorktrees(ROOT).map((w) => [resolve(w.path), w.branch ?? ""]));
  const live = rows.filter((e) => existsSync(e.path));
  const statuses = await Promise.all(live.map((e) => new Promise((done) => {
    let out = "";
    const child = spawn("git", ["-C", e.path, "status", "--porcelain"], { windowsHide: true, stdio: ["ignore", "pipe", "ignore"] });
    child.stdout.on("data", (d) => { out += d; });
    child.on("error", () => done(""));
    child.on("close", () => done(out));
  })));
  const dirtyOf = new Map(live.map((e, i) => [e.path, statuses[i].trim() ? "dirty" : "clean"]));
  for (const e of rows) {
    if (!existsSync(e.path)) {
      console.log(line(e, "MISSING"));
      continue;
    }
    let c = counts.get(e.branch) ?? { ahead: 0, behind: 0 };
    if (wt.baseOf(e) !== "main") {
      const lr = wt.git(ROOT, ["rev-list", "--left-right", "--count", `${e.branch}...origin/${wt.baseOf(e)}`]);
      const [a, b] = lr.stdout.trim().split(/\s+/).map((n) => parseInt(n, 10));
      if (lr.status === 0) {
        c = { ahead: a || 0, behind: b || 0 };
        counts.set(e.branch, c);
      }
    }
    const ahead = c.ahead;
    const behind = c.behind;
    // The branch is an ancestor of origin/main exactly when it has no commits
    // of its own (ahead 0); a missing ref counts as not merged.
    const merged = counts.has(e.branch) && ahead === 0;
    const dirty = dirtyOf.get(e.path);
    const cur = checkedOut.get(resolve(e.path)) ?? "";
    // Uncommitted edits are never "prune me": they exist nowhere else.
    let stateText = `${dirty}${cur !== e.branch ? ` on ${cur}` : ""}${ahead > 0 ? ` +${ahead}` : ""}`;
    if (merged && ahead === 0 && dirty === "clean") stateText = behind > 0 ? "merged (prune me)" : "fresh (no commits yet)";
    else if (merged && ahead === 0) stateText = "dirty, uncommitted work (commit or discard before prune)";
    console.log(line(e, stateText));
  }
  return 0;
}

function wtSyncCmd(args) {
  const slug = wtSlug(args);
  if (!slug) return 1;
  try {
    const r = wt.wtSync(ROOT, slug);
    console.log(`synced ${r.slug}: merged origin/${r.base} into ${r.branch}`);
    return 0;
  } catch (e) {
    console.log(`FAIL  ${e.message}`);
    return 1;
  }
}

function wtPruneMergedCmd(args) {
  let plan;
  try {
    plan = wt.pruneCandidates(ROOT);
  } catch (e) {
    console.log(`FAIL  ${e.message}`);
    return 1;
  }
  for (const s of plan.skip) console.log(`  keep   ${s.slug}: ${s.why}`);
  for (const t of plan.take) console.log(`  prune  ${t.slug} (${t.via})`);
  if (!plan.take.length) {
    console.log("nothing to prune");
    return 0;
  }
  if (!args.includes("--yes")) {
    console.log(`dry run: ${plan.take.length} worktree(s) qualify; re-run with --yes to prune them`);
    return 0;
  }
  let failed = 0;
  for (const t of plan.take) {
    try {
      const r = wt.wtPrune(ROOT, t.slug);
      console.log(`pruned ${r.slug}: removed ${r.path} and branch ${r.branch}`);
    } catch (e) {
      failed++;
      console.log(`FAIL  ${t.slug}: ${e.message}`);
    }
  }
  if (!args.includes("--keep-remote")) {
    try { pool.cmdPoolPrune(ROOT, []); } catch (e) { console.log(`WARN  pool prune: ${e.message}`); }
  }
  return failed ? 1 : 0;
}

function wtPruneCmd(args) {
  if (args.includes("--merged")) return wtPruneMergedCmd(args);
  const slug = wtSlug(args);
  if (!slug) return 1;
  try {
    const r = wt.wtPrune(ROOT, slug, { force: args.includes("--force") });
    console.log(`pruned ${r.slug}: removed ${r.path} and branch ${r.branch}`);
    // Its run folders on the pool's remote machines go too (best effort; a
    // machine that is down keeps them until the next `tools\cli pool prune`).
    if (!args.includes("--keep-remote")) {
      try {
        pool.cmdPoolPrune(ROOT, []);
      } catch (e) {
        console.log(`WARN  pool prune: ${e.message}`);
      }
    }
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
    return runWorktreeCli(e.path, ["test", ...flags]);
  } catch (err) {
    console.log(`FAIL  ${err.message}`);
    return 1;
  }
}

// One boot of the test-e2e.ps1 harness in the target worktree. Mirrors the
// harness invocation inside cmdTest (same ports, same cwd/env) but exposes the
// profile-keep, -loadSessionSave, mission and host-stop flags the two-boot save-state E2E needs.
function runE2ePass(root, tier, opts = {}) {
  const harness = join(root, "tools", "test", "test-e2e.ps1");
  if (!existsSync(harness)) {
    console.log(`FAIL  harness not found: ${harness}`);
    return 1;
  }
  ensureResourceDatabase(root);
  const ports = wt.portsForRoot(root);
  const psArgs = [
    "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", harness,
    "-Tier", tier,
    "-BindPort", String(ports.gamePort),
    "-A2sPort", String(ports.a2sPort),
  ];
  if (laneListenAddr()) psArgs.push("-ListenAddr", laneListenAddr());
  psArgs.push(...budgetArgs());
  if (opts.keepProfile) psArgs.push("-KeepProfile");
  if (opts.loadLatestSave) psArgs.push("-LoadLatestSave");
  else if (opts.loadSessionSave !== undefined) psArgs.push("-LoadSessionSave", opts.loadSessionSave);
  if (opts.mission) psArgs.push("-Mission", opts.mission);
  if (opts.hostStop) psArgs.push("-HostStop");
  if (opts.hostStopAfter) psArgs.push("-HostStopAfter", opts.hostStopAfter);
  if (opts.extraDefine) psArgs.push("-ExtraDefine", opts.extraDefine);
  return withServerSlot(root, 0, () => {
    const res = sh(PWSH, psArgs, { cwd: root, env: toolEnv() });
    process.stdout.write(res.stdout + res.stderr);
    return res.status;
  });
}

function runWorktreeCli(root, args) {
  const cli = join(root, "tools", "cli.mjs");
  if (!existsSync(cli)) {
    console.log(`FAIL  worktree CLI not found: ${cli}`);
    return 1;
  }
  const result = spawnSync(process.execPath, [cli, ...args], {
    windowsHide: true,
    cwd: root,
    env: toolEnv(),
    stdio: "inherit",
  });
  return result.status ?? 1;
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
  return runE2e(e.path, { ...remoteFlags(args.slice(1)), legacySave: args.includes("--legacy-save"), oldBuild: args.includes("--old-build") });
}

// The two-boot save/restart proof in one pool slot: both boots run on the same
// machine, back to back, with nothing else on that slot in between.
async function runE2e(root, opts = {}) {
  ensureResourceDatabase(root);
  for (let attempt = 1; ; attempt++) {
    const r = await e2eOnce(root, opts, attempt >= pool.FAILOVER_ATTEMPTS);
    if (!r.requeue) return r.code;
  }
}

async function e2eOnce(root, opts, lastTry) {
  let slot;
  try {
    slot = await pool.acquireSlot(root, { ...poolMode(root, opts), purpose: "e2e", priority: opts.priority, budget: opts.budget });
  } catch (err) {
    console.log(`FAIL  ${err.message}`);
    return { code: 3 };
  }
  let code = 3;
  let extra = {};
  try {
    if (!slot.local) {
      const r = await testbox.runRemote(root, slot, { args: ["e2e", "--local", ...(opts.legacySave ? ["--legacy-save"] : []), ...(opts.oldBuild ? ["--old-build"] : []), ...(slot.budgetS ? ["--budget", String(slot.budgetS)] : [])], writeSyncState: pool.writeSyncState });
      code = r.code;
      if (!lastTry && pool.machineGone(root, slot, code)) {
        extra = { lost: true };
        return { code, requeue: true };
      }
    } else {
      activeBudget = { runS: slot.budgetS ?? opts.budget, bootS: slot.bootS };
      code = runE2eHere(root, opts.legacySave, opts.oldBuild);
    }
    return { code };
  } finally {
    slot.release();
    if (!slot.inherited) pool.recordRun(root, slot, { purpose: "e2e", code, extra });
  }
}

// legacySave (--legacy-save): both boots get LF_E2E_LEGACY_SAVE, so the save boot writes the
// save format before #331 (version 23) and the reload boot proves it loads into character slot 1.
// Both boots run the suite's mission header: on a bare world the engine writes no SHUTDOWN save
// (#830). Pass 1 ends through the host stop file, the path `host stop` takes, and must log the
// SHUTDOWN save before the feature statics reset; pass 2 loads that save (tools/test/e2e-shutdown.mjs).
// oldBuild (--old-build, #980): the save boot stamps its save as game build 1.8.0.13 (LF_E2E_OLD_BUILD,
// and the engine's meta-info.json is rewritten to match), the build gate sees an update and backs the
// save up beside the profile, and the reload boot on the real build must load it whole.
function runE2eHere(root, legacySave = false, oldBuild = false) {
  const profile = join(root, "server", "profile", "test");
  const e = { path: root };
  const legacy = [legacySave ? "LF_E2E_LEGACY_SAVE" : "", oldBuild ? "LF_E2E_OLD_BUILD" : ""].filter(Boolean).join(",");
  const mission = e2eShutdown.E2E_MISSION;
  console.log(`e2e: pass 1/2 - save boot (tier=persistence, fresh profile, ${mission}, ends by the host stop file so the engine writes the SHUTDOWN save${legacySave ? ", legacy save format" : ""}${oldBuild ? ", stamped as game build 1.8.0.13" : ""}) in ${e.path}`);
  const pass1Defines = [e2eShutdown.HOST_STOP_DEFINE, legacy].filter(Boolean).join(",");
  let pass1 = runE2ePass(e.path, "persistence", { mission, hostStop: true, hostStopAfter: e2eShutdown.MARKER_READY, extraDefine: pass1Defines });
  if (pass1 === 0) {
    const log = e2eShutdown.newestTestConsole(e.path);
    const check = e2eShutdown.checkShutdownSave(log ? readFileSync(log, "utf8") : "");
    for (const line of check.lines) console.log(`e2e: pass 1 ${line}`);
    if (!check.ok) {
      console.log(`e2e: FAILED in pass 1 (shutdown save): ${check.reason}`);
      console.log(`e2e: log ${log ?? "(none)"}`);
      pass1 = 1;
    } else {
      console.log("e2e: pass 1 shutdown save written after the host stop, statics reset after it");
    }
  }
  if (pass1 !== 0) {
    console.log(`e2e: FAILED in pass 1 (no shutdown save produced); not running pass 2`);
    console.log(`e2e: profile ${profile}`);
    return pass1;
  }
  if (oldBuild) {
    const fake = "1.8.0.13";
    const point = host.stampNewestSave(profile, fake);
    const gameBuild = host.exeBuild(serverExe());
    const gate = host.buildGate({ saveBuild: host.newestSaveBuild(profile), gameBuild });
    console.log(`e2e: old build: save point ${point ?? "(none)"} stamped ${fake}; ${gate.reason}`);
    const backup = gate.changed ? host.backupSaveRoot(profile, join(root, "server", "profile", "test-backups"), `build-${fake}-to-${gameBuild}`) : null;
    if (!backup || !host.findSavePoints(backup).length) {
      console.log(`e2e: FAILED old build: the build gate made no backup (${gate.reason})`);
      return 1;
    }
    console.log(`e2e: old build: backed up the save before the reload boot -> ${backup} (${host.findSavePoints(backup).length} save point(s))`);
  }
  const pass2Defines = ["LF_E2E_LOAD_BOOT", legacySave ? "LF_E2E_LEGACY_SAVE" : "", oldBuild ? "LF_E2E_EXPECT_OLD_BUILD" : ""].filter(Boolean).join(",");
  console.log(`e2e: pass 2/2 - reload boot (tier=persistence, KeepProfile, ${mission}, -loadSessionSave latest, ${pass2Defines})`);
  const pass2 = runE2ePass(e.path, "persistence", { mission, keepProfile: true, loadLatestSave: true, extraDefine: pass2Defines });
  const log2 = e2eShutdown.newestTestConsole(e.path);
  for (const line of e2eShutdown.loadLines(log2 ? readFileSync(log2, "utf8") : "")) console.log(`e2e: pass 2 ${line}`);
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
    return runWorktreeCli(e.path, ["dev", ...flags]);
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
    return runWorktreeCli(e.path, ["build", ...flags]);
  } catch (err) {
    console.log(`FAIL  ${err.message}`);
    return 1;
  }
}

// Grep the newest test-server console.log in a worktree's profile, from anywhere.
async function wtLogsCmd(args) {
  const slug = wtSlug(args);
  if (!slug) return 1;
  const flags = args.slice(1);
  if (!flags.length || flags[0].startsWith("--")) {
    console.log("usage: tools\\cli wt logs <slug> <pattern> [--before N] [--after N] [--limit N] [--tail] [--scope test|serve|build|workbench]");
    return 1;
  }
  try {
    const e = wt.requireWorktree(ROOT, slug);
    return cmdLogs(flags, e.path);
  } catch (err) {
    console.log(`FAIL  ${err.message}`);
    return 1;
  }
}

// Only addons/ reaches the game. A branch that changes nothing there (docs,
// website, tools) gets the cheap gate: `validate` without the Workbench
// compile, build, or in-game suite, so it never queues for a pool slot.
function changedFiles(wtRoot) {
  const out = new Set();
  for (const args of [["diff", "--name-only", `origin/${wt.baseOfRoot(wtRoot)}...HEAD`], ["diff", "--name-only", "HEAD"]]) {
    const r = wt.git(wtRoot, args);
    if (r.status === 0) for (const f of r.stdout.split(/\r?\n/)) if (f.trim()) out.add(f.trim());
  }
  return [...out];
}

function gameChanged(wtRoot) {
  const r = wt.git(wtRoot, ["diff", "--name-only", `origin/${wt.baseOfRoot(wtRoot)}...HEAD`]);
  if (r.status !== 0) return true;
  const files = r.stdout.split(/\r?\n/).filter(Boolean);
  return files.some((f) => f.startsWith("addons/"));
}

function gateArgs(wtRoot, flags) {
  if (flags.includes("--full-gate") || gameChanged(wtRoot)) {
    console.log("=== gate: addons/ changed; validate --full + build + test tier=all ===");
    return ["ci", ...flags.filter((f) => f !== "--full-gate")];
  }
  console.log("=== gate: no addons/ changes; validate only (pass --full-gate to force the in-game gate) ===");
  return ["validate", "--all"];
}

async function wtGateCmd(args) {
  const slug = wtSlug(args);
  if (!slug) return 1;
  const flags = args.slice(1);
  try {
    const pr = priorityFlag(flags);
    if (pr) process.env.LF_PRIORITY = pr;
    const e = wt.requireWorktree(ROOT, slug);
    wt.git(e.path, ["fetch", "origin", "--prune"]);
    return runWorktreeCli(e.path, gateArgs(e.path, flags));
  } catch (err) {
    console.log(`FAIL  ${err.message}`);
    return 1;
  }
}

// Bring one long-lived branch into another through a PR: a worktree on a sync-*
// branch cut from origin/<base>, origin/<source> merged in (generated files
// resolved by the merge drivers), pushed, PR'd into <base>, auto-merge queued.
// Conflicts outside generated files stop with the file list; nothing is forced.
async function wtSyncBaseCmd(args) {
  const base = args[0];
  if (!base || base.startsWith("-")) {
    console.log("usage: tools\\cli wt sync-base <base> [--source main]");
    return 1;
  }
  const source = argValue(args, "--source") ?? "main";
  try {
    wt.checkBaseName(base);
    wt.checkBaseName(source);
    if (base === source) throw new Error("base and source are the same branch");
    wt.git(ROOT, ["fetch", "origin", "--prune"]);
    const behind = wt.git(ROOT, ["rev-list", "--count", `origin/${base}..origin/${source}`]);
    if (behind.status !== 0) throw new Error("cannot compare origin/" + base + " and origin/" + source + ": " + (behind.stderr || behind.stdout).trim());
    if (parseInt(behind.stdout.trim(), 10) === 0) {
      console.log("origin/" + base + " already contains origin/" + source + "; nothing to sync");
      return 0;
    }
    const stamp = new Date().toISOString().slice(0, 16).replace(/[-:T]/g, "");
    const slug = "sync-" + base.replace(/[^a-z0-9]+/gi, "-").toLowerCase() + "-" + stamp;
    const r = wt.wtNew(ROOT, slug, { base });
    const { healMerge } = await import("./prs.mjs");
    const m = healMerge(r.path, { base: "origin/" + source });
    if (m.status === "needs-human") {
      console.log("FAIL  origin/" + source + " does not merge into origin/" + base + " automatically: " + m.reason + (m.conflicted.length ? " (" + m.conflicted.join(", ") + ")" : ""));
      console.log("      resolve by hand: cd " + r.path + "; git merge origin/" + source + "; fix, commit; tools\\cli wt ship " + slug);
      return 1;
    }
    console.log("merged origin/" + source + " into " + r.branch + " (" + m.status + (m.resolved && m.resolved.length ? ": " + m.resolved.join(", ") : "") + ")");
    const push = wt.git(r.path, ["push", "-u", "origin", r.branch]);
    if (push.status !== 0) throw new Error("push: " + (push.stderr || push.stdout));
    const head = wt.git(r.path, ["rev-parse", "HEAD"]).stdout.trim();
    const pr = wt.createPrGh(r.path, {
      head: r.branch,
      base,
      title: "Sync " + source + " into " + base,
      body: "Merge origin/" + source + " into " + base + " (tools\\cli wt sync-base " + base + "). Merge commit, no force push.\n\n🤖 Generated with [Claude Code](https://claude.com/claude-code)",
    });
    console.log("PR: " + pr.url);
    wt.autoMergePrGh(r.path, pr.number);
    console.log("sync-base: auto-merge queued. Prune after merge: 'cli wt prune " + slug + "'");
    console.log("PR " + pr.url + ", head " + head);
    return 0;
  } catch (err) {
    console.log("FAIL  " + err.message);
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
  let closes = null;
  try {
    closes = wt.parseIssueFlag(flags);
  } catch (err) {
    console.log(`FAIL  ${err.message}`);
    return 1;
  }
  const bodyFile = argValue(flags, "--body-file");
  let e;
  let base;
  try {
    const pr = priorityFlag(flags);
    if (pr) process.env.LF_PRIORITY = pr;
    e = wt.requireWorktree(ROOT, slug);
    base = argValue(flags, "--base") ?? wt.baseOf(e);
    wt.checkBaseName(base);
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
  let customBody = null;
  if (bodyFile) {
    const p = resolve(wtRoot, bodyFile);
    if (!existsSync(p)) {
      console.log(`FAIL  --body-file not found: ${p}`);
      return 1;
    }
    customBody = readFileSync(p, "utf8").replace(/\r\n/g, "\n").trim();
  }
  wt.git(wtRoot, ["fetch", "origin", "--prune"]);
  const nCommits = parseInt(wt.git(wtRoot, ["rev-list", "--count", `origin/${base}..HEAD`]).stdout.trim() || "0", 10);
  if (nCommits === 0) {
    console.log(`FAIL  nothing to ship: ${e.branch} has no commits beyond origin/${base}`);
    return 1;
  }
  // The PR body is checked before the gate, so a missing picture or an open review
  // finding costs seconds, not a gate run (tools/lib/pr-quality.mjs).
  {
    const { bodyProblems, withReview } = await import("./lib/pr-quality.mjs");
    const changed = wt.git(wtRoot, ["diff", "--name-only", `origin/${base}...HEAD`]).stdout.split(/\r?\n/).filter(Boolean);
    const reviewPath = join(wtRoot, "tmp", `review-${slug}.md`);
    const reviewText = existsSync(reviewPath) ? readFileSync(reviewPath, "utf8") : null;
    const problems = bodyProblems({
      body: customBody ?? "",
      files: changed,
      exists: (p) => existsSync(join(wtRoot, p)),
      reviewText,
      hasBodyFile: customBody !== null,
    });
    if (problems.length) {
      console.log("FAIL  the PR is not ready to ship:");
      for (const p of problems) console.log(`  - ${p}`);
      return 1;
    }
    if (customBody !== null) customBody = withReview(customBody, reviewText);
  }
  // Merge main only when it conflicts. A clean branch goes up as is: the PR's
  // validate check and auto-merge cover the combination with main.
  if (!skipSync) {
    let conflicting;
    try {
      conflicting = wt.mergeWouldConflict(wtRoot, `origin/${base}`);
    } catch (err) {
      console.log(`FAIL  ${err.message}`);
      return 1;
    }
    if (conflicting) {
      console.log(`=== ship: origin/${base} conflicts; merging it ===`);
      try {
        wt.wtSync(ROOT, slug);
      } catch (err) {
        console.log(`FAIL  ${err.message}`);
        return 1;
      }
    } else {
      console.log(`=== ship: origin/${base} merges clean; no sync ===`);
    }
  }
  if (!skipGate) {
    const gateFlags = flags.includes("--full-gate") ? ["--wait", "--full-gate"] : ["--wait"];
    const code = runWorktreeCli(wtRoot, gateArgs(wtRoot, gateFlags));
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
  const head = wt.git(wtRoot, ["rev-parse", "HEAD"]).stdout.trim();
  const subjects = wt.commitSubjects(wtRoot, `origin/${base}..HEAD`);
  const title = subjects[0] ?? e.branch;
  let body = customBody ?? [
    `Automated ship from worktree \`${slug}\`.`,
    "",
    "Gate passed locally: `validate --full` + headless `build` + `test` (tier=all).",
    "",
    "## Commits",
    ...subjects.map((s) => `- ${s}`),
  ].join("\n");
  body = refsBody(body, closes);
  let pr;
  try {
    if (wt.ghAvailable()) {
      pr = wt.findOpenPrGh(wtRoot, e.branch);
      if (pr) console.log(`ship: PR already open, pushed ${head.slice(0, 7)} to it`);
      else pr = wt.createPrGh(wtRoot, { head: e.branch, title, body, base });
    } else {
      const token = process.env.GITHUB_TOKEN;
      if (!token) {
        console.log("FAIL  no PR path: install the GitHub CLI ('winget install GitHub.cli') or set GITHUB_TOKEN");
        return 1;
      }
      pr = await wt.createPrRest(wtRoot, { head: e.branch, title, body, token, base });
    }
  } catch (err) {
    console.log(`FAIL  PR creation: ${err.message}`);
    return 1;
  }
  console.log(`PR: ${pr.url}`);
  if (prOnly) {
    console.log("ship: PR left open for review (--pr-only)");
    console.log(`PR ${pr.url}, head ${head}`);
    return 0;
  }
  console.log("=== ship: auto-merge ===");
  try {
    if (wt.ghAvailable()) {
      const out = wt.autoMergePrGh(wtRoot, pr.number);
      console.log(`ship: auto-merge queued; GitHub merges once checks and reviews pass${out ? ` (${out})` : ""}`);
    } else {
      // The REST API has no auto-merge; this merges now.
      const m = await wt.mergePrRest(wtRoot, pr.number, e.branch, process.env.GITHUB_TOKEN);
      console.log(m.already ? "ship: PR already merged or no-op" : `ship: merged into ${base}`);
    }
  } catch (err) {
    console.log(`FAIL  merge: ${err.message} (PR is at ${pr.url})`);
    return 1;
  }
  console.log(`ship: done. If GitHub later reports CONFLICTING, re-run 'cli wt ship ${slug}'. Prune after merge: 'cli wt prune ${slug}'`);
  console.log(`PR ${pr.url}, head ${head}`);
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
    logs: () => wtLogsCmd(subArgs),
    sync: () => wtSyncCmd(subArgs),
    "sync-base": () => wtSyncBaseCmd(subArgs),
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
  build: (flags) => {
    const list = flags ?? [];
    return cmdBuild(ROOT, { force: list.includes("--force"), wait: list.includes("--wait"), addon: firstPositional(list, ["--out"]) });
  },
  pack: (flags) => {
    const list = flags ?? [];
    return cmdPack(ROOT, { force: list.includes("--force"), wait: list.includes("--wait"), out: argValue(list, "--out"), addon: firstPositional(list, ["--out"]) });
  },
  host: (flags) => cmdHost(flags),
  playtest: (flags) => cmdPlaytest(flags),
  session: (flags) => cmdSession(flags),
  shots: (flags) => shots.main(flags ?? []),
  audit: async (flags) => (await import("../agent-box/place-audit.mjs")).main(flags ?? []),
  place: async (flags) => (await import("./place/place.mjs")).main(flags ?? []),
  "config-check": async (flags) => (await import("./config/check.mjs")).main(flags ?? []),
  look: async (flags) => (await import("./visual/look.mjs")).main(flags ?? []),
  asset: async (flags) => (await import("./assets/pipeline/asset.mjs")).main(flags ?? [], { workbench: runWorkbenchPlugin }),
  serve: (flags) => cmdServe(ROOT, { force: (flags ?? []).includes("--force") }),
  dev: (flags) => {
    const list = flags ?? [];
    return cmdDev(argValue(list, "--tier") ?? "all", ROOT, { force: list.includes("--force"), wait: list.includes("--wait"), ...remoteFlags(list) });
  },
  test: (flags) => {
    const list = flags ?? [];
    const noBuild = list.includes("--no-build");
    const only = list.includes("--only") ? (argValue(list, "--only") ?? "") : undefined;
    return cmdTest(noBuild, argValue(list, "--tier") ?? "all", ROOT, { force: list.includes("--force"), wait: list.includes("--wait"), only, world: argValue(list, "--world"), ...remoteFlags(list) });
  },
  testbox: (flags) => testbox.cmdTestbox(ROOT, flags ?? []),
  ci: (flags) => cmdCi(ROOT, { force: (flags ?? []).includes("--force"), wait: (flags ?? []).includes("--wait"), ...remoteFlags(flags ?? []) }),
  e2e: (flags) => {
    guardHeavy(ROOT, "e2e", (flags ?? []).includes("--force"));
    return runE2e(ROOT, { ...remoteFlags(flags ?? []), legacySave: (flags ?? []).includes("--legacy-save"), oldBuild: (flags ?? []).includes("--old-build") });
  },
  pool: (flags) => pool.cmdPool(ROOT, flags ?? []),
  validate: (flags) => {
    const f = flags ?? [];
    return cmdRunArea("validate", ROOT, { full: f.includes("--full"), incremental: !f.includes("--full") && !f.includes("--all") });
  },
  "film-make": async (flags) => (await import("./film/post.mjs")).main(flags ?? []),
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
  "regen-localization": () => {
    const gen = join(ROOT, "tools", "validation", "regen-localization.ps1");
    if (!existsSync(gen)) {
      console.log(`FAIL  generator not found: ${gen}`);
      return 1;
    }
    const res = sh(PWSH, ["-NoProfile", "-ExecutionPolicy", "Bypass", "-File", gen, "-Write"]);
    process.stdout.write(res.stdout + res.stderr);
    return res.status;
  },
  logs: (flags) => cmdLogs(flags),
  "suite-stamp": (flags) => cmdSuiteStamp(flags),
  perf: (flags) => runPerf(ROOT, flags ?? []),
  "suite-valid": (flags) => cmdSuiteValid(flags),
  scope: (flags) => runScopeCli(flags),
  call: () => cmdMcpCall(rest),
  hidden: (flags) => cmdHidden(flags ?? []),
  rdb: () => ensureResourceDatabase(ROOT),
  "ui-image": () => cmdUiImage(rest),
  help: () => cmdHelp(),
};

function cmdSuiteStamp(flags) {
  const list = flags ?? [];
  if (list.includes("--selftest")) {
    const failures = selftestSuiteStamp();
    if (failures.length) {
      console.log("suite-stamp: FAIL");
      for (const failure of failures) console.log(`  ${failure}`);
      return 1;
    }
    console.log("suite-stamp: OK");
    return 0;
  }
  const tier = argValue(list, "--tier") ?? list.find((arg) => !arg.startsWith("--"));
  if (!tier) {
    console.log("usage: tools\\cli suite-stamp <tier>");
    return 1;
  }
  const file = join(ROOT, "tmp", "test-results", "summaries.jsonl");
  if (!existsSync(file)) {
    console.log(`suite-stamp: no summary for tier ${tier}`);
    return 1;
  }
  const line = newestSummaryLine(readFileSync(file, "utf8"), tier);
  if (!line) {
    console.log(`suite-stamp: no summary for tier ${tier}`);
    return 1;
  }
  console.log(line);
  return 0;
}

function cmdSuiteValid(flags) {
  const list = flags ?? [];
  if (list.includes("--selftest")) {
    const failures = selftestSuiteStamp();
    if (failures.length) {
      console.log("suite-valid: FAIL");
      for (const failure of failures) console.log(`  ${failure}`);
      return 1;
    }
    console.log("suite-valid: OK");
    return 0;
  }
  const sha = list.find((arg) => !arg.startsWith("--"));
  if (!sha) {
    console.log("usage: tools\\cli suite-valid <sha>");
    return 1;
  }
  const diff = sh("git", ["diff", "--name-only", sha], { cwd: ROOT });
  if (diff.status !== 0) {
    const detail = (diff.stderr || diff.stdout || "unknown revision").trim();
    console.log(`suite-valid: FAIL ${detail}`);
    return 1;
  }
  const untracked = sh("git", ["ls-files", "--others", "--exclude-standard"], { cwd: ROOT });
  const names = `${diff.stdout}\n${untracked.stdout}`
    .split(/\r?\n/)
    .map((line) => line.trim())
    .filter(Boolean);
  if (suiteValidCode(names) === 0) {
    console.log(`suite-valid: OK (${names.length} path(s) since ${sha})`);
    return 0;
  }
  console.log("suite-valid: FAIL");
  for (const name of names) {
    if (!isSuiteSafePath(name)) console.log(`  ${name}`);
  }
  return 1;
}

// Commands that live one file each in tools/commands/ (export `help` and `main(args, { root })`).
// Add a command there, not to `cmds` above: this file was edited by 66 commits in ten days.
function discoveredCommand(name) {
  return /^[a-z][a-z0-9-]*$/.test(name) && existsSync(join(ROOT, "tools", "commands", `${name}.mjs`));
}

async function discoveredHelp() {
  const dir = join(ROOT, "tools", "commands");
  if (!existsSync(dir)) return;
  console.log("\nMore commands (tools/commands/, one file each):");
  for (const f of readdirSync(dir).filter((n) => n.endsWith(".mjs")).sort()) {
    const mod = await import(`./commands/${f}`);
    if (mod.help) console.log(`  ${mod.help}`);
  }
}

const [a, b] = rest;
const HEAVY = ["build", "pack", "serve", "dev", "test", "ci", "e2e"];

let code;
try {
  if (cmd === "mcp") {
    const sub = cmds[a];
    code = sub ? sub(b) : (console.log(`unknown mcp subcommand: ${a}\n`), cmdHelp());
  } else if (cmd === "wt") {
    code = cmdWt(rest);
  } else if (cmd === "prs") {
    code = cmdPrs(ROOT, rest);
  } else if (cmd === "promote") {
    code = cmdPromote(ROOT, rest);
  } else if (cmd === "release") {
    code = await runRelease(ROOT, rest);
  } else if (cmd === "discord") {
    code = runDiscordCli(ROOT, rest);
  } else if (cmd && cmds[cmd]) {
    code = cmds[cmd](HEAVY.includes(cmd) || cmd === "validate" || cmd === "logs" || cmd === "host" || cmd === "playtest" || cmd === "session" || cmd === "shots" || cmd === "audit" || cmd === "place" || cmd === "look" || cmd === "asset" || cmd === "suite-stamp" || cmd === "perf" || cmd === "suite-valid" || cmd === "scope" || cmd === "testbox" || cmd === "pool" || cmd === "film-make" || cmd === "config-check" ? rest : a);
  } else if (cmd && discoveredCommand(cmd)) {
    // tools/commands/<cmd>.mjs: a new command is one file and touches no shared list.
    code = (await import(`./commands/${cmd}.mjs`)).main(rest, { root: ROOT });
  } else {
    if (cmd) console.log(`unknown command: ${cmd}\n`);
    code = cmdHelp();
    await discoveredHelp();
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
