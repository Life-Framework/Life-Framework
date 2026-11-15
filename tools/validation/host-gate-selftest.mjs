// tools/validation/host-gate-selftest.mjs
// inputs: tools/host/**, tools/commands/staff.mjs
//
// Proves `tools\cli host gate` (#1410), the #555 opening gate (docs/community/opening-gate.md):
// injected history, drill result, issue list and roster give "open" when all four lines pass, and
// "closed" on one open regression, and each line fails on its own cause: too few sessions at the
// player target, a session under the target breaking the run, a restore after a session, a drill
// that failed or never ran, a #pending or missing administrator uid. The CLI runs end to end on a
// fixture host dir with --issues-file and --roster.
// red-proof: before tools/host/gate.mjs existed this file died on the import:
// "Cannot find module '.../tools/host/gate.mjs'" (2026-11-15).

import { mkdirSync, mkdtempSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { spawnSync } from "node:child_process";
import { safeRemoveTree } from "../link-safety.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const { evaluateGate, gateText, gateSummary } = await import(pathToFileURL(join(ROOT, "tools", "host", "gate.mjs")).href);

const failures = [];
const fail = (m) => failures.push(m);
const check = (name, got, want) => { if (got !== want) fail(`${name}: ${got} != ${want}`); };

const session = (day, peak, ended = "stop", backups = []) => ({
  start: `2026-11-${day} 18:00:00`, end: `2026-11-${day} 20:00:00`, minutes: 120, ended, crashes: 0, backups, wipes: [], peakPlayers: peak,
});
const history = (peaks) => ({ sessions: peaks.map((p, i) => session(String(10 + i), p)) });
const drillOk = { ok: true, backup: "2026-11-12_20-00-00-stop", machine: "lf-testbox", accounts: 3, reason: "", at: "2026-11-13T10:00:00Z" };
const roster = "# staff\nabc123 administrator # Jasper\n";
const base = { history: history([41, 44, 40]), drill: drillOk, issues: [], roster };
const lines = (g) => Object.fromEntries(g.lines.map((l) => [l.id, l.ok]));

// 1. All four lines pass: open.
{
  const g = evaluateGate(base);
  check("all pass: open", g.open, true);
  check("all pass: four lines", g.lines.length, 4);
  if (g.lines.some((l) => !l.ok)) fail(`all pass: a line failed: ${JSON.stringify(g.lines)}`);
  if (!/^SUMMARY host-gate open /.test(gateSummary(g))) fail(`all pass: summary ${gateSummary(g)}`);
}

// 2. One open regression closes it, and only that line fails.
{
  const g = evaluateGate({ ...base, issues: [{ number: 1500, title: "Players lose bank balance" }] });
  check("regression: closed", g.open, false);
  const l = lines(g);
  check("regression: regression line", l.regression, false);
  check("regression: sessions line", l.sessions, true);
  check("regression: save line", l.save, true);
  check("regression: staff line", l.staff, true);
  if (!/#1500/.test(gateText(g))) fail("regression: the text does not name #1500");
  if (!/^SUMMARY host-gate closed /.test(gateSummary(g))) fail(`regression: summary ${gateSummary(g)}`);
}

// 3. Sessions in a row at the target.
{
  check("two of three sessions: closed", lines(evaluateGate({ ...base, history: history([41, 44]) })).sessions, false);
  check("target 40 is met by 40", lines(evaluateGate({ ...base, history: history([40, 40, 40]) })).sessions, true);
  // The last three count: an old low session does not matter, a low one among the latest does.
  check("old low session ignored", lines(evaluateGate({ ...base, history: history([12, 41, 44, 40]) })).sessions, true);
  check("low session among the last three", lines(evaluateGate({ ...base, history: history([41, 44, 39, 42]) })).sessions, false);
  check("no player column (null peak) breaks the run", lines(evaluateGate({ ...base, history: history([41, null, 44, 40]) })).sessions, false);
  check("--target 20", lines(evaluateGate({ ...base, history: history([22, 25, 21]), target: 20 })).sessions, true);
  check("--sessions 2", lines(evaluateGate({ ...base, history: history([41, 44]), sessions: 2 })).sessions, true);
  // A session still running does not count and does not break the run.
  const open = { sessions: [...history([41, 44, 40]).sessions, { ...session("20", 12), end: null, minutes: null, ended: "open" }] };
  check("running session skipped", lines(evaluateGate({ ...base, history: open })).sessions, true);
  check("no history", lines(evaluateGate({ ...base, history: { sessions: [] } })).sessions, false);
}

// 4. No save loss: a drill that passed and no restore after a session.
{
  check("drill failed", lines(evaluateGate({ ...base, drill: { ...drillOk, ok: false, reason: "no accounts" } })).save, false);
  check("drill never ran", lines(evaluateGate({ ...base, drill: null })).save, false);
  const restored = { sessions: history([41, 44, 40]).sessions.map((s, i) => (i === 1 ? { ...s, backups: ["2026-11-11_21-00-00-replaced-by-restore"] } : s)) };
  const g = evaluateGate({ ...base, history: restored });
  check("restore after a session", lines(g).save, false);
  if (!/replaced-by-restore/.test(gateText(g))) fail("restore: the text does not name the restore backup");
  // A restore before the first session (a failed preflight) is not a restore after a session.
  const before = { sessions: history([41, 44, 40]).sessions.map((s, i) => (i === 0 ? { ...s, backups: ["2026-11-09_12-00-00-replaced-by-restore"] } : s)) };
  check("restore before the first session", lines(evaluateGate({ ...base, history: before })).save, true);
}

// 5. The roster's administrator uid.
{
  check("pending administrator", lines(evaluateGate({ ...base, roster: "#pending administrator Jasper\n" })).staff, false);
  check("only a moderator", lines(evaluateGate({ ...base, roster: "abc123 moderator # Sam\n" })).staff, false);
  check("no roster", lines(evaluateGate({ ...base, roster: null })).staff, false);
  check("administrator plus a pending moderator", lines(evaluateGate({ ...base, roster: `${roster}#pending moderator Sam\n` })).staff, true);
  check("issue list unavailable fails the regression line", lines(evaluateGate({ ...base, issues: null })).regression, false);
}

// 6. The CLI end to end on a fixture host dir.
const tmp = mkdtempSync(join(tmpdir(), "lf-host-gate-"));
try {
  const dir = join(tmp, "host");
  const backups = join(dir, "backups");
  mkdirSync(backups, { recursive: true });
  mkdirSync(join(dir, "perf"), { recursive: true });
  const rows = ["time,cpu_pct,ws_mb,up_mbit,players"];
  for (const day of ["10", "11", "12"]) {
    mkdirSync(join(backups, `2026-11-${day}_18-00-00-start`));
    mkdirSync(join(backups, `2026-11-${day}_20-00-00-stop`));
    rows.push(`${new Date(2026, 10, Number(day), 19, 0, 0).toISOString()},10.0,2000,1.00,42`);
  }
  writeFileSync(join(dir, "perf", "perf-fixture.csv"), `${rows.join("\n")}\n`);
  writeFileSync(join(dir, "drill-last.json"), `${JSON.stringify(drillOk)}\n`);
  const rosterFile = join(tmp, "staff.txt");
  writeFileSync(rosterFile, roster);
  const noIssues = join(tmp, "issues-none.json");
  writeFileSync(noIssues, "[]");
  const oneIssue = join(tmp, "issues-one.json");
  writeFileSync(oneIssue, JSON.stringify([{ number: 1500, title: "regression" }]));

  const run = (issues, extra = []) => spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "host", "gate", "--roster", rosterFile, "--issues-file", issues, ...extra], {
    cwd: ROOT, encoding: "utf8", timeout: 60000, windowsHide: true, env: { ...process.env, LF_HOST_DIR: dir },
  });
  const open = run(noIssues);
  check("cli open: exit", open.status, 0);
  if (!/SUMMARY host-gate open /.test(open.stdout)) fail(`cli open: no open SUMMARY: ${open.stdout}${open.stderr}`);
  const closed = run(oneIssue);
  check("cli closed: exit", closed.status, 1);
  if (!/SUMMARY host-gate closed /.test(closed.stdout)) fail(`cli closed: no closed SUMMARY: ${closed.stdout}${closed.stderr}`);
  const json = run(noIssues, ["--json"]);
  let parsed = null;
  try { parsed = JSON.parse(json.stdout); } catch { /* reported below */ }
  if (!parsed || parsed.open !== true || parsed.lines?.length !== 4) fail(`cli --json: ${json.stdout}${json.stderr}`);
} finally {
  safeRemoveTree(tmp);
}

if (failures.length) {
  for (const f of failures) console.error(`FAIL host-gate: ${f}`);
  process.exit(1);
}
console.log("OK host-gate-selftest");
