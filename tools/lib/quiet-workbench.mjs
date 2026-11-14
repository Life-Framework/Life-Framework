// tools/lib/quiet-workbench.mjs - a rendering Workbench that nobody sees.
//
// The Workbench MCP launches Workbench itself the first time a wb_* tool finds
// no NET API, and that window lands in front of the user. tools/mcp-call.mjs
// calls ensureQuietWorkbench() first: it starts the same Workbench on the
// private LF-Agents desktop, so the MCP finds it connected and never launches
// its own. Play mode renders there; harness screenshots are real 3D
// (proof in docs/setup.md). LF_SHOW_WINDOWS=1 leaves the launch to the MCP.
//
// Opt-in dialog watcher (LF_WB_DIALOG_WATCHER=1 or { dialogWatcher: true }):
// tools/lib/wb-dialog-watcher.ps1 is started on LF-Agents beside the
// Workbench and answers "Register Resource File(s)" and "Script Authorization
// Required" for that one pid only, logging each answer to
// <checkout>/tmp/wb-dialog-watcher.jsonl. It exits with the Workbench, or on
// stopDialogWatcher().

import { cpSync, existsSync, mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import net from "node:net";
import { fileURLToPath } from "node:url";
import { quietSpawnDetached, showWindows } from "./quiet-launch.mjs";

const WATCHER = join(dirname(fileURLToPath(import.meta.url)), "wb-dialog-watcher.ps1");
const PWSH = join(process.env.SystemRoot || "C:\\Windows", "System32", "WindowsPowerShell", "v1.0", "powershell.exe");

const HANDLERS = join("Scripts", "WorkbenchGame", "EnfusionMCP");
const DEFAULT_PORT = 5775;
const STEAM = join(process.env["ProgramFiles(x86)"] || "C:\\Program Files (x86)", "Steam", "steamapps", "common");

// NET API tools need a live Workbench. wb_cli_run and wb_build_data spawn
// their own one-shot CLI Workbench instead.
export function needsWorkbench(tools) {
  return tools.some((t) => /^wb_/.test(t) && !/^wb_(cli_run|build_data)$/.test(t));
}

function listening(port) {
  return new Promise((done) => {
    const s = net.connect(port, "127.0.0.1");
    s.setTimeout(1500);
    s.on("connect", () => { s.destroy(); done(true); });
    s.on("timeout", () => { s.destroy(); done(false); });
    s.on("error", () => done(false));
  });
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// Start the modal-dialog watcher for one Workbench pid on LF-Agents. Only
// through quiet-launch: the watcher refuses to run on any other desktop, so
// with LF_SHOW_WINDOWS=1 (Workbench on the user's screen) there is none.
// Returns { pid, log, stopFile } or null.
export function startDialogWatcher({ workbenchPid, log, snapDir = "", logger = console.error } = {}) {
  if (process.platform !== "win32" || showWindows() || !workbenchPid || !log) return null;
  mkdirSync(dirname(log), { recursive: true });
  const stopFile = `${log}.stop-${workbenchPid}`;
  const args = ["-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File", WATCHER,
    "-WorkbenchPid", String(workbenchPid), "-Log", log, "-StopFile", stopFile];
  if (snapDir) args.push("-SnapDir", snapDir);
  try {
    const pid = quietSpawnDetached(PWSH, args, { cwd: dirname(WATCHER) });
    logger(`[quiet-workbench] dialog watcher pid ${pid} watches Workbench pid ${workbenchPid} on LF-Agents (log ${log})`);
    return { pid, log, stopFile };
  } catch (e) {
    logger(`[quiet-workbench] dialog watcher not started: ${e.message}`);
    return null;
  }
}

// Ask a watcher to exit (it checks the stop file every poll).
export function stopDialogWatcher(watcher) {
  if (!watcher?.stopFile) return;
  try { writeFileSync(watcher.stopFile, String(Date.now())); } catch { /* the watcher also exits with Workbench */ }
}

// serverDir: the MCP server's clone (its handler bundle is mod/<HANDLERS>).
// env: the environment the MCP server gets (ENFUSION_* paths and port).
// Returns a short status line; never throws, so the MCP can still fall back.
export async function ensureQuietWorkbench({ serverDir, env = process.env, timeoutMs = 240000, log = console.error, dialogWatcher } = {}) {
  if (process.platform !== "win32" || showWindows()) return "skip: LF_SHOW_WINDOWS=1 or not Windows";
  const port = Number(env.ENFUSION_WORKBENCH_PORT) || DEFAULT_PORT;
  if (await listening(port)) return `skip: a Workbench already serves NET API port ${port}`;

  const tools = env.ENFUSION_WORKBENCH_PATH || join(STEAM, "Arma Reforger Tools");
  const exe = [join(tools, "Workbench", "ArmaReforgerWorkbenchSteamDiag.exe"), join(tools, "ArmaReforgerWorkbenchSteamDiag.exe")].find(existsSync);
  const game = env.ENFUSION_GAME_PATH || join(STEAM, "Arma Reforger");
  const addons = env.ENFUSION_PROJECT_PATH;
  const gproj = addons ? join(addons, "LifeFramework", "LifeFramework.gproj") : "";
  // serverDir is the clone, or its dist/ when the entry came from .cursor/mcp.json.
  const bundle = serverDir
    ? [join(serverDir, "mod", HANDLERS), join(dirname(serverDir), "mod", HANDLERS)].find(existsSync) || join(serverDir, "mod", HANDLERS)
    : "";
  if (!exe || !existsSync(gproj) || !existsSync(bundle)) {
    return `skip: missing ${!exe ? "Workbench exe" : !existsSync(gproj) ? `gproj ${gproj}` : `handler bundle ${bundle}`}`;
  }

  // The MCP compiles its NET API handlers into the opened mod; do the same
  // copy it would do on launch (git-ignored folder).
  cpSync(bundle, join(dirname(gproj), HANDLERS), { recursive: true });
  const args = ["-gproj", gproj];
  if (existsSync(join(game, "addons"))) args.push("-addonsDir", join(game, "addons"));
  let pid;
  try {
    pid = quietSpawnDetached(exe, args, { cwd: dirname(exe) });
  } catch (e) {
    return `skip: ${e.message}`;
  }
  log(`[quiet-workbench] started Workbench pid ${pid} on the hidden LF-Agents desktop (${gproj})`);
  const watch = dialogWatcher ?? (env.LF_WB_DIALOG_WATCHER === "1" || process.env.LF_WB_DIALOG_WATCHER === "1");
  // <addons>/.. is the checkout the Workbench was opened from.
  // A PNG of each dialog goes to tmp/wb-dialog-snaps/ before it is answered (LF_WB_DIALOG_SNAPS overrides).
  const snapDir = env.LF_WB_DIALOG_SNAPS || process.env.LF_WB_DIALOG_SNAPS || join(dirname(addons), "tmp", "wb-dialog-snaps");
  if (watch) startDialogWatcher({ workbenchPid: pid, log: join(dirname(addons), "tmp", "wb-dialog-watcher.jsonl"), snapDir, logger: log });
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    await sleep(2000);
    if (await listening(port)) {
      // The port opens before the handler scripts finish compiling.
      await sleep(8000);
      return `started pid ${pid}; NET API ${port} up`;
    }
  }
  return `started pid ${pid}; NET API ${port} not up after ${timeoutMs / 1000}s`;
}
