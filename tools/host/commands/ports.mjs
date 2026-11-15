// `tools\cli host ports [--config <server.json>] [--external <host:a2sport>]` (#1460). The logic
// is tools/host/ports.mjs. Reads the ports from server.json (default server/flagship/server.json,
// else server/example/server.json), says whether each is free or held (by the server or by another
// program), prints the router forward rules a PC host needs, and with --external asks A2S from
// outside (bounded, 4 s). Exit 0 when the ports are consistent, 1 otherwise.
import { existsSync, readFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { pidAlive, readHostState } from "../host.mjs";
import { forwardRules, judge, portVerdicts, probeExternal, readPortTable, serverPorts } from "../ports.mjs";

export const help = "ports [--config server.json] [--external host:a2sport]   check game/A2S/RCON ports are free or held by the server, print router forward rules";

const TAG = "[LFDebug:HostPorts]";

function defaultServerJson(root) {
  for (const rel of [["server", "flagship", "server.json"], ["server", "example", "server.json"]]) {
    const p = join(root, ...rel);
    if (existsSync(p)) return p;
  }
  return join(root, "server", "flagship", "server.json");
}

function realDeps(ctx, configPath) {
  return {
    readServerJson: () => JSON.parse(readFileSync(configPath, "utf8")),
    readPortTable,
    serverPids: () => {
      const s = readHostState(ctx.hostDir);
      return s && s.pid && pidAlive(s.pid) ? [s.pid] : [];
    },
    probeExternal,
  };
}

function flag(args, name) {
  const i = args.indexOf(name);
  return i >= 0 ? args[i + 1] : undefined;
}

export async function main(args, ctx) {
  const config = flag(args, "--config");
  const external = flag(args, "--external");
  if (args.includes("--external") && !external) {
    console.error(`${TAG} --external needs host:a2sport`);
    return 1;
  }
  const configPath = config ? resolve(config) : defaultServerJson(ctx.root);
  const deps = { ...realDeps(ctx, configPath), ...(ctx.deps || {}) };

  let json;
  try {
    json = deps.readServerJson();
  } catch (e) {
    console.error(`${TAG} cannot read ${configPath}: ${e.message}`);
    return 1;
  }

  const ports = serverPorts(json);
  const verdicts = portVerdicts(ports, deps.readPortTable(ports), { serverPids: deps.serverPids() });
  const overall = judge(verdicts);
  let code = overall.ok ? 0 : 1;

  console.log(`${TAG} ports from ${configPath}`);
  for (const v of verdicts) {
    console.log(`${TAG} ${v.name.padEnd(5)} ${v.proto.toUpperCase()} ${String(v.port).padEnd(6)} ${v.verdict}${v.holder ? ` by ${v.holder}` : ""}`);
  }
  if (overall.ok) console.log(`${TAG} ${overall.running ? "the server holds every port" : "every port is free: ready to start"}`);
  else console.error(`${TAG} problem: ${overall.reason}`);

  console.log(`${TAG} router forward rules (protocol, external port -> this PC's port):`);
  for (const r of forwardRules(json)) {
    console.log(`  ${r.proto}  ${r.port} -> ${r.to}   (${r.name})`);
  }
  console.log(`${TAG} RCON is never forwarded.`);

  if (external) {
    const probe = await deps.probeExternal(external);
    if (probe.ok) {
      console.log(`${TAG} external A2S ${external}: reachable, "${probe.info.name}" ${probe.info.players}/${probe.info.maxPlayers}`);
    } else {
      console.error(`${TAG} external A2S ${external}: not reachable (${probe.error})`);
      code = 1;
    }
  }
  return code;
}
