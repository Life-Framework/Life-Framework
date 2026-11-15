// `tools\cli host config apply [<host dir>] [--repo <dir>] [--dry-run]` (#1322). The logic is tools/host/config-apply.mjs.
import { applyConfig } from "../config-apply.mjs";
import { parseModsFlag } from "../public.mjs";

export const help = "config apply [<host dir>] [--repo dir] [--dry-run] [--mods <core-id>,<pack-id>] [--admins-pending] [--roster file]   write the profile's config.json and server.json from server/flagship plus the host's secrets.json and the roster's admins; refuses while the host runs; secrets are never printed; --mods (after publishing) confirms the Workshop IDs from the two item pages for host start --public; --admins-pending writes game.admins while an administrator is still #pending (the first public boot); --roster reads that roster file instead of server/flagship/staff.txt, for a scratch host only (refused on the real host folder)";

export async function main(args, ctx) {
  const [sub, ...rest] = args;
  if (sub !== "apply") {
    console.error("usage: tools\\cli host config apply [<host dir>] [--repo <dir>] [--dry-run] [--mods <core-id>,<pack-id>] [--admins-pending] [--roster <file>]");
    return 1;
  }
  let repoDir;
  let dryRun = false;
  let mods = null;
  let adminsPending = false;
  let rosterFile = null;
  let dir = ctx.hostDir;
  for (let i = 0; i < rest.length; i++) {
    if (rest[i] === "--repo") repoDir = rest[++i];
    else if (rest[i] === "--dry-run") dryRun = true;
    else if (rest[i] === "--admins-pending") adminsPending = true;
    else if (rest[i] === "--roster") rosterFile = rest[++i];
    else if (rest[i] === "--mods") {
      const m = parseModsFlag(rest[++i]);
      if (m.error) {
        console.error(`[LFDebug:HostConfig] refused: ${m.error}`);
        return 1;
      }
      mods = m.ids;
    }
    else if (rest[i].startsWith("--")) {
      console.error(`[LFDebug:HostConfig] unknown flag ${rest[i]}`);
      return 1;
    } else dir = rest[i];
  }
  const r = applyConfig(dir, { ...(repoDir ? { repoDir } : {}), dryRun, mods, adminsPending, rosterFile });
  if (!r.ok) {
    console.error(`[LFDebug:HostConfig] refused: ${r.reason}`);
    return 1;
  }
  for (const l of r.lines) console.log(`[LFDebug:HostConfig] ${l}`);
  for (const w of r.warnings) console.log(`[LFDebug:HostConfig] ${w}`);
  console.log(`[LFDebug:HostConfig] ${dryRun ? "checked" : "applied"}: ${r.written.length} file(s) written`);
  return 0;
}
