// tools/validation/opening-b1-selftest.mjs
// inputs: tools/playtest/opening/b1-desk.mjs, tools/validation/fixtures/opening-b1/**, tools/host/**, tools/cli.mjs
//
// Proves beat script B1 of docs/playtest/m4-opening-day.md (the desk go or no-go), `node tools/playtest/opening/b1-desk.mjs
// --host <scratch dir>`, on fixture hosts with no game:
//   1. a host whose roster names the maintainer as the only administrator and whose backups hold plain and tagged folders gives
//      PASS for `roster` and `prune` (the real `host prune --dry-run`); a `host help` without `config apply` and `ready` gives
//      BLOCKED for `config-apply`, `ready` and `drill`; five `BEAT B1` lines in checkbox order; exit 0;
//   2. a roster with a second person, with no uid yet, or with the maintainer as a moderator gives FAIL for `roster` and exit 1;
//   3. a stand-in cli that lists `config apply` and `ready`: PASS when the join password lands in the host's server.json and in no
//      repo file, and when `host ready` prints go; FAIL on an empty password, a password in a repo file, a failing apply, and no-go;
//   4. a prune dry run that would delete a preflight backup gives FAIL;
//   5. the real host folder and a missing --host are refused (exit 2, no BEAT line).
// red-proof: before tools/playtest/opening/b1-desk.mjs existed every case printed "FAIL ... no BEAT line" (2026-11-15); with the
// two-person roster limit (`entries.length > 1`) loosened, "case 2: staff-two.txt gives roster FAIL" failed. Restored.

import { cpSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FIX = join(ROOT, "tools", "validation", "fixtures", "opening-b1");
const SCRIPT = join(ROOT, "tools", "playtest", "opening", "b1-desk.mjs");
const FAKE_CLI = join(FIX, "fake-cli.mjs");
const scratch = mkdtempSync(join(tmpdir(), "lf-b1-"));

let failed = 0;
function check(ok, label) {
  if (!ok) {
    failed++;
    console.log(`FAIL ${label}`);
  }
}

// A scratch host: 25 plain start/stop backups, one each of the tagged kinds, a roster at the profile path.
let n = 0;
function makeHost(staffFile) {
  const dir = join(scratch, `host-${++n}`);
  const backups = join(dir, "backups");
  for (let i = 1; i <= 25; i++) {
    const day = String(i).padStart(2, "0");
    mkdirSync(join(backups, `2026-10-${day}_08-00-00-${i % 2 ? "start" : "stop"}`), { recursive: true });
  }
  for (const tag of ["preflight", "wipe", "crash"]) mkdirSync(join(backups, `2026-09-01_08-00-00-${tag}`), { recursive: true });
  const lfDir = join(dir, "profile", "LifeFramework");
  mkdirSync(lfDir, { recursive: true });
  cpSync(join(FIX, staffFile), join(lfDir, "staff.txt"));
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
    const m = /^BEAT B1 (\S+) (PASS|FAIL|BLOCKED)\b ?(.*)$/.exec(l);
    if (m) byId[m[1]] = { verdict: m[2], reason: m[3] };
  }
  return { status: r.status, lines, byId, stderr: r.stderr || "" };
}
const verdicts = (r) => Object.fromEntries(Object.entries(r.byId).map(([k, v]) => [k, v.verdict]));

try {
  // 1. the maintainer-only roster and the real prune dry run; the commands main does not have are BLOCKED.
  const ok = run(["--host", makeHost("staff-good.txt"), "--help-file", join(FIX, "help-main.txt")]);
  check(ok.lines.length === 5, `case 1: five BEAT B1 lines (got ${ok.lines.length}: ${ok.stderr.trim().split("\n")[0] || "no BEAT line"})`);
  check(ok.lines.map((l) => l.split(" ")[2]).join(",") === "config-apply,roster,drill,ready,prune", "case 1: lines follow the checkbox order");
  check(ok.byId.roster?.verdict === "PASS", `case 1: roster PASS (got ${ok.byId.roster?.verdict} ${ok.byId.roster?.reason})`);
  check(ok.byId.prune?.verdict === "PASS", `case 1: prune PASS (got ${ok.byId.prune?.verdict} ${ok.byId.prune?.reason})`);
  for (const id of ["config-apply", "ready", "drill"]) check(ok.byId[id]?.verdict === "BLOCKED", `case 1: ${id} BLOCKED (got ${ok.byId[id]?.verdict})`);
  check(ok.status === 0, `case 1: exit 0 with no FAIL (got ${ok.status})`);

  // 2. a roster that is not "the maintainer only" is a FAIL.
  for (const f of ["staff-two.txt", "staff-pending.txt", "staff-mod-only.txt"]) {
    const r = run(["--host", makeHost(f), "--help-file", join(FIX, "help-main.txt")]);
    check(r.byId.roster?.verdict === "FAIL", `case 2: ${f} gives roster FAIL (got ${r.byId.roster?.verdict})`);
    check(r.status === 1, `case 2: ${f} exits 1 (got ${r.status})`);
  }

  // 3. config apply and ready through the stand-in cli, which lists them.
  const fake = (extra = {}) => ({ FAKE_HELP: join(FIX, "help-full.txt"), ...extra });
  const base = (host) => ["--host", host, "--cli", FAKE_CLI];
  const good = run(base(makeHost("staff-good.txt")), fake());
  check(JSON.stringify(verdicts(good)) === JSON.stringify({ "config-apply": "PASS", roster: "PASS", drill: "BLOCKED", ready: "PASS", prune: "PASS" }), `case 3: listed commands PASS (got ${JSON.stringify(verdicts(good))})`);
  check(good.status === 0, `case 3: exit 0 (got ${good.status})`);
  const emptyPw = run(base(makeHost("staff-good.txt")), fake({ FAKE_CONFIG: "nopass" }));
  check(emptyPw.byId["config-apply"]?.verdict === "FAIL" && /password/i.test(emptyPw.byId["config-apply"]?.reason || ""), "case 3: an empty join password in the host server.json is FAIL");
  const failApply = run(base(makeHost("staff-good.txt")), fake({ FAKE_CONFIG: "fail" }));
  check(failApply.byId["config-apply"]?.verdict === "FAIL" && failApply.status === 1, "case 3: a failing config apply is FAIL, exit 1");
  const noGo = run(base(makeHost("staff-good.txt")), fake({ FAKE_READY: "nogo" }));
  check(noGo.byId.ready?.verdict === "FAIL" && /no-go/i.test(noGo.byId.ready?.reason || ""), "case 3: no-go from host ready is FAIL with the reason");
  const flagship = join(scratch, "flagship-leak");
  mkdirSync(flagship, { recursive: true });
  writeFileSync(join(flagship, "server.json"), JSON.stringify({ game: { password: "fixture-join-password" } }));
  const leak = run([...base(makeHost("staff-good.txt")), "--flagship", flagship], fake());
  check(leak.byId["config-apply"]?.verdict === "FAIL" && /repo/i.test(leak.byId["config-apply"]?.reason || ""), "case 3: the join password in a repo file is FAIL");

  // 4. a prune dry run that would delete a tagged backup.
  const tagged = run(base(makeHost("staff-good.txt")), fake({ FAKE_PRUNE: "tagged" }));
  check(tagged.byId.prune?.verdict === "FAIL" && /preflight/.test(tagged.byId.prune?.reason || ""), "case 4: a tagged backup in the dry run list is FAIL naming it");

  // 5. refusals: the real host folder, a missing --host.
  const realParent = join(scratch, "appdata");
  const realHost = join(realParent, "LifeFramework", "host");
  mkdirSync(realHost, { recursive: true });
  const real = run(["--host", realHost, "--help-file", join(FIX, "help-main.txt")], { LOCALAPPDATA: realParent });
  check(real.status === 2 && real.lines.length === 0, `case 5: the real host folder is refused with exit 2 and no BEAT line (got ${real.status}, ${real.lines.length} lines)`);
  const none = run(["--help-file", join(FIX, "help-main.txt")]);
  check(none.status === 2 && none.lines.length === 0, `case 5: a missing --host is refused with exit 2 (got ${none.status})`);
  check(/--host/.test(readFileSync(SCRIPT, "utf8").split("\n").slice(0, 30).join("\n")), "case 5: the script header documents --host");
} catch (e) {
  failed++;
  console.log(`FAIL selftest crashed: ${e.message}`);
} finally {
  rmSync(scratch, { recursive: true, force: true });
}

if (failed) {
  console.log(`SUMMARY opening-b1-selftest fail ${failed}`);
  process.exit(1);
}
console.log("SUMMARY opening-b1-selftest ok");
