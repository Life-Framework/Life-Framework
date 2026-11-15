// Proves server-doctor's wiring (#1349, O13W): on a profile folder the report adds the effective-config
// section (tools/lib/server-doctor-config.mjs) and marks each script-error line known or new through
// tools/lib/log-triage.mjs; a bad config field turns the verdict to FAIL even when the boot was clean.
// Profile folders are built in a temp dir from the O12 boot-log fixtures and the O13 config fixtures.
// inputs: tools/lib/server-doctor.mjs, tools/lib/server-doctor-config.mjs, tools/lib/log-triage.mjs, tools/validation/fixtures/server-doctor, tools/validation/fixtures/server-doctor-config, docs/debug-log-triage.md
// red-proof: before diagnoseTarget existed the import failed with "does not provide an export named 'diagnoseTarget'" and the run exited 1.
import { copyFileSync, mkdirSync, mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { readFileSync } from "node:fs";
import { diagnoseTarget, renderReport } from "../lib/server-doctor.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, "..", "..");
const triage = readFileSync(join(root, "docs", "debug-log-triage.md"), "utf8");
const bootLog = (n) => join(here, "fixtures", "server-doctor", n);
const cfgFile = (n) => join(here, "fixtures", "server-doctor-config", n, "config.json");
const tmp = mkdtempSync(join(tmpdir(), "lf-doctor-wire-"));
const fails = [];
const expect = (ok, msg) => { if (!ok) fails.push(msg); };

// A profile folder as the dedicated server writes it: <profile>/config.json + <profile>/logs/logs_<stamp>/console.log
function profile(name, log, config) {
  const dir = join(tmp, name);
  const logs = join(dir, "logs", "logs_2026-11-15_10-00-00");
  mkdirSync(logs, { recursive: true });
  copyFileSync(bootLog(log), join(logs, "console.log"));
  copyFileSync(cfgFile(config), join(dir, "config.json"));
  return dir;
}

try {
  // Good boot + bad config: both sections, FAIL at the config stage, the bad field named.
  const bad = diagnoseTarget(profile("bad", "good.log", "bad"), { triage });
  const badText = renderReport(bad);
  expect(bad.verdict === "FAIL" && bad.stage === "config", `good boot + bad config is FAIL at config: got ${bad.verdict}/${bad.stage}`);
  expect(bad.config && bad.config.badFields.includes("shops.defaultSellPrice"), "bad: the config section names shops.defaultSellPrice");
  expect(/effective config/.test(badText) && /shops\.defaultSellPrice/.test(badText), "bad: the report has the effective-config section with the field");
  expect(/mods loaded \(4\)/.test(badText) && /script errors \(0\)/.test(badText), "bad: the boot-log sections are still there");
  expect(/FAIL\s*$/.test(badText.trim()), "bad: report ends with FAIL");

  // Good boot + good config: PASS, the config section says so.
  const good = diagnoseTarget(profile("good", "good.log", "good"), { triage });
  const goodText = renderReport(good);
  expect(good.verdict === "PASS", `good boot + good config is PASS: got ${good.verdict}/${good.stage}`);
  expect(good.config && good.config.ok === true, "good: config section is ok");
  expect(/effective config/.test(goodText) && /PASS\s*$/.test(goodText.trim()), "good: report has the config section and ends with PASS");

  // Script errors on a good config: each line is marked known or new (log-triage), the verdict is the boot's.
  const vme = diagnoseTarget(profile("vme", "vme.log", "good"), { triage });
  const vmeText = renderReport(vme);
  const bandage = vme.errors.find((e) => /SCR_ConsumableBandage/.test(e.line));
  const unknown = vme.errors.find((e) => /VME:/.test(e.line));
  expect(vme.verdict === "FAIL" && vme.stage === "scripts", `vme + good config keeps the scripts verdict: got ${vme.verdict}/${vme.stage}`);
  expect(bandage && bandage.known === true && /container attributes|OPEN/.test(bandage.fix ?? ""), `vme: the bandage line is known with the table fix: ${JSON.stringify(bandage)}`);
  expect(unknown && unknown.known === false && unknown.fix === null, `vme: the VME line is new, no made-up fix: ${JSON.stringify(unknown)}`);
  expect(/\[known\][^\n]*SCR_ConsumableBandage/.test(vmeText) && /\[new\][^\n]*VME:/.test(vmeText), "vme: the report prints [known] and [new] marks");

  // A bare log file has no profile: no config section is invented, the boot verdict stands.
  const bare = diagnoseTarget(bootLog("good.log"), { triage });
  expect(bare.verdict === "PASS" && bare.config === null, `a log file alone is PASS with no config section: got ${bare.verdict}/${JSON.stringify(bare.config)}`);

  // A folder with no console.log is not diagnosable.
  const none = diagnoseTarget(join(tmp, "nothing-here"), { triage });
  expect(none === null, "a missing target gives null");
} finally {
  rmSync(tmp, { recursive: true, force: true });
}

if (fails.length) { for (const f of fails) console.log(`FAIL  ${f}`); console.log("SUMMARY server-doctor-wire-selftest fail"); process.exit(1); }
console.log("SUMMARY server-doctor-wire-selftest ok");
