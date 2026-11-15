// Proves `tools\cli server-doctor` on fixture logs, no game installed: mods and versions, the script
// errors with the triage fix, the first-boot success lines and the PASS/FAIL verdict.
// inputs: tools/lib/server-doctor.mjs, tools/commands/server-doctor.mjs, tools/validation/fixtures/server-doctor
// red-proof: before tools/lib/server-doctor.mjs existed this script failed at import.
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { diagnose, renderReport } from "../lib/server-doctor.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, "..", "..");
const fx = (n) => readFileSync(join(here, "fixtures", "server-doctor", n), "utf8");
const triage = readFileSync(join(root, "docs", "debug-log-triage.md"), "utf8");
const fails = [];
const expect = (ok, msg) => { if (!ok) fails.push(msg); };

const good = diagnose(fx("good.log"), { triage });
expect(good.verdict === "PASS", `good boot is PASS: got ${good.verdict}`);
expect(good.mods.some((m) => m.name === "LifeFramework" && m.guid === "79636E668EA37AC9" && m.version === "0.4.2"), "good: LifeFramework with guid and version listed");
expect(good.mods.some((m) => m.name === "LF_Everon1989" && m.version === "0.4.1"), "good: the pack listed with its version");
expect(good.mods.length === 4, `good: four mods loaded: got ${good.mods.length}`);
expect(good.engine === "193322" && good.game === "1.9.0.37", "good: engine and game versions read");
expect(good.success.some((l) => /Entered online game state/.test(l)), "good: ready line in the success lines");
expect(good.errors.length === 0, "good: no errors");
const goodText = renderReport(good);
expect(/PASS\s*$/.test(goodText.trim()) && /LifeFramework/.test(goodText), "good: report ends with PASS and names the mods");

const miss = diagnose(fx("missing-mod.log"), { triage });
expect(miss.verdict === "FAIL" && miss.stage === "mods", `missing mod is FAIL at mods: got ${miss.verdict}/${miss.stage}`);
expect(/not found/.test(miss.line), "missing mod: the deciding line is quoted");
expect(/Workshop/i.test(miss.fix ?? ""), "missing mod: a fix naming the Workshop");
expect(!miss.mods.some((m) => m.name === "LifeFramework"), "missing mod: LifeFramework is not in the loaded list");
expect(/FAIL\s*$/.test(renderReport(miss).trim()), "missing mod: report ends with FAIL");

const vme = diagnose(fx("vme.log"), { triage });
expect(vme.verdict === "FAIL" && vme.stage === "scripts", `VME is FAIL at scripts: got ${vme.verdict}/${vme.stage}`);
expect(vme.errors.length === 3, `vme: three error lines: got ${vme.errors.length}`);
expect(vme.errors.some((e) => /SCR_ConsumableBandage/.test(e.line) && /container attributes|OPEN/.test(e.fix ?? "")), "vme: the bandage line carries the triage-table fix");
expect(vme.errors.some((e) => /VME:/.test(e.line) && e.fix === null), "vme: a line the table does not know has no fix, never a made-up one");

const empty = diagnose("", { triage });
expect(empty.verdict === "FAIL", "an empty log is FAIL");

if (fails.length) { for (const f of fails) console.log(`FAIL  ${f}`); console.log("SUMMARY server-doctor-selftest fail"); process.exit(1); }
console.log("SUMMARY server-doctor-selftest ok");
