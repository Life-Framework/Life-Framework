// tools/validation/host-logs-selftest.mjs
// inputs: tools/host/logs.mjs, tools/host/commands/logs.mjs, tools/lib/log-triage.mjs
//
// Proves `tools\cli host logs archive` moves each finished session's engine log folder
// (profile/logs/logs_<stamp>) to <host>/logs/<stamp>/, never touches the session still being
// written (the newest folder while the host runs or its files changed a moment ago), keeps the
// newest 30 archived sessions, and that `host logs list` prints each with its size and the triage
// error count. Fixture: tools/validation/fixtures/host-logs/ (three log folders) copied to a temp
// host dir, mtimes set per case.
// red-proof: before tools/host/logs.mjs existed the import failed:
// "FAIL logs.mjs missing (Cannot find module ...)" (2026-11-15).

import { cpSync, existsSync, mkdirSync, mkdtempSync, readdirSync, readFileSync, rmSync, utimesSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FIXTURE = join(ROOT, "tools", "validation", "fixtures", "host-logs");
const tmp = mkdtempSync(join(tmpdir(), "lf-host-logs-"));
let failed = 0;
function fail(msg) {
  console.log(`FAIL  ${msg}`);
  failed++;
}

const OLD1 = "logs_2026-11-13_09-00-00";
const OLD2 = "logs_2026-11-14_09-00-00";
const LIVE = "logs_2026-11-15_09-00-00";

// A fresh host dir holding the fixture profile. Old folders' files are two days old; the newest
// is `liveAgeMs` old (default: just written).
function freshHost(name, liveAgeMs = 0) {
  const host = join(tmp, name);
  cpSync(join(FIXTURE, "profile"), join(host, "profile"), { recursive: true });
  const now = Date.now();
  const touch = (folder, ageMs) => {
    const dir = join(host, "profile", "logs", folder);
    const t = new Date(now - ageMs);
    for (const f of readdirSync(dir)) utimesSync(join(dir, f), t, t);
    utimesSync(dir, t, t);
  };
  touch(OLD1, 2 * 86400000);
  touch(OLD2, 2 * 86400000);
  touch(LIVE, liveAgeMs);
  return host;
}

function names(dir) {
  return existsSync(dir) ? readdirSync(dir).sort() : [];
}

async function captured(fn) {
  const lines = [];
  const orig = console.log;
  console.log = (...a) => lines.push(a.join(" "));
  try {
    const code = await fn();
    return { code, text: lines.join("\n") };
  } finally {
    console.log = orig;
  }
}

try {
  let logs, cmd;
  try {
    logs = await import(pathToFileURL(join(ROOT, "tools", "host", "logs.mjs")).href);
    cmd = await import(pathToFileURL(join(ROOT, "tools", "host", "commands", "logs.mjs")).href);
  } catch (e) {
    fail(`logs.mjs missing (${e.message})`);
    process.exit(1);
  }

  // stamp of a folder name
  if (logs.sessionStamp(OLD1) !== "2026-11-13_09-00-00") fail(`sessionStamp gave ${logs.sessionStamp(OLD1)}`);
  if (logs.sessionStamp("notes") !== null) fail("an unstamped folder got a session stamp");

  // 1. three folders, the newest just written: plan finds two finished and one live
  const host = freshHost("a");
  const profile = join(host, "profile");
  const archive = join(host, "logs");
  const plan = logs.planArchive(profile, { hostRunning: false });
  if (plan.live !== LIVE) fail(`live folder is ${plan.live}, want ${LIVE}`);
  if (JSON.stringify(plan.finished) !== JSON.stringify([OLD1, OLD2])) fail(`finished is ${JSON.stringify(plan.finished)}`);

  // dry run moves nothing
  const dry = logs.archiveLogs(profile, archive, { hostRunning: false, dryRun: true });
  if (dry.archived.length !== 2) fail(`dry run reported ${dry.archived.length}, want 2`);
  if (names(join(profile, "logs")).length !== 3 || names(archive).length) fail("dry run moved something");

  // real run: two archived, the live one untouched
  const live1 = readFileSync(join(profile, "logs", LIVE, "console.log"), "utf8");
  const res = logs.archiveLogs(profile, archive, { hostRunning: false });
  if (res.archived.length !== 2) fail(`archived ${res.archived.length}, want 2`);
  if (JSON.stringify(names(archive)) !== JSON.stringify(["2026-11-13_09-00-00", "2026-11-14_09-00-00"])) fail(`archive holds ${JSON.stringify(names(archive))}`);
  if (JSON.stringify(names(join(profile, "logs"))) !== JSON.stringify([LIVE])) fail(`profile logs hold ${JSON.stringify(names(join(profile, "logs")))}, want only the live one`);
  if (readFileSync(join(profile, "logs", LIVE, "console.log"), "utf8") !== live1) fail("live console.log changed");
  if (!existsSync(join(archive, "2026-11-13_09-00-00", "error.log"))) fail("a second file of the session was not archived with it");
  if (!readFileSync(join(archive, "2026-11-13_09-00-00", "console.log"), "utf8").includes("save point FAILED id=12")) fail("archived console.log lost its content");

  // idempotent
  const again = logs.archiveLogs(profile, archive, { hostRunning: false });
  if (again.archived.length !== 0) fail("a second run archived again");

  // 2. the newest folder is live while the host runs, even if quiet; finished once the host is down and quiet
  const quiet = freshHost("b", 3600000);
  const keepLive = logs.archiveLogs(join(quiet, "profile"), join(quiet, "logs"), { hostRunning: true });
  if (keepLive.archived.length !== 2 || !existsSync(join(quiet, "profile", "logs", LIVE))) fail("a running host's newest folder was archived");
  const done = logs.archiveLogs(join(quiet, "profile"), join(quiet, "logs"), { hostRunning: false });
  if (done.archived.length !== 1 || existsSync(join(quiet, "profile", "logs", LIVE))) fail("a quiet folder of a stopped host was not archived");
  // a stopped host whose newest folder changed seconds ago is still treated as open
  const fresh = freshHost("c", 5000);
  if (logs.archiveLogs(join(fresh, "profile"), join(fresh, "logs"), { hostRunning: false }).archived.length !== 2) fail("a just-written folder was archived");

  // 3. retention: newest 30 archived sessions stay; unstamped folders are never touched
  const many = join(tmp, "d", "logs");
  const p2 = (n) => String(n).padStart(2, "0");
  for (let i = 0; i < 35; i++) {
    const dir = join(many, `2026-10-${p2(1 + i % 28)}_${p2(i % 24)}-00-00`);
    mkdirSync(dir, { recursive: true });
    writeFileSync(join(dir, "console.log"), "x");
  }
  mkdirSync(join(many, "notes"), { recursive: true });
  const before = names(many);
  const dryKeep = logs.pruneArchive(many, 30, { dryRun: true });
  if (dryKeep.deleted.length !== before.filter((n) => /^\d{4}-/.test(n)).length - 30) fail(`dry retention would delete ${dryKeep.deleted.length}`);
  if (names(many).length !== before.length) fail("dry retention deleted something");
  const kept = logs.pruneArchive(many, 30);
  const stamped = names(many).filter((n) => /^\d{4}-/.test(n));
  if (stamped.length !== 30) fail(`${stamped.length} sessions left, want 30`);
  if (!names(many).includes("notes")) fail("an unstamped folder was deleted");
  const newest30 = before.filter((n) => /^\d{4}-/.test(n)).sort().slice(-30);
  if (JSON.stringify(stamped) !== JSON.stringify(newest30)) fail("retention did not keep the newest 30");
  if (kept.deleted.length !== 5) fail(`retention deleted ${kept.deleted.length}, want 5`);
  for (const bad of [-1, 0, 1.5, NaN]) {
    let threw = false;
    try { logs.pruneArchive(many, bad); } catch { threw = true; }
    if (!threw) fail(`keep ${bad} accepted`);
  }

  // 4. list: size and triage error count per archived session, newest first
  const listed = logs.listArchive(archive, join(ROOT, "docs", "debug-log-triage.md"));
  if (listed.length !== 2) fail(`list has ${listed.length} sessions, want 2`);
  else {
    if (listed[0].name !== "2026-11-14_09-00-00" || listed[1].name !== "2026-11-13_09-00-00") fail(`list order ${listed.map((s) => s.name).join(",")}`);
    if (listed[1].errors !== 3) fail(`2026-11-13 has ${listed[1].errors} triage lines, want 3`);
    if (listed[0].errors !== 0) fail(`2026-11-14 has ${listed[0].errors} triage lines, want 0`);
    const want = readFileSync(join(archive, "2026-11-13_09-00-00", "console.log")).length + readFileSync(join(archive, "2026-11-13_09-00-00", "error.log")).length;
    if (listed[1].bytes !== want) fail(`size ${listed[1].bytes}, want ${want}`);
  }

  // 5. the command: archive then list, host dir from ctx, a bad --keep rejected
  if (!cmd.help || typeof cmd.main !== "function") fail("commands/logs.mjs lacks help/main");
  const host2 = freshHost("e");
  const ctx = { root: ROOT, hostDir: host2, hostRunning: false };
  const a = await captured(() => cmd.main(["archive"], ctx));
  if (a.code !== 0 || !/archived 2/.test(a.text)) fail(`logs archive: exit ${a.code}, output: ${a.text}`);
  const l = await captured(() => cmd.main(["list"], ctx));
  if (l.code !== 0 || !/2026-11-13_09-00-00/.test(l.text) || !/errors=3/.test(l.text)) fail(`logs list: exit ${l.code}, output: ${l.text}`);
  if ((await captured(() => cmd.main(["archive", "--keep", "x"], ctx))).code === 0) fail("archive --keep x returned 0");
  if ((await captured(() => cmd.main(["bogus"], ctx))).code === 0) fail("an unknown logs subcommand returned 0");
  if ((await captured(() => cmd.main([], ctx))).code === 0) fail("logs with no subcommand returned 0");
} finally {
  rmSync(tmp, { recursive: true, force: true });
}

console.log(failed ? `SUMMARY host-logs-selftest fail ${failed}` : "SUMMARY host-logs-selftest ok");
process.exit(failed ? 1 : 0);
