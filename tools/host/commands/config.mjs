// `tools\cli host config apply [<host dir>] [--repo <dir>] [--dry-run]` (#1322). The logic is tools/host/config-apply.mjs.
import { applyConfig } from "../config-apply.mjs";

export const help = "config apply [<host dir>] [--repo dir] [--dry-run]   write the profile's config.json and server.json from server/flagship plus the host's secrets.json and the roster's admins; refuses while the host runs; secrets are never printed";

export async function main(args, ctx) {
  const [sub, ...rest] = args;
  if (sub !== "apply") {
    console.error("usage: tools\\cli host config apply [<host dir>] [--repo <dir>] [--dry-run]");
    return 1;
  }
  let repoDir;
  let dryRun = false;
  let dir = ctx.hostDir;
  for (let i = 0; i < rest.length; i++) {
    if (rest[i] === "--repo") repoDir = rest[++i];
    else if (rest[i] === "--dry-run") dryRun = true;
    else if (rest[i].startsWith("--")) {
      console.error(`[LFDebug:HostConfig] unknown flag ${rest[i]}`);
      return 1;
    } else dir = rest[i];
  }
  const r = applyConfig(dir, { ...(repoDir ? { repoDir } : {}), dryRun });
  if (!r.ok) {
    console.error(`[LFDebug:HostConfig] refused: ${r.reason}`);
    return 1;
  }
  for (const l of r.lines) console.log(`[LFDebug:HostConfig] ${l}`);
  for (const w of r.warnings) console.log(`[LFDebug:HostConfig] ${w}`);
  console.log(`[LFDebug:HostConfig] ${dryRun ? "checked" : "applied"}: ${r.written.length} file(s) written`);
  return 0;
}
