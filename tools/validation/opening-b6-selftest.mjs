// tools/validation/opening-b6-selftest.mjs
// inputs: tools/playtest/opening/**, tools/host/perf.mjs, tools/validation/fixtures/opening-b6/**, server/flagship/config.json
//
// Proves beat script B6 of docs/playtest/m4-opening-day.md (#1602, the desk half of the 2-hour day):
// `node tools/playtest/opening/b6-day.mjs --host <dir>` prints one `BEAT B6 <id> PASS|FAIL|BLOCKED` line per check.
//   1. the committed flagship config (accelerated, 120-minute day) and a perf CSV with samples: day-config and
//      perf-samples PASS, and the run exits 0;
//   2. a 60-minute day, a realtime day and a missing config FAIL day-config, exit 1;
//   3. a perf CSV with a header and no rows FAILs perf-samples, and so does a CSV with a bad number and a CSV without the
//      cpu/memory/upload columns; a host with no perf csv at all is BLOCKED (nothing has sampled it yet, #1693);
//   4. clock-rate and late-join print BLOCKED naming #1301 and #1303 while the day-rate proof verb is not on main,
//      and PASS naming `day-rate` once a proofs folder has it;
//   5. no --host is a usage error (exit 2, no BEAT line); the script writes nothing under the host folder.
//
// red-proof: 2026-11-15, run before tools/playtest/opening/b6-day.mjs existed it printed
// "FAIL tools/playtest/opening/b6-day.mjs is missing" and exited 1.
import { cpSync, existsSync, mkdirSync, mkdtempSync, readdirSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";
import { safeRemoveTree } from "../link-safety.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const SCRIPT = join(ROOT, "tools", "playtest", "opening", "b6-day.mjs");
const FIX = join(ROOT, "tools", "validation", "fixtures", "opening-b6");

let failed = 0;
const fail = (m) => {
  console.log(`FAIL  ${m}`);
  failed++;
};
const expect = (ok, m) => {
  if (!ok) fail(m);
};

if (!existsSync(SCRIPT)) {
  fail("tools/playtest/opening/b6-day.mjs is missing");
  console.log(`opening-b6-selftest: ${failed} case(s) failed`);
  process.exit(1);
}

const run = (args) => {
  const env = { ...process.env };
  delete env.LF_HOST_DIR;
  const r = spawnSync(process.execPath, [SCRIPT, ...args], { cwd: ROOT, encoding: "utf8", timeout: 60000, windowsHide: true, env });
  const out = `${r.stdout ?? ""}${r.stderr ?? ""}`;
  const beats = new Map();
  for (const m of out.matchAll(/^BEAT B6 (\S+) (PASS|FAIL|BLOCKED)\b(.*)$/gm)) beats.set(m[1], { status: m[2], detail: m[3].trim() });
  return { status: r.status, out, beats };
};
const state = (r, id) => r.beats.get(id)?.status;
const list = (dir) => (existsSync(dir) ? readdirSync(dir, { recursive: true }).sort().join("|") : "");

const scratch = mkdtempSync(join(tmpdir(), "lf-b6-"));
try {
  const good = join(FIX, "host-good");
  const withVerb = join(FIX, "proofs-with");
  const withoutVerb = join(FIX, "proofs-without");

  // 1. The committed flagship config and good samples.
  const ok = run(["--host", good, "--proofs", withoutVerb]);
  expect(ok.status === 0, `the committed config with good samples exited ${ok.status}:\n${ok.out}`);
  expect(state(ok, "day-config") === "PASS", `day-config on the committed config must PASS, got ${state(ok, "day-config")}`);
  expect(state(ok, "perf-samples") === "PASS", `perf-samples on good samples must PASS, got ${state(ok, "perf-samples")}`);
  expect(/3 samples/.test(ok.beats.get("perf-samples")?.detail ?? ""), `perf-samples must count the 3 samples: ${ok.beats.get("perf-samples")?.detail}`);

  // 2. A day that is not 2 hours accelerated.
  for (const name of ["config-60min.json", "config-realtime.json", "config-missing.json"]) {
    const r = run(["--host", good, "--proofs", withoutVerb, "--config", join(FIX, name)]);
    expect(r.status === 1 && state(r, "day-config") === "FAIL", `${name}: day-config must FAIL and exit 1, got ${state(r, "day-config")} exit ${r.status}`);
    expect(state(r, "perf-samples") === "PASS", `${name}: the perf line is judged on its own, got ${state(r, "perf-samples")}`);
  }
  const sixty = run(["--host", good, "--proofs", withoutVerb, "--config", join(FIX, "config-60min.json")]);
  expect(/60/.test(sixty.beats.get("day-config")?.detail ?? ""), `the 60-minute failure must name the 60: ${sixty.beats.get("day-config")?.detail}`);

  // 3. Perf samples missing or broken.
  const badRuns = {
    "a header with no rows": join(FIX, "host-empty"),
  };
  const mk = (name, text) => {
    const h = join(scratch, name);
    mkdirSync(join(h, "perf"), { recursive: true });
    writeFileSync(join(h, "perf", "perf-2026-11-15T18-01-30-000Z.csv"), text);
    return h;
  };
  badRuns["a CSV with a non-number"] = mk("nan", "time,cpu_pct,ws_mb,up_mbit,players\n2026-11-15T18:02:00.000Z,abc,2100,6.00,\n");
  badRuns["a CSV without the upload column"] = mk("noup", "time,cpu_pct,ws_mb\n2026-11-15T18:02:00.000Z,50.0,2100\n");
  badRuns["a CSV with a short row"] = mk("short", "time,cpu_pct,ws_mb,up_mbit,players\n2026-11-15T18:02:00.000Z,50.0\n");
  for (const [what, host] of Object.entries(badRuns)) {
    const r = run(["--host", host, "--proofs", withoutVerb]);
    expect(r.status === 1 && state(r, "perf-samples") === "FAIL", `${what}: perf-samples must FAIL and exit 1, got ${state(r, "perf-samples")} exit ${r.status}`);
    expect(state(r, "day-config") === "PASS", `${what}: the config line is judged on its own, got ${state(r, "day-config")}`);
  }
  // No perf csv at all: BLOCKED, not FAIL (a fresh host has sampled nothing), and the run still exits 0 (#1693).
  const noPerf = run(["--host", join(FIX, "host-none"), "--proofs", withoutVerb]);
  expect(state(noPerf, "perf-samples") === "BLOCKED" && noPerf.status === 0, `a host with no perf folder: perf-samples must be BLOCKED and exit 0, got ${state(noPerf, "perf-samples")} exit ${noPerf.status}`);
  // The newest perf file is the one judged: an old good file under an empty newer one still FAILs.
  const two = join(scratch, "two");
  cpSync(join(good, "perf"), join(two, "perf"), { recursive: true });
  writeFileSync(join(two, "perf", "perf-2026-11-16T09-00-00-000Z.csv"), "time,cpu_pct,ws_mb,up_mbit,players\n");
  expect(state(run(["--host", two, "--proofs", withoutVerb]), "perf-samples") === "FAIL", "the newest perf file (empty) must be the one judged");

  // 4. The game-only lines.
  for (const id of ["clock-rate", "late-join"]) {
    const d = ok.beats.get(id);
    expect(d?.status === "BLOCKED" && /#1301/.test(d.detail) && /#1303/.test(d.detail), `${id} must be BLOCKED naming #1301 and #1303 with no day-rate verb: ${JSON.stringify(d)}`);
  }
  expect(ok.status === 0, "BLOCKED lines must not fail the run");
  const open = run(["--host", good, "--proofs", withVerb]);
  for (const id of ["clock-rate", "late-join"]) {
    const d = open.beats.get(id);
    expect(d?.status === "PASS" && /day-rate/.test(d.detail), `${id} must PASS naming the day-rate verb once it is on main: ${JSON.stringify(d)}`);
  }
  expect(/proof day-rate/.test(open.out) && /opening --game|harness\.mjs proof day-rate/.test(open.out), "the run must name the day-rate verb's command");
  expect(/^SUMMARY b6-day ok pass=4 blocked=0 fail=0/m.test(open.out), `SUMMARY with the verb on main: ${open.out.split("\n").filter((l) => /SUMMARY/.test(l))}`);
  expect(/^SUMMARY b6-day blocked pass=2 blocked=2 fail=0/m.test(ok.out), `SUMMARY with the verb absent: ${ok.out.split("\n").filter((l) => /SUMMARY/.test(l))}`);

  // 5. Usage, and the host folder is only read.
  const none = run([]);
  expect(none.status === 2 && none.beats.size === 0, `no --host must exit 2 with no BEAT line, got exit ${none.status}, ${none.beats.size} lines`);
  const before = list(good);
  run(["--host", good, "--proofs", withVerb]);
  expect(list(good) === before, "the script changed the host folder");
} finally {
  safeRemoveTree(scratch);
}

if (failed) {
  console.log(`opening-b6-selftest: ${failed} case(s) failed`);
  process.exit(1);
}
console.log("opening-b6-selftest ok");
