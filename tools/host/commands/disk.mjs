import { diskReport, fmtBytes, FLOOR_BYTES } from "../disk.mjs";

export const help = "disk   save and backup sizes, growth over the last 5 backups, free space, sessions that fit before 5 GB free; exits 1 under 5 GB";

// ctx.freeBytes injects the free space (the selftest); otherwise the drive is read.
export async function main(args, ctx) {
  try {
    const r = diskReport(ctx.hostDir, { freeBytes: ctx.freeBytes });
    const fits = Number.isFinite(r.sessionsFit) ? String(r.sessionsFit) : "unbounded (empty save)";
    const sign = r.growthBytes >= 0 ? "+" : "";
    console.log(`host disk: save ${fmtBytes(r.saveBytes)}`);
    console.log(`host disk: ${r.backupCount} backups, ${fmtBytes(r.backupsBytes)} total, ${sign}${fmtBytes(r.growthBytes)} over the last 5 (${sign}${fmtBytes(r.growthPerBackup)} per backup)`);
    console.log(`host disk: ${fmtBytes(r.freeBytes)} free, ${fits} sessions fit before ${fmtBytes(FLOOR_BYTES)} free`);
    if (r.low) {
      console.log(`host disk: LOW, under ${fmtBytes(FLOOR_BYTES)} free; free space or run \`host prune\``);
      return 1;
    }
    return 0;
  } catch (e) {
    console.log(`host disk: ${e.message}`);
    return 1;
  }
}
