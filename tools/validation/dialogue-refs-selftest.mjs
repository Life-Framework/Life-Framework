// inputs: tools/validation/validate-dialogue-refs.mjs, tools/validation/validate-dialogue-patch.mjs
// Red proof of validate-dialogue-refs: a clean in-memory fixture passes, then each planted violation (a key with no row,
// an unregistered dialogue id, a reply to a missing node, a talk action with no dialogue, a scene that spawns a base game
// character, a pack NPC on a one-reply sheet, a placeholder and an empty sign title, a placed sign id with no def) fails
// with its own kind. A key in a script comment and a test fixture key do not.
// red-proof: observed 2026-11-19: with the sign title check removed from checkDialogueRefs this printed
//   "FAIL a TODO sign title fails as sign" and "FAIL an empty sign title fails as sign" and exited 1; restored, it passes.
import { checkDialogueRefs } from "./validate-dialogue-refs.mjs";

const fails = [];
const check = (ok, msg) => { if (!ok) fails.push(msg); };

const ST = (ids) => `StringTable {\n Items {\n${ids.map((id) => `  CustomStringTableItem "{0}" {\n   Id "${id}"\n   Target_en_us "${id === "LF-Sign_Bad_Title" ? "TODO" : "Text"}"\n  }\n`).join("")} }\n}\n`;
const meta = (guid, path) => `MetaFileClass {\n Name "{${guid}}${path}"\n}\n`;
const dialogue = (start, opts) => `LF_DialogueConfig {\n m_sId "baker"\n m_sSpeakerKey "#LF-D_Name"\n m_sStart "${start}"\n m_aNodes {\n  LF_DialogueNode "{1}" {\n   m_sId "greet"\n   m_sLine "greet"\n   m_aOptions {\n${opts.map(([k, n]) => `    LF_DialogueOption "{2}" {\n     m_sTextKey "${k}"\n     m_sNext "${n}"\n    }\n`).join("")}   }\n  }\n  LF_DialogueNode "{3}" {\n   m_sId "bye"\n   m_sLine "bye"\n  }\n }\n m_aLines {\n  LF_DialogueLine "{4}" {\n   m_sId "greet"\n   m_sTextKey "#LF-D_Greet"\n  }\n  LF_DialogueLine "{5}" {\n   m_sId "bye"\n   m_sTextKey "#LF-D_Bye"\n  }\n }\n}\n`;
const index = (id) => `LF_DialogueIndexConfig {\n m_aEntries {\n  LF_DialogueIndexEntry "{6}" {\n   m_sId "${id}"\n   m_Conf "{AAAA000000000001}Configs/Dialogue/LF_Dialogue_Baker.conf"\n  }\n }\n}\n`;
const signs = (title) => `LF_SignConfig {\n m_aSigns {\n  LF_SignDef "{7}" {\n   m_sId "bakery"\n   m_sTitle "${title}"\n  }\n }\n}\n`;
const npc = (comp) => `SCR_ChimeraCharacter : "{9B443DD4C1F6DCAD}Prefabs/Characters/Factions/CIV/Character_CIV_baseLoadout.et" {\n components {\n${comp ? '  LF_DialogueComponent "{8}" {\n   m_sDialogueId "baker"\n  }\n' : ""}  ActionsManagerComponent "{520EA1D2F659CE02}" {\n   additionalActions + {\n    LF_DialogueTalkAction "{9}" {\n    }\n   }\n  }\n }\n}\n`;

function fixture() {
  return new Map([
    ["addons/LifeFramework/Language/core.st", ST(["LF-D_Name", "LF-D_Greet", "LF-D_Bye", "LF-D_SayMore", "LF-D_SayBye", "LF-Sign_Bakery_Title", "LF-Sign_Bad_Title"])],
    ["addons/LifeFramework/Configs/Dialogue/LF_DialogueIndex.conf", index("baker")],
    ["addons/LifeFramework/Configs/Dialogue/LF_Dialogue_Baker.conf", dialogue("greet", [["#LF-D_SayMore", "bye"], ["#LF-D_SayBye", "bye"]])],
    ["addons/LifeFramework/Configs/Dialogue/LF_Dialogue_Baker.conf.meta", meta("AAAA000000000001", "Configs/Dialogue/LF_Dialogue_Baker.conf")],
    ["addons/LifeFramework/Configs/Signs/LF_Signs.conf", signs("#LF-Sign_Bakery_Title")],
    ["addons/LF_Pack/Prefabs/Characters/LF_Baker.et", npc(true)],
    ["addons/LF_Pack/Prefabs/Characters/LF_Baker.et.meta", meta("BBBB000000000001", "Prefabs/Characters/LF_Baker.et")],
    ["addons/LF_Pack/Configs/Events/LF_Scene_Market.conf", `LF_EventScene {\n m_aPrefabs {\n  "{BBBB000000000001}Prefabs/Characters/LF_Baker.et"\n }\n}\n`],
    ["addons/LF_Pack/Worlds/Town/Town_Layers/Shops.layer", `GenericEntity Sign_1 : "{CCCC000000000001}Prefabs/Props/Signs/LF_Sign_Plate.et" {\n components {\n  LF_SignComponent "{A}" {\n   m_sSignId "bakery"\n  }\n }\n}\n`],
    ["addons/LifeFramework/Scripts/Game/Feature/X/LF_X.c", `// the key #LF-Only_In_A_Comment is prose\nclass LF_X { string k = "#LF-D_Name"; }\n`],
    ["addons/LifeFramework/Configs/Tests/LF_Test_Fixture.conf", `X {\n m_sTextKey "#LF-Test_Nowhere"\n}\n`],
  ]);
}

const clean = checkDialogueRefs(fixture());
check(clean.fails.length === 0, `the clean fixture passes (got: ${clean.fails.join(" | ")})`);

function plant(what, kind, edit) {
  const files = fixture();
  edit(files);
  const r = checkDialogueRefs(files);
  check(r.fails.some((f) => f.startsWith(kind + ":")), `${what} fails as ${kind} (got: ${r.fails.join(" | ") || "nothing"})`);
}

plant("a script key with no row", "key", (f) => f.set("addons/LifeFramework/Scripts/Game/Feature/X/LF_X.c", `class LF_X { string k = "#LF-Nowhere_Key"; }\n`));
plant("a layout key with no row", "key", (f) => f.set("addons/LifeFramework/UI/LF_X.layout", `TextWidget {\n Text "#LF-Layout_Missing"\n}\n`));
plant("an unregistered dialogue id", "id", (f) => f.set("addons/LF_Pack/Prefabs/Characters/LF_Baker.et", npc(true).replace('"baker"', '"butcher"')));
plant("a reply to a missing node", "node", (f) => f.set("addons/LifeFramework/Configs/Dialogue/LF_Dialogue_Baker.conf", dialogue("greet", [["#LF-D_SayMore", "nowhere"], ["#LF-D_SayBye", "bye"]])));
plant("a reply with no row", "node", (f) => f.set("addons/LifeFramework/Configs/Dialogue/LF_Dialogue_Baker.conf", dialogue("greet", [["#LF-D_SayMissing", "bye"], ["#LF-D_SayBye", "bye"]])));
plant("a missing start node", "node", (f) => f.set("addons/LifeFramework/Configs/Dialogue/LF_Dialogue_Baker.conf", dialogue("hello", [["#LF-D_SayMore", "bye"], ["#LF-D_SayBye", "bye"]])));
plant("a talk action with no dialogue", "talk", (f) => f.set("addons/LF_Pack/Prefabs/Characters/LF_Baker.et", npc(false)));
plant("a scene spawning a base game character", "talk", (f) => f.set("addons/LF_Pack/Configs/Events/LF_Scene_Market.conf", `LF_EventScene {\n m_aPrefabs {\n  "{11EB9A0D2A5899EA}Prefabs/Characters/Factions/CIV/GenericCivilians/Character_CIV_DenimJacket_1.et"\n }\n}\n`));
plant("a pack NPC on a one-reply sheet", "talk", (f) => f.set("addons/LifeFramework/Configs/Dialogue/LF_Dialogue_Baker.conf", dialogue("greet", [["#LF-D_SayBye", "bye"]])));
plant("a TODO sign title", "sign", (f) => f.set("addons/LifeFramework/Configs/Signs/LF_Signs.conf", signs("TODO")));
plant("an empty sign title", "sign", (f) => f.set("addons/LifeFramework/Configs/Signs/LF_Signs.conf", signs("")));
plant("a sign title whose row reads TODO", "sign", (f) => f.set("addons/LifeFramework/Configs/Signs/LF_Signs.conf", signs("#LF-Sign_Bad_Title")));
plant("a sign title key with no row", "sign", (f) => f.set("addons/LifeFramework/Configs/Signs/LF_Signs.conf", signs("#LF-Sign_Nowhere_Title")));
plant("a placed sign id with no def", "sign", (f) => f.set("addons/LF_Pack/Worlds/Town/Town_Layers/Shops.layer", `GenericEntity Sign_1 {\n components {\n  LF_SignComponent "{A}" {\n   m_sSignId "butcher"\n  }\n }\n}\n`));
plant("a pack overlay removing a placed sign", "sign", (f) => f.set("addons/LF_Pack/Configs/Signs/LF_Signs_Pack.conf", `LF_SignConfig {\n m_aSigns {\n  LF_SignDef "{B}" {\n   m_sOp "remove"\n   m_sId "bakery"\n  }\n }\n}\n`));

if (fails.length) {
  for (const f of fails) console.log(`FAIL ${f}`);
  console.log(`dialogue-refs-selftest: ${fails.length} case(s) failed`);
  process.exit(1);
}
console.log("dialogue-refs-selftest: OK (clean fixture passes, 15 planted violations fail)");
