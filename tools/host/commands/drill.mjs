// `tools\cli host drill [<backup>] [--timeout <s>] [--keep]` (#1261): the restore drill. The newest (or the
// named) backup of the host dir boots on a scratch profile on a pool machine; it passes only when the log shows
// `save loaded:` and at least one applied account record. The live host profile is never touched. Logic and the
// judge: tools/host/drill.mjs.
import { copyFileSync, existsSync, mkdirSync } from "node:fs";
import { basename, join } from "node:path";
import { safeRemoveTree } from "../../link-safety.mjs";
import { stamp } from "../host.mjs";
import { DRILL_ARCHIVE_ENV, DRILL_RESULT_FILE, parseSummaryLine, pickDrillBackup, prepareScratch, runDrillHere, summaryLine, unzipTo, writeDrillResult, zipDir } from "../drill.mjs";

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

  // Every run leaves <host dir>/drill-last.json, which `host ready` reads first (#1558). A write that fails
  // must not change the drill's own verdict.
  const record = (r) => {
    try {
      writeDrillResult(hostDir, r);
    } catch (e) {
      console.log(`drill: could not write ${DRILL_RESULT_FILE}: ${e.message}`);
    }
  };

  const backup = pickDrillBackup(hostDir, name);
  if (!backup) {
    const reason = name ? `no backup named ${name} in ${hostDir}` : `no backup in ${hostDir} (tools\\cli host backup makes one)`;
    console.log(summaryLine({ ok: false, backup: name ?? "none", reason }));
    record({ ok: false, backup: name ?? null, machine: "local", accounts: 0, reason });
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
      const machine = slot.machine?.name ?? "local";
      console.log(summaryLine({ ok: r.ok, backup: label, machine, accounts: r.accounts, log: kept, reason: r.reason }));
      record({ ok: r.ok, backup: label, machine, accounts: r.accounts, reason: r.reason });
      return r.ok ? 0 : 1;
    }
    const testbox = await import("../../remote/testbox.mjs");
    const zip = zipDir(scratch, join(tmp, `drill-${label}.zip`));
    const machine = slot.machine?.name ?? "pool";
    // The box prints its own SUMMARY line into this process's stdout; keep a copy to turn into the result file.
    let seen = "";
    const realWrite = process.stdout.write;
    process.stdout.write = function (chunk, ...rest) {
      seen += String(chunk);
      return realWrite.call(process.stdout, chunk, ...rest);
    };
    let res;
    try {
      res = await testbox.runRemote(root, slot, {
        args: ["host", "drill", label, "--timeout", String(timeoutSec), ...(keep ? ["--keep"] : [])],
        writeSyncState: pool.writeSyncState,
        fetchLog: false,
        files: { env: { [DRILL_ARCHIVE_ENV]: zip } },
      });
    } finally {
      process.stdout.write = realWrite;
    }
    if (res.code !== 0) console.log(`drill: the run on ${machine} exited ${res.code}; its SUMMARY line is above`);
    const parsed = parseSummaryLine(seen);
    // No SUMMARY line means the box never judged the drill (ssh down, sync failed): that is a failed drill, not a pass.
    if (parsed) record({ ok: parsed.ok && res.code === 0, backup: label, machine, accounts: parsed.accounts, reason: parsed.reason });
    else record({ ok: false, backup: label, machine, accounts: 0, reason: `the run on ${machine} exited ${res.code} without a result` });
    return res.code === 0 ? 0 : 1;
  } catch (e) {
    console.log(summaryLine({ ok: false, backup: label, reason: e.message }));
    record({ ok: false, backup: label, machine: slot?.machine?.name ?? "local", accounts: 0, reason: e.message });
    return 1;
  } finally {
    if (slot) slot.release();
    if (!keep) safeRemoveTree(scratch);
  }
}
