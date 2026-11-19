// pool-commit-selftest: the pool admits by COMMIT headroom as well as free RAM (tools/lib/pool-commit.mjs,
// docs/worktrees.md "RAM admission"). Faked numbers: no game, no real pool, no ssh.
// inputs: tools/lib/pool-commit.mjs, tools/lib/pool-ram.mjs, tools/lib/pool-queue.mjs, tools/remote/pool.mjs, tools/validation/pool-commit-selftest.mjs
//
// red-proof 2026-11-19: with localAdmission and remoteAdmission returning the physical-RAM verdict alone (the gate
// as it was, free RAM 9 GB read as room), the "70.2 of 71.2 GB committed" checks, the remote and holder checks and
// the ranking check fail: every one of them has plenty of free physical RAM and no commit headroom.

import { spawnSync } from "node:child_process";
import { readFileSync, rmSync } from "node:fs";
import { gitSync } from "../lib/git-retry.mjs";
import * as pc from "../lib/pool-commit.mjs";
import * as pr from "../lib/pool-ram.mjs";
import * as q from "../lib/pool-queue.mjs";
import * as pool from "../remote/pool.mjs";
import { scratchDir } from "../lib/scratch.mjs";

const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};
const GB = 1024;
const settings = { minFreeMb: 1500, minFreeCommitMb: 6 * GB, sessionReserveServerMb: 600, sessionReserveClientMb: 850, sessionReserve: { lowspec: { serverMb: 600, clientMb: 800 } }, testReserveMb: 800 };
const none = {};

// --- the numbers from 2026-11-19: 70.2 of 71.2 GB committed, about 9 GB of physical RAM free
const stuck = { usedMb: 70.2 * GB, limitMb: 71.2 * GB };
check(!pc.commitAdmit({ ...stuck, floorMb: 6 * GB, reserveMb: pc.jobCommitMb(none, { kind: "session", clients: 1 }) }).ok, "a session on a machine 1 GB under its commit limit queues");
check(!pc.commitAdmit({ ...stuck, floorMb: 6 * GB, reserveMb: pc.jobCommitMb(none, { kind: "test" }) }).ok, "even a test queues with 1 GB of commit headroom (the floor stays free for agents, git and node)");
check(pc.commitAdmit({ usedMb: 40 * GB, limitMb: 71.2 * GB, floorMb: 6 * GB, reserveMb: 8100 }).ok, "a session on a machine with 31 GB of commit headroom is admitted");
// the floor is exact: headroom - reserve >= floor admits, one MB less queues
check(pc.commitAdmit({ usedMb: 100000 - (6144 + 1500), limitMb: 100000, floorMb: 6144, reserveMb: 1500 }).ok && !pc.commitAdmit({ usedMb: 100000 - (6144 + 1500) + 1, limitMb: 100000, floorMb: 6144, reserveMb: 1500 }).ok, "the commit floor is exact");
check(pc.commitAdmit({ usedMb: NaN, limitMb: 71.2 * GB }).ok && pc.commitAdmit({ usedMb: 1, limitMb: 0 }).ok, "an unread commit does not stop a job (the physical gate still applies)");
const refused = pc.commitAdmit({ ...stuck, floorMb: 6 * GB, reserveMb: 5500, growthMb: 2000, what: "the session" });
check(/commit 70\.2\/71\.2 GB used/.test(refused.reason) && /5\.4 GB commit peak for the session/.test(refused.reason) && /2\.0 GB the running jobs still grow into/.test(refused.reason) && /under 6\.0 GB/.test(refused.reason), `the reason names commit used/limit, the peak, the growth and the floor (${refused.reason})`);
check(pc.commitText(70.2 * GB, 71.2 * GB, 6 * GB) === "commit 70.2/71.2 GB used (floor 6.0 free)" && pc.commitText(NaN, 1) === "", "commitText prints used/limit and the floor, nothing when unread");

// --- peaks: seeds, p90 once there are samples, learned from private bytes of ended processes
check(pc.commitPeakMb(none, "client") === 5500 && pc.commitPeakMb(none, "workbench") === 6000 && pc.commitPeakMb(none, "server") === 2600 && pc.commitPeakMb(none, "test") === 1500, "seeds: client 5.5, Workbench 6, server 2.6, test 1.5 GB");
check(pc.jobCommitMb(none, { kind: "session", clients: 2 }) === 2600 + 2 * 5500 && pc.jobCommitMb(none, { kind: "session", clients: 0 }) === 2600, "a session is its server plus each client");
check(pc.commitPeakMb({ "commit:client": [4000, 4200, 4400, 4600, 4800, 5000, 5200, 5400, 5600, 7000] }, "client") === 5600, "p90 of ten commit samples replaces the seed");
check(pc.commitPeakMb({ "commit:client": [4000, 4100] }, "client") === 5500, "two samples keep the seed");
{
  const t1 = pr.trackPeaks({}, [{ pid: 9, kind: "client", world: pr.SMALL, wsMb: 900, privMb: 3000 }], {});
  const t2 = pr.trackPeaks(t1.live, [{ pid: 9, kind: "client", world: pr.SMALL, wsMb: 800, privMb: 5100 }], t1.peaks);
  const t3 = pr.trackPeaks(t2.live, [], t2.peaks);
  check(t3.peaks["commit:client"]?.[0] === 5100 && t3.recorded === 1, `an ended process leaves its highest private bytes as a commit sample (${JSON.stringify(t3.peaks["commit:client"])})`);
  check(pr.withSeenMax("/nonexistent", [{ pid: 1, kind: "client" }]).length === 1, "withSeenMax tolerates a missing live file");
}
check(pc.unrealisedCommitMb([{ kind: "workbench", privMb: 1000 }], none) === 5000 && pc.unrealisedCommitMb([{ kind: "workbench", privMb: 1000, maxPrivMb: 5800 }], none) === 200 && pc.unrealisedCommitMb([{ kind: "client", privMb: 6500 }], none) === 0 && pc.unrealisedCommitMb([{ kind: "client" }], none) === 0, "a loading process counts its commit growth; one past its peak, or with no private bytes, counts nothing");
const fresh = [{ purpose: "session", at: new Date().toISOString(), clients: 1 }];
check(pc.lockGapCommitMb(fresh, none) === 2600 + 5500 && pc.lockGapCommitMb([{ ...fresh[0], at: new Date(Date.now() - 120000).toISOString() }], none) === 0, "a fresh lane lock counts its commit peak, an older one is counted through its processes");

// --- parse and read
check(JSON.stringify(pc.parseCommit("x\nlf-pool: commit 71885 72909\n")) === JSON.stringify({ usedMb: 71885, limitMb: 72909 }) && pc.parseCommit("lf-pool: ram 1 2") === null, "parseCommit reads the one line");
check(pool.parseRemoteState("lf-pool: ram 9000 47200\nlf-pool: commit 71885 72909\n").commit?.usedMb === 71885, "remoteState carries the commit of the box");
check(pc.readLocalCommit({ env: { LF_POOL_TEST_COMMIT_MB: "70000/72000" } }).usedMb === 70000 && pc.readLocalCommit({ env: { LF_POOL_TEST_FREE_MB: "9000" } }) === null, "the seams: a commit number, or none under the free-RAM seam");
check(pc.readLocalCommit({ run: () => ({ status: 0, stdout: "lf-pool: commit 100 200\n" }), env: {} })?.limitMb === 200 && pc.readLocalCommit({ run: () => ({ status: 1, stdout: "" }), env: {} }) === null, "a stubbed read parses, a failed one is unknown");
{
  // The real PowerShell read, when PowerShell exists here: one parseable line.
  const r = spawnSync("powershell", ["-NoProfile", "-NonInteractive", "-Command", pc.PS_COMMIT_READ], { windowsHide: true, encoding: "utf8", timeout: 20000 });
  if (r.error || r.status !== 0) console.log(`pool-commit-selftest: note: PowerShell unavailable; the live read check is skipped`);
  else {
    const v = pc.parseCommit(r.stdout);
    check(!!v && v.limitMb > 0 && v.usedMb > 0 && v.usedMb <= v.limitMb * 1.05, `the live read gives used/limit (${r.stdout.trim()})`);
  }
}

// --- this PC's gate: free physical RAM is plenty, commit is not
const root = scratchDir("lf-pool-commit-root-");
gitSync(["init", "-q", root], { windowsHide: true });
try {
  const m = { name: "local", via: "local", slots: 1, lanes: { test: 2, session: 2 } };
  const adm = (meta, opts) => pool.localAdmission(root, m, "server-test-1", meta, settings, { peaks: {}, freeMb: 9 * GB, procs: [], ...opts });
  check(adm({ purpose: "test" }).ok, "baseline: 9 GB of free RAM admits a test when no commit is read");
  const a = adm({ purpose: "session", clients: 1, profile: "lowspec" }, { commit: stuck });
  check(!a.ok && a.commit === true && /commit 70\.2\/71\.2/.test(a.reason), `local: a session queues at 70.2/71.2 GB committed with 9 GB of RAM free (${a.reason})`);
  check(!adm({ purpose: "test" }, { commit: stuck }).ok, "local: a test queues there too");
  check(adm({ purpose: "session", clients: 1, profile: "lowspec" }, { commit: { usedMb: 30 * GB, limitMb: 71.2 * GB } }).ok, "local: with commit headroom the session is admitted");
  // a Workbench still loading: its commit growth counts against the next job
  const wb = [{ pid: 5, kind: "workbench", world: pr.SMALL, wsMb: 500, privMb: 800 }];
  check(!adm({ purpose: "test" }, { freeMb: 20 * GB, procs: wb, commit: { usedMb: 60 * GB, limitMb: 71.2 * GB } }).ok && adm({ purpose: "test" }, { freeMb: 20 * GB, procs: [], commit: { usedMb: 60 * GB, limitMb: 71.2 * GB } }).ok, "local: a Workbench still growing to 6 GB of commit is counted");
  // the physical verdict still comes first
  check(!adm({ purpose: "test" }, { freeMb: 1 * GB, commit: { usedMb: 1 * GB, limitMb: 71.2 * GB } }).ok && adm({ purpose: "test" }, { freeMb: 1 * GB, commit: { usedMb: 1 * GB, limitMb: 71.2 * GB } }).commit !== true, "local: a RAM refusal stays a RAM refusal");
  // tryLocal reports it in `blocked` so the ticket waits as stuck on RAM
  const blocked = {};
  const slot = pool.tryLocal(root, m, { lock: "server-test-1", index: 1, port: 2002 }, { purpose: "test only", root }, { blocked, freeMb: 9 * GB, procs: [], peaks: {}, commit: stuck });
  check(slot === null && blocked.commit === true && /commit/.test(blocked.ram ?? "") && blocked.needMb === undefined, "tryLocal: the commit refusal reaches `blocked` (no physical needMb)");
  // the pool's own ranking: a machine under its commit floor goes after one with room
  const rows = q.rankMachines([{ name: "full", freeMb: 12 * GB, commitFreeMb: 1 * GB }, { name: "roomy", freeMb: 8 * GB, commitFreeMb: 30 * GB }], { minFreeMb: 1500, reserveMb: 800, commitReserveMb: 1500, minFreeCommitMb: 6 * GB });
  check(rows[0].name === "roomy" && rows[1].room === false, "rankMachines: a machine at its commit limit has no room however much RAM is free");
  const ranked = pool.rankedMachines(root, [m, { name: "lf-testbox", via: "ssh", host: "x" }], settings, { localFreeMb: 12 * GB, localCommit: stuck, stateOf: () => ({ freeMb: 8 * GB, locks: {}, procs: null, commit: { usedMb: 10 * GB, limitMb: 71 * GB } }) });
  check(ranked[0].name === "lf-testbox", "rankedMachines: this PC at its commit limit ranks behind a machine with headroom");
} finally {
  rmSync(root, { recursive: true, force: true });
}

// --- a remote machine reports it the same way
{
  const laptop = { name: "lf-testbox", via: "ssh", lanes: { test: 3, session: 2 } };
  const st = { locks: {}, freeMb: 20 * GB, totalMb: 47 * GB, procs: [], commit: stuck };
  const v = pool.remoteAdmission(laptop, st, settings, { kind: "session", clients: 1, profile: "lowspec", peaks: {} });
  check(!v.ok && v.commit === true && /commit/.test(v.reason), `remote: a box at 70.2/71.2 GB committed queues a session (${v.reason})`);
  check(pool.remoteAdmission(laptop, { ...st, commit: undefined }, settings, { kind: "session", clients: 1, profile: "lowspec", peaks: {} }).ok && pool.remoteAdmission(laptop, { ...st, commit: { usedMb: 20 * GB, limitMb: 71 * GB } }, settings, { kind: "session", clients: 1, profile: "lowspec", peaks: {} }).ok, "remote: no commit read, or headroom, admits it");
  const sv = pool.remoteSessionVerdict(laptop, st, settings, { clients: 1, profile: "lowspec", peaks: {} });
  check(sv.ok === false && /commit/.test(sv.ram ?? ""), "remoteSessionVerdict hands the commit reason to the queue");
}

// --- the holder's PowerShell gate and config
check(/commit-short/.test(pool.PS_RAM_GATE) && /FreeVirtualMemory/.test(pool.PS_RAM_GATE) && /\$cm\.floorMb/.test(pool.PS_RAM_GATE) && /\$cgrowth/.test(pool.PS_RAM_GATE) && /\$cpending/.test(pool.PS_RAM_GATE), "the holder gate reads FreeVirtualMemory and refuses with commit-short (floor, growth, fresh locks)");
{
  const hroot = scratchDir("lf-pool-commit-hold-");
  gitSync(["init", "-q", hroot], { windowsHide: true });
  try {
    const hm = { name: "lf-testbox", via: "ssh", host: "x", root: "C:/LF/Life-Framework", slots: 1, lanes: { test: 3, session: 2 } };
    const c = pool.holderConfig(hroot, hm, { sha: "a", head: "a" }, { purpose: "session", settings }).ram.commit;
    check(c.floorMb === 6 * GB && c.reserveMb === 2600 + 5500 && c.peaks.workbench === 6000 && c.testMb === 1500, `the holder config carries the commit floor, the job's commit peak and the per-kind peaks (${JSON.stringify(c)})`);
    check(pool.holderConfig(hroot, hm, { sha: "a", head: "a" }, { purpose: "test", settings }).ram.commit.reserveMb === 1500, "a test's holder commit reserve is the test peak");
  } finally {
    rmSync(hroot, { recursive: true, force: true });
  }
  // The PowerShell holder text must parse: syntax-check the gate through the real parser when PowerShell exists here.
  const ps = `$cfg = $null; $slots = @(); $ld = ''; function Test-Alive($p) { $true }; function Get-LfDummy {}; ${pool.PS_RAM_GATE.replace(/^\s*\$\{[^}]*\}/gm, "")}`;
  const r = spawnSync("powershell", ["-NoProfile", "-NonInteractive", "-Command", `$e = $null; [void][System.Management.Automation.Language.Parser]::ParseInput($env:LF_PS, [ref]$null, [ref]$e); if ($e.Count) { $e | ForEach-Object { Write-Output $_.Message }; exit 3 }`], { windowsHide: true, encoding: "utf8", timeout: 20000, env: { ...process.env, LF_PS: ps } });
  if (!r.error) check(r.status === 0, `the holder's RAM gate PowerShell parses (${(r.stdout ?? "").trim().split(/\r?\n/)[0]})`);
}

// the holder's answer is parsed (a lost backslash once turned \d into d)
{
  const src = readFileSync(new URL("../remote/pool.mjs", import.meta.url), "utf8");
  check(src.includes(String.raw`/^lf-pool: commit-short (-?\d+) (\d+) (\d+)$/`), "the holder's commit-short line is parsed into a RAM-style refusal");
}

// --- the status text and the queue line show commit
{
  const t = { stuck: "ram", clients: 1, profile: "lowspec", machines: ["local"], needMb: undefined };
  const why = q.waitReason(t, 0, { commitOf: (n) => (n === "local" ? "commit 70.2/71.2 GB used" : "") });
  check(/commit 70\.2\/71\.2 GB used/.test(why), `the queue line names commit used/limit (${why})`);
  check(/commit 70\.2\/71\.2 GB used/.test(q.waitReason({ ...t, needMb: 4 * GB }, 0, { freeMbOf: () => 9 * GB, commitOf: () => "commit 70.2/71.2 GB used" })), "and beside the free-RAM numbers when a physical need is known");
  check(q.DEFAULTS.minFreeCommitMb === 6144 && pool.poolSettings(scratchDir("lf-pool-commit-set-")).minFreeCommitMb > 0, "the floor setting defaults to 6 GB");
}

if (failures.length) {
  console.log("pool-commit-selftest: FAIL");
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}
console.log("pool-commit-selftest: OK (commit floor exact, a full-commit machine queues with free RAM, peaks learned from private bytes, local/remote/holder/ranking/queue line)");
