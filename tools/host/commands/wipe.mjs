// `tools\cli host wipe --confirm <host dir name>` (#1274). The logic is tools/host/wipe.mjs.
import { basename, join } from "node:path";
import { wipeHost } from "../wipe.mjs";

export const help = "wipe --confirm <host dir name>   back the saves up (wipe tag), move them aside, log to backups/wipes.log; refuses while the host runs or without a wipe notice posted 24 h earlier (host announce --wipe)";

export async function main(args, ctx) {
  const dir = ctx.hostDir;
  const i = args.indexOf("--confirm");
  const confirm = i >= 0 ? args[i + 1] : undefined;
  if (confirm === undefined) {
    console.error(`[LFDebug:HostWipe] refused: pass --confirm ${basename(dir)} to wipe the saves in ${dir}`);
    return 1;
  }
  // The notice is where `host announce` records it (LF_ANNOUNCE_SESSIONS overrides, as there).
  const sessions = process.env.LF_ANNOUNCE_SESSIONS || join(ctx.root, "website", "content", "sessions.json");
  const r = wipeHost(dir, { confirm, sessions });
  if (!r.ok) {
    console.error(`[LFDebug:HostWipe] refused: ${r.reason}`);
    if (r.allowedAt) console.error(`[LFDebug:HostWipe] the wipe is allowed from ${r.allowedAt}`);
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
