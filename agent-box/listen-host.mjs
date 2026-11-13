// agent-box/listen-host.mjs - first join on a listen host with the online service on.
//
// Every `session up` server runs -noBackend, so a player's identity never changes during a session
// there. A real listen host (the launcher's "Play on my own", hosting from the game menu) gets the
// platform identity about a second after the join; playtest build 4 opened the account before it
// arrived and the signed form found none. This boots the game exactly the way the launcher's solo
// mode does (-server <playtest mission> -addr 127.0.0.1:2302, Steam signed in, no -noBackend) on the
// hidden LF-Agents desktop with a fresh profile, signs form R-89 through the harness file channel
// after a delay, prints the identity, account and spawn lines, and closes the game.
//
//   node agent-box/listen-host.mjs [--addons <dir>] [--profile <dir>] [--sign-after <s>]
//
// --addons defaults to this checkout's addons; point it at an unzipped playtest build's addons to
// test a release. Exit 0 when the character spawned. It runs outside the pool's session lanes
// (the pool cannot boot a listen host), so it refuses while any Arma Reforger process is running.
// One client only: a second client on this PC is the same Steam account, so the same identity.
import { quietSpawnDetached } from "../tools/lib/quiet-launch.mjs";
import { setBoxes, sendCommand } from "./harness.mjs";
import { readdirSync, statSync, mkdirSync, existsSync, readFileSync } from "node:fs";
import { join, resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const MISSION = "{1054CC8646357708}Missions/LF_PlaytestTown.conf";
const GAME_DIR = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Arma Reforger";

function option(name, fallback) {
  const i = process.argv.indexOf(name);
  return i === -1 ? fallback : process.argv[i + 1];
}

const addonsDir = resolve(option("--addons", join(ROOT, "addons")));
const profile = resolve(option("--profile", join(ROOT, "tmp", `listen-host-${Date.now()}`)));
const signAfter = Number(option("--sign-after", "20"));

const running = spawnSync("tasklist", ["/NH", "/FO", "CSV"], { encoding: "utf8", windowsHide: true }).stdout || "";
if (/"ArmaReforger(Steam|Server|ServerDiag)?\.exe"/i.test(running)) {
  console.error("listen-host: an Arma Reforger process is already running (a pool session or test); run this when the pool is idle (tools\\cli pool status)");
  process.exit(2);
}

mkdirSync(profile, { recursive: true });
const args = ["-addonsDir", addonsDir, "-addons", "LifeFramework", "-server", MISSION, "-addr", "127.0.0.1:2302",
  "-profile", profile, "-window", "-screenWidth", "640", "-screenHeight", "360", "-maxFPS", "20",
  "-noSound", "-noFocus", "-forceUpdate", "-disableCrashReporter", "-scrDefine", "LF_HARNESS"];
const pid = quietSpawnDetached(join(GAME_DIR, "ArmaReforgerSteam.exe"), args, { cwd: GAME_DIR });
const t0 = Date.now();
const stamp = () => `${((Date.now() - t0) / 1000).toFixed(1)}s`;
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
console.log(stamp(), "listen host pid", pid, "profile", profile);

function consoleLog() {
  const logs = join(profile, "logs");
  if (!existsSync(logs)) return "";
  const dirs = readdirSync(logs).map((d) => join(logs, d)).sort((a, b) => statSync(b).mtimeMs - statSync(a).mtimeMs);
  const path = dirs.length ? join(dirs[0], "console.log") : "";
  return path && existsSync(path) ? readFileSync(path, "utf8") : "";
}

// Bounded: reads the log once a second until the pattern shows or the budget runs out.
async function logShows(re, seconds) {
  const end = Date.now() + seconds * 1000;
  while (Date.now() < end) {
    if (re.test(consoleLog())) return true;
    await sleep(1000);
  }
  return false;
}

let spawned = false;
try {
  const opened = await logShows(/registration form open/, 150);
  console.log(stamp(), "registration form open:", opened);
  if (opened) {
    await sleep(signAfter * 1000);
    // $profile of a -profile <dir> client is <dir>/profile.
    setBoxes([join(profile, "profile", "agent-box")]);
    for (const [widget, text] of [["RegFirstName", "Ada"], ["RegLastName", "Tester"], ["RegAge", "30"]]) {
      const r = sendCommand({ verb: "type", widget, text }, 15000);
      console.log(stamp(), "type", widget, r.status, r.detail);
    }
    const r = sendCommand({ verb: "click", widget: "SignButton" }, 15000);
    console.log(stamp(), "click SignButton", r.status, r.detail);
    await logShows(/spawn splash cleared|character refused|not registered/, 40);
    await sleep(3000);
    spawned = /spawn splash cleared: local player controls a character/.test(consoleLog());
  }
} finally {
  spawnSync("taskkill", ["/PID", String(pid), "/T", "/F"], { stdio: "ignore", windowsHide: true });
}

const wanted = /Updating player|LFDebug:(CharacterCreation|Account|Spawn)\]|LFDebug:Playtime\] clock|VME/;
console.log(consoleLog().split(/\r?\n/).filter((l) => wanted.test(l)).join("\n"));
console.log(spawned ? "listen-host: PASS the character spawned" : "listen-host: FAIL no character spawned");
process.exit(spawned ? 0 : 1);
