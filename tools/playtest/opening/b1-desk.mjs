// tools/playtest/opening/b1-desk.mjs
// Beat B1 of docs/playtest/m4-opening-day.md: the desk, go or no-go before opening, on a scratch host.
//
//   node tools/playtest/opening/b1-desk.mjs --host <scratch dir> [--timeout <s>] [--flagship <dir>] [--cli <file>] [--help-file <file>]
//
//   --host        the scratch host folder (required). It is passed to every command as LF_HOST_DIR. The real host folder is refused.
//   --timeout     seconds each command may run (default 60).
//   --flagship    the committed live config folder, read only (default server/flagship).
//   --cli         the cli to call as a child process (default tools/cli.mjs); the selftest points it at a stand-in.
//   --help-file   use this text instead of `host help` to decide which commands are on main; the selftest only.
//
// One line per checkbox of the beat, in the beat's order: `BEAT B1 <check-id> PASS|FAIL|BLOCKED <reason>`.
// BLOCKED is a command that `tools\cli host help` does not list (its slice is not on main), or a check this script does not
// run. Exit 1 only when a line is FAIL; exit 2 for a bad call (no BEAT line then). ODRUN collects the beat scripts by glob.
//
//   config-apply  `host config apply` writes the profile's config.json and server.json; the join password is in the host's
//                 server.json and in no file of the flagship folder.
//   roster        the staff roster names one person, an administrator (the maintainer).
//   drill         `host drill` boots the newest backup on a pool machine; this script runs with no machine, so it is BLOCKED.
//   ready         `host ready` prints go.
//   prune         `host prune --dry-run` lists no tagged (preflight, wipe, crash) backup for removal.
// Proof: tools/validation/opening-b1-selftest.mjs.

import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { hostDir } from "../../host/host.mjs";
import { isPlainBackup } from "../../host/rotate.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const LEVELS = ["moderator", "operator", "administrator"];

const val = (args, name) => {
  const i = args.indexOf(name);
  return i === -1 || i + 1 >= args.length ? null : args[i + 1];
};

function usage(msg) {
  console.error(`b1-desk: ${msg}\nusage: node tools/playtest/opening/b1-desk.mjs --host <scratch dir> [--timeout <s>] [--flagship <dir>] [--cli <file>] [--help-file <file>]`);
  process.exit(2);
}

// `<uid> [level] [# display name]`, one per line, `#` starts a comment line (server/flagship/staff.txt).
export function parseRoster(text) {
  const entries = [];
  for (const raw of text.split(/\r?\n/)) {
    const line = raw.trim();
    if (!line || line.startsWith("#")) continue;
    const [left, ...rest] = line.split("#");
    const [uid, level] = left.trim().split(/\s+/);
    entries.push({ uid, level: (level || "").toLowerCase(), name: rest.join("#").trim() });
  }
  return entries;
}

// Is `<words...>` a host command that `host help` lists: the usage line names the built-in ones, every other line starts with its name.
export function listed(helpText, words) {
  const [name, sub] = words;
  return helpText.split(/\r?\n/).some((l) => {
    const t = l.trim();
    if (/^usage:/i.test(t)) return t.split(/\s+/).some((w) => w.split("|").includes(name));
    return (t === name || t.startsWith(`${name} `)) && (!sub || t.split(/\s+/).slice(1).join(" ").includes(sub) || t.split(/[\s|]+/).includes(sub));
  });
}

export function runB1(opts) {
  const { host, cli, timeoutMs, flagship, helpFile } = opts;
  const env = { ...process.env, LF_HOST_DIR: host };
  const out = [];
  const line = (id, verdict, reason) => out.push({ id, verdict, reason: String(reason).replace(/\s+/g, " ").trim() });

  const call = (args) => {
    const r = spawnSync(process.execPath, [cli, ...args], { cwd: ROOT, env, encoding: "utf8", timeout: timeoutMs, maxBuffer: 16 * 1024 * 1024, windowsHide: true });
    if (r.error && r.error.code === "ETIMEDOUT") return { status: null, text: "", timedOut: true };
    return { status: r.status, text: `${r.stdout || ""}${r.stderr || ""}`, timedOut: false };
  };
  const firstLine = (t) => (t.split(/\r?\n/).find((l) => l.trim()) || "no output").trim();
  const failed = (r, what) => (r.timedOut ? `${what} timed out after ${timeoutMs / 1000}s` : `${what} exited ${r.status}: ${firstLine(r.text)}`);

  let helpText;
  if (helpFile) helpText = readFileSync(helpFile, "utf8");
  else {
    const h = call(["host", "help"]);
    helpText = h.status === 0 ? h.text : "";
  }
  const blocked = (words) => `\`tools\\cli host ${words.join(" ")}\` is not in \`host help\`, so its slice is not on main`;

  // config-apply
  if (!listed(helpText, ["config", "apply"])) line("config-apply", "BLOCKED", blocked(["config", "apply"]));
  else {
    const r = call(["host", "config", "apply"]);
    const serverJson = join(host, "profile", "server.json");
    const configJson = join(host, "profile", "config.json");
    if (r.timedOut || r.status !== 0) line("config-apply", "FAIL", failed(r, "host config apply"));
    else if (!existsSync(configJson) || !existsSync(serverJson)) line("config-apply", "FAIL", "host config apply left no profile/config.json and server.json in the host folder");
    else {
      let password = "";
      try {
        password = String(JSON.parse(readFileSync(serverJson, "utf8"))?.game?.password || "");
      } catch (e) {
        line("config-apply", "FAIL", `profile/server.json does not parse: ${e.message}`);
      }
      if (!out.some((o) => o.id === "config-apply")) {
        const leaked = !password || !existsSync(flagship) ? [] : readdirSync(flagship).filter((f) => statSync(join(flagship, f)).isFile() && readFileSync(join(flagship, f), "utf8").includes(password));
        if (!password) line("config-apply", "FAIL", "game.password is empty in the host's server.json (the host's secrets.json did not supply it)");
        else if (leaked.length) line("config-apply", "FAIL", `the join password is in a repo file: server/flagship/${leaked[0]}`);
        else line("config-apply", "PASS", "profile config.json and server.json written; the join password is in the host's server.json and in no repo file");
      }
    }
  }

  // roster
  {
    const hostRoster = join(host, "profile", "LifeFramework", "staff.txt");
    const file = existsSync(hostRoster) ? hostRoster : join(flagship, "staff.txt");
    const where = file === hostRoster ? "the host's staff.txt" : "server/flagship/staff.txt";
    if (!existsSync(file)) line("roster", "FAIL", `no staff roster at ${where}`);
    else {
      const entries = parseRoster(readFileSync(file, "utf8"));
      const bad = entries.find((e) => !LEVELS.includes(e.level));
      if (!entries.length) line("roster", "FAIL", `${where} lists nobody: the maintainer's uid is still pending (a human step)`);
      else if (entries.length > 1) line("roster", "FAIL", `${where} lists ${entries.length} people; the maintainer is the only staff`);
      else if (bad) line("roster", "FAIL", `${where} gives the entry no known level (moderator, operator, administrator)`);
      else if (entries[0].level !== "administrator") line("roster", "FAIL", `${where} lists the maintainer as ${entries[0].level}, not administrator`);
      else line("roster", "PASS", `${entries[0].name || entries[0].uid} is the only administrator in ${where}`);
    }
  }

  // drill: boots the game on a pool machine; this script has none.
  line("drill", "BLOCKED", "boots the newest backup on a pool machine and this script runs with no machine; run `tools\\cli host drill` and look for `save loaded:`");

  // ready
  if (!listed(helpText, ["ready"])) line("ready", "BLOCKED", blocked(["ready"]));
  else {
    const r = call(["host", "ready"]);
    if (r.timedOut) line("ready", "FAIL", failed(r, "host ready"));
    else if (r.status === 0 && /\bgo\b/i.test(r.text) && !/no-go/i.test(r.text)) line("ready", "PASS", "host ready prints go");
    else line("ready", "FAIL", r.status === 0 ? `host ready did not print go: ${firstLine(r.text)}` : failed(r, "host ready"));
  }

  // prune
  {
    const r = call(["host", "prune", "--dry-run"]);
    if (r.timedOut || r.status !== 0) line("prune", "FAIL", failed(r, "host prune --dry-run"));
    else {
      const names = [...r.text.matchAll(/^host prune: would delete ([^\s,]+)\s*$/gm)].map((m) => m[1]);
      const kept = /kept (\d+) plain/.exec(r.text);
      const tagged = names.find((n) => !isPlainBackup(n));
      const backups = join(host, "backups");
      const onDisk = existsSync(backups) ? readdirSync(backups).filter((n) => !isPlainBackup(n)).length : 0;
      if (tagged) line("prune", "FAIL", `the dry run would delete the tagged backup ${tagged}`);
      else if (!kept) line("prune", "FAIL", "the dry run printed no `kept N plain` summary");
      else line("prune", "PASS", `${names.length} plain would go, ${kept[1]} plain kept, ${onDisk} tagged or other folders untouched`);
    }
  }
  return out;
}

function main(args) {
  const hostArg = val(args, "--host");
  if (!hostArg) usage("--host <scratch dir> is required");
  const host = resolve(hostArg);
  const real = resolve(hostDir({ ...process.env, LF_HOST_DIR: "" }));
  const same = process.platform === "win32" ? host.toLowerCase() === real.toLowerCase() : host === real;
  if (same) usage(`${host} is the real host folder; give a scratch folder`);
  if (!existsSync(host)) usage(`${host} does not exist`);
  const timeout = Number(val(args, "--timeout") || 60);
  if (!(timeout > 0)) usage("--timeout must be a number of seconds above 0");
  const cli = resolve(val(args, "--cli") || join(ROOT, "tools", "cli.mjs"));
  const helpFile = val(args, "--help-file");
  const lines = runB1({
    host,
    cli,
    timeoutMs: timeout * 1000,
    flagship: resolve(val(args, "--flagship") || join(ROOT, "server", "flagship")),
    helpFile: helpFile ? resolve(helpFile) : null,
  });
  for (const l of lines) console.log(`BEAT B1 ${l.id} ${l.verdict} ${l.reason}`);
  process.exit(lines.some((l) => l.verdict === "FAIL") ? 1 : 0);
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) main(process.argv.slice(2));
