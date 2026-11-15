// `tools\cli host drill [<backup>] [--timeout <s>] [--keep]` (#1261): the restore drill. The newest (or the
// named) backup of the host dir boots on a scratch profile on a pool machine; it passes only when the log shows
// `save loaded:` and at least one applied account record. The live host profile is never touched. Logic and the
// judge: tools/host/drill.mjs.
import { copyFileSync, existsSync, mkdirSync } from "node:fs";
import { basename, join } from "node:path";
import { safeRemoveTree } from "../../link-safety.mjs";
import { stamp } from "../host.mjs";
import { DRILL_ARCHIVE_ENV, pickDrillBackup, prepareScratch, runDrillHere, summaryLine, unzipTo, zipDir } from "../drill.mjs";

export const help = "drill [<backup>] [--timeout <s>] [--keep]   boot a backup on a scratch profile on a pool machine; pass only if the save loads with its accounts";

function opt(args, name) {
  const i = args.indexOf(name);
  return i >= 0 ? args[i + 1] : undefined;
}

export async function main(args, ctx) {
  const { root, hostDir } = ctx;
  const timeoutSec = opt(args, "--timeout") ? Number(opt(args, "--timeout")) : 300;
  const keep = args.includes("--keep");
  const name = args.find((a, i) => !a.startsWith("--") && args[i - 1] !== "--timeout");
  const tmp = join(root, "tmp");
  const machineName = process.env.COMPUTERNAME ?? "pool";

  // The pool machine's leg: the scratch profile arrives as a zip.
  if (process.env[DRILL_ARCHIVE_ENV]) {
    const scratch = join(tmp, `drill-run-${process.pid}`);
    try {
      unzipTo(process.env[DRILL_ARCHIVE_ENV], scratch);
      const r = runDrillHere({ root, scratchDir: scratch, timeoutSec });
      console.log(summaryLine({ ok: r.ok, backup: name ?? "backup", machine: machineName, accounts: r.accounts, log: r.log, reason: r.reason }));
      return r.ok ? 0 : 1;
    } catch (e) {
      console.log(summaryLine({ ok: false, backup: name ?? "backup", machine: machineName, reason: e.message }));
      return 1;
    } finally {
      if (!keep) safeRemoveTree(scratch);
    }
  }

  const backup = pickDrillBackup(hostDir, name);
  if (!backup) {
    console.log(summaryLine({ ok: false, backup: name ?? "none", reason: name ? `no backup named ${name} in ${hostDir}` : `no backup in ${hostDir} (tools\\cli host backup makes one)` }));
    return 1;
  }
  const label = basename(backup);
  const scratch = join(tmp, `drill-${stamp()}`);
  mkdirSync(tmp, { recursive: true });
  let slot = null;
  try {
    prepareScratch(backup, scratch);
    console.log(`drill: ${label} copied to ${scratch}; taking a pool lane`);
    const pool = await import("../../remote/pool.mjs");
    slot = await pool.acquireSlot(root, { mode: "any", purpose: "drill", priority: "low" });
    if (slot.local) {
      const r = runDrillHere({ root, scratchDir: scratch, timeoutSec });
      // The scratch profile is deleted below; keep the console log the verdict came from.
      let kept = r.log;
      if (r.log && existsSync(r.log)) {
        kept = join(tmp, "logs", `host-drill-${stamp()}.log`);
        mkdirSync(join(tmp, "logs"), { recursive: true });
        copyFileSync(r.log, kept);
      }
      console.log(summaryLine({ ok: r.ok, backup: label, machine: slot.machine?.name ?? "local", accounts: r.accounts, log: kept, reason: r.reason }));
      return r.ok ? 0 : 1;
    }
    const testbox = await import("../../remote/testbox.mjs");
    const zip = zipDir(scratch, join(tmp, `drill-${label}.zip`));
    const res = await testbox.runRemote(root, slot, {
      args: ["host", "drill", label, "--timeout", String(timeoutSec), ...(keep ? ["--keep"] : [])],
      writeSyncState: pool.writeSyncState,
      fetchLog: false,
      files: { env: { [DRILL_ARCHIVE_ENV]: zip } },
    });
    if (res.code !== 0) console.log(`drill: the run on ${slot.machine?.name} exited ${res.code}; its SUMMARY line is above`);
    return res.code === 0 ? 0 : 1;
  } catch (e) {
    console.log(summaryLine({ ok: false, backup: label, reason: e.message }));
    return 1;
  } finally {
    if (slot) slot.release();
    if (!keep) safeRemoveTree(scratch);
  }
}
