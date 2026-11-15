import { hostPaths } from "../host.mjs";
import { verifyBackups } from "../verify.mjs";

export const help = "verify [--newest N]   open each backup: not empty, a save point a restore can load, a readable build stamp, checksum unchanged since the last run; exit 1 on any bad backup (read only)";

export async function main(args, ctx) {
  const i = args.indexOf("--newest");
  const newest = i >= 0 ? Number(args[i + 1]) : undefined;
  const { backups } = hostPaths(ctx.hostDir);
  try {
    const r = verifyBackups(backups, { newest });
    for (const x of r.results) console.log(`host verify: ${x.status.padEnd(7)} ${x.name}  ${x.detail}`);
    const bad = r.results.filter((x) => x.status !== "ok").length;
    console.log(`host verify: ${r.results.length} backup(s), ${bad} bad${r.results.length ? "" : " (no backups yet)"}`);
    return r.ok ? 0 : 1;
  } catch (e) {
    console.log(`host verify: ${e.message}`);
    return 1;
  }
}
