// tools/validation/opening-b8-selftest.mjs
// inputs: tools/playtest/opening/**, tools/host/**, tools/validation/fixtures/opening-b8/**
//
// Proves the opening-day beat script B8 (docs/playtest/m4-opening-day.md "B8") on fixture console logs:
// `node tools/playtest/opening/b8-reload.mjs --log <console.log>` judges a second boot and prints one
// `BEAT B8 <id> PASS|FAIL|BLOCKED <reason>` line per check. A clean reload passes `save-loaded` and
// `no-missing-resource`; a log with `Unable to find` or `Wrong GUID/name` FAILs with that line; a log with no
// `save loaded:` FAILs. The time check is BLOCKED by #1302 and the rejoin check names `restart-cycle` and #1268.
// The password judge never prints a password, and a --host that is the real host folder or server/flagship is refused
// before anything runs. No server is started here.
// red-proof: before tools/playtest/opening/b8-reload.mjs existed this file failed on its first run:
// "tools/playtest/opening/b8-reload.mjs is missing" (2026-11-15).

import { existsSync, mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { spawnSync } from "node:child_process";
import { safeRemoveTree } from "../link-safety.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const SCRIPT = join(ROOT, "tools", "playtest", "opening", "b8-reload.mjs");
const FIX = join(ROOT, "tools", "validation", "fixtures", "opening-b8");

let failed = 0;
const fail = (m) => {
  console.log(`FAIL  ${m}`);
  failed++;
};

if (!existsSync(SCRIPT)) {
  fail("tools/playtest/opening/b8-reload.mjs is missing");
  console.log(`opening-b8-selftest: ${failed} case(s) failed`);
  process.exit(1);
}

const run = (args, env = {}) => spawnSync(process.execPath, [SCRIPT, ...args], { cwd: ROOT, encoding: "utf8", timeout: 60000, windowsHide: true, env: { ...process.env, ...env } });
const checksOf = (out) => {
  const m = new Map();
  for (const x of out.matchAll(/^BEAT B8 (\S+) (PASS|BLOCKED|FAIL)\b ?(.*)$/gm)) m.set(x[1], { status: x[2], detail: x[3] });
  return m;
};

const tmp = mkdtempSync(join(tmpdir(), "lf-b8-selftest-"));
try {
  // 1. A clean reload: both log checks PASS, time is BLOCKED by #1302, rejoin names restart-cycle and #1268; exit 0.
  const clean = run(["--log", join(FIX, "clean-reload.log")]);
  const c = checksOf(clean.stdout);
  if (clean.status !== 0) fail(`clean reload exited ${clean.status}:\n${clean.stdout}${clean.stderr}`);
  // (the fixture carries the two vanilla Rally* `Wrong GUID/name` lines every boot logs; they are not a failure)
  for (const id of ["save-loaded", "no-missing-resource"]) if (c.get(id)?.status !== "PASS") fail(`clean reload: ${id} is ${c.get(id)?.status}, want PASS`);
  if (c.get("time-continues")?.status !== "BLOCKED" || !/#1302/.test(c.get("time-continues").detail)) fail(`time-continues must be BLOCKED naming #1302: ${JSON.stringify(c.get("time-continues"))}`);
  const rj = c.get("player-rejoin");
  if (rj?.status !== "BLOCKED" || !/restart-cycle/.test(rj.detail) || !/#1268/.test(rj.detail) || !/opening --game/.test(rj.detail)) fail(`player-rejoin must be BLOCKED naming restart-cycle, #1268 and opening --game: ${JSON.stringify(rj)}`);
  if (!/^SUMMARY b8-reload blocked\b.*\bfail=0\b/m.test(clean.stdout)) fail(`clean reload SUMMARY is not blocked with fail=0:\n${clean.stdout}`);

  // 2. Unable to find / Wrong GUID/name FAIL and quote the line; exit 1.
  for (const [file, needle] of [["unable-to-find.log", "Unable to find resource '{D8D55097BA6DB899}"], ["wrong-guid.log", "Wrong GUID/name for resource"]]) {
    const r = run(["--log", join(FIX, file)]);
    const x = checksOf(r.stdout).get("no-missing-resource");
    if (r.status !== 1) fail(`${file}: exit ${r.status}, want 1`);
    if (x?.status !== "FAIL" || !x.detail.includes(needle)) fail(`${file}: no-missing-resource must FAIL quoting the line, got ${JSON.stringify(x)}`);
    if (!/^SUMMARY b8-reload fail\b/m.test(r.stdout)) fail(`${file}: SUMMARY is not fail`);
  }

  // 3. No `save loaded:` line FAILs save-loaded.
  const ns = run(["--log", join(FIX, "no-save.log")]);
  if (ns.status !== 1 || checksOf(ns.stdout).get("save-loaded")?.status !== "FAIL") fail(`no-save.log: save-loaded must FAIL with exit 1 (exit ${ns.status})`);

  // 4. The exported judges: the log judge and the password judge (values never in the detail).
  const mod = await import(pathToFileURL(SCRIPT).href);
  for (const fn of ["judgeBoot", "judgePassword", "checkHost"]) if (typeof mod[fn] !== "function") fail(`b8-reload.mjs does not export ${fn}`);
  if (typeof mod.judgePassword === "function") {
    const OLD = "old-amber-pine-11";
    const NEW = "new-otter-fern-42";
    const ok = mod.judgePassword({ before: OLD, after: NEW, applied: NEW });
    if (ok.status !== "PASS") fail(`a rotated password with the same one applied must PASS: ${JSON.stringify(ok)}`);
    for (const [name, v] of [["same", { before: OLD, after: OLD, applied: OLD }], ["empty", { before: OLD, after: "", applied: "" }], ["not applied", { before: OLD, after: NEW, applied: OLD }]]) {
      const r = mod.judgePassword(v);
      if (r.status !== "FAIL") fail(`password ${name} must FAIL: ${JSON.stringify(r)}`);
    }
    for (const r of [ok, mod.judgePassword({ before: OLD, after: OLD, applied: OLD }), mod.judgePassword({ before: OLD, after: NEW, applied: OLD })]) {
      if (`${r.detail}`.includes(OLD) || `${r.detail}`.includes(NEW)) fail("a password judge detail holds a password");
    }
  }
  if (typeof mod.checkHost === "function") {
    if (!mod.checkHost(join(ROOT, "server", "flagship"))) fail("checkHost accepted server/flagship");
    if (!mod.checkHost(join(tmp, "..", "..", "server", "Flagship", "x"))) fail("checkHost accepted a server/Flagship path");
    if (!mod.checkHost(mod.realHostDir())) fail("checkHost accepted the real host folder");
    if (mod.checkHost(join(tmp, "scratch-host"))) fail("checkHost refused an ordinary scratch folder");
  }

  // 5. The CLI refuses those hosts before running anything (exit 2, no BEAT line), and a missing --host is usage, not a run.
  for (const bad of [join(ROOT, "server", "flagship")]) {
    const r = run(["--host", bad]);
    if (r.status !== 2 || /^BEAT /m.test(r.stdout)) fail(`--host ${bad} was not refused with exit 2 (exit ${r.status})`);
  }
  const none = run([]);
  if (none.status !== 2) fail(`no --host and no --log must exit 2, got ${none.status}`);
} finally {
  safeRemoveTree(tmp);
}

if (failed) {
  console.log(`opening-b8-selftest: ${failed} case(s) failed`);
  process.exit(1);
}
console.log("opening-b8-selftest ok");
