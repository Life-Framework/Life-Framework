import { hostPaths } from "../host.mjs";
import { DEFAULT_KEEP, pruneBackups } from "../rotate.mjs";

export const help = "prune [--keep N] [--dry-run]   delete the oldest start, stop and restart backups beyond N (default 20); tagged backups are never deleted";

export async function main(args, ctx) {
  const i = args.indexOf("--keep");
  const keep = i >= 0 ? Number(args[i + 1]) : DEFAULT_KEEP;
  const dryRun = args.includes("--dry-run");
  const { backups } = hostPaths(ctx.hostDir);
  try {
    const r = pruneBackups(backups, keep, { dryRun });
    for (const n of r.deleted) console.log(`host prune: ${dryRun ? "would delete" : "deleted"} ${n}`);
    console.log(`host prune: ${dryRun ? "would delete" : "deleted"} ${r.deleted.length}, kept ${r.kept.length} plain (tagged backups are never deleted)`);
    return 0;
  } catch (e) {
    console.log(`host prune: ${e.message}`);
    return 1;
  }
}
