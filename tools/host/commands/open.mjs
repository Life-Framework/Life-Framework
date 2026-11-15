// `tools\cli host open --confirm <host dir name> [--dry-run]` (#1512): the opening step of #555. The logic is tools/host/open.mjs.
//
// Refuses unless `host gate` is open, then clears the join password in the host's secrets.json, re-applies
// the config and drafts the opening announcement. Maintainer-only; agents run only --dry-run on a scratch host.
import { basename } from "node:path";
import { DEFAULT_SESSIONS, DEFAULT_TARGET, evaluateGate, gateSummary, gatherInputs } from "../gate.mjs";
import { openHost } from "../open.mjs";

export const help = "open --confirm <host dir name> [--dry-run] [--target N] [--sessions N] [--roster f] [--issues-file f] [--repo dir]   the #555 opening step: refuses unless `host gate` is open, then clears the join password in the host's secrets.json (nothing else), re-applies the config and drafts the opening announcement; --dry-run prints the steps and writes nothing; the host must be stopped; maintainer-only";

const say = (m) => console.log(`[LFDebug:HostOpen] ${m}`);

function opt(args, name) {
  const i = args.indexOf(name);
  return i >= 0 ? args[i + 1] : undefined;
}

// ctx.gate is a ready gate result, injected by the selftest; otherwise the real gate is read.
export async function main(args, ctx) {
  const dir = ctx.hostDir;
  const dryRun = args.includes("--dry-run");
  const confirm = opt(args, "--confirm");
  if (!dryRun && confirm === undefined) {
    console.error(`[LFDebug:HostOpen] refused: pass --confirm ${basename(dir)} to open the host in ${dir} (or --dry-run to see the steps)`);
    return 1;
  }
  const target = opt(args, "--target") ? Number(opt(args, "--target")) : DEFAULT_TARGET;
  const sessions = opt(args, "--sessions") ? Number(opt(args, "--sessions")) : DEFAULT_SESSIONS;
  if (!(target > 0) || !Number.isInteger(sessions) || sessions < 1) {
    console.error("[LFDebug:HostOpen] --target must be a positive number and --sessions a whole number of 1 or more");
    return 1;
  }
  const gate = ctx.gate ?? evaluateGate({ ...gatherInputs({ root: ctx.root, hostDir: dir, issuesFile: opt(args, "--issues-file"), rosterFile: opt(args, "--roster") }), target, sessions });
  const repo = opt(args, "--repo");
  const r = openHost(dir, { confirm, gate, dryRun, ...(repo ? { repoDir: repo } : {}) });
  for (const l of r.lines ?? []) say(l);
  if (!r.ok) {
    console.error(`[LFDebug:HostOpen] ${dryRun ? "would refuse" : "refused"}: ${r.reason}`);
    console.log(gateSummary(gate));
    console.log(`SUMMARY host-open ${dryRun ? "dry-run-closed" : "refused"}`);
    return 1;
  }
  for (const w of r.warnings ?? []) say(w);
  say("opening announcement (a draft; post it on Discord yourself):");
  console.log(r.announcement);
  console.log(`SUMMARY host-open ${dryRun ? "dry-run-ok" : "ok"}`);
  return 0;
}
