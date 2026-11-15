// tools/validation/opening-b2-selftest.mjs
// inputs: tools/playtest/opening/b2-start.mjs, tools/validation/fixtures/opening-b2/**, tools/host/ready.mjs, tools/host/host.mjs
//
// Proves beat script B2 of docs/playtest/m4-opening-day.md (the flagship start), `node tools/playtest/opening/b2-start.mjs
// --host <scratch dir>`, on fixture hosts and a stand-in cli, with no game:
//   1. a good start (`host: up`, a console log with `save loaded:` and `Entered online game state.`, the exact server name, a
//      join password, a watch that adopts the host) gives PASS for start, save-loaded, server-name, password and watch, BLOCKED
//      (#1520) for the two join checks, seven `BEAT B2` lines in the sheet's order, exit 0;
//   2. a wrong server name and an empty password each give FAIL for their own check only, exit 1; the password never reaches the output;
//   3. a start with no `host: up` line, and a failing start, give FAIL for start (and for watch, which has no host to adopt);
//   4. a console log without `save loaded:` gives FAIL for save-loaded; no log at all too;
//   5. a watch that exits at once gives FAIL for watch;
//   6. the real host folder and a missing --host are refused (exit 2, no BEAT line).
// red-proof: before tools/playtest/opening/b2-start.mjs existed every case printed "FAIL ... no BEAT line" (2026-11-15); with the
// password branch of b2-start.mjs narrowed to `if (whole)`, "case 2: an empty password gives password FAIL" failed. Restored.

import { cpSync, mkdirSync, mkdtempSync, readFileSync, rmSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FIX = join(ROOT, "tools", "validation", "fixtures", "opening-b2");
const SCRIPT = join(ROOT, "tools", "playtest", "opening", "b2-start.mjs");
const FAKE_CLI = join(FIX, "fake-cli.mjs");
const scratch = mkdtempSync(join(tmpdir(), "lf-b2-"));
const ORDER = "start,save-loaded,server-name,password,join-wrong-password,join-right-password,watch";

let failed = 0;
function check(ok, label) {
  if (!ok) {
    failed++;
    console.log(`FAIL ${label}`);
  }
}

// A scratch host: profile/server.json and, unless `log` is null, the newest console log.
let n = 0;
function makeHost(serverJson, log = "console-good.log") {
  const dir = join(scratch, `host-${++n}`);
  mkdirSync(join(dir, "profile"), { recursive: true });
  if (serverJson) cpSync(join(FIX, serverJson), join(dir, "profile", "server.json"));
  if (log) {
    mkdirSync(join(dir, "profile", "logs", "logs_2026-11-15_12-00-00"), { recursive: true });
    cpSync(join(FIX, log), join(dir, "profile", "logs", "logs_2026-11-15_12-00-00", "console.log"));
  }
  return dir;
}

function run(args, env = {}) {
  const r = spawnSync(process.execPath, [SCRIPT, ...args], {
    cwd: ROOT,
    encoding: "utf8",
    timeout: 120000,
    env: { ...process.env, LF_HOST_DIR: "", ...env },
  });
  const lines = (r.stdout || "").split(/\r?\n/).filter((l) => l.startsWith("BEAT "));
  const byId = {};
  for (const l of lines) {
    const m = /^BEAT B2 (\S+) (PASS|FAIL|BLOCKED)\b ?(.*)$/.exec(l);
    if (m) byId[m[1]] = { verdict: m[2], reason: m[3] };
  }
  return { status: r.status, lines, byId, stdout: r.stdout || "", stderr: r.stderr || "" };
}
const base = (host) => ["--host", host, "--cli", FAKE_CLI, "--watch-wait", "10"];
const v = (r, id) => r.byId[id]?.verdict;

try {
  // 1. a good start.
  const good = run(base(makeHost("server-good.json")));
  check(good.lines.length === 7, `case 1: seven BEAT B2 lines (got ${good.lines.length}: ${good.stderr.trim().split("\n")[0] || "no BEAT line"})`);
  check(good.lines.map((l) => l.split(" ")[2]).join(",") === ORDER, "case 1: lines follow the sheet's order");
  for (const id of ["start", "save-loaded", "server-name", "password", "watch"]) check(v(good, id) === "PASS", `case 1: ${id} PASS (got ${v(good, id)} ${good.byId[id]?.reason})`);
  for (const id of ["join-wrong-password", "join-right-password"]) {
    check(v(good, id) === "BLOCKED" && /#1520/.test(good.byId[id]?.reason || ""), `case 1: ${id} BLOCKED by #1520 (got ${v(good, id)} ${good.byId[id]?.reason})`);
  }
  check(good.status === 0, `case 1: exit 0 with no FAIL (got ${good.status})`);
  check(!good.stdout.includes("fixture-join-password"), "case 1: the join password is not printed");

  // 2. a wrong name and an empty password fail their own check.
  const badName = run(base(makeHost("server-badname.json")));
  check(v(badName, "server-name") === "FAIL", `case 2: a wrong name gives server-name FAIL (got ${v(badName, "server-name")})`);
  check(v(badName, "password") === "PASS", `case 2: a wrong name leaves password PASS (got ${v(badName, "password")})`);
  check(badName.status === 1, `case 2: a wrong name exits 1 (got ${badName.status})`);
  const noPass = run(base(makeHost("server-nopass.json")));
  check(v(noPass, "password") === "FAIL", `case 2: an empty password gives password FAIL (got ${v(noPass, "password")})`);
  check(v(noPass, "server-name") === "PASS", `case 2: an empty password leaves server-name PASS (got ${v(noPass, "server-name")})`);
  check(noPass.status === 1, `case 2: an empty password exits 1 (got ${noPass.status})`);
  const noJson = run(base(makeHost(null)));
  check(v(noJson, "server-name") === "FAIL" && v(noJson, "password") === "FAIL", "case 2: no server.json fails name and password");

  // 3. start without `host: up`, and a failing start.
  const noUp = run(base(makeHost("server-good.json")), { FAKE_START: "nopid" });
  check(v(noUp, "start") === "FAIL" && /host: up/.test(noUp.byId.start?.reason || ""), `case 3: no \`host: up\` line gives start FAIL naming it (got ${v(noUp, "start")} ${noUp.byId.start?.reason})`);
  const bad = run(base(makeHost("server-good.json")), { FAKE_START: "fail" });
  check(v(bad, "start") === "FAIL" && v(bad, "watch") === "FAIL", `case 3: a failing start gives start and watch FAIL (got ${v(bad, "start")}, ${v(bad, "watch")})`);
  check(bad.status === 1, `case 3: a failing start exits 1 (got ${bad.status})`);

  // 4. the save and the ready line come from the console log.
  const noSave = run(base(makeHost("server-good.json", "console-noload.log")));
  check(v(noSave, "save-loaded") === "FAIL" && /save loaded/.test(noSave.byId["save-loaded"]?.reason || ""), `case 4: a log without \`save loaded:\` gives save-loaded FAIL (got ${v(noSave, "save-loaded")})`);
  const noLog = run(base(makeHost("server-good.json", null)));
  check(v(noLog, "save-loaded") === "FAIL", `case 4: no console log gives save-loaded FAIL (got ${v(noLog, "save-loaded")})`);

  // 5. a watch that exits at once.
  const dead = run(base(makeHost("server-good.json")), { FAKE_WATCH: "die" });
  check(v(dead, "watch") === "FAIL", `case 5: a watch that exits at once gives watch FAIL (got ${v(dead, "watch")} ${dead.byId.watch?.reason})`);
  check(v(dead, "start") === "PASS", "case 5: a dead watch leaves start PASS");

  // 6. refusals: the real host folder, a missing --host.
  const realParent = join(scratch, "appdata");
  const realHost = join(realParent, "LifeFramework", "host");
  mkdirSync(realHost, { recursive: true });
  const real = run(["--host", realHost, "--cli", FAKE_CLI], { LOCALAPPDATA: realParent });
  check(real.status === 2 && real.lines.length === 0, `case 6: the real host folder is refused with exit 2 and no BEAT line (got ${real.status}, ${real.lines.length} lines)`);
  const none = run(["--cli", FAKE_CLI]);
  check(none.status === 2 && none.lines.length === 0, `case 6: a missing --host is refused with exit 2 (got ${none.status})`);
  check(/--host/.test(readFileSync(SCRIPT, "utf8").split("\n").slice(0, 30).join("\n")), "case 6: the script header documents --host");
} catch (e) {
  failed++;
  console.log(`FAIL selftest crashed: ${e.message}`);
} finally {
  rmSync(scratch, { recursive: true, force: true });
}

if (failed) {
  console.log(`SUMMARY opening-b2-selftest fail ${failed}`);
  process.exit(1);
}
console.log("SUMMARY opening-b2-selftest ok");
