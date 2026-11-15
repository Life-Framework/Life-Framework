// `tools\cli host drift [--flagship <dir>]` (#1462). The logic is tools/host/drift.mjs.
import { join } from "node:path";
import { driftHost, formatChange } from "../drift.mjs";

export const help = "drift [--flagship <dir>]   diff the host profile's config.json and server.json against server/flagship/ (host-only secrets ignored); exit 1 on drift";

export async function main(args, ctx) {
  const i = args.indexOf("--flagship");
  const flagship = i >= 0 && args[i + 1] ? args[i + 1] : join(ctx.root, "server", "flagship");
  const r = driftHost(ctx.hostDir, { flagship });
  if (r.ok) {
    console.log(`[LFDebug:HostDrift] no drift: the live config matches ${flagship}`);
    return 0;
  }
  for (const c of r.changes) console.error(`[LFDebug:HostDrift] ${formatChange(c)}`);
  console.error(`[LFDebug:HostDrift] drift: ${r.changes.length} difference(s); a hand edit on the host is overwritten by tools\\cli host config apply, a wanted change goes through a PR (docs/ops/hotfix.md)`);
  return 1;
}
