// opening-run: host=outside timeout=1500
// Opening-day beat B8, the clean restart and the reload (#1604; docs/playtest/m4-opening-day.md "B8").
//
//   node tools/playtest/opening/b8-reload.mjs --host <scratch dir>     stop, rotate the password, start, judge the boot
//   node tools/playtest/opening/b8-reload.mjs --log <console.log>      judge one boot log only (the selftest's way in)
//
// With --host the script runs the real `tools\cli host` commands against a SCRATCH host folder, never the real one:
//   1. a scratch with no save point is booted and stopped once, so the reload has a save to load;
//   2. `host stop` (a no-op when the host is already down);
//   3. `host password rotate`, which writes a new password into the scratch secrets.json and server.json;
//   4. `host start`: the second boot, judged from its console.log (the new one, not an older run's);
//   5. `host stop` again, so nothing is left running.
// `host password rotate` refuses a host folder inside the repo checkout, and a scratch under <repo>/tmp/ (what
// `tools\cli opening --host` passes every beat, B9 included) is inside it, so such a folder is copied to a temp
// folder outside the repo, the beat runs on the copy, and the copy is removed. A folder outside the repo is used in place.
// Refused before anything runs (exit 2): the real host folder (%LOCALAPPDATA%\LifeFramework\host) and anything under
// server/flagship.
//
// One `BEAT B8 <check-id> PASS|BLOCKED|FAIL <detail>` line per check (the form `tools\cli opening` reads), then
// `SUMMARY b8-reload ok|blocked|fail pass=N blocked=N fail=N`. Exit 1 only on FAIL. A password never appears in the output.
//   password-rotated      secrets.json holds a new game.password and the applied server.json carries it
//   second-boot           the scratch host started again
//   save-loaded           the console.log of the second boot has `save loaded:`
//   no-missing-resource   no `Unable to find` and no `Wrong GUID/name` in it (a FAIL quotes the line)
//   time-continues        BLOCKED by #1302 (time survives a restart is not built)
//   player-rejoin         BLOCKED: needs clients; names the `restart-cycle` verb (#1268) for `opening --game`
// A scratch with no save whose first boot makes none (the server never printed its ready line) is BLOCKED "no save yet", not
// FAIL (#1693). The beat stops the host by pid in a finally on every path (#1692; tools/playtest/opening/_scratch.mjs reapHost).
// --cli <script> runs `node <script> host ...` instead of tools/cli.mjs (the selftest's stand-in).
// Proof: tools/validation/opening-b8-selftest.mjs, tools/validation/opening-fixes-selftest.mjs.
import { cpSync, existsSync, mkdirSync, mkdtempSync, readFileSync, realpathSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { tmpdir } from "node:os";
import { dirname, join, relative, resolve, isAbsolute } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { safeRemoveTree } from "../../link-safety.mjs";
import { findSavePoints, hostDir, hostPaths, newestConsoleLog, pidAlive, readHostState } from "../../host/host.mjs";
import { isFlagshipPath } from "./b7-restart-setup.mjs";
import { reapHost } from "./_scratch.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
export const BEAT = "B8";
const MISSING = /Unable to find|Wrong GUID\/name/;
// The engine reads the base game's Rally*.conf mission headers on every boot and logs 12 `Wrong GUID/name` lines for
// worlds the server does not load: VANILLA in docs/debug-log-triage.md, never ours. Only that one family is ignored.
const VANILLA_RALLY = /Wrong GUID\/name.*Rally[A-Za-z_]*\.(ent|conf)/;
const SAVE_LOADED =/\[LFDebug:Persistence\] save loaded:/;

/** The real host folder: the default, ignoring LF_HOST_DIR (the opening runner points that at the scratch). */
export function realHostDir(env = process.env) {
  const { LF_HOST_DIR, ...rest } = env;
  return hostDir(rest);
}

const same = (a, b) => resolve(a).toLowerCase() === resolve(b).toLowerCase();
const under = (child, parent) => {
  const r = relative(resolve(parent), resolve(child));
  return r === "" || (!r.startsWith("..") && !isAbsolute(r));
};

/** A reason the folder may not be used as the scratch host, or null. */
export function checkHost(dir) {
  if (isFlagshipPath(dir)) return `${dir} is under server/flagship, which is never touched`;
  const real = realHostDir();
  if (under(dir, real)) return `${dir} is the real host folder (${real}); B8 only runs on a scratch folder`;
  try {
    if (existsSync(dir)) {
      const r = realpathSync(dir);
      if (isFlagshipPath(r)) return `${dir} resolves into server/flagship, which is never touched`;
      if (under(r, real)) return `${dir} resolves into the real host folder (${real})`;
    }
  } catch {
    // an unreadable path fails later with its own message
  }
  return null;
}

const line = (id, status, detail = "") => ({ id, status, detail });

/** Judges one boot's console.log text: the load line and the missing-resource lines. Pure. */
export function judgeBoot(text) {
  const lines = String(text ?? "").split(/\r?\n/);
  const out = [];
  const loaded = lines.find((l) => SAVE_LOADED.test(l));
  out.push(loaded ? line("save-loaded", "PASS", loaded.trim().slice(0, 200)) : line("save-loaded", "FAIL", "no '[LFDebug:Persistence] save loaded:' line: the second boot did not load the save"));
  const hits = lines.filter((l) => MISSING.test(l));
  const bad = hits.filter((l) => !VANILLA_RALLY.test(l));
  const vanilla = hits.length - bad.length;
  if (!bad.length) out.push(line("no-missing-resource", "PASS", `no 'Unable to find' or 'Wrong GUID/name' line${vanilla ? ` (${vanilla} vanilla Rally line(s) ignored, docs/debug-log-triage.md)` : ""}`));
  else out.push(line("no-missing-resource", "FAIL", `${bad.length} line(s), first: ${bad[0].trim().slice(0, 300)}`));
  return out;
}

/** Judges the rotation from the three passwords, never echoing one. Pure. */
export function judgePassword({ before, after, applied }) {
  if (!after) return line("password-rotated", "FAIL", "secrets.json has no game.password after the rotate");
  if (after === before) return line("password-rotated", "FAIL", "the password did not change");
  if (applied !== after) return line("password-rotated", "FAIL", "the applied server.json does not carry the new password");
  return line("password-rotated", "PASS", "a new game.password is in secrets.json and in the applied server.json");
}

/** The checks that wait on another slice. */
export function blockedLines(root = ROOT) {
  const verb = existsSync(join(root, "agent-box", "proofs", "restart-cycle.mjs")) ? "the verb is on main" : "the verb is not on main yet";
  return [
    line("time-continues", "BLOCKED", "#1302: the in-game time surviving a restart is not built, so it cannot be judged"),
    line("player-rejoin", "BLOCKED", `needs clients; run \`tools\\cli opening --game\` (proof verb restart-cycle, #1268; ${verb})`),
  ];
}

function readJson(file) {
  try {
    return JSON.parse(readFileSync(file, "utf8"));
  } catch {
    return null;
  }
}
const secretPassword = (dir) => String(readJson(join(dir, "secrets.json"))?.game?.password ?? "").trim();
const appliedPassword = (dir) => String(readJson(join(hostPaths(dir).profile, "server.json"))?.game?.password ?? "").trim();

let CLI = join(ROOT, "tools", "cli.mjs");
function cli(args, dir, timeoutMs) {
  const r = spawnSync(process.execPath, [CLI, "host", ...args], {
    cwd: ROOT,
    encoding: "utf8",
    timeout: timeoutMs,
    windowsHide: true,
    env: { ...process.env, LF_HOST_DIR: dir, LF_QUIET_CHILD: "1" },
  });
  return { code: r.status ?? 1, out: `${r.stdout ?? ""}${r.stderr ?? ""}` };
}
const tail = (out) => out.trim().split(/\r?\n/).slice(-4).join(" | ").replace(/\s+/g, " ").slice(0, 400);
const stopped = (dir) => {
  const s = readHostState(dir);
  return !s || !pidAlive(s.pid);
};

/** Runs the live beat on `dir` (already a scratch folder outside the repo). Returns the check lines. */
export function runLive(dir, { timeoutSec = 300 } = {}) {
  const out = [];
  // `host start --wait` queues for the port-2001 lock (a pool session may hold it); capped here at 15 min.
  const startMs = (timeoutSec + 120 + 900) * 1000;
  const hasSave = () => findSavePoints(hostPaths(dir).profile).length > 0;

  // 1. A save for the reload to load: boot a fresh scratch once and close it (the shutdown save).
  if (!hasSave()) {
    const first = cli(["start", "--wait", "--timeout", String(timeoutSec)], dir, startMs);
    cli(["stop"], dir, 240000);
    if (first.code !== 0 || !hasSave()) return [line("second-boot", "BLOCKED", `no save yet: the scratch host had no save and its first boot did not make one: ${tail(first.out)}`)];
  }

  // 2. Stop, 3. rotate.
  const stop = cli(["stop"], dir, 240000);
  if (stop.code !== 0 || !stopped(dir)) return [line("second-boot", "FAIL", `host stop did not close the scratch host: ${tail(stop.out)}`)];
  const before = secretPassword(dir);
  const rot = cli(["password", "rotate", dir], dir, 120000);
  if (rot.code !== 0 && /has no uid yet/.test(rot.out)) out.push(line("password-rotated", "BLOCKED", `uid pending: the maintainer's uid is not in staff.txt yet (a human step), so host config apply refuses; ${tail(rot.out)}`));
  else if (rot.code !== 0) out.push(line("password-rotated", "FAIL", `host password rotate exited ${rot.code}: ${tail(rot.out.split(before || "\u0000").join("<old>"))}`));
  else out.push(judgePassword({ before, after: secretPassword(dir), applied: appliedPassword(dir) }));

  // 4. The second boot, judged from its own console.log.
  const oldLog = newestConsoleLog(hostPaths(dir).profile);
  const start = cli(["start", "--wait", "--timeout", String(timeoutSec)], dir, startMs);
  const log = newestConsoleLog(hostPaths(dir).profile);
  if (start.code !== 0) out.push(line("second-boot", "FAIL", `host start exited ${start.code}: ${tail(start.out)}`));
  else if (!log || log === oldLog) out.push(line("second-boot", "FAIL", "the second boot wrote no new console.log"));
  else {
    out.push(line("second-boot", "PASS", `the scratch host started again; log ${log}`));
    out.push(...judgeBoot(readFileSync(log, "utf8")));
  }

  // 5. Leave nothing running.
  cli(["stop"], dir, 240000);
  return out;
}

function report(checks) {
  for (const c of checks) console.log(`BEAT ${BEAT} ${c.id} ${c.status}${c.detail ? ` ${c.detail}` : ""}`);
  const n = (s) => checks.filter((c) => c.status === s).length;
  const verdict = n("FAIL") ? "fail" : n("BLOCKED") ? "blocked" : "ok";
  console.log(`SUMMARY b8-reload ${verdict} pass=${n("PASS")} blocked=${n("BLOCKED")} fail=${n("FAIL")}`);
  return n("FAIL") ? 1 : 0;
}

const val = (args, name) => {
  const i = args.indexOf(name);
  return i === -1 || i + 1 >= args.length ? null : args[i + 1];
};

async function main(args) {
  const logFile = val(args, "--log");
  if (logFile) {
    if (!existsSync(logFile)) {
      console.error(`[b8] --log ${logFile} does not exist`);
      return 2;
    }
    return report([...judgeBoot(readFileSync(logFile, "utf8")), ...blockedLines()]);
  }
  const host = val(args, "--host");
  if (!host) {
    console.error("usage: node tools/playtest/opening/b8-reload.mjs --host <scratch dir> [--timeout <s>] | --log <console.log>");
    return 2;
  }
  const refused = checkHost(host);
  if (refused) {
    console.error(`[b8] refused: ${refused}`);
    return 2;
  }
  if (!existsSync(host)) mkdirSync(host, { recursive: true });
  if (val(args, "--cli")) CLI = resolve(val(args, "--cli"));
  const timeoutSec = Number(val(args, "--timeout")) > 0 ? Number(val(args, "--timeout")) : 300;
  let dir = resolve(host);
  let copy = null;
  if (under(dir, ROOT)) {
    // host password rotate refuses a folder inside the repo checkout: work on a copy outside it.
    copy = mkdtempSync(join(tmpdir(), "lf-b8-"));
    dir = join(copy, "host");
    cpSync(resolve(host), dir, { recursive: true });
    console.log(`[b8] ${host} is inside the repo: running on a copy at ${dir}, removed at the end`);
  }
  try {
    return report([...runLive(dir, { timeoutSec }), ...blockedLines()]);
  } finally {
    // by pid, whatever happened above: a first boot that timed out leaves its server up
    reapHost({ host: dir, cli: CLI, say: (m) => console.error(`[b8] ${m}`) });
    if (copy) safeRemoveTree(copy);
  }
}

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) process.exit(await main(process.argv.slice(2)));
