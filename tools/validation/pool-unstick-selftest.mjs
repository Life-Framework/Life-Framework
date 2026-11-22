// pool-unstick-selftest: the pool cannot wedge on a ticket that can never start, and our helpers do not pile up
// (2026-11-22 on lf-testbox: every lane free, two tests waiting on 23-42 GB asks, sessions waiting 35-65 min,
// Git Bash out of processes at about 520).
// inputs: tools/lib/pool-ram.mjs, tools/lib/pool-commit.mjs, tools/lib/pool-queue.mjs, tools/lib/lead.mjs, tools/lib/validate-concurrency.mjs, tools/lib/tool-orphans.mjs, tools/hooks/main-write-guard.mjs, tools/validation/pool-unstick-selftest.mjs
//
// red-proof 2026-11-22: against the code before this change (git stash of tools/lib, tools/remote, tools/hooks)
// the growth, capacity, session-priority, STUCK, lead, shared-fan-out and hook checks fail: the replayed
// lf-testbox scan reads 38 GB of growth, a 50 GB ask on a 47 GB machine holds it, a session that waited 11 min
// lets tests pass it, and the hook with an open stdin never exits.

import { spawn } from "node:child_process";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import * as pr from "../lib/pool-ram.mjs";
import * as pc from "../lib/pool-commit.mjs";
import * as q from "../lib/pool-queue.mjs";

const failures = [];
// Each new export is looked up softly, so the red run against the old code lists every failing check.
const fn = (mod, name) => (typeof mod?.[name] === "function" ? mod[name] : () => undefined);
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};
const GB = 1024;
const here = dirname(fileURLToPath(import.meta.url));

// --- the scan carries each process's age; an older scan line (no age) still parses
const scan = pr.parseScan(["lf-pool: proc server 74532 30 0 952 - 900", "lf-pool: proc client 15304 1 0 652 full 30", "lf-pool: proc workbench 53800 772 0 1248 look 1200", "lf-pool: proc test 9 500 0 800 -"].join("\n"));
check(scan.length === 4 && scan[0].ageS === 900 && scan[1].ageS === 30 && scan[3].ageS === undefined, `the scan reads the age column (${JSON.stringify(scan.map((p) => p.ageS))})`);
check(/\$_\.age\)/.test(pr.PS_PROC_SCAN) && /CreationDate/.test(pr.PS_PROC_ROWS), "the PowerShell scan prints each process's age");

// --- growth: only a game process still loading grows
const peaks = {};
const old = { kind: "server", world: pr.SMALL, wsMb: 30, privMb: 952, ageS: 900 };
check(pr.unrealisedMb([old], peaks) === 0, `a loaded session server trimmed to 30 MB does not count as growing (got ${pr.unrealisedMb([old], peaks)})`);
check(pr.unrealisedMb([{ ...old, privMb: 300, ageS: 60 }], peaks) > 0, "a server in its first minutes still counts what it grows into");
check(pr.unrealisedMb([{ ...old, privMb: 300, ageS: undefined }], peaks) > 0, "a scan without ages counts as before");
check(pr.unrealisedMb([{ kind: "workbench", world: pr.SMALL, wsMb: 800, privMb: 1200, ageS: 3600 }], peaks) > 0, "a long-lived Workbench may still load a world");
check(pc.unrealisedCommitMb([{ kind: "client", privMb: 650, ageS: 900 }], peaks) === 0 && pc.unrealisedCommitMb([{ kind: "client", privMb: 650, ageS: 30 }], peaks) > 0, "commit growth follows the same window");

// The lf-testbox scan of 2026-11-22 replayed: four sessions (server + two full clients each), trimmed and long
// loaded, two look Workbenches. Before: 38 GB of growth with 19.4 GB free, so a 2.3 GB test asked for 41.6 GB.
const live = [];
for (let s = 0; s < 4; s++) {
  live.push({ kind: "server", world: pr.SMALL, wsMb: 30, privMb: 950, ageS: 1800 });
  live.push({ kind: "client", world: pr.SMALL, profile: "full", wsMb: 2, privMb: 650, ageS: 1700 }, { kind: "client", world: pr.SMALL, profile: "full", wsMb: 2, privMb: 650, ageS: 1700 });
}
live.push({ kind: "workbench", world: pr.SMALL, profile: "look", wsMb: 800, privMb: 1250, ageS: 120 }, { kind: "workbench", world: pr.SMALL, profile: "look", wsMb: 800, privMb: 1250, ageS: 60 });
const growth = pr.unrealisedMb(live, peaks);
const v = pr.admit({ freeMb: 19.4 * GB, minFreeMb: 1500, reserveMb: 2300, growthMb: growth, totalMb: 47.2 * GB, what: "the test" });
check(growth < 8 * GB && v.ok, `the replayed lf-testbox scan admits a test (growth ${(growth / GB).toFixed(1)} GB, ${v.reason ?? "ok"})`);

// --- (a) an ask is capped at what the machine can ever free; one over that runs alone when the machine is idle
const huge = { freeMb: 20 * GB, minFreeMb: 1500, reserveMb: 50 * GB, totalMb: 47.2 * GB };
const busy = pr.admit(huge);
check(!busy.ok && busy.neverFits && busy.needMb <= 47.2 * GB - 1500 && /runs alone/.test(busy.reason), `a 50 GB job on a 47 GB machine is neverFits, capped and says it runs alone (${JSON.stringify(busy)})`);
const alone = pr.admit({ ...huge, idle: true });
check(alone.ok && alone.alone, "a neverFits job starts alone on an idle machine");
const capped = pr.admit({ freeMb: 10 * GB, minFreeMb: 1500, reserveMb: 2300, growthMb: 60 * GB, totalMb: 47.2 * GB });
check(!capped.ok && capped.needMb === 47.2 * GB - 1500, `the ask never exceeds the machine's RAM less the floor (${capped.needMb})`);

// --- (b) a waiting ticket holds its machine only when its ask fits the machine
const T = Date.parse("2026-11-22T17:00:00Z");
const m = { name: "lf-testbox", lanes: { test: 8, session: 4 } };
const tk = (o) => ({ file: `${o.at}-${o.lane}.json`, priority: "normal", machines: ["lf-testbox"], stuck: "ram", ...o, at: new Date(T - o.ageMin * 60000).toISOString() });
const test = tk({ lane: "test", purpose: "test only", ageMin: 0 });
const ctx = { starveMs: 20 * 60000, sessionStarveMs: 10 * 60000, freeMb: 18 * GB, heldMb: 10 * GB, capacityMb: 47.2 * GB - 1500 };
const bigTest = tk({ lane: "test", purpose: "test only", ageMin: 30, needMb: 41.6 * GB });
check(!q.blocks({ ...bigTest, needMb: 46 * GB }, test, m, T, 20 * 60000, ctx), "a starved ticket asking more than the machine can free does not hold it");
check(!q.blocks({ ...bigTest, neverFits: true, needMb: 4 * GB }, test, m, T, 20 * 60000, ctx), "a neverFits ticket never holds the machine");
check(q.blocks({ ...bigTest, needMb: 20 * GB }, test, m, T, 20 * 60000, ctx), "a starved ticket that draining can fit still holds the machine");

// --- (c) a session waiting over 10 min on RAM goes first: tests behind it stop starting there
const s11 = tk({ lane: "session", purpose: "session", ageMin: 11, needMb: 6 * GB, clients: 1, profile: "full" });
check(q.blocks(s11, test, m, T, 20 * 60000, ctx), "a session that waited 11 min on RAM holds the machine against a new test");
check(!q.blocks({ ...s11, at: new Date(T - 5 * 60000).toISOString() }, test, m, T, 20 * 60000, ctx), "a session that waited 5 min is still passed by tests that fit");
check(fn(q, "starved")({ lane: "test", at: new Date(T - 11 * 60000).toISOString() }, T, ctx) === false, "a test is not starved at 11 min");

// --- (d) the STUCK reason
const caps = { capacityMbOf: () => 47.2 * GB - 1500, freeMbOf: () => 18 * GB, heldMbOf: () => 2 * GB };
check(/more than lf-testbox/.test(fn(q, "stuckReason")({ ...bigTest, needMb: 46 * GB }, caps) ?? ""), "pool status names a ticket over the machine's capacity as STUCK");
check(/would not free enough/.test(fn(q, "stuckReason")({ ...bigTest, needMb: 23.2 * GB }, caps) ?? ""), "pool status names a ticket draining cannot help as STUCK");
check(fn(q, "stuckReason")({ ...bigTest, needMb: 4 * GB }, caps) === null, "a ticket that can start is not STUCK");

// --- (e) the lead lists tickets waiting over 10 min
const lead = await import("../lib/lead.mjs");
const lines = (fn(lead, "longWaitLines")([s11, tk({ lane: "test", purpose: "test only", ageMin: 2 }), { ...bigTest, neverFits: true }], { now: T, warnMin: 10 }) ?? []);
check(lines.length === 2 && /WAIT 30m test/.test(lines[0]) && /STUCK/.test(lines[0]) && /WAIT 11m session/.test(lines[1]), `lead lists long waits oldest first (${lines.join(" | ")})`);
check(lead.poolLines({ snaps: [], queue: 1, tickets: [s11], now: T }).some((l) => /waiting over 10 min/.test(l)), "lead's POOL block carries the long-wait lines");

// --- validate fan-out is shared between concurrent validates
const vc = await import("../lib/validate-concurrency.mjs");
check(fn(vc, "shareConcurrency")(12, 1) === 12 && fn(vc, "shareConcurrency")(12, 8) === vc.MIN && fn(vc, "shareConcurrency")(12, 3) === 4, "eight concurrent validates no longer start 96 checks");

// --- orphaned helpers
const to = await import("../lib/tool-orphans.mjs").catch(() => ({}));
const procs = [
  { pid: 1, ppid: 0, name: "claude.exe", ageH: 9, cmd: "claude" },
  { pid: 2, ppid: 99, name: "node.exe", ageH: 68, cmd: "node C:/LF/agents/Life-Framework/tools/hooks/main-write-guard.mjs" },
  { pid: 3, ppid: 1, name: "node.exe", ageH: 68, cmd: "node C:/LF/agents/Life-Framework/tools/cli.mjs test" },
  { pid: 4, ppid: 98, name: "node.exe", ageH: 8, cmd: "node C:\\LF\\agents\\Life-Framework\\tools\\main-guard.mjs worker C:\\LF" },
  { pid: 5, ppid: 97, name: "node.exe", ageH: 1, cmd: "node D:/LF/worktrees/Life-Framework-ws-x/tools/cli.mjs test" },
  { pid: 6, ppid: 96, name: "node.exe", ageH: 30, cmd: "node C:/somewhere/else/server.js" },
  { pid: 7, ppid: 95, name: "node.exe", ageH: 30, cmd: "node D:/LF/worktrees/Life-Framework-ws-y/tools/cli.mjs test" },
];
const plan = fn(to, "planToolOrphans")(procs, { minAgeH: 6, keep: new Set([7]) }) ?? [];
check(plan.map((o) => o.pid).join() === "2", `only our parentless, old, non-daemon helper is planned (${plan.map((o) => o.pid).join()})`);

// --- the write-guard hook exits when its caller never closes stdin
{
  const child = spawn(process.execPath, [join(here, "..", "hooks", "main-write-guard.mjs")], { stdio: ["pipe", "ignore", "ignore"], windowsHide: true });
  const code = await new Promise((res) => {
    const t = setTimeout(() => {
      child.kill();
      res("hung");
    }, 20000);
    child.on("exit", (c) => {
      clearTimeout(t);
      res(c);
    });
  });
  check(code === 0, `the write-guard hook exits on its own with an open stdin (got ${code})`);
}

if (failures.length) {
  console.log("pool-unstick-selftest: FAIL");
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}
console.log("pool-unstick-selftest: OK (growth only while loading, asks capped at capacity, neverFits runs alone, no hold over capacity, sessions first after 10 min, STUCK reasons, lead long waits, shared validate fan-out, orphan plan, hook exits)");
