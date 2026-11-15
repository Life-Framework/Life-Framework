// `tools\cli host errors` (#1500). The logic is tools/host/errors.mjs.
//
// Follows the newest server console.log during a session and prints only `[LFDebug:*]` errors,
// `SCRIPT (E)` and VME lines, each marked KNOWN or NEW against docs/debug-log-triage.md (the
// matcher is tools/lib/log-triage.mjs, as in `host triage`), so a problem shows before players
// report it. It reads the log from its start, picks up a newer log when the host restarts, and
// ends after `--for` minutes.
import { existsSync } from "node:fs";
import { join } from "node:path";
import { hostPaths, newestConsoleLog } from "../host.mjs";
import { loadTriageRows } from "../../lib/log-triage.mjs";
import { DEFAULT_FOR_MIN, follow, formatError, parseArgs } from "../errors.mjs";

export const help = `errors [--for <min>]   follow the newest server console.log and print only [LFDebug:*] errors, SCRIPT (E) and VME lines, each marked KNOWN (docs/debug-log-triage.md) or NEW; reads from the start of the log, ends after --for minutes (default ${DEFAULT_FOR_MIN})`;

function sleepMs(ms) {
  Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
}

export async function main(args, ctx) {
  const say = (m) => console.log(`[LFDebug:HostErrors] ${m}`);
  const opts = parseArgs(args);
  if (opts.error) {
    say(opts.error);
    return 1;
  }
  const triageFile = join(ctx.root, "docs", "debug-log-triage.md");
  if (!existsSync(triageFile)) {
    say(`triage table not found at ${triageFile}`);
    return 1;
  }
  const rows = loadTriageRows(triageFile);
  const profile = hostPaths(ctx.hostDir).profile;
  say(`following the newest console.log for ${opts.forMin} min (errors only; KNOWN = a row in docs/debug-log-triage.md)`);
  const t = follow({
    file: () => newestConsoleLog(profile),
    rows,
    forSec: opts.forMin * 60,
    out: (e) => console.log(formatError(e)),
    sleep: sleepMs,
    now: Date.now,
  });
  console.log(`SUMMARY host-errors ok errors=${t.errors} known=${t.known} new=${t.fresh} for=${opts.forMin}min`);
  return 0;
}
