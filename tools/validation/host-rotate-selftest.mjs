// tools/validation/host-rotate-selftest.mjs
// inputs: tools/host/rotate.mjs, tools/host/commands/prune.mjs
//
// Proves `tools\cli host prune [--keep N]` keeps the newest N plain backups
// (<stamp>-start, <stamp>-stop) and never deletes a tagged one (preflight,
// wipe, crash, replaced-by-restore, manual, anything else). Fixture: 30 dated
// backup folders of mixed tags in a temp dir.
// red-proof: before tools/host/rotate.mjs existed the import failed:
// "FAIL rotate.mjs missing (Cannot find module ...)" (2026-11-15).

import { mkdirSync, mkdtempSync, readdirSync, rmSync, writeFileSync, existsSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const tmp = mkdtempSync(join(tmpdir(), "lf-host-rotate-"));
let failed = 0;
function fail(msg) {
  console.log(`FAIL  ${msg}`);
  failed++;
}

try {
  let rot, prune;
  try {
    rot = await import(pathToFileURL(join(ROOT, "tools", "host", "rotate.mjs")).href);
    prune = await import(pathToFileURL(join(ROOT, "tools", "host", "commands", "prune.mjs")).href);
  } catch (e) {
    fail(`rotate.mjs missing (${e.message})`);
    process.exit(1);
  }

  // 30 folders, one per hour on 2026-11-01. Every 3rd is tagged; the rest
  // alternate start/stop; two share a stamp (freshDir "-2" suffix).
  const tagged = ["preflight-1.8.0.13-to-1.9.0.37", "wipe", "crash", "replaced-by-restore", "manual"];
  const backups = join(tmp, "backups");
  const names = [];
  const plain = [];
  const p = (n) => String(n).padStart(2, "0");
  for (let i = 0; i < 30; i++) {
    const stamp = `2026-11-${p(1 + Math.floor(i / 24))}_${p(i % 24)}-00-00`;
    let reason;
    if (i % 3 === 2) reason = tagged[(i / 3 | 0) % tagged.length];
    else reason = i % 2 ? "start" : "stop";
    let name = `${stamp}-${reason}`;
    if (i === 10 || i === 20) name = `${stamp}-${reason}-2`; // twin suffix
    names.push(name);
    if (/-(start|stop)(-\d+)?$/.test(name)) plain.push(name);
    mkdirSync(join(backups, name), { recursive: true });
    writeFileSync(join(backups, name, "x.txt"), "x");
  }
  mkdirSync(join(backups, "notes"), { recursive: true }); // unknown folder: never touched
  const taggedNames = names.filter((n) => !plain.includes(n));

  // classify
  if (rot.isPlainBackup("2026-11-01_00-00-00-start") !== true) fail("start not plain");
  if (rot.isPlainBackup("2026-11-01_00-00-00-stop-2") !== true) fail("stop-2 not plain");
  for (const t of ["preflight-a-to-b", "wipe", "crash", "replaced-by-restore", "manual", "start-up", "restart"]) {
    if (rot.isPlainBackup(`2026-11-01_00-00-00-${t}`)) fail(`${t} classed plain`);
  }
  if (rot.isPlainBackup("notes")) fail("unstamped folder classed plain");

  // plan is read-only
  const keep = 8;
  const plan = rot.planPrune(backups, keep);
  if (readdirSync(backups).length !== 31) fail("planPrune touched the folder");
  const expectDelete = plain.slice().sort().slice(0, plain.length - keep);
  if (JSON.stringify(plan.delete.slice().sort()) !== JSON.stringify(expectDelete)) fail(`plan.delete ${plan.delete.length}, expected ${expectDelete.length}`);

  // dry run deletes nothing
  const dry = rot.pruneBackups(backups, keep, { dryRun: true });
  if (dry.deleted.length !== expectDelete.length) fail("dry run reported wrong count");
  if (readdirSync(backups).length !== 31) fail("dry run deleted something");

  // real run
  const res = rot.pruneBackups(backups, keep);
  const left = readdirSync(backups).sort();
  const expectLeft = [...names.filter((n) => !expectDelete.includes(n)), "notes"].sort();
  if (JSON.stringify(left) !== JSON.stringify(expectLeft)) fail(`survivors differ: got ${left.length}, want ${expectLeft.length}`);
  for (const t of taggedNames) if (!existsSync(join(backups, t))) fail(`tagged backup deleted: ${t}`);
  if (res.deleted.length !== expectDelete.length) fail(`deleted ${res.deleted.length}, want ${expectDelete.length}`);
  // the newest N plain survive
  for (const n of plain.slice().sort().slice(-keep)) if (!existsSync(join(backups, n))) fail(`newest plain deleted: ${n}`);

  // idempotent, and keep larger than count deletes nothing
  if (rot.pruneBackups(backups, keep).deleted.length !== 0) fail("second run deleted more");
  if (rot.pruneBackups(backups, 999).deleted.length !== 0) fail("keep 999 deleted");
  // missing folder is not an error
  if (rot.pruneBackups(join(tmp, "nope"), 5).deleted.length !== 0) fail("missing folder");
  // bad keep rejected
  for (const bad of [-1, 1.5, NaN]) {
    let threw = false;
    try { rot.pruneBackups(backups, bad); } catch { threw = true; }
    if (!threw) fail(`keep ${bad} accepted`);
  }
  if (!prune.help || typeof prune.main !== "function") fail("prune.mjs lacks help/main");
  const code = await prune.main(["--keep", "x"], { root: ROOT, hostDir: tmp });
  if (code === 0) fail("prune --keep x returned 0");
  const code2 = await prune.main(["--keep", "3", "--dry-run"], { root: ROOT, hostDir: tmp });
  if (code2 !== 0) fail("prune --dry-run failed");
} finally {
  rmSync(tmp, { recursive: true, force: true });
}

console.log(failed ? `SUMMARY host-rotate-selftest fail ${failed}` : "SUMMARY host-rotate-selftest ok");
process.exit(failed ? 1 : 0);
