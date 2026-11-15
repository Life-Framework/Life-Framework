// `tools\cli host watch` (#1262). The logic is tools/host/watchdog.mjs.
//
// Starts the host (or adopts the one running), then supervises it: a crash is restarted with its
// console log and a save backup kept, a scheduled close (Ops O8 marker) is restarted, and an exit
// after `host stop` ends the watch. The host itself is always started through `host start`, so
// the build gate, the start backup, the lock and -loadSessionSave behave as for a hand start.
import { spawnSync } from "node:child_process";
import { join } from "node:path";
import { pidAlive } from "../host.mjs";
import { clearFlags, DEFAULT_MAX_CRASHES, hostPid, POLL_MS, readSignals, recordCrash, watchLoop } from "../watchdog.mjs";

export const help = "watch [--flagship] [--force] [--max-restarts N]   keep the host up: restart a crash (log + save backup kept in backups/<time>-crash) or a scheduled close; ends after `host stop`";

function sleepMs(ms) {
  Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
}

function valueOf(args, flag) {
  const i = args.indexOf(flag);
  return i >= 0 ? args[i + 1] : undefined;
}

export async function main(args, ctx) {
  const dir = ctx.hostDir;
  const passthrough = ["--flagship", "--force"].filter((f) => args.includes(f));
  const rawMax = valueOf(args, "--max-restarts");
  const maxRestarts = rawMax === undefined ? DEFAULT_MAX_CRASHES : Number(rawMax);
  if (!Number.isInteger(maxRestarts) || maxRestarts < 0) {
    console.log(`[LFDebug:HostWatch] --max-restarts must be a whole number >= 0 (got ${rawMax})`);
    return 1;
  }
  const say = (m) => console.log(`[LFDebug:HostWatch] ${m}`);

  // A flag file left by an earlier run would make the first exit look like a stop or a restart.
  const already = hostPid(dir);
  if (!already) clearFlags(dir);

  let pid = already;
  const launch = () => {
    if (pid && pidAlive(pid)) {
      say(`watching the running host (pid ${pid})`);
      return { ok: true, pid };
    }
    clearFlags(dir);
    const r = spawnSync(process.execPath, [join(ctx.root, "tools", "cli.mjs"), "host", "start", ...passthrough], {
      cwd: ctx.root, encoding: "utf8", timeout: 420000, windowsHide: true,
    });
    const out = `${r.stdout ?? ""}${r.stderr ?? ""}`.trim().split(/\r?\n/).slice(-6).join("\n");
    pid = hostPid(dir);
    if (!pid) return { ok: false, reason: `host start exited ${r.status} and no host runs\n${out}` };
    if (r.status !== 0) say(`host start returned ${r.status} but pid ${pid} runs; watching it\n${out}`);
    else say(`host up (pid ${pid})`);
    return { ok: true, pid };
  };

  const result = watchLoop({
    launch,
    isAlive: () => pidAlive(pid),
    signals: (final) => readSignals(dir, { final }),
    onCrash: (n) => {
      const c = recordCrash(dir);
      say(`crash ${n}: console log kept in ${c.logDir}${c.backup ? "" : " (no save to back up yet)"}`);
      if (c.pruned.length) say(`rotated ${c.pruned.length} old start/stop backup(s)`);
    },
    afterExit: (d) => {
      clearFlags(dir);
      pid = 0;
      say(`host exited: ${d.why}`);
    },
    sleep: sleepMs,
    log: say,
    maxRestarts,
    pollMs: POLL_MS,
    restartDelayMs: 2000,
  });
  return result.reason === "stop" ? 0 : 1;
}
