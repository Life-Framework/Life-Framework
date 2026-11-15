// tools\cli host logs archive [--keep N] [--dry-run] | host logs list [--json]
// archive moves each finished session's engine log folder from <host>/profile/logs/ to
// <host>/logs/<session-stamp>/ and keeps the newest 30 sessions; the session still being written is
// never touched (tools/host/logs.mjs). list prints the archive with each session's size and the
// error count `host triage` finds in its console.log.

import { join } from "node:path";
import { hostPaths, pidAlive, readHostState } from "../host.mjs";
import { ARCHIVE_KEEP, archiveLogs, listArchive } from "../logs.mjs";

export const help =
  "logs archive [--keep N] [--dry-run] | logs list [--json]   archive finished sessions' engine logs to <host>/logs/<stamp>/ (newest 30 kept, the live session untouched); list shows size and triage error count";

function mb(bytes) {
  return `${(bytes / 1048576).toFixed(1)} MB`;
}

export async function main(args, ctx) {
  const sub = args[0];
  const { profile, dir } = hostPaths(ctx.hostDir);
  const archiveDir = join(dir, "logs");
  if (sub === "archive") {
    const i = args.indexOf("--keep");
    const keep = i >= 0 ? Number(args[i + 1]) : ARCHIVE_KEEP;
    const dryRun = args.includes("--dry-run");
    const hostRunning = ctx.hostRunning ?? pidAlive(readHostState(dir)?.pid);
    try {
      const r = archiveLogs(profile, archiveDir, { keep, hostRunning, dryRun });
      const verb = dryRun ? "would archive" : "archived";
      for (const n of r.archived) console.log(`host logs: ${verb} ${n}`);
      for (const n of r.pruned) console.log(`host logs: ${dryRun ? "would delete" : "deleted"} old session ${n}`);
      for (const f of r.failed) console.log(`host logs: could not move ${f.name} (${f.error}); left in the profile`);
      const live = r.live ? `, left ${r.live} (still being written)` : "";
      console.log(`host logs: ${verb} ${r.archived.length}${live}, ${r.pruned.length} old ${dryRun ? "would go" : "deleted"}, keeping the newest ${keep}`);
      return r.failed.length ? 1 : 0;
    } catch (e) {
      console.log(`host logs: ${e.message}`);
      return 1;
    }
  }
  if (sub === "list") {
    const sessions = listArchive(archiveDir, join(ctx.root, "docs", "debug-log-triage.md"));
    if (args.includes("--json")) {
      console.log(JSON.stringify(sessions, null, 2));
      return 0;
    }
    for (const s of sessions) {
      const triage = s.errors === null ? "errors=- (no console.log)" : `errors=${s.errors} groups=${s.groups} new=${s.newGroups}`;
      console.log(`${s.name}  ${mb(s.bytes).padStart(9)}  ${triage}`);
    }
    console.log(`host logs: ${sessions.length} archived session${sessions.length === 1 ? "" : "s"} in ${archiveDir}`);
    return 0;
  }
  console.log("usage: tools\\cli host logs archive [--keep N] [--dry-run] | host logs list [--json]");
  return 1;
}
