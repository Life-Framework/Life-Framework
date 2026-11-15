// tools/validation/host-wipe-selftest.mjs
// inputs: tools/host/**
//
// Proves `tools\cli host wipe --confirm <host dir name>` (#1274) on a fixture host dir:
// it refuses without the typed confirm, refuses while the host runs, and with the right
// confirm writes a `wipe` backup, moves the save root aside (nothing deleted), leaves the
// save root empty, logs a row to backups/wipes.log and prints how to restore.
// #1413: it also refuses unless `host announce --wipe` recorded a wipe notice (the `next` row of
// website/content/sessions.json) at least 24 hours before now: no notice, a notice that is not a
// wipe, a notice with no time, and one 23 hours old are refused with the time the wipe is allowed;
// 24 and 25 hours old wipe.
// red-proof: before tools/host/wipe.mjs existed this file died on the import:
// "Cannot find module '.../tools/host/wipe.mjs'" (2026-11-15). Before the notice rule (#1413) it died on
// "TypeError: checkWipeNotice is not a function" (2026-11-15); with the rule stubbed to always allow,
// the 23-hour case printed "a notice 23 h old: the wipe was accepted".

import { existsSync, mkdirSync, mkdtempSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { basename, dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { spawnSync } from "node:child_process";
import { safeRemoveTree } from "../link-safety.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const { wipeHost, checkWipeNotice, WIPES_LOG } = await import(pathToFileURL(join(ROOT, "tools", "host", "wipe.mjs")).href);
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

// The sessions.json `next` row `host announce --wipe` writes. `hoursAgo` is when the notice posted.
const sessions = join(base, "sessions.json");
const HOUR = 3600 * 1000;
const at = (hoursAgo) => new Date(NOW.getTime() - hoursAgo * HOUR).toISOString();
function notice(next) {
  writeFileSync(sessions, JSON.stringify({ next }, null, 2) + "\n");
}
const wipeNotice = (hoursAgo) => ({ at: new Date(NOW.getTime() + 48 * HOUR).toISOString(), build: "0.1.0", wipe: true, announced: at(hoursAgo) });
const noSaveLoss = (what) => {
  if (!findSavePoints(hostPaths(dir).profile).length) fail(`${what} touched the saves`);
  if (existsSync(hostPaths(dir).backups)) fail(`${what} wrote a backup`);
};

try {
  // 0. The wipe notice. Nothing is touched while it is missing or younger than 24 hours.
  const CONFIRM = basename(dir);
  const refused = (label, opts, ...needles) => {
    const r = wipeHost(dir, { confirm: CONFIRM, now: NOW, sessions, ...opts });
    if (r.ok) fail(`${label}: the wipe was accepted`);
    else for (const n of needles) if (!n.test(r.reason ?? "")) fail(`${label}: reason lacks ${n}: ${JSON.stringify(r.reason)}`);
    noSaveLoss(label);
    return r;
  };
  refused("no sessions.json", {}, /notice/i);
  notice(null);
  refused("next is null", {}, /notice/i);
  notice({ ...wipeNotice(30), wipe: false });
  refused("a session that is not a wipe", {}, /notice/i);
  notice({ ...wipeNotice(30), announced: false });
  refused("a drafted notice not yet posted", {}, /not (been )?posted|draft/i);
  notice({ ...wipeNotice(30), announced: true });
  refused("a notice with no time", {}, /no time|time/i);
  notice({ ...wipeNotice(30), announced: "not a date" });
  refused("a notice with a garbage time", {}, /time/i);
  notice(wipeNotice(23));
  const young = refused("a notice 23 h old", {}, /notice/i);
  const allowedAt = new Date(NOW.getTime() + 1 * HOUR).toISOString();
  if (young.allowedAt !== allowedAt) fail(`a notice 23 h old: allowedAt ${young.allowedAt} is not ${allowedAt}`);
  if (!(young.reason ?? "").includes(allowedAt)) fail(`a notice 23 h old: the reason does not print the allowed time ${allowedAt}: ${young.reason}`);
  // The field names the opening-day script (B9) ages back also count as the posting time.
  for (const field of ["announcedAt", "noticeAt", "postedAt"]) {
    notice({ at: wipeNotice(0).at, build: "0.1.0", wipe: true, announced: true, [field]: at(23) });
    refused(`${field} 23 h old`, {}, /notice/i);
  }
  // 24 h exactly is enough ("at least 24 hours"); the steps below run with a 25 h notice.
  notice(wipeNotice(24));
  if (!checkWipeNotice(sessions, NOW).ok) fail("a notice exactly 24 h old was refused");
  notice(wipeNotice(25));
  if (!checkWipeNotice(sessions, NOW).ok) fail("a notice 25 h old was refused");

  // 1. No confirm / wrong confirm: refuse, touch nothing.
  for (const confirm of [undefined, "", "wrong", "HOST", "host/"]) {
    const r = wipeHost(dir, { confirm, now: NOW, sessions });
    if (r.ok) fail(`confirm ${JSON.stringify(confirm)} was accepted`);
  }
  if (!findSavePoints(hostPaths(dir).profile).length) fail("a refused wipe touched the saves");
  if (existsSync(hostPaths(dir).backups)) fail("a refused wipe wrote a backup");

  // 2. The host runs: refuse even with the right confirm.
  writeHostState(dir, { pid: process.pid });
  const running = wipeHost(dir, { confirm: basename(dir), now: NOW, sessions });
  if (running.ok || !/running/i.test(running.reason ?? "")) fail(`a running host was wiped or the reason is missing: ${JSON.stringify(running)}`);
  if (!findSavePoints(hostPaths(dir).profile).length) fail("a wipe on a running host touched the saves");
  writeHostState(dir, { pid: 0 });

  // 3. Right confirm, host stopped.
  const r = wipeHost(dir, { confirm: basename(dir), now: NOW, sessions });
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
  const again = wipeHost(dir, { confirm: basename(dir), now: new Date(2026, 10, 15, 12, 31, 0), sessions });
  if (again.ok && again.backup) fail("a wipe with no saves wrote a backup");

  // 5. The CLI subcommand loads and refuses without a confirm.
  const cli = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "host", "wipe"], {
    cwd: ROOT, encoding: "utf8", timeout: 60000, windowsHide: true, env: { ...process.env, LF_HOST_DIR: dir },
  });
  if (cli.status === 0 || !/--confirm/.test(`${cli.stdout}${cli.stderr}`)) fail(`host wipe without --confirm did not refuse with a hint (exit ${cli.status})`);

  // 6. The CLI reads the notice from LF_ANNOUNCE_SESSIONS (as `host announce` writes it) and prints the allowed time.
  // Saves first: step 3 wiped them.
  mkdirSync(sp, { recursive: true });
  writeFileSync(join(sp, "meta-info.json"), "{}");
  const live = Date.now();
  const postedAgo = (h) => new Date(live - h * HOUR).toISOString();
  const cliWipe = (next) => {
    writeFileSync(sessions, JSON.stringify({ next }) + "\n");
    return spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "host", "wipe", "--confirm", basename(dir)], {
      cwd: ROOT, encoding: "utf8", timeout: 60000, windowsHide: true,
      env: { ...process.env, LF_HOST_DIR: dir, LF_ANNOUNCE_SESSIONS: sessions },
    });
  };
  const cliYoung = cliWipe({ at: postedAgo(-48), build: "0.1.0", wipe: true, announced: postedAgo(23) });
  const youngOut = `${cliYoung.stdout}${cliYoung.stderr}`;
  const wantAllowed = new Date(Date.parse(postedAgo(23)) + 24 * HOUR).toISOString();
  if (cliYoung.status === 0 || !youngOut.includes(wantAllowed)) fail(`CLI: a 23 h old notice did not refuse with ${wantAllowed}: exit ${cliYoung.status}, ${youngOut.trim()}`);
  if (!findSavePoints(hostPaths(dir).profile).length) fail("CLI: a refused wipe touched the saves");
  const none = cliWipe(null);
  if (none.status === 0 || !/notice/i.test(`${none.stdout}${none.stderr}`)) fail(`CLI: no notice did not refuse (exit ${none.status})`);
  const old = cliWipe({ at: postedAgo(-48), build: "0.1.0", wipe: true, announced: postedAgo(25) });
  if (old.status !== 0 || !/wiped: backup/.test(old.stdout)) fail(`CLI: a 25 h old notice did not wipe (exit ${old.status}): ${old.stdout}${old.stderr}`);
} finally {
  safeRemoveTree(base);
}

if (failures.length) {
  for (const f of failures) console.log(`FAIL  ${f}`);
  process.exit(1);
}
console.log("host-wipe-selftest ok");
