// tools/validation/host-verify-selftest.mjs
// inputs: tools/host/verify.mjs, tools/host/commands/verify.mjs, tools/validation/fixtures/host-verify/**
//
// Proves `tools\cli host verify` (#1454): on a fixture host with one good, one empty and one
// tampered backup it reports ok, empty and changed and exits 1; a clean host exits 0; a
// backup with no build stamp is flagged; --newest N looks at the newest N; the run never
// deletes or edits a backup; a save point whose WorldState data is a folder passes (#1636).
// red-proof: before tools/host/verify.mjs existed this printed
// "FAIL cannot load tools/host/verify.mjs: ERR_MODULE_NOT_FOUND" (2026-11-15).

import { cpSync, existsSync, mkdirSync, mkdtempSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { spawnSync } from "node:child_process";
import { safeRemoveTree } from "../link-safety.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FIX = join(ROOT, "tools", "validation", "fixtures", "host-verify", "savepoint");
let failed = 0;

function fail(msg) {
  console.log(`FAIL  ${msg}`);
  failed++;
}
function check(cond, msg) {
  if (!cond) fail(msg);
}

let verify;
let cmd;
try {
  verify = await import(pathToFileURL(join(ROOT, "tools", "host", "verify.mjs")).href);
  cmd = await import(pathToFileURL(join(ROOT, "tools", "host", "commands", "verify.mjs")).href);
} catch (e) {
  fail(`cannot load tools/host/verify.mjs or commands/verify.mjs: ${e.code ?? e.message}`);
  console.log(`SUMMARY host-verify-selftest fail ${failed}`);
  process.exit(1);
}

const SP = ["u", "game", "1054CC8646357708-LF-PlaytestTown", "playthrough000", "savepoint000"];
function goodBackup(backups, name) {
  const dest = join(backups, name, ...SP);
  mkdirSync(dirname(dest), { recursive: true });
  cpSync(FIX, dest, { recursive: true });
  return join(backups, name);
}
function quiet(fn) {
  const log = console.log;
  const lines = [];
  console.log = (...a) => lines.push(a.join(" "));
  return Promise.resolve()
    .then(fn)
    .finally(() => (console.log = log))
    .then((code) => ({ code, lines }));
}
const statuses = (r) => Object.fromEntries(r.results.map((x) => [x.name, x.status]));

const tmp = mkdtempSync(join(tmpdir(), "lf-host-verify-"));
try {
  const host = join(tmp, "host");
  const backups = join(host, "backups");
  mkdirSync(backups, { recursive: true });
  goodBackup(backups, "2026-11-10_08-00-00-stop");
  goodBackup(backups, "2026-11-11_08-00-00-stop");
  mkdirSync(join(backups, "2026-11-12_08-00-00-stop"));
  mkdirSync(join(backups, "2026-11-12_09-00-00-crash", ...SP), { recursive: true });
  writeFileSync(join(backups, "2026-11-12_09-00-00-crash", ...SP, "meta-info.json"), "");

  // First pass: good one records a checksum, empty ones are flagged, exit 1.
  let r = verify.verifyBackups(backups);
  let s = statuses(r);
  check(s["2026-11-10_08-00-00-stop"] === "ok", `good backup is ${s["2026-11-10_08-00-00-stop"]}`);
  check(s["2026-11-12_08-00-00-stop"] === "empty", `empty backup is ${s["2026-11-12_08-00-00-stop"]}`);
  check(s["2026-11-12_09-00-00-crash"] === "empty", `zero-byte backup is ${s["2026-11-12_09-00-00-crash"]}`);
  check(!r.ok, "a host with an empty backup reports ok");
  check(existsSync(join(backups, "2026-11-10_08-00-00-stop.sha256.json")), "no checksum recorded beside the good backup");
  check(!existsSync(join(backups, "2026-11-12_08-00-00-stop.sha256.json")), "a checksum was recorded for an empty backup");
  check(!readdirSync(join(backups, "2026-11-10_08-00-00-stop")).some((n) => n.endsWith(".json")), "a checksum file was written inside the backup");

  // Tamper with the second backup after its checksum is recorded.
  verify.verifyBackups(backups);
  const meta = join(backups, "2026-11-11_08-00-00-stop", ...SP, "lf-store.json");
  writeFileSync(meta, '{"version":32,"payloads":[]}\n');
  r = verify.verifyBackups(backups);
  s = statuses(r);
  check(s["2026-11-10_08-00-00-stop"] === "ok", `untouched backup is ${s["2026-11-10_08-00-00-stop"]} on the second run`);
  check(s["2026-11-11_08-00-00-stop"] === "changed", `tampered backup is ${s["2026-11-11_08-00-00-stop"]}`);
  check(s["2026-11-12_08-00-00-stop"] === "empty", "empty backup is not flagged on the second run");
  check(!r.ok, "tampered host reports ok");
  check(readFileSync(meta, "utf8") === '{"version":32,"payloads":[]}\n', "verify edited a backup");

  // --newest N: only the newest N folders are looked at.
  r = verify.verifyBackups(backups, { newest: 1 });
  check(r.results.length === 1 && r.results[0].name === "2026-11-12_09-00-00-crash", `--newest 1 looked at ${r.results.map((x) => x.name)}`);

  // Command: exit codes and one line per backup.
  let out = await quiet(() => cmd.main([], { hostDir: host }));
  check(out.code === 1, `command exit is ${out.code} on a bad host`);
  check(out.lines.some((l) => /changed/.test(l) && l.includes("2026-11-11_08-00-00-stop")), "command output does not name the changed backup");
  out = await quiet(() => cmd.main(["--newest", "x"], { hostDir: host }));
  check(out.code === 1, "command accepted a bad --newest");

  // A clean host: one good backup, exit 0; no backups folder at all is not a failure.
  const clean = join(tmp, "clean");
  goodBackup(join(clean, "backups"), "2026-11-10_08-00-00-stop");
  out = await quiet(() => cmd.main([], { hostDir: clean }));
  check(out.code === 0, `command exit is ${out.code} on a clean host`);
  out = await quiet(() => cmd.main([], { hostDir: join(tmp, "none") }));
  check(out.code === 0, `command exit is ${out.code} with no backups folder`);

  // No readable build stamp is flagged.
  const nb = join(tmp, "nostamp");
  const nbDir = goodBackup(join(nb, "backups"), "2026-11-10_08-00-00-stop");
  writeFileSync(join(nbDir, ...SP, "meta-info.json"), '{"m_sName":"x"}');
  r = verify.verifyBackups(join(nb, "backups"));
  check(statuses(r)["2026-11-10_08-00-00-stop"] === "nostamp", `backup with no build stamp is ${statuses(r)["2026-11-10_08-00-00-stop"]}`);
  const none = join(tmp, "nosave");
  mkdirSync(join(none, "backups", "2026-11-10_08-00-00-stop", "u"), { recursive: true });
  writeFileSync(join(none, "backups", "2026-11-10_08-00-00-stop", "u", "x.txt"), "hello");
  check(statuses(verify.verifyBackups(join(none, "backups")))["2026-11-10_08-00-00-stop"] === "nosave", "backup with files but no save point is not nosave");

  // The engine's real save point keeps its data in a WorldState entry that can be a folder
  // (#1636): that is a save point. An empty folder, or meta-info.json alone, is not.
  const real = join(tmp, "real");
  const realDir = goodBackup(join(real, "backups"), "2026-11-10_08-00-00-stop");
  const realSp = join(realDir, ...SP);
  for (const n of readdirSync(realSp)) if (n !== "meta-info.json") safeRemoveTree(join(realSp, n));
  mkdirSync(join(realSp, "WorldState", "nested"), { recursive: true });
  writeFileSync(join(realSp, "WorldState", "nested", "blob.bin"), "state");
  check(statuses(verify.verifyBackups(join(real, "backups")))["2026-11-10_08-00-00-stop"] === "ok", `backup whose WorldState is a folder is ${statuses(verify.verifyBackups(join(real, "backups")))["2026-11-10_08-00-00-stop"]}`);
  const hollow = join(tmp, "hollow");
  const hollowSp = join(goodBackup(join(hollow, "backups"), "2026-11-10_08-00-00-stop"), ...SP);
  for (const n of readdirSync(hollowSp)) if (n !== "meta-info.json") safeRemoveTree(join(hollowSp, n));
  mkdirSync(join(hollowSp, "WorldState"));
  check(statuses(verify.verifyBackups(join(hollow, "backups")))["2026-11-10_08-00-00-stop"] === "nosave", "a save point with only an empty WorldState folder is not nosave");

  // Wired through the CLI: exit code 1 on the bad host.
  const p = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "host", "verify"], { encoding: "utf8", env: { ...process.env, LF_HOST_DIR: host }, timeout: 60000, windowsHide: true });
  check(p.status === 1, `tools\cli host verify exits ${p.status} on a bad host`);
} finally {
  safeRemoveTree(tmp);
}

if (failed) {
  console.log(`SUMMARY host-verify-selftest fail ${failed}`);
  process.exit(1);
}
console.log("SUMMARY host-verify-selftest ok");
