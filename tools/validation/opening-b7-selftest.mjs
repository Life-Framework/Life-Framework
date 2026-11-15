// Proves BEAT B7's setup helper on a fixture profile (no game files needed): it writes the next
// restart time into the profile's own config.json, `--undo` gives back the original bytes, and
// a profile under server/flagship/ is refused.
// It also proves the beat script's judge (b7-restart.mjs) on fixture logs.
// inputs: tools/playtest/opening/**, tools/validation/opening-b7-selftest.mjs, tools/validation/fixtures/opening-b7/**
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const script = join(root, "tools", "playtest", "opening", "b7-restart-setup.mjs");

let failed = 0;
const check = (ok, what, detail = "") => {
  if (!ok) {
    failed++;
    console.log(`FAIL ${what}${detail ? `: ${detail}` : ""}`);
  }
};
const run = (args) => spawnSync(process.execPath, [script, ...args], { encoding: "utf8", windowsHide: true });

const scratch = mkdtempSync(join(tmpdir(), "lf-b7-"));
try {
  // A scratch profile whose config has the real schedule, a fixed 06:00 and 18:00, plus other sections.
  const profile = join(scratch, "profile");
  mkdirSync(profile);
  const original = '{\n  "schemaVersion": 1,\n  "restarts": { "times": ["06:00", "18:00"], "warningMinutes": [15, 5, 1] },\n  "staff": { "webhookUrl": "" }\n}\n';
  const cfg = join(profile, "config.json");
  writeFileSync(cfg, original);

  // 12:00:30 local plus 16 minutes is 12:16:30, so the restart lands on the next whole minute: 12:17.
  const now = new Date(2026, 10, 15, 12, 0, 30).toISOString();
  const up = run(["--profile", profile, "--in", "16", "--now", now]);
  check(up.status === 0, "setup exits 0", up.stderr || up.stdout);
  check(/BEAT B7 setup PASS 12:17\b/.test(up.stdout), "setup prints PASS with the time", up.stdout.trim());
  const after = JSON.parse(readFileSync(cfg, "utf8"));
  check(JSON.stringify(after.restarts?.times) === '["12:17"]', "restart time written", JSON.stringify(after.restarts));
  check(JSON.stringify(after.restarts?.warningMinutes) === "[15,5,1]", "15, 5 and 1 minute warnings kept", JSON.stringify(after.restarts));
  check(after.staff?.webhookUrl === "" && after.schemaVersion === 1, "other sections untouched");

  // A second setup must not overwrite the backup: undo still returns the first original.
  const again = run(["--profile", profile, "--in", "20", "--now", now]);
  check(again.status === 0 && /PASS 12:21\b/.test(again.stdout), "re-run moves the time", again.stdout.trim());

  const undo = run(["--profile", profile, "--undo"]);
  check(undo.status === 0, "undo exits 0", undo.stderr || undo.stdout);
  check(readFileSync(cfg, "utf8") === original, "undo restores the original bytes");
  const undoTwice = run(["--profile", profile, "--undo"]);
  check(undoTwice.status !== 0, "undo with nothing to undo fails");
  check(readFileSync(cfg, "utf8") === original, "failed undo leaves the file alone");

  // A config without the restarts section gets one; undo removes the change again.
  const bare = join(scratch, "bare");
  mkdirSync(bare);
  const bareOriginal = '{"schemaVersion":1}';
  writeFileSync(join(bare, "config.json"), bareOriginal);
  const bareUp = run(["--profile", bare, "--in", "16", "--now", now]);
  check(bareUp.status === 0, "setup on a config with no restarts section", bareUp.stderr || bareUp.stdout);
  const bareAfter = JSON.parse(readFileSync(join(bare, "config.json"), "utf8"));
  check(JSON.stringify(bareAfter.restarts) === '{"times":["12:17"],"warningMinutes":[15,5,1]}', "restarts section created", JSON.stringify(bareAfter));
  check(run(["--profile", bare, "--undo"]).status === 0 && readFileSync(join(bare, "config.json"), "utf8") === bareOriginal, "undo restores the bare config");

  // server/flagship is refused, with either slash and any case, and nothing is written there.
  for (const flagship of [join(scratch, "server", "flagship"), join(scratch, "Server", "FlagShip", "sub"), join(scratch, "server\\flagship")]) {
    mkdirSync(flagship, { recursive: true });
    const flagCfg = join(flagship, "config.json");
    writeFileSync(flagCfg, original);
    const r = run(["--profile", flagship, "--in", "16", "--now", now]);
    check(r.status !== 0, `flagship refused: ${flagship}`, r.stdout.trim());
    check(!/PASS/.test(r.stdout), "no PASS line for flagship");
    check(readFileSync(flagCfg, "utf8") === original, "flagship config untouched");
    check(!existsSync(`${flagCfg}.b7-backup`), "no backup written under flagship");
    check(run(["--profile", flagship, "--undo"]).status !== 0, "undo under flagship refused too");
  }

  // Bad input is refused, not guessed.
  check(run(["--profile", profile, "--in", "0", "--now", now]).status !== 0, "--in 0 refused");
  check(run(["--profile", profile, "--in", "abc", "--now", now]).status !== 0, "--in abc refused");
  check(run(["--profile", join(scratch, "missing"), "--in", "16"]).status !== 0, "a profile without config.json refused");
  check(run(["--in", "16"]).status !== 0, "no --profile refused");
} finally {
  rmSync(scratch, { recursive: true, force: true });
}

// The beat script's judge (#1603): fixture logs of one restart cycle. run1.log is the console log of the boot
// that closes, watch.log what `host watch` printed, run2.log the console log of the boot the watchdog starts.
// red-proof: written before tools/playtest/opening/b7-restart.mjs existed (the import failed); then with the
// "save completed (SHUTDOWN)" test removed from judgeRestart, the no-save fixture passed and its check failed.
const beat = join(root, "tools", "playtest", "opening", "b7-restart.mjs");
const FIX = join(root, "tools", "validation", "fixtures", "opening-b7");
let judge = null;
try {
  ({ judgeRestart: judge } = await import(pathToFileURL(beat).href));
} catch (e) {
  check(false, "b7-restart.mjs loads", String(e.message).split("\n")[0]);
}
if (judge) {
  const read = (set) => {
    const f = (n) => readFileSync(join(FIX, set, n), "utf8");
    return judge({ run1: f("run1.log"), watch: f("watch.log"), run2: f("run2.log") });
  };
  const byId = (rows, id) => rows.find((r) => r.id === id);

  const full = read("full");
  check(full.length > 0 && full.every((r) => r.status === "PASS"), "a full cycle is PASS on every check", JSON.stringify(full.filter((r) => r.status !== "PASS")));
  for (const id of ["warning-3", "warning-2", "warning-1", "toast-only", "close", "save", "watchdog", "back-up"]) check(byId(full, id)?.status === "PASS", `full cycle has a PASS for ${id}`);

  const cases = [
    ["no-warning", "warning-1", /1 minute/],
    ["no-toast", "warning-1", /toast/],
    ["no-save", "save", /shutdown save/i],
    ["siren", "toast-only", /siren/i],
    ["no-close", "close", /restart due: wrote/],
    ["crash", "watchdog", /crash/i],
    ["no-reload", "back-up", /save loaded/],
  ];
  for (const [set, id, why] of cases) {
    const rows = read(set);
    const row = byId(rows, id);
    check(row?.status === "FAIL" && why.test(row.detail), `${set}: ${id} FAILs naming the cause`, JSON.stringify(row));
    check(rows.filter((r) => r.status === "FAIL").length === 1, `${set}: only ${id} FAILs`, JSON.stringify(rows.filter((r) => r.status === "FAIL").map((r) => r.id)));
  }

  // Leads that cannot fit the time left when the ticker started are not expected (15 minutes into a 5 minute window).
  const noFit = judge({ run1: readFileSync(join(FIX, "full", "run1.log"), "utf8").replace("warnings 3, 2, 1 min before, next in 339", "warnings 15, 3, 2, 1 min before, next in 339"), watch: "[LFDebug:HostWatch] host exited: scheduled restart\n[LFDebug:HostWatch] restarting the host (scheduled restart)\n[LFDebug:HostWatch] host up (pid 1)\n", run2: readFileSync(join(FIX, "full", "run2.log"), "utf8") });
  check(!byId(noFit, "warning-15") && byId(noFit, "warning-3")?.status === "PASS", "a lead longer than the window is not expected");
  check(judge({ run1: "", watch: "", run2: "" }).some((r) => r.status === "FAIL" && r.id === "ticker"), "an empty log FAILs at the ticker");

  // The command line: --judge <dir> prints BEAT B7 lines and exits 1 only on a FAIL.
  const cli = (set) => spawnSync(process.execPath, [beat, "--judge", join(FIX, set)], { encoding: "utf8", windowsHide: true });
  const ok = cli("full");
  check(ok.status === 0 && /^BEAT B7 save PASS/m.test(ok.stdout) && /^BEAT B7 warning-1 PASS/m.test(ok.stdout), "--judge on a full cycle: BEAT lines, exit 0", ok.stdout.trim().slice(0, 300));
  const bad = cli("no-save");
  check(bad.status === 1 && /^BEAT B7 save FAIL /m.test(bad.stdout), "--judge on a log with no save: BEAT B7 save FAIL, exit 1", bad.stdout.trim().slice(0, 300));
  // A live run only ever works inside <repo>/tmp/: a --host outside it is refused before anything starts.
  const outside = spawnSync(process.execPath, [beat, "--host", tmpdir()], { encoding: "utf8", windowsHide: true });
  check(outside.status === 2 && !/BEAT B7 .* PASS/.test(outside.stdout), "--host outside tmp/ is refused", `${outside.status} ${outside.stderr.trim().slice(0, 200)}`);
}

if (failed) {
  console.log(`SUMMARY opening-b7-selftest fail ${failed}`);
  process.exit(1);
}
console.log("SUMMARY opening-b7-selftest ok");
