// tools/validation/host-ports-selftest.mjs
// inputs: tools/host/ports.mjs, tools/host/commands/ports.mjs, tools/host/a2s.mjs
//
// Proves `tools\cli host ports` (#1460): ports are read from a server.json, an injected port
// table gives the free / held-by-server / held-by-other verdicts, the router forward rules
// match the fixture, and the external probe is bounded. No socket, netstat or game is touched.
// red-proof: before tools/host/ports.mjs existed the import failed: "FAIL import tools/host/ports.mjs"
// (2026-11-15).

import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
let failed = 0;

function fail(msg) {
  console.log(`FAIL  ${msg}`);
  failed++;
}

function eq(label, got, want) {
  const a = JSON.stringify(got);
  const b = JSON.stringify(want);
  if (a !== b) fail(`${label}: got ${a}, want ${b}`);
}

let ports;
let command;
try {
  ports = await import(pathToFileURL(join(ROOT, "tools", "host", "ports.mjs")).href);
  command = await import(pathToFileURL(join(ROOT, "tools", "host", "commands", "ports.mjs")).href);
} catch (e) {
  fail(`import tools/host/ports.mjs: ${e.message}`);
  console.log(`host-ports-selftest: ${failed} case(s) failed`);
  process.exit(1);
}

const fixture = {
  bindPort: 2001,
  publicPort: 2002,
  a2s: { address: "0.0.0.0", port: 17777 },
  rcon: { address: "127.0.0.1", port: 19999, password: "x" },
  game: { name: "Life Framework | Everon 1989 (Alpha)" },
};
const noRcon = { bindPort: 2001, publicPort: 2001, a2s: { port: 17777 } };
const bare = {};

// 1. Ports come out of server.json: game, A2S, RCON only when set; defaults when absent.
eq("ports(fixture)", ports.serverPorts(fixture).map((p) => [p.name, p.port, p.proto]), [
  ["game", 2001, "udp"],
  ["a2s", 17777, "udp"],
  ["rcon", 19999, "udp"],
]);
eq("ports(noRcon)", ports.serverPorts(noRcon).map((p) => p.name), ["game", "a2s"]);
eq("ports(bare) defaults", ports.serverPorts(bare).map((p) => [p.name, p.port]), [["game", 2001], ["a2s", 17777]]);

// 2. Verdicts from an injected table: port -> { pid, proc }.
const table = new Map([
  [2001, { pid: 4242, proc: "ArmaReforgerServer.exe" }],
  [17777, { pid: 900, proc: "chrome.exe" }],
]);
const v = ports.portVerdicts(ports.serverPorts(fixture), table, { serverPids: [4242] });
eq("verdicts", v.map((r) => [r.name, r.verdict]), [["game", "held-by-server"], ["a2s", "held-by-other"], ["rcon", "free"]]);
eq("holder named", v[1].holder, "chrome.exe (pid 900)");
// A server is also recognised by its process name when no pid is known.
const byName = ports.portVerdicts(ports.serverPorts(noRcon), new Map([[2001, { pid: 7, proc: "ArmaReforgerServer.exe" }]]), {});
eq("verdict by process name", byName.map((r) => r.verdict), ["held-by-server", "free"]);
eq("empty table is all free", ports.portVerdicts(ports.serverPorts(fixture), new Map(), {}).map((r) => r.verdict), ["free", "free", "free"]);

// 3. The overall judgement: before a start everything must be free; after, held by the server.
eq("judge ok before start", ports.judge(ports.portVerdicts(ports.serverPorts(noRcon), new Map(), {})).ok, true);
eq("judge ok when server holds all", ports.judge(ports.portVerdicts(ports.serverPorts(noRcon), new Map([
  [2001, { pid: 1, proc: "ArmaReforgerServer.exe" }],
  [17777, { pid: 1, proc: "ArmaReforgerServer.exe" }],
]), {})).ok, true);
eq("judge fails on a foreign holder", ports.judge(v).ok, false);
eq("judge fails when the server holds only some", ports.judge(ports.portVerdicts(ports.serverPorts(noRcon), new Map([
  [2001, { pid: 1, proc: "ArmaReforgerServer.exe" }],
]), {})).ok, false);

// 4. Router forward rules: game and A2S (UDP), never RCON; internal port is bindPort.
eq("forward rules", ports.forwardRules(fixture), [
  { proto: "UDP", port: 2002, to: 2001, name: "game" },
  { proto: "UDP", port: 17777, to: 17777, name: "a2s" },
]);
eq("forward rules, same public port", ports.forwardRules(noRcon).map((r) => [r.proto, r.port, r.to]), [["UDP", 2001, 2001], ["UDP", 17777, 17777]]);

// 5. netstat output parses into the table; tasklist names the pid.
const netstat = [
  "Active Connections",
  "  Proto  Local Address          Foreign Address        State           PID",
  "  UDP    0.0.0.0:2001           *:*                                    4242",
  "  UDP    [::]:17777             *:*                                    900",
  "  TCP    0.0.0.0:2001           0.0.0.0:0              LISTENING       55",
  "",
].join("\r\n");
const parsed = ports.parseNetstat(netstat, "udp");
eq("netstat udp rows", [...parsed.entries()].map(([p, e]) => [p, e.pid]), [[2001, 4242], [17777, 900]]);
eq("tasklist csv", ports.parseTasklistName('"ArmaReforgerServer.exe","4242","Console","1","1,234,567 K"\r\n'), "ArmaReforgerServer.exe");
eq("tasklist none", ports.parseTasklistName("INFO: No tasks are running which match the specified criteria.\r\n"), "");

// 6. The external probe is bounded and reports reachable / unreachable.
let seen;
const okProbe = await ports.probeExternal("203.0.113.9:17777", {
  query: async (host, port, opts) => {
    seen = { host, port, timeoutMs: opts.timeoutMs };
    return { name: "Life Framework | Everon 1989 (Alpha)", players: 3, maxPlayers: 16 };
  },
  timeoutMs: 1500,
});
eq("probe args", seen, { host: "203.0.113.9", port: 17777, timeoutMs: 1500 });
eq("probe reachable", [okProbe.ok, okProbe.info.players], [true, 3]);
const badProbe = await ports.probeExternal("203.0.113.9:17777", {
  query: async () => {
    throw new Error("A2S query to 203.0.113.9:17777 timed out after 1500ms");
  },
});
eq("probe unreachable", [badProbe.ok, /timed out/.test(badProbe.error)], [false, true]);
const defaultTimeout = ports.EXTERNAL_TIMEOUT_MS;
if (!(defaultTimeout > 0 && defaultTimeout <= 10000)) fail(`EXTERNAL_TIMEOUT_MS must be a bounded 1..10000 ms, got ${defaultTimeout}`);

// 7. The command: injected deps, no real scan. Exit codes follow the judgement.
async function run(args, deps) {
  const lines = [];
  const log = console.log;
  const err = console.error;
  console.log = (...a) => lines.push(a.join(" "));
  console.error = (...a) => lines.push(a.join(" "));
  let code;
  try {
    code = await command.main(args, { root: ROOT, hostDir: join(ROOT, "tmp", "no-host"), deps });
  } finally {
    console.log = log;
    console.error = err;
  }
  return { code, out: lines.join("\n") };
}
const freeDeps = { readServerJson: () => fixture, readPortTable: () => new Map(), serverPids: () => [] };
const r1 = await run([], freeDeps);
eq("command free exit", r1.code, 0);
if (!r1.out.includes("free") || !/UDP\s+2002\s+->\s+2001/.test(r1.out)) fail(`command output lacks verdicts or forward rules:\n${r1.out}`);
if (/UDP\s+19999\s+->/.test(r1.out) || /\(rcon\)/.test(r1.out)) fail("RCON must not be listed as a router forward");
const r2 = await run([], { ...freeDeps, readPortTable: () => table, serverPids: () => [4242] });
eq("command foreign holder exit", r2.code, 1);
if (!r2.out.includes("held-by-other") || !r2.out.includes("chrome.exe")) fail(`command output lacks the foreign holder:\n${r2.out}`);
const r3 = await run(["--external", "203.0.113.9:17777"], { ...freeDeps, probeExternal: async () => ({ ok: false, error: "A2S query to 203.0.113.9:17777 timed out after 1500ms" }) });
eq("command unreachable external exit", r3.code, 1);
if (!r3.out.includes("timed out")) fail(`command output lacks the probe error:\n${r3.out}`);
const r4 = await run(["--external", "bad:port:x"], freeDeps);
eq("command bad external exit", r4.code, 1);

if (failed) {
  console.log(`host-ports-selftest: ${failed} case(s) failed`);
  process.exit(1);
}
console.log("host-ports-selftest: OK (verdicts free/held-by-server/held-by-other, forward rules, bounded external probe)");
