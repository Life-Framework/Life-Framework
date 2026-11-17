// tools\cli tmp-sweep [--hours N] [--dry-run]: deletes the `lf-*` folders older than N hours (default 24) from the OS
// temp dir. Node selftests used to leave them behind (400,000 in a week on lf-testbox, 18 GB, #2589); selftests now use
// tools/lib/scratch.mjs, and this sweeps whatever an older run, a killed run or a crash still left. Never follows
// links (safeRemoveTree), never touches a folder that is not named `lf-*`.
import { lstatSync, readdirSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { safeRemoveTree } from "../link-safety.mjs";

export const help = "tmp-sweep [--hours N] [--dry-run]   delete lf-* folders older than N hours (default 24) from the OS temp dir; link-safe";

// Pure: the names in `entries` ({ name, mtimeMs }) that are `lf-*` and older than `hours`.
export function planSweep(entries, { now = Date.now(), hours = 24 } = {}) {
  const cutoff = now - hours * 3600 * 1000;
  return entries.filter((e) => /^lf-/.test(e.name) && e.mtimeMs < cutoff).map((e) => e.name);
}

export function listTemp(dir) {
  const out = [];
  for (const name of readdirSync(dir)) {
    try {
      const st = lstatSync(join(dir, name));
      if (st.isDirectory() && !st.isSymbolicLink()) out.push({ name, mtimeMs: st.mtimeMs });
    } catch {}
  }
  return out;
}

export async function main(args) {
  const hi = args.indexOf("--hours");
  const hours = hi >= 0 ? Number(args[hi + 1]) : 24;
  if (!Number.isFinite(hours) || hours < 0) {
    console.log("tmp-sweep: --hours needs a number");
    return 2;
  }
  const dry = args.includes("--dry-run");
  const dir = tmpdir();
  const old = planSweep(listTemp(dir), { hours });
  let removed = 0;
  let failed = 0;
  for (const name of old) {
    if (dry) continue;
    try {
      safeRemoveTree(join(dir, name));
      removed++;
    } catch (e) {
      failed++;
      console.log(`tmp-sweep: could not remove ${name}: ${e.message}`);
    }
  }
  console.log(`SUMMARY tmp-sweep ${failed ? "fail" : "ok"} dir=${dir} found=${old.length} removed=${removed} failed=${failed}${dry ? " (dry run)" : ""}`);
  return failed ? 1 : 0;
}
