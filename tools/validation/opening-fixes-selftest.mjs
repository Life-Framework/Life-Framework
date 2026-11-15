// tools/validation/opening-fixes-selftest.mjs
// inputs: tools/commands/opening.mjs, tools/playtest/opening/**, tools/validation/fixtures/opening-fixes/**, tools/validation/fixtures/opening-b2/fake-cli.mjs, tools/validation/fixtures/opening-b1/**, tools/validation/fixtures/opening-b6/**
//
// Proves the three runner fixes of the opening-day run (#1691, #1692, #1693; report docs/playtest/m4-opening-partial-2026-11.md):
//   #1691 one --host cannot satisfy every beat: `tools\cli opening` gives each beat a folder of its own that the beat accepts
//         (a beat's `// opening-run: host=tmp` header says it needs tmp/; the default is a folder outside the repo), derived
//         from --host, and every beat gets one even with no --host;
//   #1692 a beat stops what it started, by pid, on exit: B2's `host watch` is dead when B2 returns; a failed or timed-out start
//         leaves nothing behind; the runner stops what a beat leaked (a FAIL `cleanup` line) and `reapHost` kills a recorded pid;
//   #1693 a fresh host reports BLOCKED with the reason, never a crash: B9 (no save: no stack trace, BEAT lines), B1 (uid
//         pending, no save yet), B2 (a start that never reaches ready on a host with no save), B6 (no perf csv), B8 (a first boot
//         that makes no save); a script that exits 2 is BLOCKED, any other non-zero exit stays a FAIL; B10 prints BEAT lines.
// red-proof: written before any of the fixes (2026-11-15): the runner handed one folder to every beat, B2 left its watcher up, B9
// died with ENOENT, B1/B2/B6/B8 printed FAIL, and exit 2 scored FAIL: every case below failed. Each fix then turned its cases green.

import { existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { spawn, spawnSync } from "node:child_process";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { safeRemoveTree } from "../link-safety.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FIX = join(ROOT, "tools", "validation", "fixtures");
const FIXES = join(FIX, "opening-fixes");
const OPEN = join(ROOT, "tools", "playtest", "opening");
const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};
const alive = (pid) => {
  try {
    process.kill(pid, 0);
    return true;
  } catch (e) {
    return e.code === "EPERM";
  }
};
const waitDead = (pid, ms = 4000) => {
  const end = Date.now() + ms;
  while (alive(pid) && Date.now() < end) Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, 100);
  return !alive(pid);
};

const scratch = mkdtempSync(join(tmpdir(), "lf-opening-fixes-"));
const repoScratch = join(ROOT, "tmp", `opening-fixes-selftest-${process.pid}`);
const spawned = [];

try {
  // ---- #1691 / #1692 / exit 2 through the runner (a fixture root, fixture beats) ----
  const { main } = await import(pathToFileURL(join(ROOT, "tools", "commands", "opening.mjs")).href);
  const capture = async (args, root) => {
    const out = [];
    const [log, err] = [console.log, console.error];
    console.log = (...a) => out.push(a.join(" "));
    console.error = (...a) => out.push(a.join(" "));
    try {
      return { code: await main(args, { root }), text: out.join("\n") };
    } finally {
      console.log = log;
      console.error = err;
    }
  };
  const common = (root, extra = []) => ["--dir", join(FIXES, "beats"), "--sheet", join(FIX, "opening-run", "sheet.md"), "--proofs", join(FIX, "opening-run", "proofs"), "--date", "2026-11-15", "--beats", "B1,B7", ...extra];
  const report = (root) => readFileSync(join(root, "tmp", "playtest", "opening-2026-11-15.md"), "utf8");

  // #1691: three ways to call it; B1 (outside the repo) and B7 (under tmp/) must both pass each time, in folders of their own.
  const placements = {
    "no --host": (root) => [],
    "--host under tmp/": (root) => ["--host", join(root, "tmp", "base-in-tmp")],
    "--host outside the repo": (root) => ["--host", join(scratch, "base-outside")],
  };
  for (const [what, args] of Object.entries(placements)) {
    const root = mkdtempSync(join(scratch, "root-"));
    const r = await capture(common(root, args(root)), root);
    const rep = report(root);
    check(r.code === 0, `#1691 ${what}: both beats pass, exit 0 (got ${r.code}: ${r.text})`);
    check(/^- B1:? .*PASS/m.test(rep), `#1691 ${what}: B1 (outside the repo) passes (${rep.split("\n").filter((l) => /^- B1/.test(l))})`);
    check(/^- B7:? .*PASS/m.test(rep), `#1691 ${what}: B7 (under tmp/) passes (${rep.split("\n").filter((l) => /^- B7/.test(l))})`);
    check(/Host folders/.test(rep) && /B1 /.test(rep.split("Host folders")[1] || "") && /B7 /.test(rep.split("Host folders")[1] || ""), `#1691 ${what}: the report names each beat's host folder`);
  }

  // exit 2 is BLOCKED, another non-zero exit is still a FAIL (#1693).
  {
    const root = mkdtempSync(join(scratch, "root-"));
    const r = await capture(["--dir", join(FIXES, "beats"), "--sheet", join(FIX, "opening-run", "sheet.md"), "--proofs", join(FIX, "opening-run", "proofs"), "--date", "2026-11-15", "--beats", "B3,B4"], root);
    const rep = report(root);
    check(/^- B3:? .*BLOCKED.*site not built/m.test(rep), `#1693 a script that exits 2 is BLOCKED with its reason (${rep.split("\n").filter((l) => /^- B3/.test(l))})`);
    check(/^- B4:? .*FAIL.*exited 3/m.test(rep), `#1693 an exit 3 with no BEAT line is still a FAIL (${rep.split("\n").filter((l) => /^- B4/.test(l))})`);
    check(/blocked=1/.test(r.text) && /fail=1/.test(r.text), `#1693 the SUMMARY counts one blocked and one fail (${r.text.split("\n").pop()})`);
  }

  // #1693: a beat that runs out of time is BLOCKED "no save yet" on a host with no save point, a FAIL on one that has saved.
  for (const [what, save] of [["a host with no save", false], ["a host that has saved", true]]) {
    const root = mkdtempSync(join(scratch, "root-"));
    const base = join(scratch, save ? "base-hang-saved" : "base-hang-fresh");
    if (save) {
      const sp = join(base, "B5", "profile", "profile", ".save", "u", "game", "M", "playthrough000", "savepoint000");
      mkdirSync(sp, { recursive: true });
      writeFileSync(join(sp, "meta-info.json"), "{}");
    }
    await capture(["--dir", join(FIXES, "beats"), "--sheet", join(FIX, "opening-run", "sheet.md"), "--proofs", join(FIX, "opening-run", "proofs"), "--date", "2026-11-15", "--beats", "B5", "--host", base, "--timeout", "2"], root);
    const line = report(root).split(/\r?\n/).filter((l) => /^- B5/.test(l)).join(" ");
    if (save) check(/FAIL.*timed out after 2s/.test(line), `#1693 a beat that times out on ${what} is a FAIL (${line})`);
    else check(/BLOCKED.*no save yet.*timed out after 2s/.test(line), `#1693 a beat that times out on ${what} is BLOCKED "no save yet" (${line})`);
  }

  // #1692: a beat that leaks a process; the runner stops it by pid and says so.
  {
    const root = mkdtempSync(join(scratch, "root-"));
    const host = join(scratch, "base-leak");
    const r = await capture(["--dir", join(FIXES, "beats"), "--sheet", join(FIX, "opening-run", "sheet.md"), "--proofs", join(FIX, "opening-run", "proofs"), "--date", "2026-11-15", "--beats", "B2", "--host", host], root);
    const pidFile = join(host, "B2", "leaked.pid");
    const pid = existsSync(pidFile) ? Number(readFileSync(pidFile, "utf8")) : 0;
    if (pid) spawned.push(pid);
    check(pid > 0, `#1692 the fixture beat ran and wrote its pid (${r.text})`);
    check(pid > 0 && waitDead(pid), `#1692 the process a beat left running is stopped by the runner (pid ${pid} still alive)`);
    check(/^- B2:? .*FAIL.*cleanup.*left/m.test(report(root)), `#1692 a leaked process is a FAIL cleanup line (${report(root).split("\n").filter((l) => /^- B2/.test(l))})`);
  }

  // #1692: reapHost kills a recorded pid.
  {
    const scratchMod = join(OPEN, "_scratch.mjs");
    check(existsSync(scratchMod), "#1692 tools/playtest/opening/_scratch.mjs (recordPid, reapHost) exists");
    if (existsSync(scratchMod)) {
      const { recordPid, reapHost } = await import(pathToFileURL(scratchMod).href);
      const host = join(scratch, "reap-host");
      mkdirSync(host, { recursive: true });
      const child = spawn(process.execPath, ["-e", "setTimeout(() => {}, 120000)"], { detached: true, stdio: "ignore", windowsHide: true });
      child.unref();
      spawned.push(child.pid);
      recordPid(host, "watcher", child.pid);
      const done = reapHost({ host });
      check(done.stopped.some((p) => p.pid === child.pid) && waitDead(child.pid), "#1692 reapHost stops a recorded pid and reports it");
      check(done.leftover.length === 0, "#1692 reapHost leaves nothing behind");
      check(reapHost({ host }).stopped.length === 0, "#1692 reapHost on a clean host stops nothing");
    }
  }

  // #1692: B2 stops its watcher before it returns.
  const b2 = join(OPEN, "b2-start.mjs");
  const fakeB2 = join(FIX, "opening-b2", "fake-cli.mjs");
  const mkHost = (name, { save = false, serverJson = false } = {}) => {
    const dir = join(scratch, name);
    mkdirSync(join(dir, "profile"), { recursive: true });
    if (serverJson) writeFileSync(join(dir, "profile", "server.json"), readFileSync(join(FIX, "opening-b2", "server-good.json")));
    if (save) {
      const sp = join(dir, "profile", "profile", ".save", "u", "game", "M", "playthrough000", "savepoint000");
      mkdirSync(sp, { recursive: true });
      writeFileSync(join(sp, "meta-info.json"), "{}");
    }
    return dir;
  };
  const beats = (text, beat) => {
    const byId = {};
    for (const m of text.matchAll(new RegExp(`^BEAT ${beat} (\\S+) (PASS|FAIL|BLOCKED)\\b ?(.*)$`, "gm"))) byId[m[1]] = { v: m[2], reason: m[3] };
    return byId;
  };
  {
    const h = mkHost("b2-up", { save: true, serverJson: true });
    const r = spawnSync(process.execPath, [b2, "--host", h, "--cli", fakeB2, "--watch-wait", "10"], { cwd: ROOT, encoding: "utf8", timeout: 120000, env: { ...process.env, LF_HOST_DIR: "" } });
    const w = beats(r.stdout, "B2").watch;
    const pid = Number(/pid (\d+)/.exec(w?.reason || "")?.[1] || 0);
    if (pid) spawned.push(pid);
    check(w?.v === "PASS" && pid > 0, `#1692 B2's watch PASSes and names the watcher pid (${w?.v} ${w?.reason})`);
    check(pid > 0 && waitDead(pid, 1500), `#1692 B2's watcher (pid ${pid}) is dead when B2 returns, not left to outlive the beat`);

    // a start that never reaches ready on a host WITH a save is still a FAIL (the host should have come up)
    const hs = mkHost("b2-hang-save", { save: true });
    const t = spawnSync(process.execPath, [b2, "--host", hs, "--cli", fakeB2, "--timeout", "2", "--watch-wait", "3"], { cwd: ROOT, encoding: "utf8", timeout: 120000, env: { ...process.env, LF_HOST_DIR: "", FAKE_START: "hang" } });
    check(beats(t.stdout, "B2").start?.v === "FAIL" && t.status === 1, `#1693 a start that times out on a host with a save is a FAIL, exit 1 (got ${beats(t.stdout, "B2").start?.v}, exit ${t.status})`);
  }

  // #1693: B2 on a fresh host: the start never reaches ready and nothing is judged.
  {
    const h = mkHost("b2-fresh");
    const r = spawnSync(process.execPath, [b2, "--host", h, "--cli", fakeB2, "--timeout", "2", "--watch-wait", "3"], { cwd: ROOT, encoding: "utf8", timeout: 120000, env: { ...process.env, LF_HOST_DIR: "", FAKE_START: "hang" } });
    const by = beats(r.stdout, "B2");
    for (const id of ["start", "save-loaded", "server-name", "password", "watch"]) check(by[id]?.v === "BLOCKED", `#1693 B2 on a fresh host: ${id} is BLOCKED (got ${by[id]?.v} ${by[id]?.reason})`);
    check(/no save yet/i.test(by.start?.reason || ""), `#1693 B2's start says "no save yet" (${by.start?.reason})`);
    check(r.status === 0, `#1693 B2 on a fresh host exits 0, nothing is a FAIL (got ${r.status})`);
  }

  // #1693: B9 on an empty host: BLOCKED with BEAT lines, no stack trace.
  {
    const host = join(repoScratch, "empty-host");
    mkdirSync(host, { recursive: true });
    const r = spawnSync(process.execPath, [join(OPEN, "b9-wipe.mjs"), "--host", host], { cwd: ROOT, encoding: "utf8", timeout: 240000, windowsHide: true });
    const out = `${r.stdout}${r.stderr}`;
    check(!/ENOENT|at simulatePost|Error:/.test(out), `#1693 B9 on an empty host prints no stack trace (${out.trim().split("\n").slice(0, 3).join(" | ")})`);
    const by = beats(r.stdout, "B9");
    check(Object.keys(by).length > 0 && Object.values(by).every((b) => b.v === "BLOCKED"), `#1693 B9 on an empty host prints only BEAT B9 BLOCKED lines (got ${JSON.stringify(by)})`);
    check(Object.values(by).some((b) => /no save yet/i.test(b.reason)), "#1693 B9 says \"no save yet\"");
    check(r.status === 0, `#1693 B9 on an empty host exits 0 (got ${r.status})`);
    check(!existsSync(join(ROOT, "tmp", "opening-b9")) || !readdirSync(join(ROOT, "tmp", "opening-b9")).some((n) => n.startsWith("run-") && existsSync(join(ROOT, "tmp", "opening-b9", n, "empty-host"))), "#1693 B9 leaves no scratch copy of the empty host");
  }

  // #1693: B1 on a fresh host: uid pending, no save yet.
  {
    const host = join(scratch, "b1-fresh");
    mkdirSync(join(host, "profile", "LifeFramework"), { recursive: true });
    writeFileSync(join(host, "profile", "LifeFramework", "staff.txt"), readFileSync(join(FIX, "opening-b1", "staff-pending.txt")));
    const r = spawnSync(process.execPath, [join(OPEN, "b1-desk.mjs"), "--host", host, "--cli", join(FIX, "opening-b1", "fake-cli.mjs")], {
      cwd: ROOT,
      encoding: "utf8",
      timeout: 120000,
      env: { ...process.env, LF_HOST_DIR: "", FAKE_HELP: join(FIX, "opening-b1", "help-full.txt"), FAKE_CONFIG: "uid", FAKE_READY: "nogo" },
    });
    const by = beats(r.stdout, "B1");
    check(by["config-apply"]?.v === "BLOCKED" && /uid pending/i.test(by["config-apply"]?.reason || ""), `#1693 B1: config-apply refused for a missing uid is BLOCKED "uid pending" (got ${by["config-apply"]?.v} ${by["config-apply"]?.reason})`);
    check(by.roster?.v === "BLOCKED" && /uid pending/i.test(by.roster?.reason || ""), `#1693 B1: a roster with nobody (uid pending) is BLOCKED (got ${by.roster?.v} ${by.roster?.reason})`);
    check(by.ready?.v === "BLOCKED" && /no save yet/i.test(by.ready?.reason || ""), `#1693 B1: no-go on a host with no save is BLOCKED "no save yet" (got ${by.ready?.v} ${by.ready?.reason})`);
    check(r.status === 0, `#1693 B1 on a fresh host exits 0 (got ${r.status})`);
    // the same refusal text on a plain failure is still a FAIL
    const hostFail = join(scratch, "b1-fail");
    cpSyncDir(host, hostFail);
    const f = spawnSync(process.execPath, [join(OPEN, "b1-desk.mjs"), "--host", hostFail, "--cli", join(FIX, "opening-b1", "fake-cli.mjs")], {
      cwd: ROOT,
      encoding: "utf8",
      timeout: 120000,
      env: { ...process.env, LF_HOST_DIR: "", FAKE_HELP: join(FIX, "opening-b1", "help-full.txt"), FAKE_CONFIG: "fail" },
    });
    check(beats(f.stdout, "B1")["config-apply"]?.v === "FAIL", "#1693 B1: a config apply that fails for another reason is still a FAIL");
  }

  // #1693: B6 with no perf csv.
  {
    const r = spawnSync(process.execPath, [join(OPEN, "b6-day.mjs"), "--host", join(FIX, "opening-b6", "host-none"), "--proofs", join(FIX, "opening-b6", "proofs-without")], {
      cwd: ROOT,
      encoding: "utf8",
      timeout: 60000,
      env: { ...process.env, LF_HOST_DIR: "" },
    });
    const by = beats(r.stdout, "B6");
    check(by["perf-samples"]?.v === "BLOCKED" && /perf/i.test(by["perf-samples"]?.reason || ""), `#1693 B6: no perf csv is BLOCKED (got ${by["perf-samples"]?.v} ${by["perf-samples"]?.reason})`);
    check(r.status === 0, `#1693 B6 with no perf csv exits 0 (got ${r.status})`);
  }

  // #1693: B8 whose first boot makes no save.
  {
    const host = mkdtempSync(join(scratch, "b8-fresh-"));
    const r = spawnSync(process.execPath, [join(OPEN, "b8-reload.mjs"), "--host", host, "--cli", join(FIXES, "fresh-cli", "cli.mjs"), "--timeout", "5"], {
      cwd: ROOT,
      encoding: "utf8",
      timeout: 120000,
      windowsHide: true,
      env: { ...process.env, LF_HOST_DIR: "" },
    });
    const by = beats(r.stdout, "B8");
    check(by["second-boot"]?.v === "BLOCKED" && /no save yet/i.test(by["second-boot"]?.reason || ""), `#1693 B8: a first boot that makes no save is BLOCKED "no save yet" (got ${by["second-boot"]?.v} ${by["second-boot"]?.reason}; ${r.stderr.trim().split("\n")[0]})`);
    check(r.status === 0, `#1693 B8 on a fresh host exits 0 (got ${r.status})`);
  }

  // #1693: B8 whose password rotate is refused for a missing uid is BLOCKED "uid pending", not FAIL.
  {
    const host = mkdtempSync(join(scratch, "b8-uid-"));
    const r = spawnSync(process.execPath, [join(OPEN, "b8-reload.mjs"), "--host", host, "--cli", join(FIXES, "fresh-cli", "cli.mjs"), "--timeout", "5"], {
      cwd: ROOT,
      encoding: "utf8",
      timeout: 120000,
      windowsHide: true,
      env: { ...process.env, LF_HOST_DIR: "", FAKE_MODE: "uid" },
    });
    const by = beats(r.stdout, "B8");
    check(by["password-rotated"]?.v === "BLOCKED" && /uid pending/i.test(by["password-rotated"]?.reason || ""), `#1693 B8: a rotate refused for a missing uid is BLOCKED "uid pending" (got ${by["password-rotated"]?.v} ${by["password-rotated"]?.reason})`);
  }

  // #1693: B10 prints BEAT lines, and still exits 2 when the site is not built.
  {
    const empty = mkdtempSync(join(scratch, "dist-"));
    const r = spawnSync(process.execPath, [join(OPEN, "b10-site.mjs"), "--dist", empty], { cwd: ROOT, encoding: "utf8", timeout: 60000, windowsHide: true });
    check(r.status === 2 && /^BEAT B10 site BLOCKED .*not built/m.test(r.stdout), `#1693 B10 with no built site prints a BEAT B10 BLOCKED line and exits 2 (exit ${r.status}: ${r.stdout.trim()})`);
  }

  // #1692: every beat that starts a server reaps by pid in a finally.
  for (const [file, what] of [["b2-start.mjs", "B2"], ["b7-restart.mjs", "B7"], ["b8-reload.mjs", "B8"]]) {
    const src = readFileSync(join(OPEN, file), "utf8");
    check(/finally\s*\{[^}]*reapHost\(/.test(src), `#1692 ${what} calls reapHost in a finally block`);
  }
} catch (e) {
  failures.push(`selftest crashed: ${e.stack || e.message}`);
} finally {
  for (const pid of spawned) {
    try {
      process.kill(pid);
    } catch {
      // already gone
    }
  }
  safeRemoveTree(scratch);
  if (existsSync(repoScratch)) safeRemoveTree(repoScratch);
}

function cpSyncDir(from, to) {
  mkdirSync(join(to, "profile", "LifeFramework"), { recursive: true });
  writeFileSync(join(to, "profile", "LifeFramework", "staff.txt"), readFileSync(join(from, "profile", "LifeFramework", "staff.txt")));
}

if (failures.length) {
  console.log(`SUMMARY opening-fixes-selftest fail ${failures.length}`);
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}
console.log("SUMMARY opening-fixes-selftest ok");
