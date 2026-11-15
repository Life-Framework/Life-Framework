// Port checks for `tools\cli host ports` (#1460). Pure functions plus two bounded scanners.
//
// Reforger's game, A2S and RCON ports are all UDP. The host reads them from server.json,
// asks the OS who holds each one (netstat, then tasklist for the name), and gives every port a
// verdict: free, held-by-server (the Reforger server, by pid or process name) or held-by-other.
// The overall judgement is "before a start nothing is held; after it the server holds them all".
import { spawnSync } from "node:child_process";
import { queryInfo, parseAddress } from "./a2s.mjs";

export const EXTERNAL_TIMEOUT_MS = 4000;
export const SCAN_TIMEOUT_MS = 8000;
const SERVER_PROC = /ArmaReforger/i;

// [{ name, port, proto }] for the game port, the A2S port and RCON only when server.json sets it.
export function serverPorts(json) {
  const j = json || {};
  const out = [
    { name: "game", port: Number(j.bindPort) || 2001, proto: "udp" },
    { name: "a2s", port: Number(j.a2s && j.a2s.port) || 17777, proto: "udp" },
  ];
  const rcon = Number(j.rcon && j.rcon.port);
  if (rcon) out.push({ name: "rcon", port: rcon, proto: "udp" });
  return out;
}

// table: Map port -> { pid, proc }. opts.serverPids: pids known to be the server.
export function portVerdicts(ports, table, opts = {}) {
  const pids = new Set((opts.serverPids || []).map(Number));
  return ports.map((p) => {
    const held = table.get(p.port);
    if (!held) return { ...p, verdict: "free", holder: "" };
    const mine = pids.has(Number(held.pid)) || SERVER_PROC.test(held.proc || "");
    return { ...p, verdict: mine ? "held-by-server" : "held-by-other", holder: `${held.proc || "unknown"} (pid ${held.pid})` };
  });
}

// ok when every port is free (before a start) or every port is held by the server (after it).
export function judge(verdicts) {
  const kinds = new Set(verdicts.map((v) => v.verdict));
  if (kinds.has("held-by-other")) return { ok: false, reason: "a port is held by another program" };
  if (kinds.size === 1) return { ok: true, running: kinds.has("held-by-server"), reason: "" };
  return { ok: false, reason: "the server holds only some of its ports" };
}

// What a PC host forwards on the router: game (public -> bind port) and A2S. Never RCON.
export function forwardRules(json) {
  const j = json || {};
  const ports = serverPorts(j);
  const game = ports[0];
  const publicPort = Number(j.publicPort) || game.port;
  const rules = [{ proto: "UDP", port: publicPort, to: game.port, name: "game" }];
  const a2s = ports[1];
  rules.push({ proto: "UDP", port: a2s.port, to: a2s.port, name: "a2s" });
  return rules;
}

// `netstat -ano -p UDP` text -> Map port -> { pid }. Lines read "UDP  0.0.0.0:2001  *:*  4242".
export function parseNetstat(text, proto = "udp") {
  const want = proto.toUpperCase();
  const table = new Map();
  for (const line of String(text).split(/\r?\n/)) {
    const cols = line.trim().split(/\s+/);
    if (cols[0] !== want || cols.length < 4) continue;
    const m = /:(\d+)$/.exec(cols[1]);
    const pid = Number(cols[cols.length - 1]);
    if (m && pid >= 0 && !table.has(Number(m[1]))) table.set(Number(m[1]), { pid, proc: "" });
  }
  return table;
}

// `tasklist /FO CSV /NH` row -> image name, "" when no task matched.
export function parseTasklistName(text) {
  const m = /^"([^"]+)"/m.exec(String(text));
  return m ? m[1] : "";
}

function runBounded(cmd, args) {
  const r = spawnSync(cmd, args, { encoding: "utf8", timeout: SCAN_TIMEOUT_MS, windowsHide: true });
  if (r.error || r.status !== 0) return "";
  return r.stdout || "";
}

// The real table for this machine. Windows only (the PC host); elsewhere it is empty and the
// command says so instead of guessing.
export function readPortTable(ports) {
  if (process.platform !== "win32") return new Map();
  const table = parseNetstat(runBounded("netstat", ["-ano", "-p", "UDP"]), "udp");
  const wanted = new Map();
  for (const p of ports) {
    const e = table.get(p.port);
    if (e) {
      e.proc = e.proc || parseTasklistName(runBounded("tasklist", ["/FI", `PID eq ${e.pid}`, "/FO", "CSV", "/NH"]));
      wanted.set(p.port, e);
    }
  }
  return wanted;
}

// A2S from outside: reachable when the server answers within timeoutMs.
export async function probeExternal(address, { query = queryInfo, timeoutMs = EXTERNAL_TIMEOUT_MS } = {}) {
  let addr;
  try {
    addr = parseAddress(address);
  } catch (e) {
    return { ok: false, error: e.message };
  }
  try {
    const info = await query(addr.host, addr.port, { timeoutMs });
    return { ok: true, info };
  } catch (e) {
    return { ok: false, error: e.message };
  }
}
