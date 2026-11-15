// Proves the gate-tier classifier (tools/lib/gate-tier.mjs, docs/worktrees.md "Gate tiers") on diff
// fixtures cut from real repo files: what is T0, what is data-only T1, and what must stay T2.
// inputs: tools/lib/gate-tier.mjs, tools/lib/world-diff.mjs, tools/test/fixtures/world-diff/
// red-proof: with the GUID checks in dataOnlyProblem off this printed "conf: prefab reference swapped: expected T2,
//   got T1"; with the Ids-block check off, "language conf: an Id changed: expected T2, got T1".
//   T1w: with MAINTAINER_BRANCH never matching this printed "maintainer layer move: expected T1w, got gate tier T2".
import { classify, fileTokens, selectTests, tierLine } from "../lib/gate-tier.mjs";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { diffFiles, handEditProblem, touchedNames } from "../lib/world-diff.mjs";

const fails = [];
const check = (ok, msg) => { if (!ok) fails.push(msg); };

// Real files (trimmed) from addons/LifeFramework at 8fedda14.
const CHAR_CONF = "LF_CharacterConfig {\n m_iMaxPerAccount 1\n}\n";
const PUMP_ET = [
  "GenericEntity {",
  ' ID "203C94B2D40384CE"',
  " components {",
  '  LF_AmbientSwitchComponent "{8963B9BE3BB8E51F}" {',
  "   m_bStartsOn 0",
  "  }",
  '  LF_AmbientEmitterComponent "{E5C7586129AD61DE}" {',
  '   m_sSoundSlot "atmo_fuel_pump"',
  "   m_fRadius 13",
  '   m_sGroup "fuel_pump"',
  "   m_vOffset 0 1.2 0",
  "  }",
  " }",
  "}",
  "",
].join("\n");
const ST = [
  "StringTable {",
  ' ItemClassName "CustomStringTableItem"',
  " Items {",
  '  CustomStringTableItem "{1CB6BD5316FBA315}" {',
  '   Id "EL-ATM_Amount"',
  '   Target_en_us "Amount:"',
  '   Target_de_de "Betrag:"',
  "  }",
  " }",
  "}",
  "",
].join("\n");
const LANG = 'StringTableRuntime {\n Ids {\n  "EL-ATM_Amount"\n  "EL-ATM_Balance"\n }\n Values {\n  "Betrag:"\n  "Kontostand: $"\n }\n}\n';
const LAYOUT = [
  'FrameWidgetClass "{5F1B6C0F31A9E0C2}" {',
  ' components {',
  '  TextWidgetClass "{5F1B6C0F31A9E0C3}" {',
  "   Color 1 1 1 1",
  '   Text "#EL-Death_Title"',
  "  }",
  " }",
  "}",
  "",
].join("\n");
const SHOP_CONF = 'LF_ShopCatalog {\n m_aItems {\n  LF_ShopItem "{2C6B1E7A5D4F3B21}" {\n   m_sPrefab "{A1B2C3D4E5F60718}Prefabs/Items/Bread.et"\n   m_iPrice 5\n  }\n }\n}\n';

const sub = (text, a, b) => {
  if (!text.includes(a)) throw new Error(`fixture: ${a} not in text`);
  return text.replace(a, b);
};
const M = (path, before, after) => ({ path, status: "M", before, after });
const CONF = "addons/LifeFramework/Configs/Account/LF_Characters.conf";
const ET = "addons/LifeFramework/Prefabs/Atmosphere/LF_FuelPumpHum.et";
const STP = "addons/LifeFramework/Language/everonlife_localization.st";
const LANGP = "addons/LifeFramework/Language/everonlife_localization.de_de.conf";
const LAYP = "addons/LifeFramework/UI/layouts/DeathScreen.layout";
const SHOP = "addons/LF_Everon1989/Configs/Shops/General.conf";

const cases = [
  ["docs and tools only", [{ path: "docs/worktrees.md", status: "M" }, { path: "tools/cli.mjs", status: "M" }, { path: "website/src/a.tsx", status: "A" }], 0],
  ["conf: one value", [M(CONF, CHAR_CONF, sub(CHAR_CONF, "m_iMaxPerAccount 1", "m_iMaxPerAccount 2"))], 1],
  ["conf: one value plus docs", [M(CONF, CHAR_CONF, sub(CHAR_CONF, " 1\n", " 3\n")), { path: "docs/features.md", status: "M" }], 1],
  // Decision: a .conf that adds a key is T2. The key may switch on a code path its default never ran,
  // and the line count is how the classifier knows the block structure did not move.
  ["conf adds a key", [M(CONF, CHAR_CONF, sub(CHAR_CONF, " m_iMaxPerAccount 1\n", " m_iMaxPerAccount 1\n m_iSlots 2\n"))], 2],
  ["conf removes a key", [M(CONF, CHAR_CONF, sub(CHAR_CONF, " m_iMaxPerAccount 1\n", ""))], 2],
  ["conf: key renamed in place", [M(CONF, CHAR_CONF, sub(CHAR_CONF, "m_iMaxPerAccount", "m_iMaxPerUser"))], 2],
  ["conf: price changes in a shop", [M(SHOP, SHOP_CONF, sub(SHOP_CONF, "m_iPrice 5", "m_iPrice 7"))], 1],
  ["conf: prefab reference swapped", [M(SHOP, SHOP_CONF, sub(SHOP_CONF, "{A1B2C3D4E5F60718}Prefabs/Items/Bread.et", "{0F1E2D3C4B5A6978}Prefabs/Items/Milk.et"))], 2],
  ["conf: element class swapped", [M(SHOP, SHOP_CONF, sub(SHOP_CONF, "LF_ShopItem ", "LF_ShopService "))], 2],
  ["et: only a float", [M(ET, PUMP_ET, sub(PUMP_ET, "m_fRadius 13", "m_fRadius 14.5"))], 1],
  ["et: a vector", [M(ET, PUMP_ET, sub(PUMP_ET, "m_vOffset 0 1.2 0", "m_vOffset 0 1.5 0.25"))], 1],
  ["et: a string value", [M(ET, PUMP_ET, sub(PUMP_ET, '"fuel_pump"', '"fuel_pump_2"'))], 2],
  ["et adds a component", [M(ET, PUMP_ET, sub(PUMP_ET, "  }\n }\n}", '  }\n  LF_InteractComponent "{1111222233334444}" {\n  }\n }\n}'))], 2],
  // The real diff of 5399a91b: same line count, but the key changed and a GUID reference went.
  ["et: key swapped with a reference (5399a91b)", [M(ET, sub(PUMP_ET, 'm_sSoundSlot "atmo_fuel_pump"', 'm_sSoundProject "{EC2B286A4A5D758A}Sounds/Atmosphere/Atmosphere_FuelPump.acp"'), PUMP_ET)], 2],
  ["et: ID changed", [M(ET, PUMP_ET, sub(PUMP_ET, "203C94B2D40384CE", "203C94B2D40384CF"))], 2],
  ["et: a component GUID changed", [M(ET, PUMP_ET, sub(PUMP_ET, "{8963B9BE3BB8E51F}", "{8963B9BE3BB8E510}"))], 2],
  ["st: German text", [M(STP, ST, sub(ST, '"Betrag:"', '"Betrag (EUR):"')), M(LANGP, LANG, sub(LANG, '"Betrag:"', '"Betrag (EUR):"'))], 1],
  ["st: an Id renamed", [M(STP, ST, sub(ST, '"EL-ATM_Amount"', '"EL-ATM_Sum"'))], 2],
  ["st: a row added", [M(STP, ST, sub(ST, " }\n}", '  CustomStringTableItem "{0A0B0C0D0E0F1011}" {\n   Id "EL-ATM_New"\n   Target_en_us "New"\n  }\n }\n}'))], 2],
  ["language conf: an Id changed", [M(LANGP, LANG, sub(LANG, '"EL-ATM_Balance"', '"EL-ATM_Saldo"'))], 2],
  ["layout: text key", [M(LAYP, LAYOUT, sub(LAYOUT, "#EL-Death_Title", "#EL-Death_Subtitle"))], 1],
  ["layout: colour", [M(LAYP, LAYOUT, sub(LAYOUT, "Color 1 1 1 1", "Color 1 0.8 0.8 1"))], 1],
  ["layout: widget GUID", [M(LAYP, LAYOUT, sub(LAYOUT, "{5F1B6C0F31A9E0C3}", "{5F1B6C0F31A9E0C4}"))], 2],
  ["script", [M("addons/LifeFramework/Scripts/Game/Feature/Money/LF_Money.c", "a\n", "b\n")], 2],
  ["script plus a value", [M(CONF, CHAR_CONF, sub(CHAR_CONF, " 1\n", " 2\n")), M("addons/LifeFramework/Scripts/Game/Core/LF_X.c", "a\n", "b\n")], 2],
  ["meta changed", [{ path: `${CONF}.meta`, status: "M", before: "a", after: "b" }], 2],
  ["new conf (untracked)", [{ path: "addons/LifeFramework/Configs/Account/LF_New.conf", status: "?" }], 2],
  ["conf added", [{ path: "addons/LifeFramework/Configs/Account/LF_New.conf", status: "A" }], 2],
  ["conf deleted", [{ path: CONF, status: "D" }], 2],
  ["layer moved prop", [M("addons/LifeFramework/Worlds/DebugWorld/DebugWorld_Layers/default.layer", "a 1\n", "a 2\n")], 2],
  ["texture", [{ path: "addons/LifeFramework/Assets/x.edds", status: "M" }], 2],
  ["unreadable conf", [M(CONF, null, CHAR_CONF)], 2],
];
for (const [name, files, want] of cases) {
  let got;
  try { got = classify(files); } catch (e) { got = { tier: `threw ${e.message}`, reasons: [] }; }
  check(got.tier === want, `${name}: expected T${want}, got T${got.tier} (${(got.reasons ?? []).join("; ")})`);
}

// --full-gate wins over a data-only diff.
check(classify([M(CONF, CHAR_CONF, sub(CHAR_CONF, " 1\n", " 2\n"))], { forceFull: true }).tier === 2, "--full-gate must force T2");
// The changed string key comes back for the test mapping.
const st = classify([M(STP, ST, sub(ST, '"Betrag:"', '"Summe:"'))]);
check(st.ids.join() === "EL-ATM_Amount", `st: expected ids EL-ATM_Amount, got ${st.ids}`);
check(/T1 \(data only\)/.test(tierLine(st)), `tierLine: ${tierLine(st)}`);

// Test mapping: file stem, GUID from the .meta, config class from the first line.
const toks = fileTokens(CONF, CHAR_CONF, 'MetaFileClass {\n Name "{CBD57E33C475D480}Configs/Account/LF_Characters.conf"\n}\n');
check(["LF_Characters", "CBD57E33C475D480", "LF_CharacterConfig"].every((t) => toks.includes(t)), `fileTokens: ${toks}`);
const tests = [
  { file: "LF_Test_Fast.c", text: "// tier: LOGIC\nclass LF_Test_Fast : LF_Test\n{\n}\n// tier: WORLD\nclass LF_Test_FastWorld : LF_Test\n{\n}\n" },
  { file: "LF_Test_Chars.c", text: "// tier: WORLD\nclass LF_Test_Chars : LF_Test\n{\n  // reads LF_CharacterConfig\n}\n" },
  { file: "LF_Test_Guid.c", text: "// tier: PERSISTENCE\nclass LF_Test_Guid : LF_Test\n{\n  string p = \"{CBD57E33C475D480}\";\n}\n" },
  { file: "LF_Test_Atm.c", text: "// tier: WORLD\nclass LF_Test_Atm : LF_Test\n{\n  string k = \"#EL-ATM_Amount\";\n}\n" },
  { file: "LF_Test_Other.c", text: "// tier: WORLD\nclass LF_Test_Other : LF_Test\n{\n}\n" },
  { file: "LF_Test_Timing.c", text: "// tier: PERF\nclass LF_Test_Timing : LF_Test\n{\n  // reads LF_CharacterConfig\n}\n" },
];
const sel = selectTests(tests, [...toks, "EL-ATM_Amount"]);
check(sel.logic.join() === "LF_Test_Fast", `selectTests logic: ${sel.logic}`);
check(sel.related.sort().join() === "LF_Test_Atm,LF_Test_Chars,LF_Test_Guid", `selectTests related (a PERF test is never pulled into a gate): ${sel.related}`);

// T1w: a maintainer hand edit on a town-maintainer-* branch (docs/worktrees.md "Maintainer world edits"),
// on the real Workbench save in tools/test/fixtures/world-diff/.
{
  const fx = (f) => readFileSync(join(dirname(fileURLToPath(import.meta.url)), "..", "test", "fixtures", "world-diff", f), "utf8");
  const SJ = "addons/LifeFramework/Worlds/DebugWorld/DebugWorld_Layers/Street_Jobs.layer";
  const DF = "addons/LifeFramework/Worlds/DebugWorld/DebugWorld_Layers/default.layer";
  const sjB = fx("Street_Jobs.before.layer");
  const sjA = fx("Street_Jobs.after.layer");
  const known = (ref) => /S105_beige|Jerrycan_01_item|CounterPostOffice/.test(ref);
  const worldEdit = (layers, extra) => {
    const d = diffFiles(layers.map((f) => ({ path: f.path, before: f.before, after: f.after })));
    extra.touched = touchedNames(d);
    return handEditProblem(d, known);
  };
  const real = [M(SJ, sjB, sjA), M(DF, fx("default.before.layer"), fx("default.after.layer"))];
  const w = classify(real, { branch: "ws/town-maintainer-edits", worldEdit });
  check(w.tier === 1 && w.world === true, `maintainer layer move: expected T1w, got ${tierLine(w)}`);
  check(/T1w/.test(tierLine(w)), `T1w tier line: ${tierLine(w)}`);
  check(w.touched?.length === 1 && w.data.length === 2, `T1w touched/data: ${JSON.stringify(w.touched)} ${w.data}`);
  check(classify(real, { branch: "ws/some-slice", worldEdit }).tier === 2, "a layer move on a non-maintainer branch must stay T2");
  check(classify(real, { branch: "ws/town-maintainer-edits" }).tier === 2, "without the world-edit check a layer change must stay T2");
  const prop = [M(SJ, sjB, sjA.replace("m_eSite DEPOT", "m_eSite DROP"))];
  check(classify(prop, { branch: "ws/town-maintainer-edits", worldEdit }).tier === 2, "a component value change in a layer must stay T2");
  const unknownAdd = [M(SJ, sjB, `${sjB}GenericEntity Hand_New : "{0123456789ABCDEF}Prefabs/New.et" {\n coords 1 0 1\n}\n`)];
  check(classify(unknownAdd, { branch: "ws/town-maintainer-edits", worldEdit }).tier === 2, "an add of a never-used prefab must stay T2");
  const mixed = [...real, M(CONF, CHAR_CONF, sub(CHAR_CONF, " 1\n", " 2\n"))];
  check(classify(mixed, { branch: "ws/town-maintainer-edits", worldEdit }).tier === 2, "a layer edit plus a .conf change must not be T1w");
  check(classify([{ path: "addons/X/New.layer", status: "?" }], { branch: "ws/town-maintainer-edits", worldEdit }).tier === 2, "a new layer file must stay T2");
}

if (fails.length) {
  for (const f of fails) console.log(`FAIL  ${f}`);
  process.exit(1);
}
console.log(`PASS  gate-tier: ${cases.length} diff cases, --full-gate, string keys, test mapping, T1w maintainer world edits`);
