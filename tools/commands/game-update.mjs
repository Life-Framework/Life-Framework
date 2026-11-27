// tools\cli game-update: bring this PC's Arma Reforger server (and client, tools) to the game version Steam wants.
// Waits (bounded) for running servers to finish, asks Steam to update, waits for the build to match. It refuses while
// any session is held, never stops a process, and only touches this machine's own install (never a hosted server).
// See docs/worktrees.md "Game update guard".
import { spawnSync } from "node:child_process";
import * as gu from "../lib/game-update.mjs";
import * as pool from "../remote/pool.mjs";
import * as q from "../lib/pool-queue.mjs";
import * as wt from "../wt.mjs";
import { readFileSync } from "node:fs";

export const help = "game-update [--check] [--dry-run] [--wait-min N] [--update-min N]   wait for servers to end, update the Steam apps that lag, wait for the build to match; refuses while a session is held";

function pidAlive(pid) {
  try {
    process.kill(pid, 0);
    return true;
  } catch (e) {
    return e.code === "EPERM";
  }
}

function minutes(args, flag, dflt) {
  const i = args.indexOf(flag);
  if (i < 0) return dflt;
  const n = Number(args[i + 1]);
  return Number.isFinite(n) && n > 0 ? n : dflt;
}

export async function main(args, { root }) {
  if (args.includes("--help") || args.includes("-h")) {
    console.log(`usage: tools\\cli ${help}`);
    return 0;
  }
  const dirs = gu.installDirs(root);
  const read = () => gu.readInstall(dirs);
  if (args.includes("--check")) {
    const install = read();
    const line = gu.pendingLine(install);
    console.log(line ?? gu.okLine(install));
    return line ? 1 : 0;
  }
  const machines = pool.loadMachines(root).filter((m) => m.via === "local");
  const lockInfo = (name) => {
    try {
      return JSON.parse(readFileSync(wt.lockFileOf(root, name), "utf8"));
    } catch {
      return null;
    }
  };
  const deps = {
    read,
    held: () => gu.heldSessions(machines, lockInfo, pidAlive, q.allLaneLocks),
    running: (apps) => gu.runningUnder(Object.fromEntries(apps.map((a) => [a, dirs[a]]))),
    trigger: (appId) => {
      // Steam's own URL handler: for an installed app with a newer build it queues the update.
      spawnSync("cmd", ["/c", "start", "", `steam://install/${appId}`], { windowsHide: true, stdio: "ignore" });
    },
    sleep: (ms) => new Promise((r) => setTimeout(r, ms)),
    now: () => Date.now(),
    log: (l) => console.log(l),
  };
  const r = await gu.updateRun({ waitMs: minutes(args, "--wait-min", 20) * 60000, updateMs: minutes(args, "--update-min", 30) * 60000, dryRun: args.includes("--dry-run") }, deps);
  console.log(`SUMMARY game-update ${r.code === 0 ? "ok" : "fail"} ${r.why}`);
  return r.code;
}
