// tools/validation/host-ready-selftest.mjs
// inputs: tools/host/ready.mjs, tools/host/commands/ready.mjs
//
// Proves `tools\cli host ready` (#1408): the five pre-session checks (build, roster, backup, drill,
// disk) each print one line, then GO or NO-GO. Every check is injected here: all green gives GO and
// exit 0; a failed drill or a stale backup (or any other red check) gives NO-GO naming it and exit 1;
// a check that throws is red, never a crash. The real backup-age and drill-log readers are proven on
// a fixture host dir.
// red-proof: before tools/host/ready.mjs existed the import failed:
// "FAIL ready.mjs missing (Cannot find module ...)" (2026-11-15).

import { mkdirSync, mkdtempSync, rmSync, utimesSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const tmp = mkdtempSync(join(tmpdir(), "lf-host-ready-"));
let failed = 0;
function fail(msg) {
  console.log(`FAIL  ${msg}`);
  failed++;
}

// Runs the command's main with console.log captured; returns { code, out }.
async function run(main, args, ctx) {
  const lines = [];
  const real = console.log;
  console.log = (...a) => lines.push(a.join(" "));
  let code;
  try {
    code = await main(args, ctx);
  } finally {
    console.log = real;
  }
  return { code, out: lines.join("\n") };
}

const green = (name) => ({ name, run: async () => ({ ok: true, detail: `${name} fine` }) });
const red = (name, detail) => ({ name, run: async () => ({ ok: false, detail }) });
const NAMES = ["build", "roster", "backup", "drill", "disk"];
const all = (over = {}) => NAMES.map((n) => over[n] ?? green(n));

try {
  let ready, cmd;
  try {
    ready = await import(pathToFileURL(join(ROOT, "tools", "host", "ready.mjs")).href);
    cmd = await import(pathToFileURL(join(ROOT, "tools", "host", "commands", "ready.mjs")).href);
  } catch (e) {
    fail(`ready.mjs missing (${e.message.split("\n")[0]})`);
    throw e;
  }
  if (typeof cmd.help !== "string" || !cmd.help.startsWith("ready")) fail("commands/ready.mjs must export help starting with 'ready'");

  // 1. all green: five lines, GO, exit 0
  let r = await run(cmd.main, [], { root: ROOT, hostDir: tmp, checks: all() });
  if (r.code !== 0) fail(`all green exited ${r.code}`);
  if (!/^host ready: GO$/m.test(r.out)) fail(`all green did not print GO:\n${r.out}`);
  for (const n of NAMES) if (!new RegExp(`^host ready: ok +${n}\\b`, "m").test(r.out)) fail(`all green: no ok line for ${n}:\n${r.out}`);

  // 2. a failed drill: NO-GO naming it
  r = await run(cmd.main, [], { root: ROOT, hostDir: tmp, checks: all({ drill: red("drill", "the restore drill failed: no applied account record") }) });
  if (r.code !== 1) fail(`a failed drill exited ${r.code}, want 1`);
  if (!/^host ready: NO-GO\b.*\bdrill\b/m.test(r.out)) fail(`a failed drill did not print NO-GO naming drill:\n${r.out}`);
  if (/^host ready: GO$/m.test(r.out)) fail("a failed drill still printed GO");
  if (!/^host ready: FAIL +drill\b/m.test(r.out)) fail(`a failed drill has no FAIL drill line:\n${r.out}`);

  // 3. a stale backup: NO-GO naming it; the other four stay ok
  r = await run(cmd.main, [], { root: ROOT, hostDir: tmp, checks: all({ backup: red("backup", "newest backup is 40 h old") }) });
  if (r.code !== 1) fail(`a stale backup exited ${r.code}, want 1`);
  if (!/^host ready: NO-GO\b.*\bbackup\b/m.test(r.out)) fail(`a stale backup did not print NO-GO naming backup:\n${r.out}`);

  // 4. two reds are both named
  r = await run(cmd.main, [], { root: ROOT, hostDir: tmp, checks: all({ backup: red("backup", "x"), disk: red("disk", "y") }) });
  if (!/^host ready: NO-GO\b.*\bbackup\b.*\bdisk\b/m.test(r.out)) fail(`two reds not both named:\n${r.out}`);

  // 5. a throwing check is red, not a crash
  r = await run(cmd.main, [], { root: ROOT, hostDir: tmp, checks: all({ roster: { name: "roster", run: async () => { throw new Error("boom"); } } }) });
  if (r.code !== 1 || !/NO-GO\b.*\broster\b/.test(r.out) || !r.out.includes("boom")) fail(`a throwing check was not red with its message:\n${r.out}`);

  // 6. the real backup-age reader: no backup, stale, fresh
  const { backupCheck, drillCheck } = ready;
  const hostDir = join(tmp, "host");
  const now = new Date("2026-11-15T12:00:00");
  const stampOf = (d) => {
    const p = (n) => String(n).padStart(2, "0");
    return `${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())}_${p(d.getHours())}-${p(d.getMinutes())}-${p(d.getSeconds())}`;
  };
  let res = await backupCheck({ hostDir, now });
  if (res.ok) fail("no backup passed the backup check");
  const mk = (ageH) => {
    const d = join(hostDir, "backups", `${stampOf(new Date(now.getTime() - ageH * 3600e3))}-session-stop`);
    mkdirSync(join(d, "profile", ".save", "sessions", "s1"), { recursive: true });
    writeFileSync(join(d, "profile", ".save", "sessions", "s1", "meta-info.json"), "{}");
    return d;
  };
  mk(40);
  res = await backupCheck({ hostDir, now });
  if (res.ok || !/40/.test(res.detail)) fail(`a 40 h old backup should be stale, got ${JSON.stringify(res)}`);
  mk(2);
  res = await backupCheck({ hostDir, now });
  if (!res.ok) fail(`a 2 h old newest backup should pass, got ${JSON.stringify(res)}`);
  res = await backupCheck({ hostDir, now, maxBackupHours: 1 });
  if (res.ok) fail("a 2 h old backup passed a 1 h limit");

  // 7. the real drill reader: no log, failing log, passing log, old passing log
  const root = join(tmp, "repo");
  const logs = join(root, "tmp", "logs");
  mkdirSync(logs, { recursive: true });
  res = await drillCheck({ root, now });
  if (res.ok) fail("no drill log passed the drill check");
  const goodLog = [
    "[LFDebug:Persistence] save loaded: version 32",
    "[LFDebug:Persistence] applied 3 account record(s)",
    "[LFDebug:Persistence] save completed (SHUTDOWN)",
  ].join("\n");
  const badLog = ["[LFDebug:Persistence] save loaded: version 32", "[LFDebug:Persistence] save load damaged: 3 payload(s)"].join("\n");
  const setAge = (file, ageH) => {
    const t = new Date(now.getTime() - ageH * 3600e3);
    utimesSync(file, t, t);
  };
  const bad = join(logs, "host-drill-2026-11-15_10-00-00.log");
  writeFileSync(bad, badLog);
  setAge(bad, 2);
  res = await drillCheck({ root, now });
  if (res.ok) fail(`a damaged drill log passed: ${JSON.stringify(res)}`);
  const good = join(logs, "host-drill-2026-11-15_11-00-00.log");
  writeFileSync(good, goodLog);
  setAge(good, 1);
  res = await drillCheck({ root, now });
  if (!res.ok) fail(`a passing, fresh drill log failed: ${JSON.stringify(res)}`);
  setAge(good, 24 * 10);
  res = await drillCheck({ root, now });
  if (res.ok) fail("a 10-day-old drill passed the default 7-day limit");
} catch (e) {
  if (!failed) fail(`selftest crashed: ${e.stack ?? e}`);
} finally {
  rmSync(tmp, { recursive: true, force: true });
}

if (failed) {
  console.log(`host-ready-selftest: ${failed} failure(s)`);
  process.exit(1);
}
console.log("host-ready-selftest: ok");
