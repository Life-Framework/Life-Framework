// tools\cli economy (#3122): the newcomer's money ladder from the merged configs, no game needed.
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { buildReport, renderReport } from "../lib/economy.mjs";

export const help = "economy [--mission LF_Everon1989] [--run-minutes parcel=6,fuel=8,taxi=10] [--root <dir>] [--json]   what a newcomer earns and can buy by hour 1, 2 and 4 (core + pack overlays + server/flagship/config.json), with flags for loops, empty fish and zero pay";

const REPO = join(dirname(fileURLToPath(import.meta.url)), "..", "..");

export async function main(args = []) {
  const opt = { json: false, mission: undefined, root: REPO, runMinutes: {} };
  for (let i = 0; i < args.length; i++) {
    const a = args[i];
    if (a === "--json") opt.json = true;
    else if (a === "--mission") opt.mission = args[++i];
    else if (a === "--root") opt.root = args[++i];
    else if (a === "--run-minutes") {
      for (const pair of String(args[++i] ?? "").split(",")) {
        const [k, v] = pair.split("=");
        if (k && Number(v) > 0) opt.runMinutes[k.trim()] = Number(v);
      }
    } else {
      console.error(`economy: unknown argument '${a}'\n${help}`);
      return 2;
    }
  }
  const report = buildReport(opt.root, opt);
  if (report.error) {
    console.error(`economy: ${report.error}`);
    return 2;
  }
  if (opt.json) console.log(JSON.stringify(report, null, 2));
  else for (const line of renderReport(report)) console.log(line);
  return 0;
}
