// tools/validation/us-english-selftest.mjs - validate-us-english.
// inputs: tools/validation/validate-us-english.mjs, tools/lib/us-english.mjs, tools/validation/us-english-selftest.mjs
//
// Runs the check over a planted tree:
//   - a French shop name in a pack string row fails with its file:line; the English one passes;
//   - a British spelling in a core row fails; an Id or Comment that holds the word does not;
//   - a literal French title in a sign config fails, a `#Key` passes;
//   - a texture script that paints "FERMÉ" fails; a docstring that says "colour" does not;
//   - a baselined hit passes; place names (Saint-Philippe) and people's names pass.
//
// red-proof: 2026-11-22 with findNonUs returning [] the "French shop name fails" case printed FAIL.
import { execFileSync } from "node:child_process";
import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { scratchDir } from "../lib/scratch.mjs";

const here = dirname(fileURLToPath(import.meta.url));
let failed = 0;
const check = (ok, msg) => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${msg}`);
  if (!ok) failed++;
};

const row = (id, text) => `    CustomStringTableItem "{0000000000000001}" {\n     Id "${id}"\n     Target_en_us "${text}"\n     Comment "Licence row, Boulangerie in a comment"\n    }\n`;
const st = (rows) => `StringTable {\n Items {\n${rows.join("")} }\n}\n`;

function run(files, baseline) {
  const root = scratchDir("lf-usenglish-");
  for (const [rel, text] of Object.entries(files)) {
    mkdirSync(dirname(join(root, rel)), { recursive: true });
    writeFileSync(join(root, rel), text);
  }
  if (baseline) {
    mkdirSync(join(root, "tools", "validation"), { recursive: true });
    writeFileSync(join(root, "tools", "validation", "us-english-baseline.json"), JSON.stringify({ entries: baseline }));
  }
  try {
    const out = execFileSync("node", [join(here, "validate-us-english.mjs")], { env: { ...process.env, LF_USENGLISH_ROOT: root }, encoding: "utf8" });
    return { code: 0, out };
  } catch (e) {
    return { code: e.status, out: `${e.stdout}${e.stderr}` };
  }
}

const PACK = "addons/P/Language/p.st";
const CORE = "addons/C/Language/c.st";

let r = run({ [PACK]: st([row("LF-Shop_Name_Bakery", "Boulangerie")]) });
check(r.code === 1 && /p\.st:\d+ LF-Shop_Name_Bakery: "Boulangerie"/.test(r.out), "a French shop name fails with file:line");

r = run({ [PACK]: st([row("LF-Shop_Name_Bakery", "Bakery"), row("LF-Sign_Town_Title", "Welcome to Saint-Philippe"), row("LF-Who", "Mr. O. Marchand, Rémi Duval")]) });
check(r.code === 0, "English, place names and people's names pass; an Id or Comment with the word is not text");

r = run({ [CORE]: st([row("LF-Licence_Title", "Your licence")]) });
check(r.code === 1 && /"licence" \(british; US: license\)/.test(r.out), "a British spelling fails and names the US form");

for (const [bad, us] of [["Authorises arrests", "authorizes"], ["a grey badge", "gray"], ["Driving license", "driver's license"], ["Trip cancelled", "canceled"], ["a lay-by", "pull-off"], ["Tinned fish", "canned"], ["Casualty calls", "emergency"], ["Your behaviour", "behavior"], ["He organises it", "organizes"]]) {
  r = run({ [CORE]: st([row("LF-Bad", bad)]) });
  check(r.code === 1 && r.out.includes(`US: ${us}`), `"${bad}" fails and names the US form (${us})`);
}
r = run({ [CORE]: st([row("LF-Ok", "Authorizes a gray badge, the driver's license, a trip canceled, a pull-off, canned fish and the behavior")]) });
check(r.code === 0, "the US forms of those words pass");

r = run({ [PACK]: st([row("LF-Lock", "À vendre : voir le panneau")]) });
check(r.code === 1 && /À vendre/.test(r.out), "an accented French phrase fails");

r = run({ "addons/C/Configs/Signs/S.conf": ' m_sTitle "Pharmacie"\n m_sWhat "#LF-Sign_X_What"\n' });
check(r.code === 1 && /S\.conf:1 m_sTitle: "Pharmacie"/.test(r.out), "a literal French title in a sign config fails");

r = run({ "addons/C/Configs/Signs/S.conf": ' m_sTitle "#LF-Sign_Pharmacy_Title"\n' });
check(r.code === 0, "a sign config with a #Key passes");

r = run({ "tools/assets/door/model.py": '"""Base colour and roughness."""\nc.text("FERMÉ", 1, 2, 3, RED)\n' });
check(r.code === 1 && /model\.py:2 texture text: "FERMÉ"/.test(r.out) && !/colour/.test(r.out), "painted French text fails; a docstring does not");

r = run({ "tools/assets/door/model.py": 'c.text("CLOSED", 1, 2, 3, RED)\n' });
check(r.code === 0 && /texture-text\.json is out of date/.test(r.out), "painted English passes; a missing manifest is a note, not a failure");

r = run({ [PACK]: st([row("LF-Dialogue_X", "Bonjour.")]) }, [{ file: PACK, where: "LF-Dialogue_X", word: "Bonjour" }]);
check(r.code === 0, "a baselined hit passes");

r = run({ [PACK]: st([row("LF-Dialogue_X", "Hello.")]) }, [{ file: PACK, where: "LF-Dialogue_X", word: "Bonjour" }]);
check(r.code === 0 && /STALE/.test(r.out), "a baselined hit that is gone prints STALE");

console.log(failed ? `SUMMARY us-english-selftest fail ${failed}` : "PASS  us-english-selftest");
process.exit(failed ? 1 : 0);
