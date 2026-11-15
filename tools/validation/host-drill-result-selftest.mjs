// tools/validation/host-drill-result-selftest.mjs
// inputs: tools/host/drill.mjs, tools/host/ready.mjs, tools/host/commands/drill.mjs
//
// Proves the drill result file (#1558): `host drill` writes <host dir>/drill-last.json
// ({ ok, backup, machine, accounts, reason, at }) and `host ready` reads it first, so a drill that ran on a
// pool machine (whose log never reaches this checkout) is not "no drill on record". On a fixture host dir:
// a passing result 2 days old is GO; a failing one is NO-GO naming its reason; one 8 days old is stale; no
// file and no log is "no drill on record"; the log reader still answers when there is no file; the SUMMARY
// line a pool machine prints round-trips into the file.
// red-proof: before ready.mjs read drill-last.json the 2-day-old passing result failed with
// "a passing 2-day-old drill-last.json was not GO: no restore drill on record" (2026-11-15).

import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, utimesSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const tmp = mkdtempSync(join(tmpdir(), "lf-host-drill-result-"));
let failed = 0;
function fail(msg) {
  console.log(`FAIL  ${msg}`);
  failed++;
}

try {
  const load = (p) => import(pathToFileURL(join(ROOT, p)).href);
  const drill = await load("tools/host/drill.mjs");
  const ready = await load("tools/host/ready.mjs");
  const now = new Date("2026-11-15T12:00:00Z");
  const ago = (days) => new Date(now.getTime() - days * 86400e3).toISOString();
  const root = join(tmp, "repo"); // no tmp/logs here: only the result file can answer
  mkdirSync(root, { recursive: true });
  let n = 0;
  const freshHost = () => join(tmp, `host-${n++}`);
  const put = (hostDir, result) => {
    mkdirSync(hostDir, { recursive: true });
    writeFileSync(join(hostDir, "drill-last.json"), JSON.stringify(result));
  };
  const pass = (days) => ({ ok: true, backup: "2026-11-14_20-00-00-session-stop", machine: "lf-testbox", accounts: 4, reason: "save loaded", at: ago(days) });

  // 1. a passing result 2 days old: GO
  let host = freshHost();
  put(host, pass(2));
  let res = await ready.drillCheck({ root, hostDir: host, now });
  if (!res.ok) fail(`a passing 2-day-old drill-last.json was not GO: ${res.detail}`);
  else if (!/4 account/.test(res.detail)) fail(`the GO detail does not name the accounts: ${res.detail}`);

  // 2. a failing result: NO-GO naming its reason
  host = freshHost();
  put(host, { ok: false, backup: "b", machine: "lf-testbox", accounts: 0, reason: "save load damaged: 3 payload(s)", at: ago(1) });
  res = await ready.drillCheck({ root, hostDir: host, now });
  if (res.ok) fail("a failing drill-last.json passed");
  else if (!res.detail.includes("save load damaged: 3 payload(s)")) fail(`the NO-GO does not name the reason: ${res.detail}`);

  // 3. 8 days old: stale; the limit is the caller's
  host = freshHost();
  put(host, pass(8));
  res = await ready.drillCheck({ root, hostDir: host, now });
  if (res.ok) fail("an 8-day-old drill passed the default 7-day limit");
  else if (!/8 days old/.test(res.detail)) fail(`the stale detail does not say how old: ${res.detail}`);
  res = await ready.drillCheck({ root, hostDir: host, now, maxDrillDays: 10 });
  if (!res.ok) fail(`an 8-day-old drill failed a 10-day limit: ${res.detail}`);

  // 4. no file and no log: no drill on record. An unreadable file is the same, never a crash.
  host = freshHost();
  mkdirSync(host, { recursive: true });
  res = await ready.drillCheck({ root, hostDir: host, now });
  if (res.ok || !/no restore drill on record/.test(res.detail)) fail(`no file and no log was not "no drill on record": ${JSON.stringify(res)}`);
  writeFileSync(join(host, "drill-last.json"), "{ not json");
  res = await ready.drillCheck({ root, hostDir: host, now });
  if (res.ok || !/no restore drill on record/.test(res.detail)) fail(`a corrupt drill-last.json was not "no drill on record": ${JSON.stringify(res)}`);
  writeFileSync(join(host, "drill-last.json"), JSON.stringify({ ok: true }));
  res = await ready.drillCheck({ root, hostDir: host, now });
  if (res.ok) fail("a drill-last.json without a time passed");

  // 5. the log reader stays as the fallback when the host dir holds no file
  const logRoot = join(tmp, "logrepo");
  const logs = join(logRoot, "tmp", "logs");
  mkdirSync(logs, { recursive: true });
  const log = join(logs, "host-drill-2026-11-15_11-00-00.log");
  writeFileSync(log, ["[LFDebug:Persistence] save loaded: version 32", "[LFDebug:Persistence] applied 3 account record(s)", "[LFDebug:Persistence] save completed (SHUTDOWN)"].join("\n"));
  const t = new Date(now.getTime() - 3600e3);
  utimesSync(log, t, t);
  res = await ready.drillCheck({ root: logRoot, hostDir: freshHost(), now });
  if (!res.ok) fail(`the log fallback no longer answers: ${res.detail}`);
  // ...and the file wins over a passing log
  host = freshHost();
  put(host, { ok: false, backup: "b", machine: "lf-testbox", accounts: 0, reason: "the server did not start", at: ago(0.5) });
  res = await ready.drillCheck({ root: logRoot, hostDir: host, now });
  if (res.ok || !res.detail.includes("the server did not start")) fail(`a failing file did not beat a passing log: ${JSON.stringify(res)}`);

  // 6. the writer and the pool machine's SUMMARY line
  host = freshHost();
  const written = drill.writeDrillResult(host, { ok: true, backup: "b1", machine: "lf-testbox", accounts: 2, reason: "save loaded", at: now.toISOString() });
  const file = join(host, "drill-last.json");
  if (!existsSync(file)) fail("writeDrillResult wrote no drill-last.json");
  else {
    const j = JSON.parse(readFileSync(file, "utf8"));
    for (const k of ["ok", "backup", "machine", "accounts", "reason", "at"]) if (!(k in j)) fail(`drill-last.json lacks ${k}`);
    if (j.ok !== true || j.machine !== "lf-testbox" || j.accounts !== 2) fail(`drill-last.json holds the wrong values: ${JSON.stringify(j)}`);
  }
  if (written !== file) fail(`writeDrillResult returned ${written}, want ${file}`);
  res = await ready.drillCheck({ root, hostDir: host, now });
  if (!res.ok) fail(`a freshly written passing result was not GO: ${res.detail}`);

  const okLine = drill.summaryLine({ ok: true, backup: "b1", machine: "lf-testbox", accounts: 3, log: "C:/x/console.log" });
  let p = drill.parseSummaryLine(`remote noise\n${okLine}\nmore noise`);
  if (!p || p.ok !== true || p.backup !== "b1" || p.machine !== "lf-testbox" || p.accounts !== 3) fail(`the passing SUMMARY line did not parse: ${JSON.stringify(p)}`);
  const badLine = drill.summaryLine({ ok: false, backup: "b1", machine: "lf-testbox", reason: 'the "server" did not start: boom' });
  p = drill.parseSummaryLine(badLine);
  if (!p || p.ok !== false || p.machine !== "lf-testbox" || !/did not start: boom/.test(p.reason)) fail(`the failing SUMMARY line did not parse: ${JSON.stringify(p)}`);
  if (drill.parseSummaryLine("SUMMARY test ok") !== null || drill.parseSummaryLine("") !== null) fail("a line that is not a host-drill SUMMARY parsed");
} catch (e) {
  if (!failed) fail(`selftest crashed: ${e.stack ?? e}`);
} finally {
  rmSync(tmp, { recursive: true, force: true });
}

if (failed) {
  console.log(`host-drill-result-selftest: ${failed} failure(s)`);
  process.exit(1);
}
console.log("host-drill-result-selftest: ok");
