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
import { existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { homedir, tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { bmpFileToPng, classifyMesh, classifyMeshFile, decodeBmp, decodePng, encodePng, syntheticBmp } from "./bmp.mjs";
import { checkChains, formatPlan, runChainProbes } from "./chains.mjs";
import { findPad, loadPads, loadPlaced } from "./layers.mjs";
import { judgeRoadmapRow, judgeStreet, menuActionFromLook, missingRoadmapNames, presentedMesh, renderRoadmapReport, roadmapSites, rowCamera, runRoadmapReport, selectRoadmapSites, siteLook, siteShotOffset } from "./report.mjs";

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
  if (spec.verb === "kit") {
    const step = token(spec.step, "kit step");
    if (step === "show" || step === "hide") return `${id} kit ${step}`;
    if (step === "line") return `${id} kit line ${token(spec.part, "part")} ${token(spec.text, "text")}`;
    if (step === "status") {
      return `${id} kit status ${token(spec.part, "part")} ${token(spec.label, "label")} ${token(spec.statusText, "status")} ${token(spec.action, "action")}`;
    }
    if (step === "quantity") {
      const total = Number(spec.total);
      const take = Number(spec.take);
      if (!Number.isInteger(total) || !Number.isInteger(take)) {
        throw new Error("quantity needs integer total and take");
      }
      return `${id} kit quantity ${token(spec.part, "part")} ${total} ${take}`;
    }
    throw new Error(`kit step must be show, hide, line, status, or quantity: ${step}`);
  }
  if (spec.verb === "capture") {
    const name = token(spec.name, "capture name");
    if (spec.mode === "ui") return `${id} capture ui ${name}`;
    if (spec.mode !== "world") throw new Error("capture mode must be ui or world");
    const coords = [spec.x, spec.y, spec.z].map((n) => Number(n));
    if (coords.some((n) => Number.isNaN(n))) throw new Error("world capture needs x y z");
    const line = `${id} capture world ${name} ${coords.map(trimNum).join(" ")}`;
    if (spec.ox === undefined && spec.oy === undefined && spec.oz === undefined) return line;
    const offset = [spec.ox, spec.oy, spec.oz].map((n) => Number(n));
    if (offset.some((n) => Number.isNaN(n))) throw new Error("camera offset needs ox oy oz");
    return `${line} ${offset.map(trimNum).join(" ")}`;
  }
  throw new Error(`unknown verb: ${spec.verb}`);
}

function trimNum(n) {
  return String(Math.round(Number(n) * 1000) / 1000);
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
    else if (verb === "kit") result = sendKit(rest);
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

function sendKit(rest) {
  const step = rest[0];
  if (step === "show" || step === "hide") return sendCommand({ verb: "kit", step });
  if (step === "line") return sendCommand({ verb: "kit", step, part: rest[1], text: rest[2] });
  if (step === "status") {
    return sendCommand({
      verb: "kit",
      step,
      part: rest[1],
      label: rest[2],
      statusText: rest[3],
      action: rest[4],
    });
  }
  if (step === "quantity") {
    return sendCommand({ verb: "kit", step, part: rest[1], total: rest[2], take: rest[3] });
  }
  throw new Error(`kit step must be show, hide, line, status, or quantity: ${step}`);
}

const KIT_SHOTS = ["kit-sheet", "kit-balance", "kit-status", "kit-keep", "kit-take", "kit-focus"];

function waitForSession(timeoutMs = 120000) {
  const deadline = Date.now() + timeoutMs;
  let last = null;
  while (Date.now() < deadline) {
    last = sendCommand({ verb: "ui" }, 3000);
    if (last.status === "ok") return last;
    pause(1000);
  }
  return last;
}

export function runKit() {
  const state = mcpBatch([{ tool: "wb_state", args: {} }]);
  if (state.code !== 0) return { code: state.code, lines: ["play failed"] };
  if (workbenchMode(state.text) !== "play") {
    const entered = enterPlay();
    if (entered !== 0) return { code: entered, lines: ["play failed"] };
    const ready = waitForSession();
    if (!ready || ready.status !== "ok") {
      const line = ready && ready.raw ? ready.raw : "no_session";
      return { code: 1, lines: [line] };
    }
  }
  const played = runFlow(readFlow("ui-kit"));
  const images = played.lines.filter((line) => line.startsWith("image:"));
  for (const name of KIT_SHOTS) {
    if (!images.some((line) => line.includes(name))) {
      played.lines.push(`missing image: ${name}`);
      played.code = 1;
    }
  }
  const listed = played.lines.some((line) => line.split(" ").slice(2).join(" ").split(",").includes("Button"));
  if (!listed) {
    played.lines.push("missing widget: Button");
    played.code = 1;
  }
  return played;
}

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
  const kitLine = buildLine(21, { verb: "kit", step: "line", part: "Balance", text: "400" });
  if (kitLine !== "21 kit line Balance 400") throw new Error(kitLine);
  const kitStatus = buildLine(22, {
    verb: "kit",
    step: "status",
    part: "Status",
    label: "Warrant",
    statusText: "Closed",
    action: "none",
  });
  if (kitStatus !== "22 kit status Status Warrant Closed none") throw new Error(kitStatus);
  const kitTake = buildLine(23, { verb: "kit", step: "quantity", part: "Quantity", total: 5, take: 4 });
  if (kitTake !== "23 kit quantity Quantity 5 4") throw new Error(kitTake);
  const kitFlow = readFlow("ui-kit").filter((line) => line.trim() && !line.trim().startsWith("#"));
  for (const name of ["kit-sheet", "kit-balance", "kit-status", "kit-keep", "kit-take", "kit-focus"]) {
    if (!kitFlow.some((line) => line.includes(name))) throw new Error(`ui-kit flow missing ${name}`);
  }
  if (!kitFlow.includes("kit show") || !kitFlow.includes("kit hide")) throw new Error(kitFlow.join("|"));
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
  const wideLine = buildLine(15, { verb: "capture", mode: "world", name: "roadmap_row", x: 300, y: 1, z: 121, ox: 0, oy: 40, oz: 90 });
  if (wideLine !== "15 capture world roadmap_row 300 1 121 0 40 90") throw new Error(wideLine);
  const street = judgeStreet([
    { name: "Roadmap_A", x: 210, y: 0, z: 120, look: "LF_BreachArmAction@Roadmap_A", mesh: "ok", menu: null, src: "shots/Roadmap_A.png" },
    { name: "Roadmap_B", x: 228, y: 0, z: 120, look: "none", mesh: "ok", menu: null, src: "shots/Roadmap_B.png" },
    { name: "Roadmap_Far", x: 228, y: 0, z: 200, look: "EL_RobAction@Roadmap_Far", mesh: "ok", menu: null, src: "shots/Roadmap_Far.png" },
  ]);
  const byName = Object.fromEntries(street.map((row) => [row.name, row]));
  if (byName.Roadmap_B.ok || !byName.Roadmap_B.failures.includes("look offers no interaction")) {
    throw new Error("empty look must fail");
  }
  if (byName.Roadmap_Far.ok || byName.Roadmap_Far.distanceM !== 80 || !byName.Roadmap_Far.failures.includes("more than 30 m from the rest of the street")) {
    throw new Error("entity 80 m away must fail");
  }
  if (!byName.Roadmap_A.ok) throw new Error("a close site with an interaction must pass");
  const selfActions = judgeRoadmapRow({
    name: "Roadmap_Garage",
    look: "SCR_LootDeadBodyAction,EL_ConfiscateAction",
    mesh: "ok",
    distanceM: 18,
    menu: null,
  });
  if (selfActions.ok || !selfActions.failures.includes("look offers no interaction")) {
    throw new Error("the character's own actions are not the site");
  }
  if (siteLook("Roadmap_Garage", "EL_VehicleStoreAction@Roadmap_Garage,SCR_LootDeadBodyAction") !== "EL_VehicleStoreAction@Roadmap_Garage") {
    throw new Error("a named site keeps only its own action");
  }
  const garageShot = siteShotOffset("Roadmap_Garage");
  const carShot = siteShotOffset("Roadmap_Escort_Car");
  if (!garageShot || garageShot.oz <= 0 || !carShot || carShot.oz <= 0) {
    throw new Error("the player camera faces -Z, so the row is photographed from the north");
  }
  if (!judgeRoadmapRow({ look: "EL_RobAction@X", mesh: "ok", distanceM: 30, menu: null }).ok) {
    throw new Error("30 m is still with the street");
  }
  const whiteRow = judgeRoadmapRow({ look: "EL_RobAction@X", mesh: "white", distanceM: 18, menu: null });
  if (whiteRow.ok || !whiteRow.failures.includes("mesh is white or missing")) throw new Error("white mesh must fail");
  const blankMenu = judgeRoadmapRow({
    look: "EL_OrganizationDeskAction@Roadmap_Org_Desk",
    mesh: "ok",
    distanceM: 18,
    menu: "none",
  });
  if (blankMenu.ok || !blankMenu.failures.includes("menu is blank")) throw new Error("blank menu must fail");
  if (menuActionFromLook("none") || menuActionFromLook("LF_BreachArmAction@Roadmap_Breach_Door")) {
    throw new Error("world actions are not menus");
  }
  if (menuActionFromLook("EL_OrganizationDeskAction@Roadmap_Org_Desk") !== "EL_OrganizationDeskAction@Roadmap_Org_Desk") {
    throw new Error("desk action opens a menu");
  }
  const whitePixels = Buffer.alloc(2 * 2 * 3, 255);
  if (classifyMesh({ width: 2, height: 2, pixels: whitePixels }) !== "white") throw new Error("white pixels");
  if (classifyMesh(decodePng(encodePng(2, 2, whitePixels))) !== "white") throw new Error("white png");
  if (classifyMesh(decodeBmp(syntheticBmp())) !== "ok") throw new Error("colored bmp must not be white");
  if (classifyMeshFile("") !== "missing") throw new Error("missing shot");
  const camera = rowCamera([
    { x: 228, y: 0, z: 120 },
    { x: 372, y: 0, z: 122 },
  ]);
  if (camera.oz < 40 || camera.oy < 18) throw new Error("row camera must step back and up");
  const placed = loadPlaced(ROOT).filter((site) => site.name.startsWith("Roadmap_"));
  if (!placed.some((site) => site.name === "Roadmap_Breach_Charge")) throw new Error("charge is part of the walk");
  const cash = placed.find((site) => site.name === "Roadmap_Bank_Bag_Cash");
  if (!cash || cash.x !== 189 || cash.z !== 122) throw new Error("bag cash world position");
  if (roadmapSites(ROOT).length < 12) throw new Error("roadmap walk is short");
  const page = renderRoadmapReport({
    wide: { src: "shots/roadmap-row.png", mesh: "ok" },
    rows: street,
    note: "selftest",
  });
  if (!page.includes("shots/roadmap-row.png") || !page.includes("Roadmap_Far") || !page.includes("look offers no interaction")) {
    throw new Error("report page must put the picture beside the text");
  }
  const farArticle = page.split("<article").find((part) => part.includes("Roadmap_Far"));
  const emptyArticle = page.split("<article").find((part) => part.includes("Roadmap_B"));
  if (!farArticle || !farArticle.includes("shots/Roadmap_Far.png") || !farArticle.includes("more than 30 m from the rest of the street")) {
    throw new Error("80 m picture must sit beside its failure");
  }
  if (!emptyArticle || !emptyArticle.includes("shots/Roadmap_B.png") || !emptyArticle.includes("look offers no interaction")) {
    throw new Error("empty look picture must sit beside its failure");
  }
  const painted = presentedMesh("Roadmap_Jail_Door", "white", "LF_BreachArmAction@Roadmap_Jail_Door");
  if (painted !== "painted-light") throw new Error("prison door with an action is painted-light");
  const paintedCell = presentedMesh("Roadmap_Jail_Cell", "white", "LF_JailArrestAction@Roadmap_Jail_Cell");
  if (paintedCell !== "painted-light") throw new Error("prison cell with an action is painted-light");
  if (presentedMesh("Roadmap_Jail_Door", "white", "none") !== "white") throw new Error("a white door with no action stays white");
  if (presentedMesh("Roadmap_Garage", "white", "EL_VehicleStoreAction@Roadmap_Garage") !== "white") {
    throw new Error("a white mesh on any other site stays a failure");
  }
  const paintedRow = judgeRoadmapRow({
    name: "Roadmap_Jail_Door",
    look: "LF_BreachArmAction@Roadmap_Jail_Door",
    mesh: painted,
    distanceM: 3,
    menu: null,
  });
  if (!paintedRow.ok) throw new Error("painted-light prison door must pass");
  const whiteMenu = judgeRoadmapRow({
    look: "EL_OrganizationDeskAction@Roadmap_Org_Desk",
    mesh: "ok",
    distanceM: 3,
    menu: "CloseButton",
    menuMesh: "white",
  });
  if (whiteMenu.ok || !whiteMenu.failures.includes("menu is blank")) throw new Error("a white menu shot is blank");
  const listedMenu = judgeRoadmapRow({
    look: "EL_OrganizationDeskAction@Roadmap_Org_Desk",
    mesh: "ok",
    distanceM: 3,
    menu: "CloseButton",
    menuMesh: "ok",
  });
  if (!listedMenu.ok) throw new Error("a menu with widgets and a picture passes");
  const picked = selectRoadmapSites(
    [
      { name: "Roadmap_Garage" },
      { name: "Roadmap_Jail_Door" },
      { name: "Roadmap_Escort_Car" },
    ],
    ["Roadmap_Garage", "Roadmap_Escort_Car"],
  );
  if (picked.length !== 2 || picked.some((site) => site.name === "Roadmap_Jail_Door")) {
    throw new Error("name filter must keep only the named row");
  }
  if (missingRoadmapNames(picked, ["Roadmap_Garage", "Roadmap_Missing"]).join(",") !== "Roadmap_Missing") {
    throw new Error("a missing name must be reported");
  }
  const scratch = mkdtempSync(join(tmpdir(), "roadmap-"));
  const samePng = join(scratch, "same.png");
  writeFileSync(samePng, encodePng(2, 2, Buffer.alloc(2 * 2 * 3, 40)));
  const stuckLogs = [];
  let stuckShots = 0;
  const stuckCode = runRoadmapReport({
    root: scratch,
    names: ["Roadmap_Garage", "Roadmap_Escort_Car", "Roadmap_Jail_Door"],
    sites: () => [
      { name: "Roadmap_Garage", layer: "Roadmap.layer", x: 186, y: 0, z: 108 },
      { name: "Roadmap_Escort_Car", layer: "Roadmap.layer", x: 204, y: 0, z: 108 },
      { name: "Roadmap_Jail_Door", layer: "Roadmap.layer", x: 192, y: 0, z: 120 },
    ],
    stand: () => ({ status: "ok", detail: "stood" }),
    look: () => ({ status: "ok", detail: "EL_VehicleStoreAction@Roadmap_Garage" }),
    ui: () => ({ status: "ok", detail: "CloseButton" }),
    use: () => ({ status: "ok" }),
    input: () => ({ status: "ok" }),
    captureWorld: () => {
      stuckShots++;
      return { status: "ok", detail: "$logs:el-captures/same.png" };
    },
    captureUi: () => ({ status: "ok", detail: "" }),
    resolve: () => samePng,
    pause: () => {},
    log: (line) => stuckLogs.push(line),
  });
  if (stuckShots !== 2) throw new Error(`camera_stuck must stop after the repeated shot, saw ${stuckShots}`);
  if (stuckLogs.filter((line) => line === "camera_stuck").length !== 1) throw new Error(stuckLogs.join(" | "));
  if (stuckCode !== 1) throw new Error("camera_stuck must fail the report");
  if (stuckLogs.some((line) => line.startsWith("pass Roadmap_Jail_Door") || line.startsWith("fail Roadmap_Jail_Door"))) {
    throw new Error("camera_stuck must not walk the next site");
  }
  const filterLogs = [];
  const filterSeen = [];
  runRoadmapReport({
    root: join(scratch, "filter"),
    names: ["Roadmap_Garage"],
    sites: () => [
      { name: "Roadmap_Garage", layer: "Roadmap.layer", x: 186, y: 0, z: 108 },
      { name: "Roadmap_Jail_Door", layer: "Roadmap.layer", x: 192, y: 0, z: 120 },
    ],
    stand: () => ({ status: "ok", detail: "stood" }),
    look: () => ({ status: "ok", detail: "EL_VehicleStoreAction@Roadmap_Garage" }),
    ui: () => ({ status: "ok", detail: "none" }),
    use: () => ({ status: "ok" }),
    input: () => ({ status: "ok" }),
    captureWorld: (spec) => {
      filterSeen.push(spec.name);
      return { status: "error", detail: "skipped" };
    },
    captureUi: () => ({ status: "ok", detail: "" }),
    resolve: () => "",
    pause: () => {},
    log: (line) => filterLogs.push(line),
  });
  if (filterSeen.includes("Roadmap_Jail_Door") || !filterSeen.includes("Roadmap_Garage") || !filterSeen.includes("roadmap_row")) {
    throw new Error(`filter walked ${filterSeen.join(",")}`);
  }
  const wide108 = rowCamera(
    [
      { name: "Roadmap_Garage", x: 186, y: 0, z: 108 },
      { name: "Roadmap_Escort_Car", x: 204, y: 0, z: 108 },
    ],
    [
      { name: "Roadmap_Garage", x: 186, y: 0, z: 108 },
      { name: "Roadmap_Escort_Car", x: 204, y: 0, z: 108 },
      { name: "Roadmap_Breach_Door", x: 186, y: 0, z: 120 },
    ],
  );
  if (wide108.oz <= 0 || wide108.z + wide108.oz <= 108) {
    throw new Error("Z=108 wide shot stands north of that row, the way the player camera faces");
  }
  const wideStreet = rowCamera([
    { name: "Roadmap_Breach_Door", x: 186, y: 0, z: 120 },
    { name: "Roadmap_Vehicle_Lot", x: 243, y: 0, z: 120 },
  ]);
  if (wideStreet.oz <= 0) throw new Error("the release street wide shot stays on +Z");
  const placedGarage = loadPlaced(ROOT).find((site) => site.name === "Roadmap_Garage");
  const placedCar = loadPlaced(ROOT).find((site) => site.name === "Roadmap_Escort_Car");
  if (!placedGarage || placedGarage.z !== 102.1 || placedGarage.x !== 186) throw new Error("garage use point is at 186 0 102.1");
  if (!placedCar || placedCar.z !== 108 || placedCar.x !== 204) throw new Error("escort car is at 204 0 108");
  const release = loadPlaced(ROOT).filter((site) => site.name.startsWith("Roadmap_") && site.z === 120);
  const releaseX = release.map((site) => site.x);
  if (Math.min(...releaseX) !== 186 || Math.max(...releaseX) !== 243) {
    throw new Error("release street stays on Z=120 from x=186 to x=243");
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
  node agent-box/harness.mjs kit
  node agent-box/harness.mjs shot ui <name>
  node agent-box/harness.mjs shot world <name> [x y z]
  node agent-box/harness.mjs flow <name>
  node agent-box/harness.mjs chains [--run]
  node agent-box/harness.mjs report roadmap [EntityName...]
  node agent-box/harness.mjs pillar script [--run]

Pillars
  script  Logic suite. --run refuses the main checkout (world-editor copy).
  world   Teleport the character to the camera offset and screenshot the player view.
  ui      Screenshot whatever menu is on screen. Open and navigate it first.
  kit     Photograph every UI kit state. Reuse a play session that is already up.

Play mode has to be running. Start it with: node agent-box/harness.mjs play
kit enters play only when a session is not already up.
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
    if (cmd === "kit") {
      const played = runKit();
      for (const line of played.lines) console.log(line);
      return played.code;
    }
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
    if (cmd === "report") {
      if (rest[0] !== "roadmap") throw new Error("report roadmap");
      const names = [];
      for (let i = 1; i < rest.length; i++) {
        if (rest[i] === "--name") {
          i++;
          if (rest[i]) names.push(rest[i]);
        } else if (!rest[i].startsWith("--")) {
          names.push(rest[i]);
        }
      }
      return runRoadmapReport({
        root: ROOT,
        names,
        sites: () => roadmapSites(ROOT),
        stand: (entityName) => sendCommand({ verb: "stand", entityName }),
        look: () => sendCommand({ verb: "look" }),
        ui: () => sendCommand({ verb: "ui" }),
        use: (className, entityName) => sendCommand({ verb: "use", className, entityName }),
        input: (action) => sendCommand({ verb: "input", action }),
        captureWorld: (spec) => sendCommand({ verb: "capture", mode: "world", ...spec }, 25000),
        captureUi: (name) => sendCommand({ verb: "capture", mode: "ui", name }, 25000),
        resolve: (result) => resolveCapture(result.detail),
        pause,
        log: (line) => console.log(line),
      });
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
