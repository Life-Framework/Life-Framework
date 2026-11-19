// tools\cli economy (#3122): the newcomer's money ladder from the merged configs, no game needed.
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { buildReport, renderReport } from "../lib/economy.mjs";

export const help = "economy [--mission LF_Everon1989] [--run-minutes parcel=6,fuel=8,taxi=10] [--taxi-metres 2000] [--wholesale-carry] [--hours 1,2,4,6] [--root <dir>] [--json]   what a newcomer earns and can buy by hour 1, 2, 4 and 6 (core + pack overlays + server/flagship/config.json): rank duty pay, taxi fares, contracts, licence fees, and flags for loops, empty fish and zero pay; the lumber loops (#3270) with steady and first-hour $/h and a flag outside the 1.5 to 2.5 duty band (--wholesale-carry keeps the wholesale curve between hours)";

const REPO = join(dirname(fileURLToPath(import.meta.url)), "..", "..");

export async function main(args = []) {
  const opt = { json: false, lumber: {}, mission: undefined, root: REPO, runMinutes: {} };
  for (let i = 0; i < args.length; i++) {
    const a = args[i];
    if (a === "--json") opt.json = true;
    else if (a === "--wholesale-carry") opt.lumber.carryCurve = true;
    else if (a === "--mission") opt.mission = args[++i];
    else if (a === "--root") opt.root = args[++i];
    else if (a === "--taxi-metres") {
      opt.taxiMetres = Number(args[++i]);
      if (!(opt.taxiMetres > 0)) {
        console.error(`economy: --taxi-metres needs a positive number\n${help}`);
        return 2;
      }
    } else if (a === "--hours") {
      opt.hours = String(args[++i] ?? "").split(",").map(Number);
      if (opt.hours.some((h) => !(h > 0))) {
        console.error(`economy: --hours needs positive numbers like 1,2,4,6\n${help}`);
        return 2;
      }
    } else if (a === "--run-minutes") {
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
