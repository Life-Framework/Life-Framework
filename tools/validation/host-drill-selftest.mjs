// tools/validation/host-drill-selftest.mjs
// inputs: tools/host/**
//
// Judges fixture console logs the way `tools\cli host drill` does (#1261): a good
// load passes; an empty load (no applied account record), a damaged load, a VME and a
// boot that never loaded a save all fail. Also proves the scratch profile is a copy that
// leaves the source backup and the live host profile untouched.
// red-proof: before tools/host/drill.mjs existed this check failed to import it
// (ERR_MODULE_NOT_FOUND, 2026-11-15).

import { existsSync, mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FIX = join(ROOT, "tools", "host", "fixtures", "drill");
const drill = await import(pathToFileURL(join(ROOT, "tools", "host", "drill.mjs")).href);
const { safeRemoveTree } = await import(pathToFileURL(join(ROOT, "tools", "link-safety.mjs")).href);

const failures = [];
const fail = (m) => failures.push(m);
const log = (name) => readFileSync(join(FIX, name), "utf8");

const good = drill.drillVerdict(log("good-load.txt"));
if (!good.ok || good.accounts !== 3) fail(`good load: ${JSON.stringify(good)}`);
for (const [name, want] of [["empty-load.txt", /account/], ["damaged-load.txt", /damaged/], ["vme.txt", /script error/], ["no-load.txt", /save loaded/]]) {
  const v = drill.drillVerdict(log(name));
  if (v.ok) fail(`${name} passed`);
  else if (!want.test(v.reason)) fail(`${name} failed for the wrong reason: ${v.reason}`);
}
if (drill.drillVerdict("").ok) fail("an empty log passed");
if (drill.drillVerdict(undefined).ok) fail("a missing log passed");

const line = drill.summaryLine({ ok: true, backup: "2026-11-15_10-00-00-stop", machine: "local", accounts: 3, log: "L" });
if (!/^SUMMARY host-drill ok /.test(line) || !line.includes("accounts=3") || !line.includes("log=L")) fail(`summary line: ${line}`);
if (!/^SUMMARY host-drill fail /.test(drill.summaryLine({ ok: false, backup: "b", machine: "local", reason: "x" }))) fail("a failed drill prints ok");

// Scratch: a copy of the backup in a throwaway host dir; the live host dir and the backup are not changed.
const base = mkdtempSync(join(tmpdir(), "lf-drill-selftest-"));
try {
  const host = join(base, "host");
  const sp = join(host, "backups", "2026-11-15_10-00-00-stop", "u", "game", "1054CC8646357708-LF-PlaytestTown", "playthrough000", "savepoint000");
  mkdirSync(sp, { recursive: true });
  writeFileSync(join(sp, "meta-info.json"), "{}");
  writeFileSync(join(sp, "data.bin"), "abc");
  const live = join(host, "profile", "profile", ".save", "live.txt");
  mkdirSync(dirname(live), { recursive: true });
  writeFileSync(live, "live");
  const picked = drill.pickDrillBackup(host);
  if (!picked || !picked.endsWith("2026-11-15_10-00-00-stop")) fail(`pickDrillBackup: ${picked}`);
  if (drill.pickDrillBackup(join(base, "none"))) fail("pickDrillBackup invented a backup");
  const scratch = drill.prepareScratch(picked, join(base, "scratch"));
  if (!existsSync(join(scratch, "profile", "profile", ".save", "u", "game", "1054CC8646357708-LF-PlaytestTown", "playthrough000", "savepoint000", "data.bin"))) fail("the scratch profile holds no copy of the backup");
  if (scratch.startsWith(host)) fail("the scratch profile is inside the live host dir");
  writeFileSync(join(scratch, "profile", "profile", ".save", "u", "game", "1054CC8646357708-LF-PlaytestTown", "playthrough000", "savepoint000", "data.bin"), "changed");
  if (readFileSync(join(sp, "data.bin"), "utf8") !== "abc") fail("the drill changed the source backup");
  if (readFileSync(live, "utf8") !== "live") fail("the drill changed the live profile");
  let threw = false;
  try { drill.prepareScratch(join(base, "empty"), join(base, "scratch2")); } catch { threw = true; }
  if (!threw) fail("prepareScratch accepted a folder with no save point");
} finally {
  safeRemoveTree(base);
}

if (failures.length) {
  for (const f of failures) console.log(`FAIL  ${f}`);
  process.exit(1);
}
console.log("host-drill-selftest: ok");
