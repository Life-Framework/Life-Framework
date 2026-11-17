// ram-selftest: RAM by role and orphan reaping (tools/lib/ram.mjs, tools\cli ram, docs/architecture/ram-budget.md).
// inputs: tools/lib/ram.mjs, tools/commands/ram.mjs, tools/validation/ram-selftest.mjs
//
// red-proof 2026-11-17: with planOrphans returning [] (no reaper, as before) the "gone checkout", "stopped
// session", "session folder without a record", "hung test server" and "hung rdb Workbench" checks fail; with the
// profile guard removed the "the user's own game is never a candidate" check fails.

import { spawnSync } from "node:child_process";
import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import * as ram from "../lib/ram.mjs";

const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};

// --- roles
{
  const procs = [
    { pid: 1, ppid: 0, name: "T3 Code (Beta).exe", ws: 300, priv: 400 },
    { pid: 2, ppid: 1, name: "claude.exe", ws: 200, priv: 600 },
    { pid: 3, ppid: 2, name: "node.exe", ws: 40, priv: 30, cmd: "node some-mcp-server.js" },
    { pid: 4, ppid: 99, name: "node.exe", ws: 50, priv: 25, cmd: "C:\\nvm4w\\nodejs\\node.exe C:\\Users\\u\\Life-Framework-ws-a\\tools\\cli.mjs look grid:x" },
    { pid: 5, ppid: 0, name: "ArmaReforgerServerDiag.exe", ws: 750, priv: 950, cmd: "x -profile C:\\w\\server\\profile\\test -maxFPS 30" },
    { pid: 6, ppid: 0, name: "ArmaReforgerServer.exe", ws: 2200, priv: 2500, cmd: "x -profile C:\\s\\server" },
    { pid: 7, ppid: 0, name: "ArmaReforgerSteam.exe", ws: 1700, priv: 4400, cmd: "x -client 127.0.0.1" },
    { pid: 8, ppid: 0, name: "node.exe", ws: 30, priv: 20, cmd: "node C:\\LF\\a\\tools\\main-guard.mjs worker" },
    { pid: 9, ppid: 0, name: "MsMpEng.exe", ws: 1100, priv: 1800 },
    { pid: 10, ppid: 0, name: "svchost.exe", ws: 20, priv: 10 },
    { pid: 11, ppid: 0, name: "Superhuman.exe", ws: 100, priv: 900 },
    { pid: 12, ppid: 0, name: "steamwebhelper.exe", ws: 300, priv: 300 },
  ];
  const g = ram.groupRoles(procs);
  const want = { "T3": 300, "claude agents": 200, "MCP servers (claude children)": 40, "tools/cli node": 50, "game: test servers": 750, "game: session servers": 2200, "game: clients": 1700, "main guard": 30, "svc: Defender": 1100, Windows: 20, "other user apps": 100, "svc: Steam": 300 };
  for (const [role, mb] of Object.entries(want)) check(g.roles[role]?.wsMb === mb, `role ${role} holds ${mb} MB (got ${JSON.stringify(g.roles[role])})`);
  check(g.mcpPerClaude[2] === 1, "MCP children are counted per claude process");
  check(ram.ROLES.every((r) => typeof r === "string") && Object.keys(g.roles).every((r) => ram.ROLES.includes(r)), "every role is listed in ROLES");
}

// --- orphans
{
  const base = { name: "ArmaReforgerServerDiag.exe", ageMin: 30, wsMb: 700, tool: false, look: false };
  const plan = (f, o) => ram.planOrphans([{ pid: 1, ...base, ...f }], o);
  check(plan({ profile: "C:\\LF\\gone\\server\\profile\\test", root: "C:\\LF\\gone", rootOk: false }).length === 1, "a server whose checkout is gone is an orphan");
  check(plan({ profile: "C:\\LF\\w\\server\\profile\\test", root: "C:\\LF\\w", rootOk: true }).length === 0, "a young-enough test server of a live checkout is kept");
  check(plan({ profile: "C:\\LF\\w\\server\\profile\\test", root: "C:\\LF\\w", rootOk: true, ageMin: 150 }).length === 1, "a hung test server (past 120 min) is an orphan");
  const S = "C:\\Users\\u\\AppData\\Local\\LifeFramework\\session\\Life-Framework-ws-a\\server";
  check(plan({ profile: S, rec: { stoppedAt: "2026-11-17T08:00:00Z", serverPid: 1, clientPids: [] } }).length === 1, "a process of a stopped session is an orphan");
  check(plan({ profile: S, rec: "none" }).length === 1, "a session folder without a record is an orphan");
  check(plan({ profile: S, rec: { serverPid: 1, clientPids: [2] } }).length === 0, "a process its session record names is kept");
  check(plan({ profile: S, rec: { serverPid: 5, clientPids: [6] } }).length === 1, "a process its session record does not name is an orphan");
  check(plan({ profile: S, rec: "unreadable" }).length === 0, "an unreadable record is left alone");
  check(plan({ profile: "C:\\LF\\gone\\server\\profile\\test", root: "C:\\LF\\gone", rootOk: false, ageMin: 3 }).length === 0, "nothing younger than the grace minutes is touched");
  check(plan({ profile: null }).length === 0 && plan({ profile: "C:\\Users\\u\\Documents\\My Games\\ArmaReforger", ageMin: 900 }).length === 0, "the user's own game is never a candidate");
  const wb = { name: "ArmaReforgerWorkbench" + "SteamDiag.exe", profile: "C:\\LF\\r\\server\\profile\\rdb", root: "C:\\LF\\r", rootOk: true };
  check(plan({ ...wb, tool: true, ageMin: 90 }).length === 1 && plan({ ...wb, tool: true, ageMin: 20 }).length === 0, "an rdb/compile Workbench past 60 min is hung; younger is kept");
  check(plan({ ...wb, tool: false, ageMin: 900 }).length === 0, "an editor Workbench (no -exitAfterInit) is never reaped by age");
  check(plan({ ...wb, look: true, root: "C:\\LF\\gone", rootOk: false, ageMin: 90 }).length === 0, "the look viewer is left to look-serve");
  check(/taskkill \/T \/F \/PID 42/.test(ram.killScript([{ pid: 42 }])) && /ArmaReforger\*/.test(ram.killScript([{ pid: 42 }])), "the kill re-checks the pid is ArmaReforger before taskkill");
}

// --- the PowerShell facts on this machine (shape only; no game needs to run)
{
  const out = ram.runOn({ name: "local", via: "local" }, ram.PS_FACTS);
  if (out === null) console.log("ram-selftest: note: PowerShell unavailable; the facts shape check is skipped");
  else check(Array.isArray(ram.parseFacts(out)), `PS_FACTS prints one facts line (${String(out).slice(0, 120)})`);
  check(ram.parseFacts("noise") === null && ram.parseFacts('lf-ram: facts {"pid":1}').length === 1, "parseFacts reads a single object and rejects noise");
}

// --- throttle: once per machine per window, off under the selftest seams
{
  const hub = mkdtempSync(join(tmpdir(), "lf-ram-"));
  try {
    const calls = [];
    const fake = { name: "fake", via: "ssh", host: "fake" };
    const sshPs = (host) => (calls.push(host), { status: 1, stdout: "" });
    ram.reapOrphansThrottled(hub, [fake], { sshPs, env: {}, now: 1e12 });
    ram.reapOrphansThrottled(hub, [fake], { sshPs, env: {}, now: 1e12 + 1000 });
    ram.reapOrphansThrottled(hub, [fake], { sshPs, env: {}, now: 1e12 + 700000 });
    check(calls.length === 2, `reaping runs once per machine per 10 min (ran ${calls.length})`);
    ram.reapOrphansThrottled(hub, [fake], { sshPs, env: { LF_ORPHAN_REAP: "0" }, now: 9e12 });
    ram.reapOrphansThrottled(hub, [fake], { sshPs, env: { LF_POOL_TEST_FREE_MB: "12000" }, now: 9e12 });
    check(calls.length === 2, "LF_ORPHAN_REAP=0 and the pool selftest seam turn it off");
  } finally {
    rmSync(hub, { recursive: true, force: true });
  }
}

// --- the command loads
{
  const r = spawnSync(process.execPath, ["-e", "import('./tools/commands/ram.mjs').then(m => { if (!m.help || typeof m.main !== 'function') process.exit(3) })"], { encoding: "utf8", windowsHide: true });
  check(r.status === 0, `tools/commands/ram.mjs exports help and main (${r.stderr.slice(0, 200)})`);
}

if (failures.length) {
  console.log("ram-selftest: FAIL");
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}
console.log("ram-selftest: OK (roles, MCP children per claude, orphan rules: gone checkout, stopped or foreign session, hung test and rdb, the user's own and the look viewer kept, throttle)");
