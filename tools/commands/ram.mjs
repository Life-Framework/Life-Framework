// tools\cli ram [--machine <name>] [--reap] [--dry-run] [--json]: where each pool machine's RAM goes, by role
// (claude agents, their MCP children, tools/cli node, game servers and clients, Workbench, main guard, services,
// user apps), and the ArmaReforger processes no one owns (tools/lib/ram.mjs, docs/architecture/ram-budget.md).
// --reap stops those orphans; the pool also does it by itself every 10 minutes per machine.
import { loadMachines } from "../remote/pool.mjs";
import { sshPs } from "../remote/testbox.mjs";
import { groupRoles, parseFacts, parseLine, planOrphans, PS_FACTS, PS_SNAPSHOT, reapOrphans, ROLES, runOn } from "../lib/ram.mjs";

export const help = "ram [--machine <name>] [--reap|--dry-run] [--json]   RAM by role per pool machine, and orphaned game/Workbench processes";

const gb = (mb) => (mb / 1024).toFixed(2);

export async function main(args, { root }) {
  const want = args.includes("--machine") ? args[args.indexOf("--machine") + 1] : null;
  const machines = loadMachines(root).filter((m) => !m.disabled && (!want || m.name === want));
  if (!machines.length) {
    console.log(`ram: no pool machine${want ? ` named ${want}` : ""}`);
    return 1;
  }
  const json = [];
  let code = 0;
  for (const m of machines) {
    const snap = parseLine(runOn(m, PS_SNAPSHOT, { sshPs, timeout: 60000 }), "snapshot");
    if (!snap) {
      console.log(`ram: ${m.name} did not answer`);
      code = 1;
      continue;
    }
    const { roles, mcpPerClaude } = groupRoles(snap.procs);
    const facts = parseFacts(runOn(m, PS_FACTS, { sshPs })) ?? [];
    const orphans = planOrphans(facts);
    json.push({ machine: m.name, ...snap, procs: undefined, roles, mcpPerClaude, orphans });
    if (args.includes("--json")) continue;
    console.log(`\n${m.name}: ${gb(snap.totalMb)} GB, free ${gb(snap.freeMb)} GB, committed ${gb(snap.commitMb)} of ${gb(snap.commitLimitMb)} GB`);
    console.log("  role                              n    WS GB  private GB");
    for (const r of ROLES.filter((k) => roles[k]).sort((a, b) => roles[b].wsMb - roles[a].wsMb)) {
      console.log(`  ${r.padEnd(32)}${String(roles[r].n).padStart(3)}  ${gb(roles[r].wsMb).padStart(7)}  ${gb(roles[r].privMb).padStart(10)}`);
    }
    const counts = Object.values(mcpPerClaude);
    if (counts.length) console.log(`  MCP children per claude process: ${counts.join(", ")}`);
    for (const f of facts) console.log(`  game ${f.name} pid ${f.pid}: ${gb(f.wsMb)} GB, ${f.ageMin} min, ${f.look ? "look viewer" : f.profile ? `-profile ${f.profile}` : "not ours (no -profile)"}`);
    if (!orphans.length) console.log("  orphans: none");
    if (args.includes("--reap") || args.includes("--dry-run")) reapOrphans(m, { dryRun: !args.includes("--reap"), sshPs });
    else for (const o of orphans) console.log(`  orphan ${o.name} pid ${o.pid} (${o.why}; ${gb(o.wsMb)} GB); --reap stops it`);
  }
  if (args.includes("--json")) console.log(JSON.stringify(json));
  return code;
}
