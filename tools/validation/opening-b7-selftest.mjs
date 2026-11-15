// Proves BEAT B7's setup helper on a fixture profile (no game files needed): it writes the next
// restart time into the profile's own config.json, `--undo` gives back the original bytes, and
// a profile under server/flagship/ is refused.
// inputs: tools/playtest/opening/**, tools/validation/opening-b7-selftest.mjs
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

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

if (failed) {
  console.log(`SUMMARY opening-b7-selftest fail ${failed}`);
  process.exit(1);
}
console.log("SUMMARY opening-b7-selftest ok");
