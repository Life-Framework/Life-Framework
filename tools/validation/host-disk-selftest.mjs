// tools/validation/host-disk-selftest.mjs
// inputs: tools/host/disk.mjs, tools/host/commands/disk.mjs
//
// Proves `tools\cli host disk`: a fixture host dir with a save and sized
// backups plus an injected free-space value gives the save size, the backups'
// total, growth over the last five backups, and how many sessions fit before
// free space drops under 5 GB; 4 GB free exits 1, 100 GB exits 0.
// red-proof: before tools/host/disk.mjs existed the import failed:
// "FAIL disk.mjs missing (Cannot find module ...)" (2026-11-15).

import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const GB = 1024 ** 3;
const tmp = mkdtempSync(join(tmpdir(), "lf-host-disk-"));
let failed = 0;
function fail(msg) {
  console.log(`FAIL  ${msg}`);
  failed++;
}

function sized(dir, bytes) {
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "data.bin"), Buffer.alloc(bytes, 1));
}

try {
  let disk, cmd;
  try {
    disk = await import(pathToFileURL(join(ROOT, "tools", "host", "disk.mjs")).href);
    cmd = await import(pathToFileURL(join(ROOT, "tools", "host", "commands", "disk.mjs")).href);
  } catch (e) {
    fail(`disk.mjs missing (${e.message})`);
    console.log("SUMMARY host-disk-selftest fail 1");
    process.exit(1);
  }

  // Save is 1000 bytes; seven backups grow 1000, 1100, ... 1600; two are tagged.
  // Sizes are tiny; the injected free space stands in for the drive.
  // A save point is a folder holding meta-info.json (2 bytes here + 998 = 1000).
  const point = join(tmp, "profile", "profile", ".save", "game", "G-world");
  sized(point, 998);
  writeFileSync(join(point, "meta-info.json"), "{}");
  const sizes = [1000, 1100, 1200, 1300, 1400, 1500, 1600];
  const names = [
    "2026-11-01_08-00-00-start", "2026-11-01_09-00-00-stop", "2026-11-02_08-00-00-start",
    "2026-11-02_09-00-00-wipe", "2026-11-03_08-00-00-start", "2026-11-03_09-00-00-stop",
    "2026-11-04_08-00-00-crash",
  ];
  names.forEach((n, i) => sized(join(tmp, "backups", n), sizes[i]));
  const total = sizes.reduce((a, b) => a + b, 0); // 9100

  const r = disk.diskReport(tmp, { freeBytes: 100 * GB });
  if (r.saveBytes !== 1000) fail(`saveBytes ${r.saveBytes}, want 1000`);
  if (r.backupsBytes !== total) fail(`backupsBytes ${r.backupsBytes}, want ${total}`);
  if (r.backupCount !== 7) fail(`backupCount ${r.backupCount}, want 7`);
  // last five: 1200..1600; growth = 1600 - 1200 = 400 over 4 steps = 100 per backup
  if (r.growthBytes !== 400) fail(`growthBytes ${r.growthBytes}, want 400`);
  if (r.growthPerBackup !== 100) fail(`growthPerBackup ${r.growthPerBackup}, want 100`);
  if (r.freeBytes !== 100 * GB) fail(`freeBytes ${r.freeBytes}`);
  // a session = a start and a stop backup, each the size of the newest (1600)
  const want = Math.floor((100 * GB - 5 * GB) / (2 * 1600));
  if (r.sessionsFit !== want) fail(`sessionsFit ${r.sessionsFit}, want ${want}`);
  if (r.low !== false) fail("100 GB flagged low");

  // 4 GB free: under the floor, nothing fits
  const low = disk.diskReport(tmp, { freeBytes: 4 * GB });
  if (low.low !== true) fail("4 GB not flagged low");
  if (low.sessionsFit !== 0) fail(`4 GB sessionsFit ${low.sessionsFit}, want 0`);

  // no backups, no save: zeros, no throw
  const empty = disk.diskReport(join(tmp, "nope"), { freeBytes: 50 * GB });
  if (empty.saveBytes !== 0 || empty.backupsBytes !== 0 || empty.backupCount !== 0) fail("empty host not zero");
  if (empty.growthBytes !== 0) fail("empty growth not 0");

  // command: exit codes follow the injected free space
  if (!cmd.help || typeof cmd.main !== "function") fail("commands/disk.mjs lacks help/main");
  const ok = await cmd.main([], { root: ROOT, hostDir: tmp, freeBytes: 100 * GB });
  if (ok !== 0) fail(`100 GB exit ${ok}, want 0`);
  const bad = await cmd.main([], { root: ROOT, hostDir: tmp, freeBytes: 4 * GB });
  if (bad !== 1) fail(`4 GB exit ${bad}, want 1`);
} finally {
  rmSync(tmp, { recursive: true, force: true });
}

console.log(failed ? `SUMMARY host-disk-selftest fail ${failed}` : "SUMMARY host-disk-selftest ok");
process.exit(failed ? 1 : 0);
