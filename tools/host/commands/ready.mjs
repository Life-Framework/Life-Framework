// `tools\cli host ready [--max-backup-hours N] [--max-drill-days N]` (#1408): the go / no-go before a session.
// One line per check, then `host ready: GO` (exit 0) or `host ready: NO-GO <failed checks>` (exit 1). Logic and
// the checks: tools/host/ready.mjs. ctx.checks injects the checks (the selftest).
import { defaultChecks, runChecks } from "../ready.mjs";

export const help = "ready [--max-backup-hours N] [--max-drill-days N]   go / no-go before a session: build gate, staff roster, backup age, last restore drill, disk; exits 1 on NO-GO";

function num(args, name) {
  const i = args.indexOf(name);
  if (i < 0) return undefined;
  const n = Number(args[i + 1]);
  return Number.isFinite(n) && n > 0 ? n : undefined;
}

export async function main(args, ctx) {
  const { go, results, failed } = await runChecks(ctx.checks ?? defaultChecks(), {
    root: ctx.root,
    hostDir: ctx.hostDir,
    now: ctx.now ?? new Date(),
    freeBytes: ctx.freeBytes,
    rosterFile: ctx.rosterFile,
    maxBackupHours: num(args, "--max-backup-hours"),
    maxDrillDays: num(args, "--max-drill-days"),
  });
  for (const r of results) console.log(`host ready: ${r.ok ? "ok  " : "FAIL"} ${r.name.padEnd(7)} ${r.detail}`);
  if (go) {
    console.log("host ready: GO");
    return 0;
  }
  console.log(`host ready: NO-GO ${failed.join(", ")}`);
  return 1;
}
