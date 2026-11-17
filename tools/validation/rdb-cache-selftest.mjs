// tools/validation/rdb-cache-selftest.mjs
// inputs: tools/lib/rdb-cache.mjs, tools/lib/rdb-fresh.mjs, tools/lib/wb-lock.mjs, tools/lib/lock-wait.mjs
//
// Proves the shared rdb cache (tools/lib/rdb-cache.mjs) and the queue hooks that let a waiting refresh
// stop once another worktree published the rdb: same resource set hits and is fresh, one extra .meta
// misses, a stale entry is rejected and deleted, LF_RDB_CACHE=0 disables, the oldest entry is pruned,
// until() ends a queued acquireWorkbenchSync and waitForLock without the lock, and a waiter re-reads
// the Workbench limit while queued.
// red-proof: 2026-11-17, with take() stubbed to return false and wb-lock.mjs/lock-wait.mjs unmodified this printed
// "FAIL same resource set is a hit", "FAIL hit leaves the rdb byte-identical", "FAIL stale cached file is deleted", then the
// until() case threw "workbench: no free Workbench slot within 20 s" (no until(), no limit re-read) and exited 1.

import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readdirSync, utimesSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import * as cache from "../lib/rdb-cache.mjs";
import { guidBytes } from "../lib/rdb-fresh.mjs";
import * as wb from "../lib/wb-lock.mjs";
import { Budget, waitForLock } from "../lib/lock-wait.mjs";
import * as wt from "../wt.mjs";
import { scratchDir } from "../lib/scratch.mjs";

let failed = 0;
const check = (name, ok) => {
  if (!ok) {
    failed++;
    console.log(`FAIL ${name}`);
  }
};

const root = scratchDir("lf-rdb-cache-");
spawnSync("git", ["init", "-q", root], { windowsHide: true });
const hub = wt.ensureHub(root);
const addonDir = join(root, "addons", "Demo");
mkdirSync(addonDir, { recursive: true });
const guids = ["AAAAAAAAAAAAAAA1", "AAAAAAAAAAAAAAA2", "AAAAAAAAAAAAAAA3"];
const addMeta = (i, path) => {
  mkdirSync(join(addonDir, "Prefabs"), { recursive: true });
  writeFileSync(join(addonDir, `${path}.meta`), `MetaFileClass {\n Name "{${guids[i]}}${path}"\n}\n`);
};
const rdbOf = (paths) => Buffer.concat(paths.flatMap((p, i) => [Buffer.from(p), Buffer.from([0, 6, 0, 0, 0, 0, 0]), guidBytes(guids[i])]));
const paths = ["Prefabs/A.et", "Prefabs/B.et"];
paths.forEach((p, i) => addMeta(i, p));
const exe = join(root, "wb.exe");
writeFileSync(exe, "x");
const open = () => cache.open({ hub, addonName: "Demo", addonDir, exe });
const rdbFile = join(addonDir, "resourceDatabase.rdb");

// Worktree A refreshed: its fresh rdb is stored.
writeFileSync(rdbFile, rdbOf(paths));
const a = open();
check("store of a fresh rdb", a.store() === true && a.has());
// Worktree B: same resource set, no rdb yet (or an old one).
writeFileSync(rdbFile, "old");
check("same resource set is a hit", open().key === a.key && open().take() === true);
check("hit leaves the rdb byte-identical", readdirSync(addonDir).filter((n) => n.includes(".tmp-")).length === 0 && rdbOf(paths).equals(Buffer.from(require_read(rdbFile))));
function require_read(f) {
  return spawnSync(process.execPath, ["-e", `process.stdout.write(require("fs").readFileSync(${JSON.stringify(f)}))`], { encoding: "buffer" }).stdout;
}
// One extra .meta: a different key, a miss.
addMeta(2, "Prefabs/C.et");
const c = open();
check("one extra .meta is a miss", c.key !== a.key && c.has() === false && c.take() === false);
// A store with a stale rdb (missing C) is refused.
check("a stale rdb is not stored", c.store() === false);
// A cached file that does not cover the resources is rejected and deleted.
mkdirSync(join(hub, "rdb-cache", "Demo"), { recursive: true });
writeFileSync(c.file, rdbOf(paths));
check("stale cached file is rejected", c.take() === false);
check("stale cached file is deleted", !existsSync(c.file));
// Kill switch.
process.env.LF_RDB_CACHE = "0";
check("LF_RDB_CACHE=0 disables", open() === null);
delete process.env.LF_RDB_CACHE;
// Prune: oldest beyond MAX_ENTRIES goes.
const dir = cache.cacheDir(hub, "Demo");
const old = join(dir, `${"0".repeat(40)}.rdb`);
writeFileSync(old, "x");
utimesSync(old, new Date(2020, 0, 1), new Date(2020, 0, 1));
for (let i = 0; i < cache.MAX_ENTRIES; i++) writeFileSync(join(dir, `fill-${i}.rdb`), "x");
writeFileSync(rdbFile, rdbOf(["Prefabs/A.et", "Prefabs/B.et", "Prefabs/C.et"]));
check("store after fill", c.store() === true);
check("oldest entry pruned, count capped", !existsSync(old) && readdirSync(dir).filter((n) => n.endsWith(".rdb")).length <= cache.MAX_ENTRIES);

// until(): the only Workbench slot is held (by this live pid); a queued waiter returns without a lock.
delete process.env.LF_WB_LIMIT;
const localFile = join(root, "lf.local.json");
const setLimit = (n) => writeFileSync(localFile, JSON.stringify({ pool: { workbenchLimit: n } }) + " ".repeat(n * 7));
setLimit(1);
const held = wt.acquireLock(root, "workbench-1", { waitMs: 0, meta: { purpose: "selftest-holder", pid: process.pid } });
let polls = 0;
const got = wb.acquireWorkbenchSync(root, { purpose: "selftest", pollMs: 20, log: () => {}, waitMs: 20000, until: () => ++polls >= 3 });
check("until() returns satisfied", got.satisfied === true && got.name === null);
check("until() dropped the queue ticket", wb.liveTickets(root).length === 0);
// limit re-read: raised to 2 while queued, the waiter takes workbench-2.
polls = 0;
const got2 = wb.acquireWorkbenchSync(root, {
  purpose: "selftest",
  pollMs: 20,
  log: () => {},
  waitMs: 20000,
  until: () => {
    if (++polls === 3) setLimit(2);
    return false;
  },
});
check("queued waiter picked up the raised limit", got2.name === "workbench-2");
got2.release();
wt.releaseLock(root, "workbench-1");
void held;
// waitForLock until().
let n = 0;
const lw = waitForLock({ what: "x", budget: new Budget(20000), sliceMs: 30, log: () => {}, until: () => ++n >= 3, acquire: () => { throw new Error("x is held"); } });
check("waitForLock until() ends the wait", lw?.satisfied === true);

if (failed) {
  console.log(`SUMMARY rdb-cache-selftest fail ${failed}`);
  process.exit(1);
}
console.log("SUMMARY rdb-cache-selftest ok");
