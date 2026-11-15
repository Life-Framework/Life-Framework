// tools/validation/host-history-selftest.mjs
// inputs: tools/host/**, tools/validation/fixtures/host-history/**
//
// Proves `tools\cli host history [--json]` (#1409): a fixture host dir with three sessions, one
// crash and one wipe (backup folder stamps, backups/wipes.log, perf/perf-*.csv) gives the expected
// session rows (start, end, minutes, crashes, the backups taken, the wipe, peak players), a session
// with no player column gives null, a perf row outside every session is ignored, and the read
// writes nothing.
// red-proof: before tools/host/history.mjs existed this file died on the import:
// "Cannot find module '.../tools/host/history.mjs'" (2026-11-15).

import { mkdirSync, mkdtempSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { spawnSync } from "node:child_process";
import { safeRemoveTree } from "../link-safety.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const { hostHistory } = await import(pathToFileURL(join(ROOT, "tools", "host", "history.mjs")).href);
const fx = JSON.parse(readFileSync(join(ROOT, "tools", "validation", "fixtures", "host-history", "scenario.json"), "utf8"));

const failures = [];
const fail = (m) => failures.push(m);

// "2026-11-10_18-45-00" (local) -> Date.
function local(s) {
  const [y, mo, d, h, mi, se] = s.match(/\d+/g).map(Number);
  return new Date(y, mo - 1, d, h, mi, se);
}

const base = mkdtempSync(join(tmpdir(), "lf-host-history-"));
const dir = join(base, "host");
const backups = join(dir, "backups");
mkdirSync(backups, { recursive: true });
mkdirSync(join(dir, "perf"), { recursive: true });
for (const name of fx.backups) mkdirSync(join(backups, name), { recursive: true });
writeFileSync(join(backups, "wipes.log"), fx.wipes.map((w) => `${local(w.at).toISOString()} wipe host=host backup=${w.backup} aside=${w.aside}\n`).join(""));
writeFileSync(join(dir, "perf", "perf-fixture.csv"),
  `time,cpu_pct,ws_mb,up_mbit,players\n${fx.perf.map((p) => `${local(p.at).toISOString()},10.0,2000,1.00,${p.players}`).join("\n")}\n`);

const snapshot = () => JSON.stringify(readdirSync(dir, { recursive: true }).sort());

try {
  const before = snapshot();
  const got = hostHistory(dir).sessions;
  if (snapshot() !== before) fail("host history wrote or changed files in the host dir");
  if (got.length !== fx.expected.length) fail(`expected ${fx.expected.length} sessions, got ${got.length}: ${JSON.stringify(got)}`);
  fx.expected.forEach((want, i) => {
    const row = got[i];
    if (!row) return;
    if (JSON.stringify(row) !== JSON.stringify(want)) fail(`session ${i + 1} differs:\n  got  ${JSON.stringify(row)}\n  want ${JSON.stringify(want)}`);
  });

  // The CLI subcommand loads and prints the same rows as JSON.
  const cli = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "host", "history", "--json"], {
    cwd: ROOT, encoding: "utf8", timeout: 60000, windowsHide: true, env: { ...process.env, LF_HOST_DIR: dir },
  });
  let parsed = null;
  try { parsed = JSON.parse(cli.stdout); } catch { /* reported below */ }
  if (cli.status !== 0 || !parsed) fail(`host history --json failed (exit ${cli.status}): ${cli.stdout}${cli.stderr}`);
  else if (JSON.stringify(parsed.sessions) !== JSON.stringify(fx.expected)) fail(`host history --json rows differ: ${cli.stdout}`);
  const text = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "host", "history"], {
    cwd: ROOT, encoding: "utf8", timeout: 60000, windowsHide: true, env: { ...process.env, LF_HOST_DIR: dir },
  });
  if (text.status !== 0 || !/2026-11-11 19:12:00/.test(text.stdout) || !/crash/.test(text.stdout)) fail(`host history text output missing the rows (exit ${text.status}): ${text.stdout}${text.stderr}`);

  // An empty host dir is not an error.
  const empty = join(base, "empty");
  mkdirSync(empty, { recursive: true });
  if (hostHistory(empty).sessions.length !== 0) fail("an empty host dir gave sessions");
} finally {
  safeRemoveTree(base);
}

if (failures.length) {
  for (const f of failures) console.log(`FAIL  ${f}`);
  process.exit(1);
}
console.log("host-history-selftest ok");
