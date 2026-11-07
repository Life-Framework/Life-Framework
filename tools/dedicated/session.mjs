#!/usr/bin/env node
// Dedicated-server session harness.
//
// Describes one session the repo can run later on a worktree's own ports:
// dedicated server up, one player in game, a second player joins after that,
// a save, a process restart, and a load. The load must show the account, the
// bank balance, the organization, and the vehicle lock.
//
// This script does not start ArmaReforgerServer. `node tools/dedicated/session.mjs
// --selftest` judges fixture logs.

import { readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { portsForRoot } from "../wt.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const FIXTURES = join(ROOT, "tools", "dedicated", "fixtures");

const SERVER_UP = /\[ELDebug:Persistence\] resolved SCR_PersistenceSystem, subscribed to save events/;
const SESSION_ENDING = /\[ELDebug:Persistence\] session ending, engine shutdown save expected/;
const SAVE = /\[ELDebug:Persistence\] (?:save point created|save completed)/;
const LOAD = /\[ELDebug:Persistence\] loading the latest save point/;
const JOIN = /\[ELDebug:CharacterCreation\] flow start for player (\d+)/;
const SPAWN = /\[ELDebug:CharacterCreation\] spawning player (\d+)/;

const LOAD_LINES = [
  ["account", /\[ELDebug:Persistence\] applied [1-9]\d* account record\(s\)/],
  ["bank balance", /\[ELDebug:Persistence\] applied [1-9]\d* bank account record\(s\)/],
  ["organization", /\[ELDebug:Persistence\] applied [1-9]\d* organization record\(s\)/],
  ["vehicle lock", /\[ELDebug:VehicleLock\] lock state -> locked/],
];

const PORT_WORDS = new Set([
  "port",
  "bind",
  "bindport",
  "default",
  "engine",
  "listening",
  "listen",
  "server",
  "on",
  "text",
  "network",
  "starting",
  "started",
  "game",
]);

export function configBindPort(root = ROOT) {
  const config = JSON.parse(readFileSync(join(root, "server", "configs", "test-server.json"), "utf8"));
  return config.bindPort;
}

export function worktreePorts(root = ROOT) {
  const ports = portsForRoot(root);
  if (!Number.isInteger(ports.gamePort) || !Number.isInteger(ports.a2sPort)) {
    throw new Error("the worktree game port and a2s port are required");
  }
  return { gamePort: ports.gamePort, a2sPort: ports.a2sPort };
}

// The real run passes these flags. test-server.json hardcodes bindPort 2001;
// that file is not the launch port.
export function serverLaunchArgs({ gamePort, a2sPort, load = false }) {
  if (!Number.isInteger(gamePort) || !Number.isInteger(a2sPort)) {
    throw new Error("the worktree game port and a2s port are required");
  }
  const args = [
    "-server", "Worlds/DebugWorld/DebugWorld.ent",
    "-addons", "LifeFramework",
    "-maxFPS", "30",
    "-logLevel", "normal",
    "-disableCrashReporter",
    "-bindPort", String(gamePort),
    "-a2sPort", String(a2sPort),
  ];
  if (load) args.push("-loadSessionSave");
  return args;
}

export function formatSession(root = ROOT, ports = worktreePorts(root)) {
  const jsonPort = configBindPort(root);
  const first = serverLaunchArgs(ports);
  const second = serverLaunchArgs({ ...ports, load: true });
  return [
    "dedicated server session",
    "The server is not started.",
    "",
    `Worktree ports: game ${ports.gamePort} a2s ${ports.a2sPort}`,
    `test-server.json bindPort: ${jsonPort}`,
    `Launch passes -bindPort ${ports.gamePort} and -a2sPort ${ports.a2sPort}.`,
    "A console line that only mentions port 2001 is not a failure.",
    "",
    "1. Dedicated server up",
    `   ArmaReforgerServer ${first.join(" ")}`,
    "2. One player is in game",
    "3. A second player joins after the first is in game",
    "4. Save",
    "5. Stop the process",
    "6. Start the process again and load",
    `   ArmaReforgerServer ${second.join(" ")}`,
    "   The load must show the account, the bank balance, the organization, and the vehicle lock.",
    "",
  ].join("\n");
}

export function isEngineDefaultPortLine(line) {
  if (!/\b2001\b/.test(line)) return false;
  const words = line
    .replace(/\b2001\b/g, " ")
    .replace(/[^A-Za-z]+/g, " ")
    .trim()
    .toLowerCase()
    .split(/\s+/)
    .filter(Boolean);
  return words.every((word) => PORT_WORDS.has(word));
}

function events(lines, pattern, kind) {
  const found = [];
  lines.forEach((line, index) => {
    const match = line.match(pattern);
    if (!match) return;
    found.push({ index, id: match[1], kind });
  });
  return found;
}

function firstIndex(lines, pattern, after = -1) {
  for (let i = after + 1; i < lines.length; i++) {
    if (pattern.test(lines[i])) return i;
  }
  return -1;
}

export function judgeSession(text) {
  const lines = String(text)
    .split(/\r?\n/)
    .filter((line) => line.trim() && !isEngineDefaultPortLine(line));
  const failures = [];

  const spawns = events(lines, SPAWN, "spawn");
  const joins = events(lines, JOIN, "join");
  const first = spawns[0];
  const firstUp = firstIndex(lines, SERVER_UP);
  if (firstUp < 0 || (first && firstUp > first.index)) {
    failures.push("dedicated server did not come up");
  }
  if (!first) failures.push("no player is in game");

  const saveAt = first ? firstIndex(lines, SAVE, first.index) : firstIndex(lines, SAVE);
  const joiner = first
    ? [...joins, ...spawns].find((event) => {
        if (event.id === first.id || event.index <= first.index) return false;
        if (saveAt >= 0 && event.index >= saveAt) return false;
        return true;
      })
    : null;
  if (!joiner) {
    failures.push("late joiner missing: no second player joins after the first is in game");
  }
  if (saveAt < 0) failures.push("save missing");

  const ending = saveAt >= 0 ? firstIndex(lines, SESSION_ENDING, saveAt) : -1;
  if (ending < 0) failures.push("process restart missing: the server process did not stop");

  const secondUp = ending >= 0 ? firstIndex(lines, SERVER_UP, ending) : -1;
  if (secondUp < 0) failures.push("process restart missing: the server did not come back up");

  const loadAt = secondUp >= 0 ? firstIndex(lines, LOAD, secondUp) : -1;
  if (loadAt < 0) failures.push("load missing after the process restart");

  if (loadAt >= 0) {
    let cursor = loadAt + 1;
    for (const [name, pattern] of LOAD_LINES) {
      let found = -1;
      for (let i = cursor; i < lines.length; i++) {
        if (pattern.test(lines[i])) {
          found = i;
          break;
        }
      }
      if (found < 0) failures.push(`load missing ${name}`);
      else cursor = found + 1;
    }
  }

  return { ok: failures.length === 0, failures };
}

function readFixture(name) {
  return readFileSync(join(FIXTURES, name), "utf8");
}

function flagValue(args, flag) {
  const index = args.indexOf(flag);
  if (index < 0 || index + 1 >= args.length) return "";
  return args[index + 1];
}

function selftest() {
  const jsonPort = configBindPort(ROOT);
  if (jsonPort !== 2001) throw new Error(`test-server.json bindPort is ${jsonPort}, expected 2001`);

  const sample = serverLaunchArgs({ gamePort: 2031, a2sPort: 17807 });
  if (sample.includes("-config")) throw new Error("the launch must not take bindPort from test-server.json");
  if (flagValue(sample, "-bindPort") !== "2031") throw new Error(`-bindPort ${flagValue(sample, "-bindPort")}`);
  if (flagValue(sample, "-a2sPort") !== "17807") throw new Error(`-a2sPort ${flagValue(sample, "-a2sPort")}`);
  if (flagValue(sample, "-bindPort") === String(jsonPort)) throw new Error("worktree -bindPort collapsed to 2001");

  const reload = serverLaunchArgs({ gamePort: 2031, a2sPort: 17807, load: true });
  if (!reload.includes("-loadSessionSave")) throw new Error("the restarted process must load");
  if (flagValue(reload, "-bindPort") !== "2031" || flagValue(reload, "-a2sPort") !== "17807") {
    throw new Error("the restarted process must keep the worktree ports");
  }

  const live = worktreePorts(ROOT);
  const liveArgs = serverLaunchArgs(live);
  if (flagValue(liveArgs, "-bindPort") !== String(live.gamePort)) throw new Error("live -bindPort");
  if (flagValue(liveArgs, "-a2sPort") !== String(live.a2sPort)) throw new Error("live -a2sPort");

  const plan = formatSession(ROOT, { gamePort: 2031, a2sPort: 17807 });
  if (!plan.includes("-bindPort 2031") || !plan.includes("-a2sPort 17807")) {
    throw new Error("the session plan must name the worktree ports");
  }
  if (!plan.includes("The server is not started.")) throw new Error("the plan must not boot the server");
  if (plan.includes(`-bindPort ${jsonPort}`)) throw new Error("the plan used the json bind port");

  if (!isEngineDefaultPortLine("2001")) throw new Error("a line that only says 2001 is the engine default");
  if (!isEngineDefaultPortLine("DEFAULT: Starting server on port 2001")) {
    throw new Error("engine default bind text");
  }
  const onlyPort = judgeSession("2001\n");
  if (onlyPort.ok) throw new Error("a port line is not a session");
  if (onlyPort.failures.some((failure) => failure.includes("2001"))) {
    throw new Error("a line that only says 2001 must not be a failure");
  }

  const missing = judgeSession(readFixture("missing-joiner.log"));
  if (missing.ok) throw new Error("missing late joiner must fail");
  if (!missing.failures.some((failure) => failure.startsWith("late joiner missing"))) {
    throw new Error(missing.failures.join("; "));
  }
  if (missing.failures.some((failure) => failure.startsWith("load missing"))) {
    throw new Error(`missing-joiner failed the load: ${missing.failures.join("; ")}`);
  }

  const passed = judgeSession(readFixture("restart-load.log"));
  if (!passed.ok) throw new Error(passed.failures.join("; "));

  const earlyLines = readFixture("restart-load.log").split(/\r?\n/).flatMap((line) => {
    if (line.includes("flow start for player 2") || line.includes("spawning player 2")) return [];
    if (line.includes("flow start for player 1")) {
      return [
        "08:00:09.000 SCRIPT       : [ELDebug:CharacterCreation] flow start for player 2",
        line,
      ];
    }
    return [line];
  });
  const early = earlyLines.join("\n");
  const tooSoon = judgeSession(early);
  if (tooSoon.ok || !tooSoon.failures.some((failure) => failure.startsWith("late joiner missing"))) {
    throw new Error("a second player who joins before the first is in game must fail");
  }

  console.log("selftest ok");
  return 0;
}

function checkFile(path) {
  if (!path) {
    console.error("usage: node tools/dedicated/session.mjs --check <console.log>");
    return 2;
  }
  const result = judgeSession(readFileSync(path, "utf8"));
  if (result.ok) {
    console.log("session ok");
    return 0;
  }
  for (const failure of result.failures) console.log(`FAIL  ${failure}`);
  return 1;
}

function main(argv) {
  try {
    if (argv.includes("--run") || argv.includes("boot") || argv.includes("--boot")) {
      console.error("This script does not start ArmaReforgerServer.");
      return 2;
    }
    if (argv.includes("--selftest")) return selftest();
    const checkAt = argv.indexOf("--check");
    if (checkAt >= 0) return checkFile(argv[checkAt + 1]);
    console.log(formatSession());
    return 0;
  } catch (error) {
    console.error(error.message || error);
    return 1;
  }
}

const invoked = process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href;
if (invoked) {
  process.exit(main(process.argv.slice(2)));
}
