// `tools\cli host remote 0grind <cmd>`: the flagship on the rented 0grind server, through 0grind's API
// (no shell on the box). The logic is tools/host/remote-0grind.mjs; docs/owner/host-the-flagship.md "0grind".
//
// Read-only: status, config show, logs pull, saves pull, rcon #players.
// Changes the live server, so each needs --yes: start, stop, restart, system fix, config push, any other rcon command.
import { get as httpsGet } from "node:https";
import { join } from "node:path";
import { queryInfo } from "../a2s.mjs";
import { isMainCheckout } from "../../wt.mjs";
import { boxMode } from "../../lib/host-box.mjs";
import {
  DEFAULT_EVERY_HOURS,
  DEFAULT_KEEP,
  MAX_EVERY_HOURS,
  currentUser,
  pruneRemotePulls,
  queryTask,
  readSchedule,
  realRunner,
  scheduleTask,
  unscheduleTask,
  wholeNumber,
} from "../remote-schedule.mjs";
import {
  FLAGSHIP_0GRIND,
  API_BASE,
  authHint,
  configPush,
  createClient,
  isReadOnlyRcon,
  systemSettingsPlan,
  logsPull,
  makeRedactor,
  maskSecrets,
  readToken,
  remoteDir,
  savesPull,
  secretValues,
  systemSettingsWarnings,
  SERVER_CONFIG_PATH,
  fileSize,
} from "../remote-0grind.mjs";

export const help =
  "remote 0grind status|start|stop|restart|system fix|config show|config push|logs pull|saves pull [--prune] [--keep N]|saves pull --schedule [--every H] [--keep N]|rcon <cmd> [--yes] [--verbose]   the flagship on the rented 0grind server through its API: status (API + A2S), config show (secrets masked), config push (server/flagship + secrets.json onto ServerConfig.json and server_settings; a diff unless --yes; ports, bind address and rcon kept; --only name,password,passwordAdmin pushes just those server_settings fields and leaves the scenario, mods and files alone; --only staff uploads only the mod's staff.txt), logs pull (newest console.log for host triage/summary/attendance), saves pull (/profile/.save to backups/<stamp>-remote; --prune then deletes the plain -remote pulls beyond the newest --keep N, default 20, never a tagged or local backup), saves pull --schedule (a Windows scheduled task on this PC pulls every --every H hours, default 6, hidden, and prunes; --unschedule removes it, --schedule-status says whether it is registered and when it last ran, --dry-run prints the schtask without running it); system fix (the System-page settings the flagship needs: restart on failure, session save loading, A2S on, the save-deleting options off; a diff unless --yes); start, stop, restart, system fix, push and non-read-only rcon need --yes";

const USAGE = "usage: tools\\cli host remote 0grind status | start|stop|restart --yes | system fix [--yes] | config show | config push [--yes] [--admins-pending] [--only name,password,passwordAdmin,staff] | logs pull [--all] | saves pull [--prune] [--keep N] [--schedule [--every H] [--keep N] [--dry-run] | --unschedule | --schedule-status] | rcon <command> [--yes]";
const say = (m) => console.log(`[LFDebug:Host0grind] ${m}`);

// node:https with no agent, not fetch: a second fetch origin left open at process.exit trips a libuv
// assertion on Windows (Node 24, "UV_HANDLE_CLOSING", exit 0xC0000409).
function publicIp() {
  return new Promise((resolve) => {
    const q = httpsGet("https://api.ipify.org", { agent: false, timeout: 5000 }, (s) => {
      let b = "";
      s.on("data", (c) => (b += c));
      s.on("end", () => resolve(/^\d+\.\d+\.\d+\.\d+$/.test(b.trim()) ? b.trim() : null));
    });
    q.on("error", () => resolve(null));
    q.on("timeout", () => {
      q.destroy();
      resolve(null);
    });
  });
}

// One failed API call: the reason (redacted), and the panel clicks on a 401.
async function failed(r, ctx) {
  say(`FAILED ${r.reason}`);
  if (r.status === 401) for (const l of authHint(ctx.ipLookup ? await ctx.ipLookup() : await publicIp())) say(l);
  return 1;
}

// --dry-run: reads as "not scheduled", prints what a real run would send, never starts schtasks.
function dryRunner() {
  return (a) => {
    if (a[0] === "/Query") return { status: 1, stdout: "", stderr: "" };
    say(`dry-run: schtasks ${a.map((x) => (/\s/.test(x) ? `"${x}"` : x)).join(" ")}`);
    return { status: 0, stdout: "", stderr: "" };
  };
}

// The value after a flag, or undefined when the flag is absent.
function flagValue(rest, flag) {
  const i = rest.indexOf(flag);
  return i < 0 ? undefined : String(rest[i + 1] ?? "");
}

// `saves pull --schedule|--unschedule|--schedule-status` (#1725): the Windows task that keeps an off-site copy.
// Needs no 0grind token: the task runs the pull later. ctx.runner, ctx.isMain, ctx.nodePath, ctx.user are injected by the selftest.
function schedule(rest, ctx) {
  const dry = rest.includes("--dry-run");
  const isMain = (ctx.isMain ?? isMainCheckout(ctx.root)) && !boxMode(ctx.root, ctx.env ?? process.env).on;
  const status = rest.includes("--schedule-status");
  if (isMain && !status) {
    say("REFUSE: this is the main checkout, the world-editor copy; the task would point at it. Run from a worktree (tools\\cli wt new <slug>), or on a rented box run `tools\\cli host box on` once.");
    return 1;
  }
  if (!ctx.runner && !dry && process.platform !== "win32") {
    say("scheduling needs Windows (schtasks)");
    return 1;
  }
  const run = dry ? dryRunner() : ctx.runner ?? realRunner;
  if (rest.includes("--unschedule")) {
    const r = unscheduleTask({ run });
    say(r.message);
    return r.ok ? 0 : 1;
  }
  if (status) {
    const q = queryTask(run);
    if (q.error) {
      say(q.error);
      return 1;
    }
    const i = q.info;
    if (!i.registered) {
      say("not scheduled; `host remote 0grind saves pull --schedule` registers it");
      return 1;
    }
    const sch = readSchedule(i.command);
    say(`scheduled: ${sch ? `every ${sch.everyHours} h, keeps the newest ${sch.keep} -remote pulls` : "(interval not readable from the task)"}, as ${i.user || "?"}, status ${i.status || "?"}`);
    say(i.ran ? `last run ${i.lastRun}, result ${i.lastResult}` : "never ran since it was scheduled");
    return 0;
  }
  const everyText = flagValue(rest, "--every");
  const keepText = flagValue(rest, "--keep");
  const everyHours = everyText === undefined ? DEFAULT_EVERY_HOURS : wholeNumber(everyText, 1, MAX_EVERY_HOURS);
  const keep = keepText === undefined ? DEFAULT_KEEP : wholeNumber(keepText, 1, 1000);
  if (everyHours === null || keep === null) {
    say(`usage: saves pull --schedule [--every <1-${MAX_EVERY_HOURS} hours>] [--keep <1-1000 pulls>] [--dry-run]`);
    return 1;
  }
  const r = scheduleTask({ run, root: ctx.root, nodePath: ctx.nodePath ?? process.execPath, user: ctx.user ?? currentUser(), everyHours, keep });
  say(dry ? `dry-run, nothing registered: ${r.message}` : r.message);
  return r.ok ? 0 : 1;
}

export async function main(args, ctx) {
  const [provider, sub, ...rest] = args;
  if (provider !== "0grind") {
    say(provider ? `unknown remote '${provider}' (only 0grind)` : USAGE);
    return 1;
  }
  if (sub === "saves" && rest[0] === "pull" && ["--schedule", "--unschedule", "--schedule-status"].some((f) => rest.includes(f))) return schedule(rest.slice(1), ctx);
  const yes = rest.includes("--yes");
  const verbose = rest.includes("--verbose");
  const env = ctx.env ?? process.env;
  const dir = remoteDir(env);
  const t = readToken(env);
  if (t.error && sub !== "status") {
    say(t.error);
    return 1;
  }
  const serviceId = env.LF_0GRIND_SERVICE || FLAGSHIP_0GRIND.serviceId;
  const client = t.token
    ? createClient({
        token: t.token,
        serviceId,
        base: env.LF_0GRIND_API || API_BASE,
        lastCallFile: join(dir, ".api-last-call"),
        ...(ctx.minIntervalMs !== undefined ? { minIntervalMs: ctx.minIntervalMs } : {}),
        log: verbose ? (m) => say(`api ${m}`) : () => {},
      })
    : null;

  if (sub === "status") {
    let code = 0;
    if (!client) {
      say(`api: ${t.error}`);
      code = 1;
    } else {
      const s = await client.service();
      if (!s.ok) code = await failed(s, ctx);
      else {
        const d = s.data ?? {};
        const pick = ["status", "alias", "name", "node", "uptime_seconds", "players", "max_players", "version"].filter((k) => d[k] !== undefined);
        say(`api: service ${serviceId} ${pick.map((k) => `${k}=${JSON.stringify(d[k])}`).join(" ") || "(no status field)"}`);
        if (verbose) say(`api: ${client.redact(JSON.stringify(maskSecrets(d)))}`);
      }
      const sy = await client.systemSettings();
      if (!sy.ok) say(`system settings: ${sy.reason}`);
      else {
        const warn = systemSettingsWarnings(sy.data?.settings);
        for (const w of warn) say(`WARNING ${w}`);
        if (!warn.length) say("system settings: restart on failure, session save loading, A2S as the flagship needs");
      }
    }
    const host = env.LF_0GRIND_A2S_HOST || FLAGSHIP_0GRIND.address;
    const port = Number(env.LF_0GRIND_A2S_PORT || FLAGSHIP_0GRIND.a2sPort);
    try {
      const i = await queryInfo(host, port, { timeoutMs: Number(env.LF_0GRIND_A2S_TIMEOUT_MS || 4000) });
      say(`a2s: ${host}:${port} up: "${i.name}" map ${i.map} players ${i.players}/${i.maxPlayers}`);
    } catch (e) {
      say(`a2s: ${host}:${port} no answer (${e.message}): the server is stopped, booting, or A2S is off in system settings`);
    }
    return code;
  }

  if (["start", "stop", "restart"].includes(sub)) {
    if (!yes) {
      say(`refused: ${sub} changes the live server ${FLAGSHIP_0GRIND.address}; add --yes`);
      return 1;
    }
    const r = await client.action(sub);
    if (!r.ok) return failed(r, ctx);
    say(`${sub}: sent (0grind waits at most 10 s; run host remote 0grind status for the result)`);
    return 0;
  }

  if (sub === "system" && rest[0] === "fix") {
    const sy = await client.systemSettings();
    if (!sy.ok) return failed(sy, ctx);
    const { changes, lines } = systemSettingsPlan(sy.data?.settings);
    if (!lines.length) {
      say("system settings: already as the flagship needs");
      return 0;
    }
    for (const l of lines) say(l);
    if (!yes) {
      say("dry run: nothing written; add --yes (the change applies on the next restart)");
      return 0;
    }
    const p = await client.patchSystemSettings(changes);
    if (!p.ok) return failed(p, ctx);
    say(`system settings written: ${(p.data?.changed ?? Object.keys(changes)).join(", ")}; restart to apply`);
    return 0;
  }

  if (sub === "config" && rest[0] === "show") {
    const ss = await client.serverSettings();
    if (!ss.ok) return failed(ss, ctx);
    const sy = await client.systemSettings();
    if (!sy.ok) return failed(sy, ctx);
    const f = await client.get(SERVER_CONFIG_PATH);
    if (!f.ok) return failed(f, ctx);
    let file;
    try {
      file = JSON.parse(f.bytes.toString("utf8"));
    } catch {
      file = { unparsed: `${f.bytes.length} bytes, not JSON` };
    }
    const red = makeRedactor([t.token, ...secretValues(ss.data), ...secretValues(sy.data), ...secretValues(file)]);
    say(`server_settings: ${red(JSON.stringify(maskSecrets(ss.data?.settings ?? ss.data), null, 2))}`);
    say(`system_settings: ${red(JSON.stringify(maskSecrets(sy.data?.settings ?? sy.data), null, 2))}`);
    say(`${SERVER_CONFIG_PATH}: ${red(JSON.stringify(maskSecrets(file), null, 2))}`);
    return 0;
  }

  if (sub === "config" && rest[0] === "push") {
    const oi = rest.indexOf("--only");
    const only = oi >= 0 ? String(rest[oi + 1] ?? "").split(",").map((x) => x.trim()).filter(Boolean) : null;
    if (oi >= 0 && !only.length) {
      say("usage: config push --only <game field>[,<field>...] (name, password, passwordAdmin, scenarioId, mods, ... or staff for staff.txt)");
      return 1;
    }
    const r = await configPush(client, { dir, adminsPending: rest.includes("--admins-pending"), yes, repoDir: ctx.repoDir, only });
    if (!r.ok) return failed(r, ctx);
    for (const l of r.lines) say(l);
    for (const w of r.warnings) say(w);
    return 0;
  }

  if (sub === "logs" && rest[0] === "pull") {
    const r = await logsPull(client, { dir, all: rest.includes("--all") });
    if (!r.ok) return failed(r, ctx);
    say(`logs: ${r.remote} -> ${r.file} (${fileSize(r.file)} bytes${r.pulled.length > 1 ? `, ${r.pulled.length} files` : ""})`);
    say(`read it: tools\\cli host triage "${r.file}" | host summary "${r.file}" | host attendance "${r.file}" | server-doctor "${r.file}"`);
    return 0;
  }

  if (sub === "saves" && rest[0] === "pull") {
    const keepText = flagValue(rest, "--keep");
    const keep = keepText === undefined ? DEFAULT_KEEP : wholeNumber(keepText, 1, 1000);
    if (keep === null) {
      say("usage: saves pull [--prune] [--keep <1-1000 pulls>]");
      return 1;
    }
    const r = await (ctx.savesPull ?? savesPull)(client, { dir });
    if (!r.ok) return failed(r, ctx);
    for (const w of r.warnings) say(`WARNING ${w}`);
    say(`saves: /profile/.save -> ${r.dest} (${r.files} files, ${r.bytes} bytes, sizes checked)`);
    if (rest.includes("--prune")) {
      const p = pruneRemotePulls(join(dir, "backups"), keep);
      say(`prune: kept the newest ${p.kept.length} -remote pulls, deleted ${p.deleted.length}${p.deleted.length ? ` (${p.deleted.join(", ")})` : ""}; tagged and local backups untouched`);
    }
    return 0;
  }

  if (sub === "rcon") {
    const command = rest.filter((a) => !a.startsWith("--")).join(" ").trim();
    if (!command) {
      say("usage: host remote 0grind rcon <command> [--yes]");
      return 1;
    }
    if (!isReadOnlyRcon(command) && !yes) {
      say(`refused: '${command}' is not a read-only RCON command (only #players runs without --yes)`);
      return 1;
    }
    const r = await client.rcon(command);
    if (!r.ok) return failed(r, ctx);
    say(`rcon: ${client.redact(typeof r.data === "string" ? r.data : JSON.stringify(r.data))}`);
    return 0;
  }

  say(USAGE);
  return 1;
}
