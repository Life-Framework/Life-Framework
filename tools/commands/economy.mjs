// tools\cli economy (#3122): the newcomer's money ladder from the merged configs, no game needed.
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { buildReport, renderReport } from "../lib/economy.mjs";
import { dutyLadder, renderDutyLadder } from "../lib/economy-lumber.mjs";
import { perksReport } from "../lib/economy-perks.mjs";

export const help = "economy [--mission LF_Everon1989] [--run-minutes parcel=6,fuel=8,taxi=10] [--taxi-metres 2000] [--wholesale-carry] [--hours 1,2,4,6] [--root <dir>] [--json]   what a newcomer earns and can buy by hour 1, 2, 4 and 6 (core + pack overlays + server/flagship/config.json): rank duty pay with each rank's shift, bonus cap and rung (the 0.3.0 pay ladder, #4178), taxi fares, contracts, licence fees, and flags for loops, empty fish and zero pay; the lumber loops (#3270) and the mining chains (#3240) with steady and first-hour $/h and a flag for a loop under the Cadet shift, then each chain with full perks (WK-W11_2: double yield and good word) beside the novice line, flagged under its rung or past crime (--wholesale-carry keeps the wholesale curve between hours)";

/** The duty rungs of a built report (#4178): { duty: { rows, flags }, lines } for every police and medic rank. */
export function ladderReport(report) {
  const duty = dutyLadder(report.duty);
  return { duty, lines: renderDutyLadder(duty) };
}

/** The full-perks section of a built report (WK-W11_2, #4409): { rows, lines, flags } for every lumber, wheat and mining chain. */
export function perksSection(report) {
  return perksReport(report);
}

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
  const ladder = ladderReport(report);
  report.dutyLadder = ladder.duty.rows;
  report.flags.push(...ladder.duty.flags);
  const perks = perksSection(report);
  report.perks = perks;
  report.flags.push(...perks.flags);
  if (opt.json) console.log(JSON.stringify(report, null, 2));
  else {
    // The duty rungs print right after the duty pay lines, before the purchases; the full-perks lines follow the chains.
    const lines = renderReport(report);
    const at = lines.findIndex((l) => l.startsWith("cheapest purchase:"));
    lines.splice(at < 0 ? lines.length : at, 0, ...ladder.lines);
    const flagsAt = lines.findIndex((l) => /^flags?\b/i.test(l));
    lines.splice(flagsAt < 0 ? lines.length : flagsAt, 0, ...perks.lines);
    for (const line of lines) console.log(line);
  }
  return 0;
}
