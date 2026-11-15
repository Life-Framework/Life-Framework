// tools/validation/host-wipe-selftest.mjs
// inputs: tools/host/**
//
// Proves `tools\cli host wipe --confirm <host dir name>` (#1274) on a fixture host dir:
// it refuses without the typed confirm, refuses while the host runs, and with the right
// confirm writes a `wipe` backup, moves the save root aside (nothing deleted), leaves the
// save root empty, logs a row to backups/wipes.log and prints how to restore.
// red-proof: before tools/host/wipe.mjs existed this file died on the import:
// "Cannot find module '.../tools/host/wipe.mjs'" (2026-11-15).

import { existsSync, mkdirSync, mkdtempSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { basename, dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { spawnSync } from "node:child_process";
import { safeRemoveTree } from "../link-safety.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const { wipeHost, WIPES_LOG } = await import(pathToFileURL(join(ROOT, "tools", "host", "wipe.mjs")).href);
const { findSavePoints, hostPaths, writeHostState, saveRoot } = await import(pathToFileURL(join(ROOT, "tools", "host", "host.mjs")).href);

const failures = [];
const fail = (m) => failures.push(m);

const base = mkdtempSync(join(tmpdir(), "lf-host-wipe-"));
const dir = join(base, "host");
mkdirSync(dir, { recursive: true });
const sp = join(dir, "profile", "profile", ".save", "u", "game", "M", "playthrough000", "savepoint000");
mkdirSync(sp, { recursive: true });
writeFileSync(join(sp, "meta-info.json"), "{}");
writeFileSync(join(sp, "payload.bin"), "state");
const root = saveRoot(hostPaths(dir).profile);
const NOW = new Date(2026, 10, 15, 12, 30, 45);

function tree(d) {
  return readdirSync(d, { recursive: true }).length;
}

try {
  // 1. No confirm / wrong confirm: refuse, touch nothing.
  for (const confirm of [undefined, "", "wrong", "HOST", "host/"]) {
    const r = wipeHost(dir, { confirm, now: NOW });
    if (r.ok) fail(`confirm ${JSON.stringify(confirm)} was accepted`);
  }
  if (!findSavePoints(hostPaths(dir).profile).length) fail("a refused wipe touched the saves");
  if (existsSync(hostPaths(dir).backups)) fail("a refused wipe wrote a backup");

  // 2. The host runs: refuse even with the right confirm.
  writeHostState(dir, { pid: process.pid });
  const running = wipeHost(dir, { confirm: basename(dir), now: NOW });
  if (running.ok || !/running/i.test(running.reason ?? "")) fail(`a running host was wiped or the reason is missing: ${JSON.stringify(running)}`);
  if (!findSavePoints(hostPaths(dir).profile).length) fail("a wipe on a running host touched the saves");
  writeHostState(dir, { pid: 0 });

  // 3. Right confirm, host stopped.
  const r = wipeHost(dir, { confirm: basename(dir), now: NOW });
  if (!r.ok) fail(`the wipe was refused: ${r.reason}`);
  else {
    if (!r.backup || !findSavePoints(r.backup).length) fail("no wipe backup with a save point");
    if (!/-wipe$/.test(r.backup ?? "")) fail(`backup is not tagged wipe: ${r.backup}`);
    if (!r.aside || !findSavePoints(r.aside).length) fail(`the old save root was not moved aside: ${r.aside}`);
    if (!existsSync(root) || tree(root) !== 0) fail("the save root is not empty after the wipe");
    if (findSavePoints(hostPaths(dir).profile).length) fail("a save point is still under the profile");
    const log = join(hostPaths(dir).backups, WIPES_LOG);
    const rows = existsSync(log) ? readFileSync(log, "utf8").split("\n").filter(Boolean) : [];
    if (rows.length !== 1 || !rows[0].startsWith("2026-11-15") || !rows[0].includes(basename(r.backup))) fail(`wipes.log row missing or wrong: ${JSON.stringify(rows)}`);
    if (!/host restore/.test(r.restore ?? "")) fail(`no restore hint: ${r.restore}`);
  }

  // 4. A second wipe has nothing to wipe: no new backup, no new row.
  const again = wipeHost(dir, { confirm: basename(dir), now: new Date(2026, 10, 15, 12, 31, 0) });
  if (again.ok && again.backup) fail("a wipe with no saves wrote a backup");

  // 5. The CLI subcommand loads and refuses without a confirm.
  const cli = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "host", "wipe"], {
    cwd: ROOT, encoding: "utf8", timeout: 60000, windowsHide: true, env: { ...process.env, LF_HOST_DIR: dir },
  });
  if (cli.status === 0 || !/--confirm/.test(`${cli.stdout}${cli.stderr}`)) fail(`host wipe without --confirm did not refuse with a hint (exit ${cli.status})`);
} finally {
  safeRemoveTree(base);
}

if (failures.length) {
  for (const f of failures) console.log(`FAIL  ${f}`);
  process.exit(1);
}
console.log("host-wipe-selftest ok");
