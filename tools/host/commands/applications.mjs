// `tools\cli host applications [--all]`: prints the police application queue from the host
// profile's LifeFramework/police_applications.txt (open applications oldest first, then the
// officers on trial with the hours left) so the maintainer can review it before a session.
// Read only: approving and rejecting stay in the game. `--file <path>` reads another file and
// `--now <unix>` fixes the clock (the selftest uses both).
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { formatQueue, parseApplications } from "../applications.mjs";
import { hostPaths } from "../host.mjs";

export const help = "applications [--all]   the police application queue (oldest open first, officers on trial with hours left) from the host profile; --all adds closed rows";

function option(args, name) {
  const i = args.indexOf(name);
  return i >= 0 ? args[i + 1] : undefined;
}

export async function main(args, ctx) {
  const file = option(args, "--file") ?? join(hostPaths(ctx.hostDir).profile, "LifeFramework", "police_applications.txt");
  const nowArg = option(args, "--now");
  const now = nowArg === undefined ? Math.floor(Date.now() / 1000) : Number(nowArg);
  if (!Number.isFinite(now)) {
    console.log(`[LFHost:applications] --now needs a unix time, got "${nowArg}"`);
    return 1;
  }
  if (!existsSync(file)) {
    console.log(`[LFHost:applications] no applications file at ${file}: nobody has applied yet`);
    return 0;
  }
  let text;
  try {
    text = readFileSync(file, "utf8");
  } catch (e) {
    console.log(`[LFHost:applications] cannot read ${file}: ${e.message}`);
    return 1;
  }
  const { rows, warnings } = parseApplications(text);
  for (const w of warnings) console.log(`[LFHost:applications] warning: ${w}`);
  process.stdout.write(formatQueue(rows, now, { all: args.includes("--all") }));
  return 0;
}
