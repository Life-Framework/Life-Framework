import { resolve } from "node:path";
import { hostPaths } from "../host.mjs";
import { DEFAULT_KEEP } from "../rotate.mjs";
import { offsiteBackups } from "../offsite.mjs";

export const help = "offsite <dir> [--keep N] [--dry-run]   copy every backup not already in <dir> (second drive or NAS), verify by size and hash, rotate it like the host (default keep 20, tagged backups kept); never deletes from the source";

export async function main(args, ctx) {
  const dryRun = args.includes("--dry-run");
  const i = args.indexOf("--keep");
  const keep = i >= 0 ? Number(args[i + 1]) : DEFAULT_KEEP;
  const dir = args.find((a, j) => !a.startsWith("--") && !(i >= 0 && j === i + 1));
  if (!dir) {
    console.log("host offsite: give the folder to copy to, e.g. host offsite D:\\lf-backups");
    return 1;
  }
  const { backups } = hostPaths(ctx.hostDir);
  try {
    const r = offsiteBackups(backups, resolve(dir), { keep, dryRun });
    for (const n of r.copied) console.log(`host offsite: ${dryRun ? "would copy" : "copied"} ${n}`);
    for (const n of r.replaced) console.log(`host offsite: ${dryRun ? "would replace" : "replaced"} damaged copy ${n}`);
    for (const n of r.pruned) console.log(`host offsite: ${dryRun ? "would prune" : "pruned"} ${n} from the target (beyond --keep ${keep})`);
    for (const f of r.failed) console.log(`host offsite: FAILED ${f.name}: ${f.why}`);
    console.log(`host offsite: ${dryRun ? "dry run: " : ""}${r.copied.length} copied, ${r.replaced.length} replaced, ${r.skipped.length} already there, ${r.failed.length} failed (source never touched)`);
    return r.failed.length ? 1 : 0;
  } catch (e) {
    console.log(`host offsite: ${e.message}`);
    return 1;
  }
}
