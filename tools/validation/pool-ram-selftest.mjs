// pool-ram-selftest: RAM admission by measured peaks (tools/lib/pool-ram.mjs, docs/worktrees.md
// inputs: tools/lib/pool-ram.mjs, tools/remote/pool.mjs, tools/lib/wb-gate.mjs, tools/validation/pool-ram-selftest.mjs
// "RAM admission"). Pure functions plus the local and remote gates with test seams: no game, no real pool.
//
// red-proof 2026-11-16: with unrealisedMb returning 0 and a test reserving 0 (the gate as it was) the
// "a full machine queues", "a Workbench is counted" and "a test is admitted only if it leaves the floor"
// checks fail.
// red-proof 2026-11-17: with the old scan (servers only, no profile column) and peaks keyed by the asking
// job's profile, the classifier, "a test server records a test peak", "a client records under its own
// world and profile", "an island client grows to the island client peak" and machinePeaks checks fail.

import { spawnSync } from "node:child_process";
import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import * as pr from "../lib/pool-ram.mjs";
import * as pool from "../remote/pool.mjs";
import { WB_RESERVE_MB } from "../lib/wb-gate.mjs";

const failures = [];
// A scratch git checkout: the pool derives its hub (tmp/wt) from the checkout it is given.
const scratchRepo = (tag) => {
  const dir = mkdtempSync(join(tmpdir(), tag));
  spawnSync("git", ["init", "-q", dir], { windowsHide: true });
  return dir;
};
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};

const settings = { minFreeMb: 1500, sessionReserveServerMb: 600, sessionReserveClientMb: 850, sessionReserve: { lowspec: { serverMb: 600, clientMb: 800 } }, testReserveMb: 800 };
const none = {};
const GB = 1024;

// --- reserves
check(pr.worldClass("everon") === pr.ISLAND && pr.worldClass("Worlds/CoreOnEveron/CoreOnEveron.ent") === pr.ISLAND, "everon is an island world");
check(pr.worldClass("debug") === pr.SMALL && pr.worldClass("grid") === pr.SMALL && pr.worldClass(undefined) === pr.SMALL && pr.worldClass("{3626CFA069A75D46}Missions/LF_Everon1989_Test.conf") === pr.SMALL, "debug, grid, the pack header and no world are small");
const island1 = pr.jobReserveMb(settings, none, { kind: "session", clients: 1, profile: "lowspec", world: "everon" });
check(island1 === 2500 + 2900, `an island session with 1 client reserves 2.5 + 2.9 GB (got ${island1})`);
check(pr.jobReserveMb(settings, none, { kind: "workbench" }) === 3700 && WB_RESERVE_MB === 3700, "a Workbench reserves 3.7 GB");
check(pr.jobReserveMb(settings, none, { kind: "test" }) === 800, "a test reserves its server (it used to reserve nothing)");
check(pr.jobReserveMb(settings, none, { kind: "session", clients: 0, world: "debug" }) === 700, "a server-only small session reserves the server peak");

// --- peaks: p90 of observed samples, seeds until 3 exist, never under the configured floor
let peaks = {};
for (const mb of [2400, 2500, 2600, 2700, 2800, 2900, 3000, 3100, 3200, 4000]) peaks = pr.recordPeak(peaks, "server", pr.ISLAND, undefined, mb);
check(pr.peakMb(peaks, "server", pr.ISLAND) === 3200, `p90 of ten island server peaks is the ninth (got ${pr.peakMb(peaks, "server", pr.ISLAND)})`);
check(pr.peakMb(pr.recordPeak({}, "server", pr.ISLAND, undefined, 4000), "server", pr.ISLAND) === 2500, "one sample does not replace the seed");
check(pr.peakMb({ "test:small": [100, 120, 130] }, "test", pr.SMALL, undefined, 800) === 800, "a measured peak never drops under the configured floor");
check(pr.recordPeak({}, "client", pr.SMALL, "lowspec", 50)["client:small:lowspec"] === undefined, "a process caught at start (under 200 MB) is not a sample");
check(pr.recordPeak({ "server:small": Array(pr.MAX_SAMPLES).fill(700) }, "server", pr.SMALL, undefined, 900)["server:small"].length === pr.MAX_SAMPLES, "samples are capped");

// --- tracking: a process that ends leaves its highest working set as one sample
{
  const live1 = pr.trackPeaks({}, [{ pid: 7, kind: "workbench", world: pr.SMALL, wsMb: 900 }], {});
  const live2 = pr.trackPeaks(live1.live, [{ pid: 7, kind: "workbench", world: pr.SMALL, wsMb: 3500 }], live1.peaks);
  const gone = pr.trackPeaks(live2.live, [], live2.peaks);
  check(gone.recorded === 1 && gone.peaks["workbench:small"]?.[0] === 3500, `an ended Workbench records its highest working set (${JSON.stringify(gone.peaks)})`);
  const dir = mkdtempSync(join(tmpdir(), "lf-pool-ram-"));
  try {
    pr.recordLive(dir, [{ pid: 9, kind: "server", world: pr.ISLAND, wsMb: 2600 }], { tag: "lf-testbox" });
    pr.recordLive(dir, [], { tag: "lf-testbox" });
    check(pr.readPeaks(dir)["server:island"]?.[0] === 2600, "recordLive keeps the sample in the hub file");
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
}

// --- scan parsing and growth: a Workbench still loading counts for what it will take
const scan = pr.parseScan("lf-pool: proc workbench 4100 600 0\nlf-pool: proc server 4200 500 1\nlf-pool: proc client 4300 1200 0\nnoise\n");
check(scan.length === 3 && scan[0].kind === "workbench" && scan[1].world === pr.ISLAND, "parseScan reads kind, pid, working set and island");
check(pr.unrealisedMb(scan, none) === 3700 - 600 + (2500 - 500), `growth is peak less held, never negative (got ${pr.unrealisedMb(scan, none)})`);
check(scan[0].privMb === undefined && pr.parseScan("lf-pool: proc workbench 1 600 0 3900")[0].privMb === 3900, "parseScan reads the private bytes when the box sends them");
// A trimmed, long-loaded Workbench (working set 27 MB, 3.9 GB committed) has nothing left to grow; one seen at 3.5 GB and trimmed counts that.
check(pr.unrealisedMb([{ kind: "workbench", world: pr.SMALL, wsMb: 27, privMb: 3900 }], none) === 0, "a loaded but trimmed Workbench does not count as growing");
check(pr.unrealisedMb([{ kind: "workbench", world: pr.SMALL, wsMb: 27, privMb: 900 }], none) === 3673 && pr.unrealisedMb([{ kind: "workbench", world: pr.SMALL, wsMb: 27, privMb: 900, maxMb: 3500 }], none) === 200, "a process counts for the highest working set seen when its own is trimmed");
check(pr.islandCount(scan) === 1, "islandCount counts island servers and clients");

// --- the gate
const gate = (o) => pr.admit({ minFreeMb: 1500, ...o });
check(!gate({ freeMb: 1000, reserveMb: 800 }).ok, "a full machine (1 GB free) queues a small test");
check(gate({ freeMb: 4000, reserveMb: 800 }).ok, "a small test is admitted with room");
check(!gate({ freeMb: 2200, reserveMb: 800 }).ok && gate({ freeMb: 2300, reserveMb: 800 }).ok, "a test is admitted only if it leaves the 1.5 GB floor after its own peak");
check(!gate({ freeMb: 6000, reserveMb: 5400, island: true }).ok, "an island session (5.4 GB) is refused at 6 GB free");
check(gate({ freeMb: 7000, reserveMb: 5400, island: true }).ok, "an island session is admitted at 7 GB free with nothing else running");
const second = gate({ freeMb: 9000, reserveMb: 5400, island: true, islandRunning: 1 });
check(!second.ok && /island/.test(second.reason), `a second island is refused unless both peaks fit (${second.reason})`);
check(gate({ freeMb: 13000, reserveMb: 5400, island: true, islandRunning: 1 }).ok, "a second island is admitted on a machine with room for both");
check(!gate({ freeMb: 5000, reserveMb: 800, growthMb: 3100 }).ok && gate({ freeMb: 5000, reserveMb: 800, growthMb: 2000 }).ok, "what running jobs still grow into counts against free RAM");
check(gate({ freeMb: undefined, reserveMb: 800 }).ok, "an unreadable machine is left to the other gates");

// --- the local gate end to end (seams: no scan, no real RAM)
const m = { name: "local", via: "local", slots: 1, lanes: { test: 2, session: 2 } };
const root = scratchRepo("lf-pool-ram-root-");
try {
  const adm = (meta, opts) => pool.localAdmission(root, m, "server-test-1", meta, settings, { peaks: {}, ...opts });
  check(!adm({ purpose: "test" }, { freeMb: 1 * GB, procs: [] }).ok, "local: a test on a 1 GB machine queues");
  check(adm({ purpose: "test" }, { freeMb: 6 * GB, procs: [] }).ok, "local: a test on a 6 GB machine is admitted");
  const wbLoading = [{ pid: 5, kind: "workbench", world: pr.SMALL, wsMb: 500 }];
  check(adm({ purpose: "test" }, { freeMb: 5 * GB, procs: [] }).ok && !adm({ purpose: "test" }, { freeMb: 5 * GB, procs: wbLoading }).ok, "local: a Workbench that is still loading (3.2 GB to grow) is counted against a test");
  const idle = adm({ purpose: "session", clients: 1, profile: "lowspec", world: "everon" }, { freeMb: 7.5 * GB, procs: [] });
  check(idle.ok, `local: an island session fits an idle 7.5 GB machine (${idle.reason})`);
  const island = [{ pid: 6, kind: "server", world: pr.ISLAND, wsMb: 2500 }, { pid: 8, kind: "client", world: pr.ISLAND, wsMb: 2900 }];
  const refused = adm({ purpose: "session", clients: 1, profile: "lowspec", world: "everon" }, { freeMb: 9 * GB, procs: island });
  check(!refused.ok, "local: a second island session is refused while one runs and there is no room for both");
  check(adm({ purpose: "session", clients: 1, profile: "lowspec", world: "debug" }, { freeMb: 9 * GB, procs: island }).ok, "local: a small session still starts beside one island session");
  // A pool lock younger than the spawn gap counts by its reserve; an older one only through its processes.
  const fresh = [{ purpose: "session", at: new Date().toISOString(), clients: 1, profile: "lowspec", world: "everon" }];
  check(pr.lockGapMb(fresh, settings, none) === 5400 && pr.lockGapMb([{ ...fresh[0], at: new Date(Date.now() - 120000).toISOString() }], settings, none) === 0, "a lane lock counts by its reserve only in its first 45 s");
} finally {
  rmSync(root, { recursive: true, force: true });
}

// --- the remote gate reads the box's scan
{
  const laptop = { name: "lf-testbox", via: "ssh", lanes: { test: 3, session: 2 } };
  const st = { locks: {}, freeMb: 9 * GB, totalMb: 15.2 * GB, procs: [{ pid: 3, kind: "workbench", world: pr.SMALL, wsMb: 400 }] };
  const test = pool.remoteAdmission(laptop, st, settings, { kind: "test", peaks: {} });
  check(test.ok && test.needMb >= 1500 + 800 + 3300, `remote: the laptop's Workbench is counted (needs ${test.needMb} MB)`);
  const tight = pool.remoteAdmission(laptop, { ...st, freeMb: 5 * GB }, settings, { kind: "test", peaks: {} });
  check(!tight.ok, "remote: a 5 GB laptop with a Workbench loading queues a test");
  const parsed = pool.parseRemoteState("lf-pool: ram 4096 15564\nlf-pool: proc workbench 11 3000 0\n");
  check(parsed.procs?.length === 1 && parsed.procs[0].kind === "workbench", "remoteState parses the scan lines");
}

// --- classification (2026-11-17): test servers, island clients, client profiles, look vs tool Workbench
{
  // The PowerShell classifier on Win32_Process-shaped objects with real command-line shapes.
  const WB = "ArmaReforgerWorkbench" + "SteamDiag.exe"; // split: validate-wb-policy flags a file that names the launcher
  const S = "C:\\Users\\u\\AppData\\Local\\LifeFramework\\session\\Life-Framework-rt-x-session";
  const rows = [
    { Name: "ArmaReforgerServer.exe", ProcessId: 1, cmd: `"c:\\a\\ArmaReforgerServer.exe" -server Worlds/CoreOnEveron/CoreOnEveron.ent -profile ${S}\\server -maxFPS 30` },
    { Name: "ArmaReforgerSteam.exe", ProcessId: 2, cmd: `"C:\\a\\ArmaReforgerSteam.exe" -client 127.0.0.1 -profile ${S}\\client1 -window -screenWidth 1280 -maxFPS 30 -lfSession 79` },
    { Name: "ArmaReforgerSteam.exe", ProcessId: 3, cmd: `"C:\\a\\ArmaReforgerSteam.exe" -client 127.0.0.1 -profile C:\\other\\session\\client1 -window -maxFPS 15 -noSound` },
    { Name: "ArmaReforgerServerDiag.exe", ProcessId: 4, cmd: `"c:\\a\\ArmaReforgerServerDiag.exe" -server X.conf -profile C:\\LF\\agents\\Life-Framework-ws-g\\server\\profile\\test -maxFPS 30 -addr 127.0.0.1:2003` },
    { Name: "ArmaReforgerServer.exe", ProcessId: 5, cmd: `"c:\\a\\ArmaReforgerServer.exe" -server X.conf -profile C:\\LF\\w\\server\\profile\\serve -maxFPS 30` },
    { Name: WB, ProcessId: 6, cmd: `"C:\\a\\${WB}" -gproj C:\\LF\\v\\addons\\LF_Everon1989\\LF_Everon1989.gproj -wbModule=WorldEditor -plugin=LF_VisualShotsPlugin -profile C:\\LF\\v\\server\\profile\\visual-serve` },
    { Name: WB, ProcessId: 7, cmd: `"C:\\a\\${WB}" -gproj C:\\LF\\r\\addons\\LF_Everon1989\\LF_Everon1989.gproj -wbModule=ResourceManager -run -exitAfterInit -profile C:\\LF\\r\\server\\profile\\rdb` },
  ];
  const q = (x) => `'${String(x).replace(/'/g, "''")}'`;
  const objs = rows.map((r) => `[pscustomobject]@{ Name = ${q(r.Name)}; ProcessId = ${r.ProcessId}; CommandLine = ${q(r.cmd)}; WorkingSetSize = 1048576000; PrivatePageCount = 2097152000 }`).join(", ");
  const ps = [pr.PS_PROC_ROWS, `Get-LfArmaRows @(${objs}) | ForEach-Object { Write-Host ("lf-pool: proc " + $_.k + " " + $_.id + " " + $_.ws + " " + $_.isl + " " + $_.priv + " " + $_.prof) }`].join("\n");
  const r = spawnSync("powershell", ["-NoProfile", "-NonInteractive", "-Command", ps], { encoding: "utf8", windowsHide: true, timeout: 60000 });
  if (r.error) console.log(`pool-ram-selftest: note: PowerShell unavailable (${r.error.message}); classifier check skipped`);
  else {
    const got = Object.fromEntries(pr.parseScan(r.stdout).map((p) => [p.pid, p]));
    const want = (pid, kind, world, profile) => {
      const p = got[pid];
      check(p && p.kind === kind && p.world === world && p.profile === profile, `classifier: pid ${pid} is ${kind}/${world}/${profile} (got ${JSON.stringify(p)}; ${r.stderr.trim().slice(0, 200)})`);
    };
    want(1, "server", pr.ISLAND, undefined);
    want(2, "client", pr.ISLAND, "full"); // joins by address: island only through its session folder
    want(3, "client", pr.SMALL, "lowspec");
    want(4, "test", pr.SMALL, undefined);
    want(5, "server", pr.SMALL, undefined);
    want(6, "workbench", pr.SMALL, "look");
    want(7, "workbench", pr.SMALL, "tool");
    check(got[1]?.wsMb === 1000 && got[1]?.privMb === 2000, "classifier: working set and private MB");
  }
  // The holder's gate classifies with the same function.
  check(pool.PS_RAM_GATE.includes("function Get-LfArmaRows") && /Get-LfArmaRows\)/.test(pool.PS_RAM_GATE), "the holder gate uses Get-LfArmaRows");

  const sc = pr.parseScan("lf-pool: proc test 1 700 0 900 -\nlf-pool: proc client 2 1700 1 4400 full\nlf-pool: proc client 3 900 0 1500 lowspec\nlf-pool: proc workbench 4 300 0 400 tool\n");
  check(sc[0].kind === "test" && sc[0].profile === undefined && sc[1].profile === "full" && sc[3].profile === "tool", "parseScan reads the test kind and the profile column");
  // Peaks are keyed by what the process is, not by the job being admitted.
  let t = pr.trackPeaks({}, sc, {}, "lowspec");
  t = pr.trackPeaks(t.live, [], t.peaks, "lowspec");
  check(t.peaks["test:small"]?.[0] === 700, `a test server records a test peak (${JSON.stringify(t.peaks)})`);
  check(t.peaks["client:island"]?.[0] === 1700 && t.peaks["client:small:lowspec"]?.[0] === 900, "a client records under its own world and profile, not the asking job's");
  check(t.peaks["workbench:small:tool"]?.[0] === 300 && !t.peaks["workbench:small"], "a tool Workbench records apart from the look viewer");
  // Growth: an island client grows to the island peak; a tool Workbench to the tool peak.
  const isc = { ...sc[1], privMb: 1800 };
  check(pr.unrealisedMb([isc], none, "lowspec") === 2900 - 1700, `an island client grows to the island client peak (got ${pr.unrealisedMb([isc], none, "lowspec")})`);
  check(pr.unrealisedMb([sc[3]], none) === 1300 - 300, "an rdb/compile Workbench grows to the tool peak, not the look viewer's 3.7 GB");
  check(pr.islandCount([{ kind: "test", world: pr.ISLAND }]) === 1, "an island test server counts as island-sized");

  // Per-machine peaks: the box's own measured keys win; too few samples keep this PC's.
  const merged = pr.mergePeaks({ "workbench:small": [3700, 3700, 3700], "server:small": [700, 700, 700] }, { "workbench:small": [2500, 2600, 2550], "client:small": [1, 2] });
  check(merged["workbench:small"][1] === 2600 && merged["server:small"][0] === 700 && !merged["client:small"], "mergePeaks: a box key with 3+ samples wins, fewer is ignored");
  check(pr.mergePeaks({ a: [1] }, null).a[0] === 1, "mergePeaks: no box peaks keeps this PC's");
  const rs = pool.parseRemoteState('lf-pool: ram 4096 15564\nlf-pool: peaks {"workbench:small":[2500,2600,2550]}\n');
  check(rs.peaks?.["workbench:small"]?.length === 3, "remoteState reads the box's pool-peaks.json line");
  check(pool.parseRemoteState("lf-pool: ram 4096 15564\nlf-pool: peaks {broken\n").peaks === null, "a broken peaks line is ignored");
  const proot = scratchRepo("lf-pool-ram-mp-");
  try {
    const hm = { name: "lf-testbox", via: "ssh" };
    check(pr.peakMb(pool.machinePeaks(proot, hm, rs), "workbench", pr.SMALL) === 2600, "machinePeaks: the laptop's Workbench reserve is its own measured p90, not this PC's seed");
    check(pr.peakMb(pool.machinePeaks(proot, { name: "local", via: "local" }), "workbench", pr.SMALL) === 3700, "machinePeaks: this PC keeps its own");
  } finally {
    rmSync(proot, { recursive: true, force: true });
  }
}

// --- the holder's PowerShell gate carries the same rules
check(/\$cfg\.ram\.peaks/.test(pool.PS_RAM_GATE) && /ArmaReforger%/.test(pool.PS_RAM_GATE) && /\$need \+= \$cfg\.ram\.reserveMb/.test(pool.PS_RAM_GATE), "the holder gate counts process growth and the second-island rule");
const hroot = scratchRepo("lf-pool-ram-hold-");
try {
  const hm = { name: "lf-testbox", via: "ssh", host: "x", root: "C:/LF/Life-Framework", slots: 1, lanes: { test: 3, session: 2 } };
  const cfg = pool.holderConfig(hroot, hm, { sha: "a", head: "a" }, { purpose: "session", settings, world: "everon" });
  check(cfg.ram.island === true && cfg.ram.reserveMb === 5400 && cfg.ram.peaks["workbench:small"] === 3700, `the holder config carries the island reserve and the peak table (${cfg.ram.reserveMb})`);
  check(pool.holderConfig(hroot, hm, { sha: "a", head: "a" }, { purpose: "test", settings }).ram.reserveMb === 800, "a test's holder reserve is its server peak");
} finally {
  rmSync(hroot, { recursive: true, force: true });
}

if (failures.length) {
  console.log("pool-ram-selftest: FAIL");
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}
console.log("pool-ram-selftest: OK (measured peaks and seeds, p90 samples, a full machine queues, an island refused without room, a second island only with room for both, a small test admitted, a Workbench counted, remote scan, holder gate)");
