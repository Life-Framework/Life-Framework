// tools/lib/quiet-workbench.mjs - a rendering Workbench that nobody sees.
//
// The Workbench MCP launches Workbench itself the first time a wb_* tool finds
// no NET API, and that window lands in front of the user. tools/mcp-call.mjs
// calls ensureQuietWorkbench() first: it starts the same Workbench on the
// private LF-Agents desktop, so the MCP finds it connected and never launches
// its own. Play mode renders there; harness screenshots are real 3D
// (proof in docs/setup.md). LF_SHOW_WINDOWS=1 leaves the launch to the MCP.

import { cpSync, existsSync } from "node:fs";
import { dirname, join } from "node:path";
import net from "node:net";
import { quietSpawnDetached, showWindows } from "./quiet-launch.mjs";

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

// serverDir: the MCP server's clone (its handler bundle is mod/<HANDLERS>).
// env: the environment the MCP server gets (ENFUSION_* paths and port).
// Returns a short status line; never throws, so the MCP can still fall back.
export async function ensureQuietWorkbench({ serverDir, env = process.env, timeoutMs = 240000, log = console.error } = {}) {
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
