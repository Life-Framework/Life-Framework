// `tools\cli host history [--json]` (#1409). The logic is tools/host/history.mjs.
import { historyText, hostHistory } from "../history.mjs";

export const help = "history [--json]   one row per past session (start, end, minutes, crashes, backups, wipe, peak players) rebuilt from the backup folders, backups/wipes.log and perf/*.csv; writes nothing";

export async function main(args, ctx) {
  const { sessions } = hostHistory(ctx.hostDir);
  if (args.includes("--json")) console.log(JSON.stringify({ sessions }, null, 2));
  else console.log(historyText(sessions));
  return 0;
}
