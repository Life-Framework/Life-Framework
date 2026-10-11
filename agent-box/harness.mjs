#!/usr/bin/env node
// Agent-box harness. One command surface for the three pillars:
//   script — Enforce logic suite (worktree only)
//   world  — screenshot a world position in play mode
//   ui     — screenshot the current menu, after the same inputs a player uses
//
// Player actions are not a side door. `use` calls ScriptedUserAction.PerformAction
// after CanBeShownScript and CanBePerformedScript. `input` pulses an allowlisted
// menu action through InputManager.SetActionValue. `open DebugMenu` toggles the
// same menu the debug key opens.

import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { homedir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { bmpFileToPng, decodeBmp, syntheticBmp } from "./bmp.mjs";
import { checkChains, formatPlan, runChainProbes } from "./chains.mjs";
import { findPad, loadPads } from "./layers.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..");

const MENU_ACTIONS = new Set([
  "MenuBack",
  "MenuUp",
  "MenuDown",
  "MenuLeft",
  "MenuRight",
  "MenuSelect",
  "EL_OpenDebugMenu",
]);

export function profileBoxes() {
  const docs = join(homedir(), "Documents", "My Games");
  return [
    join(docs, "ArmaReforgerWorkbench", "profile", "agent-box"),
    join(docs, "ArmaReforger", "profile", "agent-box"),
  ];
}

function token(value, label) {
  if (!/^[A-Za-z0-9_]+$/.test(value)) {
    throw new Error(`${label} must be letters, digits, or underscore: ${value}`);
  }
  return value;
}

export function buildLine(id, spec) {
  if (!Number.isInteger(id) || id <= 0) throw new Error("id must be a positive integer");
  if (spec.verb === "use") {
    const cls = token(spec.className, "class");
    if (spec.entityName) return `${id} use ${cls} ${token(spec.entityName, "entity")}`;
    return `${id} use ${cls}`;
  }
  if (spec.verb === "interact") {
    if (spec.step !== "confirm" && spec.step !== "next" && spec.step !== "prev") {
      throw new Error(`interact step must be confirm, next, or prev: ${spec.step}`);
    }
    return `${id} interact ${spec.step}`;
  }
  if (spec.verb === "stand") {
    return `${id} stand ${token(spec.entityName, "entity")}`;
  }
  if (spec.verb === "look") {
    return `${id} look`;
  }
  if (spec.verb === "ui") {
    return `${id} ui`;
  }
  if (spec.verb === "click") {
    const widget = token(spec.widget, "widget");
    const index = spec.index === undefined ? 0 : Number(spec.index);
    if (!Number.isInteger(index) || index < 0) throw new Error(`click index must be a non-negative integer: ${spec.index}`);
    if (index === 0) return `${id} click ${widget}`;
    return `${id} click ${widget} ${index}`;
  }
  if (spec.verb === "input") {
    if (!MENU_ACTIONS.has(spec.action)) {
      throw new Error(`action not allowed: ${spec.action}. Menu actions only.`);
    }
    return `${id} input ${spec.action}`;
  }
  if (spec.verb === "open") {
    if (spec.menu !== "DebugMenu") throw new Error("only DebugMenu can be opened by name");
    const mode = spec.mode || "toggle";
    if (mode !== "show" && mode !== "hide" && mode !== "toggle") {
      throw new Error(`open mode must be show, hide, or toggle: ${mode}`);
    }
    if (mode === "toggle") return `${id} open DebugMenu`;
    return `${id} open DebugMenu ${mode}`;
  }
  if (spec.verb === "capture") {
    const name = token(spec.name, "capture name");
    if (spec.mode === "ui") return `${id} capture ui ${name}`;
    if (spec.mode !== "world") throw new Error("capture mode must be ui or world");
    const coords = [spec.x, spec.y, spec.z].map((n) => Number(n));
    if (coords.some((n) => Number.isNaN(n))) throw new Error("world capture needs x y z");
    return `${id} capture world ${name} ${coords.join(" ")}`;
  }
  throw new Error(`unknown verb: ${spec.verb}`);
}

function nextId() {
  return (Date.now() % 2000000000) || 1;
}

export function sendCommand(spec, timeoutMs = 8000) {
  const id = nextId();
  const line = buildLine(id, spec);
  for (const dir of profileBoxes()) {
    mkdirSync(dir, { recursive: true });
    rmSync(join(dir, "result.txt"), { force: true });
    writeFileSync(join(dir, "command.txt"), line + "\n", "utf8");
  }

  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    for (const dir of profileBoxes()) {
      const path = join(dir, "result.txt");
      if (!existsSync(path)) continue;
      const text = readFileSync(path, "utf8").trim();
      const parts = text.split(" ");
      if (Number(parts[0]) !== id) continue;
      const status = parts[1] ?? "";
      const detail = parts.slice(2).join(" ");
      if (status === "pending") continue;
      return { id, status, detail, line, raw: text };
    }
    Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, 200);
  }
  return {
    id,
    status: "error",
    detail: "no_session",
    line,
    raw: "",
  };
}

export function toolchainPaths() {
  const programFiles = process.env["ProgramFiles(x86)"] || "C:\\Program Files (x86)";
  const steam = join(programFiles, "Steam", "steamapps", "common");
  return {
    workbench: process.env.ENFUSION_WORKBENCH_PATH || join(steam, "Arma Reforger Tools"),
    game: process.env.ENFUSION_GAME_PATH || join(steam, "Arma Reforger"),
    server: process.env.ENFUSION_SERVER_PATH || join(steam, "Arma Reforger Server"),
    project: process.env.ENFUSION_PROJECT_PATH || join(ROOT, "addons"),
  };
}

function mcpEntry(distRelative) {
  const dist = join(ROOT, distRelative);
  if (!existsSync(dist)) return null;
  const paths = toolchainPaths();
  return {
    command: "node",
    args: [dist],
    env: {
      ENFUSION_WORKBENCH_PATH: paths.workbench,
      ENFUSION_GAME_PATH: paths.game,
      ENFUSION_SERVER_PATH: paths.server,
      ENFUSION_PROJECT_PATH: paths.project,
    },
  };
}

export function cursorMcpConfig() {
  const servers = {};
  const primary = mcpEntry("tools/mcp/enfusion-mcp-bk/dist/index.js");
  const workbench = mcpEntry("tools/mcp/enfusion-workbench-mcp/dist/index.js");
  if (primary) servers["enfusion-mcp"] = primary;
  if (workbench) servers["enfusion-workbench"] = workbench;
  servers["life-agent-box"] = {
    command: "node",
    args: [join(ROOT, "agent-box", "mcp-server.mjs")],
  };
  const windowsMcp = join(homedir(), ".local", "bin", "windows-mcp.exe");
  if (existsSync(windowsMcp)) {
    servers["windows-mcp"] = {
      command: windowsMcp,
      args: ["serve", "--tools", "Screenshot,Click,Shortcut,Wait,Move,Type,DisplayInventory"],
      env: {
        WINDOWS_MCP_SCREENSHOT_SCALE: "0.5",
        ANONYMIZED_TELEMETRY: "false",
        WINDOWS_MCP_DISABLE_FLASH: "1",
      },
    };
  }
  return { mcpServers: servers };
}

function mergeMcp(path, config) {
  let current = { mcpServers: {} };
  if (existsSync(path)) {
    try {
      current = JSON.parse(readFileSync(path, "utf8"));
    } catch {
      current = { mcpServers: {} };
    }
  }
  current.mcpServers = { ...(current.mcpServers ?? {}), ...config.mcpServers };
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, JSON.stringify(current, null, 2) + "\n", "utf8");
  return path;
}

function runNode(args, extraEnv) {
  const res = spawnSync("node", args, {
    cwd: ROOT,
    encoding: "utf8",
    env: extraEnv ? { ...process.env, ...extraEnv } : process.env,
  });
  if (res.stdout) process.stdout.write(res.stdout);
  if (res.stderr) process.stderr.write(res.stderr);
  return res.status ?? 1;
}

const DEBUG_WORLD = "Worlds/DebugWorld/DebugWorld.ent";

// The Workbench bridge caches editor mode per process and starts it as unknown.
// wb_play and wb_stop refuse until wb_state has run in that same process.
export function playSteps(mode) {
  const state = { tool: "wb_state", args: {} };
  const enter = [
    state,
    { tool: "wb_open_resource", args: { path: DEBUG_WORLD } },
    state,
    { tool: "wb_play", args: { debugMode: false, fullScreen: false } },
  ];
  if (mode === "play") return [[state, { tool: "wb_stop", args: {} }], enter];
  return [enter];
}

function workbenchEnv() {
  const paths = toolchainPaths();
  return {
    ENFUSION_WORKBENCH_PATH: paths.workbench,
    ENFUSION_GAME_PATH: paths.game,
    ENFUSION_SERVER_PATH: paths.server,
    ENFUSION_PROJECT_PATH: paths.project,
  };
}

function mcpBatch(steps) {
  const dir = join(ROOT, "tmp");
  mkdirSync(dir, { recursive: true });
  const file = join(dir, "agent-box-batch.json");
  writeFileSync(file, JSON.stringify(steps));
  const res = spawnSync("node", ["tools/mcp-call.mjs", "--batch", `@${file}`, "enfusion-workbench"], {
    cwd: ROOT,
    encoding: "utf8",
    env: { ...process.env, ...workbenchEnv() },
  });
  const text = `${res.stdout || ""}${res.stderr || ""}`;
  if (res.stdout) process.stdout.write(res.stdout);
  if (res.stderr) process.stderr.write(res.stderr);
  return { code: res.status ?? 1, text };
}

function workbenchMode(text) {
  const match = text.match(/\*\*Mode:\*\*\s*(\w+)/);
  return match ? match[1] : "";
}

export function enterPlay() {
  const state = mcpBatch([{ tool: "wb_state", args: {} }]);
  if (state.code !== 0) return state.code;
  const mode = workbenchMode(state.text);
  const batches = playSteps(mode);
  for (const steps of batches) {
    const result = mcpBatch(steps);
    if (result.code !== 0) return result.code;
  }
  return 0;
}

export function runScriptPillar(shouldRun) {
  if (!shouldRun) {
    return {
      code: 0,
      text: "script pillar: node tools/cli.mjs test --tier fast\nPass --run from a worktree. The main checkout refuses the suite.\n",
    };
  }
  if (isMainCheckout()) {
    return {
      code: 1,
      text: "refusing script pillar on the main checkout. Use a worktree: tools\\cli wt new <feature>\n",
    };
  }
  const res = spawnSync("node", ["tools/cli.mjs", "test", "--tier", "fast"], { cwd: ROOT, encoding: "utf8" });
  return { code: res.status ?? 1, text: `${res.stdout || ""}${res.stderr || ""}` };
}

export function isMainCheckout() {
  const res = spawnSync("git", ["rev-parse", "--git-dir"], { cwd: ROOT, encoding: "utf8" });
  const dir = (res.stdout || "").trim().replaceAll("/", "\\");
  if (!dir) return false;
  return !dir.toLowerCase().includes("\\worktrees\\");
}

export function setup() {
  const install = runNode(["tools/cli.mjs", "mcp", "install"]);
  const verify = runNode(["tools/cli.mjs", "mcp", "verify"]);
  const config = cursorMcpConfig();
  const projectConfig = mergeMcp(join(ROOT, ".cursor", "mcp.json"), config);
  const userConfig = mergeMcp(join(homedir(), ".cursor", "mcp.json"), config);
  const names = Object.keys(config.mcpServers).join(", ");
  console.log(`cursor mcp: ${projectConfig}`);
  console.log(`user mcp:   ${userConfig}`);
  console.log(`servers:    ${names}`);
  if (!config.mcpServers["enfusion-mcp"] || !config.mcpServers["enfusion-workbench"]) {
    console.log("WARN  an Enfusion MCP dist/index.js is missing. Re-run setup after install succeeds.");
  }
  return install === 0 && verify === 0 ? 0 : 1;
}

export function statusText() {
  const paths = toolchainPaths();
  const lines = [
    `repo: ${ROOT}`,
    `main checkout: ${isMainCheckout()}`,
    `workbench: ${paths.workbench} (${existsSync(paths.workbench) ? "ok" : "missing"})`,
    `game: ${paths.game} (${existsSync(paths.game) ? "ok" : "missing"})`,
    `server: ${paths.server} (${existsSync(paths.server) ? "ok" : "missing"})`,
    `enfusion-mcp dist: ${existsSync(join(ROOT, "tools/mcp/enfusion-mcp-bk/dist/index.js"))}`,
    `enfusion-workbench dist: ${existsSync(join(ROOT, "tools/mcp/enfusion-workbench-mcp/dist/index.js"))}`,
    `profiles: ${profileBoxes().join(" | ")}`,
  ];
  return lines.join("\n");
}

function pause(ms) {
  Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
}

export function logRoots() {
  const docs = join(homedir(), "Documents", "My Games");
  return [join(docs, "ArmaReforgerWorkbench", "logs"), join(docs, "ArmaReforger", "logs")];
}

function captureFile(full) {
  if (existsSync(full)) return full;
  if (full.toLowerCase().endsWith(".png")) {
    const bmp = full.slice(0, -4) + ".bmp";
    if (existsSync(bmp)) return bmpFileToPng(bmp, full);
  }
  return "";
}

export function resolveCapture(detail) {
  if (!detail || !detail.includes("$logs:")) return "";
  const alias = detail.split(" ").find((part) => part.startsWith("$logs:"));
  if (!alias) return "";
  const rel = alias.slice("$logs:".length).replaceAll("/", "\\");
  for (const root of logRoots()) {
    const direct = captureFile(join(root, rel));
    if (direct) return direct;
    if (!existsSync(root)) continue;
    const sessions = readdirSync(root)
      .filter((name) => name.startsWith("logs_"))
      .sort()
      .reverse();
    for (const session of sessions) {
      const hit = captureFile(join(root, session, rel));
      if (hit) return hit;
    }
  }
  return "";
}

const FLOW_DIR = join(ROOT, "agent-box", "flows");

export function readFlow(name) {
  if (!/^[A-Za-z0-9_-]+$/.test(name)) throw new Error(`flow name must be letters, digits, hyphen, or underscore: ${name}`);
  const safe = name;
  const path = join(FLOW_DIR, `${safe}.txt`);
  if (!existsSync(path)) {
    const names = existsSync(FLOW_DIR)
      ? readdirSync(FLOW_DIR)
          .filter((file) => file.endsWith(".txt"))
          .map((file) => file.slice(0, -4))
      : [];
    throw new Error(`unknown flow ${name}. Known: ${names.join(", ") || "(none)"}`);
  }
  return readFileSync(path, "utf8").split(/\r?\n/);
}

export function runFlow(lines) {
  const out = [];
  let code = 0;
  for (const raw of lines) {
    const line = raw.trim();
    if (!line || line.startsWith("#")) continue;
    const [verb, ...rest] = line.split(/\s+/);
    let result = null;
    if (verb === "wait") {
      const ms = Number(rest[0]);
      if (!Number.isFinite(ms) || ms < 0 || ms > 30000) throw new Error(`bad wait: ${line}`);
      pause(ms);
      out.push(`wait ${ms}`);
      continue;
    }
    if (verb === "stand") result = sendCommand({ verb: "stand", entityName: rest[0] });
    else if (verb === "look") result = sendCommand({ verb: "look" });
    else if (verb === "ui") result = sendCommand({ verb: "ui" });
    else if (verb === "click") result = sendCommand({ verb: "click", widget: rest[0], index: rest[1] });
    else if (verb === "press") {
      const spec = PRESS[rest[0]];
      if (!spec) throw new Error(`press one of F, WheelUp, WheelDown, F10: ${line}`);
      result = sendCommand(spec);
    } else if (verb === "use") result = sendCommand({ verb: "use", className: rest[0], entityName: rest[1] });
    else if (verb === "input") result = sendCommand({ verb: "input", action: rest[0] });
    else if (verb === "open") result = sendCommand({ verb: "open", menu: rest[0], mode: rest[1] });
    else if (verb === "layer") {
      const played = playLayer(rest[0], rest[1]);
      out.push(...played.lines);
      if (played.code !== 0) code = played.code;
      continue;
    } else if (verb === "shot" && rest[0] === "ui") {
      result = sendCommand({ verb: "capture", mode: "ui", name: rest[1] }, 25000);
    } else if (verb === "shot" && rest[0] === "world") {
      const target = resolveWorldTarget(rest[1], rest[2], rest[3], rest[4]);
      result = sendCommand(
        { verb: "capture", mode: "world", name: target.name, x: target.x, y: target.y, z: target.z },
        25000,
      );
    } else {
      throw new Error(`unknown flow step: ${line}`);
    }
    const text = result.raw || `${result.id} ${result.status} ${result.detail}`;
    out.push(text);
    const image = resolveCapture(result.detail);
    if (image) out.push(`image: ${image}`);
    if (result.status !== "ok") code = 1;
  }
  return { code, lines: out };
}

export function pads() {
  return loadPads(ROOT);
}

export function formatPads(list = pads()) {
  if (!list.length) return "no interactable pads in DebugWorld layers";
  return list
    .map((pad) => `${pad.name}  layer=${pad.layer}  actions=${pad.actions.join(",")}  at ${pad.x} ${pad.y} ${pad.z}`)
    .join("\n");
}

function resolveWorldTarget(name, x, y, z) {
  if (x !== undefined && y !== undefined && z !== undefined) {
    return { name, x, y, z };
  }
  const pad = findPad(pads(), name);
  if (!pad) {
    throw new Error(`unknown world target ${name}. Pass x y z, or use an entity from 'layers'.`);
  }
  return { name: pad.name, x: pad.x, y: pad.y, z: pad.z };
}

const PRESS = {
  F: { verb: "interact", step: "confirm" },
  WheelUp: { verb: "interact", step: "next" },
  WheelDown: { verb: "interact", step: "prev" },
  F10: { verb: "open", menu: "DebugMenu" },
};

function waitForAction(className, entityName, timeoutMs = 5000) {
  const deadline = Date.now() + timeoutMs;
  let last = null;
  while (Date.now() < deadline) {
    last = sendCommand({ verb: "look" }, 2500);
    if (last.status !== "ok") return last;
    const hit = last.detail.includes(`${className}@${entityName}`) || last.detail.split(",").includes(className);
    if (hit) return last;
    pause(300);
  }
  return last;
}

export function playLayer(name, actionName) {
  const pad = findPad(pads(), name);
  if (!pad) {
    console.error(`unknown layer entity ${name}`);
    console.error(formatPads());
    return { code: 1, lines: [`unknown layer entity ${name}`] };
  }
  const action = actionName || pad.actions[0];
  const lines = [];
  const stood = sendCommand({ verb: "stand", entityName: pad.name });
  lines.push(stood.raw || `${stood.id} ${stood.status} ${stood.detail}`);
  if (stood.status !== "ok") return { code: 1, lines, pad };
  if (action) {
    const seen = waitForAction(action, pad.name);
    lines.push(seen.raw || `${seen.id} ${seen.status} ${seen.detail}`);
    if (seen.status !== "ok" || !seen.detail.includes(action)) return { code: 1, lines, pad };
    const used = sendCommand({ verb: "use", className: action, entityName: pad.name });
    lines.push(used.raw || `${used.id} ${used.status} ${used.detail}`);
    if (used.status !== "ok") return { code: 1, lines, pad };
    pause(400);
  }
  const shot = sendCommand({ verb: "capture", mode: "ui", name: pad.name }, 25000);
  lines.push(shot.raw || `${shot.id} ${shot.status} ${shot.detail}`);
  const image = resolveCapture(shot.detail);
  if (image) lines.push(`image: ${image}`);
  return { code: shot.status === "ok" ? 0 : 1, lines, pad };
}

function printResult(result) {
  console.log(result.raw || `${result.id} ${result.status} ${result.detail}`);
  console.log(`command: ${result.line}`);
  const image = resolveCapture(result.detail);
  if (image) console.log(`image: ${image}`);
  return result.status === "ok" ? 0 : 1;
}

function selftest() {
  const check = spawnSync(process.execPath, ["--check", join(ROOT, "agent-box", "mcp-server.mjs")], { encoding: "utf8" });
  if ((check.status ?? 1) !== 0) throw new Error(check.stderr || "life-agent-box MCP server failed to parse");
  const use = buildLine(4, { verb: "use", className: "EL_ShopAction", entityName: "Trader_01" });
  if (use !== "4 use EL_ShopAction Trader_01") throw new Error(use);
  const input = buildLine(5, { verb: "input", action: "MenuSelect" });
  if (input !== "5 input MenuSelect") throw new Error(input);
  const world = buildLine(8, { verb: "capture", mode: "world", name: "atm_pad", x: 128, y: 1, z: 133 });
  if (world !== "8 capture world atm_pad 128 1 133") throw new Error(world);
  const interact = buildLine(9, { verb: "interact", step: "confirm" });
  if (interact !== "9 interact confirm") throw new Error(interact);
  const stand = buildLine(10, { verb: "stand", entityName: "ATM_Debug" });
  if (stand !== "10 stand ATM_Debug") throw new Error(stand);
  const list = pads();
  const atm = findPad(list, "ATM_Debug");
  if (!atm || !atm.actions.includes("EL_ATMAction")) throw new Error("ATM_Debug missing from layer catalog");
  const shop = findPad(list, "Shop_GeneralStore_Debug");
  if (!shop || shop.actions[0] !== "EL_ShopAction") throw new Error("Shop_GeneralStore_Debug missing from layer catalog");
  if (shop.x !== 142 || shop.z !== 130) throw new Error(`shop coords ${shop.x} ${shop.z}`);
  const apple = findPad(list, "AppleTradeContainer");
  if (!apple || apple.x !== 91.4 || apple.z !== 140.7) throw new Error(`apple coords ${apple && apple.x} ${apple && apple.z}`);
  const police = findPad(list, "PoliceStation_Debug");
  if (!police || police.x !== 170 || police.z !== 120) throw new Error(`police coords ${police && police.x} ${police && police.z}`);
  const pile = findPad(list, "CementPile_Debug");
  if (!pile || !pile.actions.includes("EL_GatherAction")) throw new Error("CementPile_Debug gather missing");
  const ore = findPad(list, "IronOre_Debug");
  if (!ore || !ore.actions.includes("EL_GatherAction")) throw new Error("IronOre_Debug gather missing");
  const counter = findPad(list, "TraderChainCounter");
  if (!counter || !counter.actions.includes("EL_OpenTraderAction")) throw new Error("TraderChainCounter missing");
  const saw = findPad(list, "Sawmill_Debug");
  if (!saw || !saw.actions.includes("EL_ProcessAction")) throw new Error("Sawmill_Debug process missing");
  let rejected = false;
  try {
    buildLine(1, { verb: "input", action: "MoveForward" });
  } catch {
    rejected = true;
  }
  if (!rejected) throw new Error("movement action was accepted");
  const bmp = decodeBmp(syntheticBmp());
  if (bmp.width !== 2 || bmp.height !== 2) throw new Error("bmp size");
  if (bmp.pixels[0] !== 255 || bmp.pixels[1] !== 0 || bmp.pixels[2] !== 0) throw new Error("bmp top pixel");
  const shown = buildLine(11, { verb: "open", menu: "DebugMenu", mode: "show" });
  if (shown !== "11 open DebugMenu show") throw new Error(shown);
  const click = buildLine(12, { verb: "click", widget: "CivilianButton" });
  if (click !== "12 click CivilianButton") throw new Error(click);
  const indexed = buildLine(13, { verb: "click", widget: "BuyButton", index: 1 });
  if (indexed !== "13 click BuyButton 1") throw new Error(indexed);
  const flow = readFlow("ui-debug-menu").filter((line) => line.trim() && !line.trim().startsWith("#"));
  if (!flow.includes("open DebugMenu show") || !flow.includes("shot ui debug_menu")) throw new Error(flow.join("|"));
  const intake = readFlow("ui-intake").filter((line) => line.trim() && !line.trim().startsWith("#"));
  if (!intake.includes("click CivilianButton") || !intake.includes("click TownButton")) throw new Error(intake.join("|"));
  const chain = checkChains(ROOT);
  if (!chain.ok) throw new Error(chain.errors.join("; "));
  const appleFlow = readFlow("chain-apple").filter((line) => line.trim() && !line.trim().startsWith("#"));
  if (!appleFlow.includes("use EL_GatherAction AppleGatherTree") || !appleFlow.includes("use EL_OpenTraderAction AppleTradeContainer")) {
    throw new Error(appleFlow.join("|"));
  }
  const editPlay = playSteps("edit");
  if (editPlay.length !== 1 || editPlay[0][0].tool !== "wb_state" || editPlay[0].at(-1).tool !== "wb_play") {
    throw new Error("edit play batch must read state before play");
  }
  console.log("selftest ok");
  return 0;
}

function usage() {
  console.log(`agent-box harness

  node agent-box/harness.mjs setup
  node agent-box/harness.mjs status
  node agent-box/harness.mjs selftest
  node agent-box/harness.mjs play
  node agent-box/harness.mjs layers
  node agent-box/harness.mjs layer <EntityName> [ActionClass]
  node agent-box/harness.mjs stand <EntityName>
  node agent-box/harness.mjs look
  node agent-box/harness.mjs ui
  node agent-box/harness.mjs click <WidgetName> [index]
  node agent-box/harness.mjs press F|WheelUp|WheelDown|F10
  node agent-box/harness.mjs use <ClassName> [EntityName]
  node agent-box/harness.mjs input <MenuAction>
  node agent-box/harness.mjs open DebugMenu
  node agent-box/harness.mjs shot ui <name>
  node agent-box/harness.mjs shot world <name> [x y z]
  node agent-box/harness.mjs flow <name>
  node agent-box/harness.mjs chains [--run]
  node agent-box/harness.mjs pillar script [--run]

Pillars
  script  Logic suite. --run refuses the main checkout (world-editor copy).
  world   Aim the play-mode camera and screenshot that spot.
  ui      Screenshot whatever menu is on screen. Open and navigate it first.

Play mode has to be running. Start it with: node agent-box/harness.mjs play
`);
}

function main(argv) {
  const [cmd, ...rest] = argv;
  try {
    if (cmd === "setup") return setup();
    if (cmd === "status") {
      console.log(statusText());
      return 0;
    }
    if (cmd === "selftest") return selftest();
    if (cmd === "play") return enterPlay();
    if (cmd === "chains") {
      if (!rest.includes("--run")) {
        const check = checkChains(ROOT);
        console.log(formatPlan());
        if (!check.ok) {
          for (const error of check.errors) console.error(error);
          return 1;
        }
        console.log("chains ok (static). Pass --run in a play session to stand, look, and use.");
        return 0;
      }
      const played = runChainProbes({
        send: (spec) => sendCommand(spec),
        pause,
        onLine: (line) => console.log(line),
      });
      return played.code;
    }
    if (cmd === "layers") {
      console.log(formatPads());
      return 0;
    }
    if (cmd === "layer") {
      const played = playLayer(rest[0], rest[1]);
      for (const line of played.lines) console.log(line);
      return played.code;
    }
    if (cmd === "stand") {
      return printResult(sendCommand({ verb: "stand", entityName: rest[0] }));
    }
    if (cmd === "look") {
      return printResult(sendCommand({ verb: "look" }));
    }
    if (cmd === "ui") {
      return printResult(sendCommand({ verb: "ui" }));
    }
    if (cmd === "click") {
      return printResult(sendCommand({ verb: "click", widget: rest[0], index: rest[1] }));
    }
    if (cmd === "press") {
      const spec = PRESS[rest[0]];
      if (!spec) throw new Error("press one of F, WheelUp, WheelDown, F10");
      return printResult(sendCommand(spec));
    }
    if (cmd === "use") {
      return printResult(sendCommand({ verb: "use", className: rest[0], entityName: rest[1] }));
    }
    if (cmd === "input") {
      return printResult(sendCommand({ verb: "input", action: rest[0] }));
    }
    if (cmd === "open") {
      return printResult(sendCommand({ verb: "open", menu: rest[0], mode: rest[1] }));
    }
    if (cmd === "shot" && rest[0] === "ui") {
      if (rest[1] === "debug_menu") {
        const opened = sendCommand({ verb: "open", menu: "DebugMenu", mode: "show" });
        if (opened.status !== "ok") return printResult(opened);
        pause(500);
      }
      return printResult(
        sendCommand({ verb: "capture", mode: "ui", name: rest[1] }, 25000),
      );
    }
    if (cmd === "shot" && rest[0] === "world") {
      const target = resolveWorldTarget(rest[1], rest[2], rest[3], rest[4]);
      return printResult(
        sendCommand(
          { verb: "capture", mode: "world", name: target.name, x: target.x, y: target.y, z: target.z },
          25000,
        ),
      );
    }
    if (cmd === "flow") {
      const played = runFlow(readFlow(rest[0]));
      for (const line of played.lines) console.log(line);
      return played.code;
    }
    if (cmd === "pillar" && rest[0] === "script") {
      const result = runScriptPillar(rest.includes("--run"));
      if (result.text) process.stdout.write(result.text.endsWith("\n") ? result.text : result.text + "\n");
      return result.code;
    }
    usage();
    return cmd ? 1 : 0;
  } catch (err) {
    console.error(err.message);
    return 1;
  }
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  process.exit(main(process.argv.slice(2)));
}
