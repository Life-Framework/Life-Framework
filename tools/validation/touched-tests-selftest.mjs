// Selftest for tools/lib/touched-tests.mjs (revert audit 2026-11-27): the tests `wt ship` tells an agent to run for
// the exact file lists of five merges the main guard reverted. Reads the real test sources of this checkout.
// inputs: tools/lib/touched-tests.mjs, tools/lib/ship-direct.mjs, tools/cli.mjs, addons/**/Scripts/Game/Tests/*.c
// red-proof (conf readers, 2026-11-27): with the previous lib (no scanSources, no loader or accessor pass) it printed
// "FAIL  #5173: LF_Signs.conf rows run LF_Test_SignNext although no test names the file", "FAIL  #5173: the printed data
// line carries LF_Test_SignNext" and "FAIL  only runnable tests are named", exit 1.
// red-proof: observed 2026-11-27 with pickTests returning `packLeg: null` for every diff, it printed "FAIL  #5074 job
// tables: the pack leg names LF_Test_EveronJobTables", "FAIL  #5074: the printed command carries the test", "FAIL  #5096
// mannequins: the pack leg names LF_Test_EveronMannequins" and "FAIL  a pack string change prints the pack leg", exit 1.
import { readFileSync, existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { addedStringIds, isPackData, pickTests, renderLines, scanSources, scanTests, tokensFor } from "../lib/touched-tests.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const fails = [];
const expect = (cond, msg) => { if (!cond) fails.push(msg); };

const tests = scanTests(ROOT);
expect(tests.length > 500 && tests.some((t) => t.pack), `the scan finds the core and pack tests (got ${tests.length})`);
const pick = (files, addedIds = []) => pickTests({ files, tests, addedIds, readFile: (p) => (existsSync(join(ROOT, p)) ? readFileSync(join(ROOT, p), "utf8") : null) });
const P = "addons/LF_Everon1989/";
const C = "addons/LifeFramework/";

// #5074 (reverted for everon/job-tables): a pack jobs conf and its level card; the test never names either file.
{
  const r = pick([`${P}Configs/Jobs/LF_Jobs_Everon1989.conf`, `${P}Configs/Jobs/LF_TradeUnlocks_Everon1989.conf`, `${P}Language/lf_everon1989_localization.st`, "docs/features/jobs-level-licenses-whitelist.md"]);
  expect(r.packLeg?.includes("LF_Test_EveronJobTables"), "#5074 job tables: the pack leg names LF_Test_EveronJobTables");
  const text = renderLines(r).join("\n");
  expect(/test --no-build --only [^\n]*LF_Test_EveronJobTables/.test(text), "#5074: the printed command carries the test");
}
// #5096 (everon/mannequins): a pack catalogue conf and a core UI script.
{
  const r = pick([`${P}Configs/Catalogue/LF_Catalogue_Clothing_Everon1989.conf`, `${P}Scripts/Game/Tests/LF_Test_EveronShopNames.c`, `${C}Scripts/Game/Feature/UIKit/LF_UIScreen.c`]);
  expect(r.packLeg?.includes("LF_Test_EveronMannequins"), "#5096 mannequins: the pack leg names LF_Test_EveronMannequins");
  expect(r.named.has("LF_Test_EveronShopNames"), "a changed test file runs its own class");
  expect([...r.named.keys()].some((c) => !c.startsWith("LF_Test_Everon")), "a core class change names the core tests that use it");
}
// #4899 (signs/next, a core leg test): a core conf named by the tests, not a pack file.
{
  const r = pick([`${C}Configs/Signs/LF_Signs.conf`, `${C}Prefabs/Props/LF_ShiftBoardPost.et`, `${P}Worlds/MainWorld/MainWorld_Layers/Place_coalmans-brow_mine.layer`]);
  expect(r.named.has("LF_Test_SignNext"), "#4899: LF_Signs.conf runs LF_Test_SignNext");
  expect(r.packLeg === null, "a core conf alone does not print the pack leg");
  expect(r.layers.length === 1, "a world layer asks for the placement audit");
}
// #5173 (reverted as #5174 for signs/next, relanded as #5175): LF_Signs.conf gained rows without the Next line every
// sign needs. LF_Test_SignNext never names the conf's file name in code; it loads it through LF_SignCatalog.CONFIG_PATH
// (its only "LF_Signs.conf" is the words of an assertion message) and walks `config.m_aSigns`. The file list is the real one.
{
  const files5173 = [
    `${C}Configs/Signs/LF_Signs.conf`, `${C}Language/everonlife_localization.de_de.conf`, `${C}Language/everonlife_localization.en_us.conf`,
    `${C}Language/everonlife_localization.st`, `${C}Prefabs/Props/Property/LF_HouseSaleSign.et`, `${C}Prefabs/Props/Property/LF_PlotSign.et`,
    `${C}Scripts/Game/Feature/Property/LF_HouseSaleComponent.c`, `${C}Scripts/Game/Feature/Property/LF_PlotComponent.c`,
    `${C}Scripts/Game/Feature/Signs/LF_SignComponent.c`, `${C}Scripts/Game/Tests/LF_Test_SignCoverage.c`, `${C}Scripts/Game/Tests/LF_Test_SoldSignFace.c`,
    "docs/features/property.md",
  ];
  const noProse = tests.map((t) => (t.cls === "LF_Test_SignNext" ? { ...t, text: t.text.replace("LF_Signs.conf loads", "the sign config loads") } : t));
  expect(!/LF_Signs/.test(noProse.find((t) => t.cls === "LF_Test_SignNext").text.replace(/LF_SignsOverlay/g, "")), "the replay test source no longer names LF_Signs in prose");
  const r = pickTests({ files: files5173, tests: noProse, sources: () => scanSources(ROOT), readFile: (p) => (existsSync(join(ROOT, p)) ? readFileSync(join(ROOT, p), "utf8") : null) });
  expect(r.named.has("LF_Test_SignNext"), "#5173: LF_Signs.conf rows run LF_Test_SignNext although no test names the file");
  const why = (r.named.get("LF_Test_SignNext") ?? []).join(" | ");
  expect(/LF_Signs\.conf via LF_SignCatalog/.test(why) && /rows via m_aSigns/.test(why), `#5173: the reasons are the loader class and the accessor (got ${why})`);
  expect(r.dataCls?.has("LF_Test_SignNext") && /tests that read the changed data:\n\s+tools\\cli test --no-build --only [^\n]*LF_Test_SignNext/.test(renderLines(r).join("\n")), "#5173: the printed data line carries LF_Test_SignNext");
  expect(renderLines(r).length <= 22, `#5173: the advice stays short (${renderLines(r).length} lines)`);
  // A test that is only a fixture (no [LF_TestInfo]) is never printed: `--only` cannot run it.
  expect([...r.named.keys()].every((c) => tests.find((t) => t.cls === c)?.runnable), "only runnable tests are named");
}
// A conf nobody loads by name, and a comment that names a file, add nothing.
{
  const r = pickTests({ files: [`${C}Configs/Signs/LF_Unread_Nobody.conf`], tests, sources: () => scanSources(ROOT), readFile: () => null });
  expect(r.named.size === 0 && renderLines(r).join("\n").includes("no test names it"), "an unread conf names no test and falls back to the fast tier");
}
// #3653 (persist-owned-implements): a save payload.
{
  const r = pick([`${C}Scripts/Game/Feature/Harvest/LF_HarvestContractSavePayload.c`, "tools/validation/record-shapes/harvest_deposits.json"]);
  expect(r.persistence.length === 2 && /tier persistence/.test(renderLines(r).join("\n")), "#3653: a save payload asks for --tier persistence");
}
// #1910 (world pins): the pin list decides the world each test boots in.
expect(/--tier all/.test(renderLines(pick(["tools/test/world-pins.json"])).join("\n")), "#1910: world-pins.json asks for --tier all");

// Quiet diffs print nothing.
expect(renderLines(pick(["docs/features/money-economy.md", "website/public/docs-images/1/a.png", "tools/validation/x-selftest.mjs"])).length === 0, "a docs, website and tools diff prints nothing");
expect(renderLines(pick([])).length === 0, "an empty diff prints nothing");
// Pack data classification.
expect(isPackData(`${P}Configs/Jobs/x.conf`) && isPackData(`${P}Language/x.st`) && isPackData(`${P}Prefabs/a.et`), "pack configs, strings and prefabs are pack data");
expect(!isPackData(`${P}Configs/Jobs/x.conf.meta`) && !isPackData(`${P}Scripts/Game/Tests/LF_Test_EveronX.c`) && !isPackData(`${P}Sounds/a.wav`) && !isPackData(`${C}Configs/x.conf`), "metas, tests, sounds and core files are not pack data");
// Tokens.
expect(tokensFor(`${P}Language/lf_everon1989_localization.en_us.conf`, "").length === 0, "a string table adds no file-name token");
expect(tokensFor(`${C}Scripts/Game/Feature/X/LF_Thing.c`, "modded class LF_Other : Managed\nclass LF_Thing {}").join() === "LF_Thing,LF_Other", "a .c adds its stem and its declared classes");
expect(tokensFor(`${C}Configs/Jobs.conf`, "").length === 0, "a stem under 6 characters is no token");
expect(addedStringIds('+     Id "LF-A_B"\n-     Id "LF-Gone"\n+     Target_en_us "x"\n+   Id "LF-C"').join() === "LF-A_B,LF-C", "only added Ids are read from a string diff");
{
  const r = pick([`${P}Language/lf_everon1989_localization.st`], ["LF-ItemName_LF_Reaper_01"]);
  expect(r.packLeg?.length > 0, "a pack string change prints the pack leg");
}

for (const f of fails) console.log(`FAIL  ${f}`);
if (fails.length) process.exit(1);
console.log("touched-tests-selftest: ok");
