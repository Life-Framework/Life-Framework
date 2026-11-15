// `tools\cli host gate [--target N] [--sessions N] [--roster <staff.txt>] [--issues-file <json>] [--json]` (#1410):
// the #555 opening gate, one verdict. Reads `host history`, the last restore drill, the open `regression`
// issues and the staff roster; exit 0 only when every line passes. Writes nothing. Logic: tools/host/gate.mjs.
import { DEFAULT_SESSIONS, DEFAULT_TARGET, evaluateGate, gateSummary, gateText, gatherInputs } from "../gate.mjs";

export const help = "gate [--target N] [--sessions N] [--roster f] [--issues-file f] [--json]   the #555 opening gate: sessions in a row at the player target, no save loss, no open regression, an administrator uid in the roster; exit 0 only when all pass";

function opt(args, name) {
  const i = args.indexOf(name);
  return i >= 0 ? args[i + 1] : undefined;
}

export async function main(args, ctx) {
  const target = opt(args, "--target") ? Number(opt(args, "--target")) : DEFAULT_TARGET;
  const sessions = opt(args, "--sessions") ? Number(opt(args, "--sessions")) : DEFAULT_SESSIONS;
  if (!(target > 0) || !Number.isInteger(sessions) || sessions < 1) {
    console.log("[LFHost:gate] --target must be a positive number and --sessions a whole number of 1 or more");
    return 2;
  }
  const inputs = gatherInputs({ root: ctx.root, hostDir: ctx.hostDir, issuesFile: opt(args, "--issues-file"), rosterFile: opt(args, "--roster") });
  const g = evaluateGate({ ...inputs, target, sessions });
  if (args.includes("--json")) console.log(JSON.stringify(g, null, 2));
  else {
    console.log(gateText(g));
    console.log(gateSummary(g));
  }
  return g.open ? 0 : 1;
}
