// `tools\cli host wipe --confirm <host dir name>` (#1274). The logic is tools/host/wipe.mjs.
import { basename } from "node:path";
import { wipeHost } from "../wipe.mjs";

export const help = "wipe --confirm <host dir name>   back the saves up (wipe tag), move them aside, log to backups/wipes.log; refuses while the host runs";

export async function main(args, ctx) {
  const dir = ctx.hostDir;
  const i = args.indexOf("--confirm");
  const confirm = i >= 0 ? args[i + 1] : undefined;
  if (confirm === undefined) {
    console.error(`[LFDebug:HostWipe] refused: pass --confirm ${basename(dir)} to wipe the saves in ${dir}`);
    return 1;
  }
  const r = wipeHost(dir, { confirm });
  if (!r.ok) {
    console.error(`[LFDebug:HostWipe] refused: ${r.reason}`);
    return 1;
  }
  if (!r.backup) {
    console.log(`[LFDebug:HostWipe] ${r.note}`);
    return 0;
  }
  console.log(`[LFDebug:HostWipe] wiped: backup ${r.backup}`);
  console.log(`[LFDebug:HostWipe] old saves moved to ${r.aside}`);
  console.log(`[LFDebug:HostWipe] to restore: ${r.restore}`);
  return 0;
}
