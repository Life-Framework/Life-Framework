// tools/validation/rumor-places-selftest.mjs - validate-rumor-places (#4569).
// inputs: tools/validation/validate-rumor-places.mjs, tools/validation/rumor-places-selftest.mjs, tools/map/lib/places.mjs
//
// Builds a throwaway pack with one place layer, one rumor file and the two mission headers, then plants each
// violation and checks the validator names it:
//   - a clean tree passes;
//   - a row whose place no layer registers fails (place);
//   - a duplicate rumor id fails (id);
//   - a rumor file missing from a header fails (header);
//   - a voice entry naming a file that is not there fails (voice).
//
// red-proof: 2026-11-23, the real validator on the tree before this change (the two mission headers without the
// pack rumor file) printed two `FAIL header:` lines and exit 1; with the headers listing it, it passes.
import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { scratchDir } from "../lib/scratch.mjs";
import { checkRumorPlaces } from "./validate-rumor-places.mjs";

let failed = 0;
const check = (ok, msg) => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${msg}`);
  if (!ok) failed++;
};

const GUID = "4A002D70729E375D";
const row = (id, place) => ` LF_RumorDef "{A00B78A173E7FA1D}" {\n  m_sId "${id}"\n  m_sPlaceId "${place}"\n  m_sLineKey "#LF-Line_${id}"\n }\n`;
const rumors = (rows, voices = "") => `LF_RumorsConfig {\n m_aVoices {\n${voices} }\n m_aRumors {\n${rows} }\n}\n`;
const header = (listed) => `LF_MissionHeader {\n m_aLF_ConfigOverlays {\n${listed ? `  "{${GUID}}Configs/Residents/LF_Rumors_Test.conf"\n` : ""} }\n}\n`;

const dir = scratchDir("lf-rumor-places-");
const pack = join(dir, "addons", "LF_Everon1989");
const layers = join(pack, "Worlds", "MainWorld", "MainWorld_Layers");
for (const d of [layers, join(pack, "Configs", "Residents"), join(pack, "Missions"), join(dir, "addons", "LifeFramework", "Configs", "Residents")]) mkdirSync(d, { recursive: true });

const write = (path, text) => writeFileSync(join(pack, path), text);
write("Worlds/MainWorld/MainWorld_Layers/Place_mine.layer", 'GenericEntity Mine {\n components {\n  LF_PlaceComponent "{43954724202F2C18}" {\n   m_sKind "Processor"\n   m_sId "test.mine"\n  }\n }\n coords 1 2 3\n}\n');
write("Configs/Residents/LF_Rumors_Test.conf.meta", `MetaFileClass {\n Name "{${GUID}}Configs/Residents/LF_Rumors_Test.conf"\n}\n`);
writeFileSync(join(dir, "addons", "LifeFramework", "Configs", "Residents", "LF_Rumors.conf"), "LF_RumorsConfig {\n m_aRumors {\n }\n}\n");

try {
  const set = (rows, listed = true, voices = "") => {
    write("Configs/Residents/LF_Rumors_Test.conf", rumors(rows, voices));
    write("Missions/LF_Everon1989.conf", header(listed));
    write("Missions/LF_PlaytestTown.conf", header(listed));
    return checkRumorPlaces(dir).fails;
  };

  let fails = set(row("a", "test.mine"));
  check(fails.length === 0, `a clean tree passes (${fails.join("; ")})`);

  fails = set(row("a", "test.nowhere"));
  check(fails.some((f) => f.startsWith("place:") && f.includes("test.nowhere")), "unknown place fails");

  fails = set(row("a", "test.mine") + row("a", "test.mine"));
  check(fails.some((f) => f.startsWith("id:") && f.includes("already used")), "duplicate rumor id fails");

  fails = set(row("a", "test.mine"), false);
  check(fails.filter((f) => f.startsWith("header:")).length === 2, "a rumor file in neither header fails twice");

  fails = set(row("a", "test.mine"), true, '  "{1111111111111111}Configs/Dialogue/Nope.conf"\n');
  check(fails.some((f) => f.startsWith("voice:") && f.includes("Nope.conf")), "a missing voice conversation fails");
} finally {
  // scratchDir removes the tree when the process ends.
}

process.exit(failed ? 1 : 0);
