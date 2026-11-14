// Selftest for validate-sound-slots.mjs (#323): one planted defect per check must fail, and clean
// files with a pack overlay must pass.
// inputs: tools/**
import { CORE_FILE, checkSoundSlots } from "./validate-sound-slots.mjs";

const fails = [];
const expect = (cond, msg) => { if (!cond) fails.push(msg); };

const conf = (slots) => `LF_SoundSlotsConfig {
 m_aSlots {
${slots.join("\n")}
 }
}
`;
const slot = (id, project, op) => `  LF_SoundSlot "{AAAAAAAAAAAAAAA1}" {
${op ? `   m_sOp "${op}"\n` : ""}${id === null ? "" : `   m_sSlot "${id}"\n`}${project ? `   m_Project "${project}"\n` : ""}  }`;

const RADIO = "{2BC9B5C5F72B71E7}Sounds/Atmosphere/Atmosphere_PubRadio.acp";
const BASE = "{1353C6EAD1DCFE43}Sounds/Props/Siren.acp";
const ctx = {
  addonMeta: (p) => (p === "Sounds/Atmosphere/Atmosphere_PubRadio.acp" ? "2BC9B5C5F72B71E7" : p.startsWith("Sounds/LF_") ? null : undefined),
  basePaths: new Set(["Sounds/Props/Siren.acp"]),
};
const prefab = { file: "addons/LifeFramework/Prefabs/Radio.et", text: ' m_sSoundSlot "pub_radio"\n' };
const pack = "addons/LF_Pack/Configs/Sounds.conf";

const clean = checkSoundSlots(
  [{ file: CORE_FILE, text: conf([slot("pub_radio", RADIO), slot("siren", BASE)]) }, { file: pack, text: conf([slot("pub_radio", BASE, "patch"), slot("siren", null, "remove")]) }],
  [prefab, { file: "addons/LF_Pack/Worlds/Town.layer", text: ` m_sSoundProject "${RADIO}"\n` }, { file: "addons/LifeFramework/Scripts/Game/Tests/LF_Test_X.c", text: `"${RADIO}"` }], ctx);
expect(clean.fails.length === 0, `clean files pass (a pack layer and a core test may name the sound): ${clean.fails.join("; ")}`);
expect(clean.files === 2 && clean.slots === 2, "two files, two core slots");
expect(clean.notes.some((n) => /1 core slot default/.test(n)), "a core default naming a pack-owned sound is noted");

const bad = checkSoundSlots(
  [
    { file: CORE_FILE, text: conf([slot("pub_radio", "{0000000000000001}Sounds/Atmosphere/Atmosphere_PubRadio.acp"), slot("pub_radio", RADIO), slot("noise", null)]) },
    { file: pack, text: conf([slot("pub_radio", "{0000000000000002}Sounds/LF_Missing.acp", "patch"), slot("siren", "{0000000000000003}Sounds/Nope/Gone.acp", "patch")]) },
    { file: "addons/LifeFramework/Configs/Tests/Fixture.conf", text: conf([slot("pub_radio", "{0000000000000004}Sounds/LF_Bad.acp")]) },
  ],
  [
    { file: "addons/LifeFramework/Prefabs/Furnace.et", text: ' m_sSoundSlot "furnace"\n' },
    { file: "addons/LifeFramework/Scripts/Game/Feature/X/LF_X.c", text: 'LF_SoundSlots.Get("kettle");' },
    { file: "addons/LifeFramework/Prefabs/Sawmill.et", text: `  m_sSoundProject "${RADIO}"\n` },
    { file: "addons/LifeFramework/Worlds/DebugWorld/DebugWorld_Layers/A.layer", text: `  m_sSoundProject "${RADIO}"\n` },
  ], ctx);
const has = (re, msg) => expect(bad.fails.some((f) => re.test(f)), `${msg}: ${bad.fails.join(" | ")}`);
has(/has GUID 2BC9B5C5F72B71E7 in its \.meta, the slot says 0000000000000001/, "a wrong GUID fails");
has(/lists this slot twice/, "a duplicate core id fails");
has(/LF_Missing\.acp is an addon path with no \.meta/, "a missing addon project fails");
has(/Gone\.acp is neither an addon resource nor a base game sound project/, "a missing base game project fails");
has(/Furnace\.et: asks for sound slot 'furnace'/, "a prefab slot id the core file lacks fails");
has(/LF_X\.c: asks for sound slot 'kettle'/, "a script slot id the core file lacks fails");
has(/Sawmill\.et:1: names a pack-owned sound/, "a core prefab naming an atmosphere sound fails");
has(/A\.layer:1: names a pack-owned sound/, "a core layer naming an atmosphere sound fails");
expect(!bad.fails.some((f) => /Fixture\.conf/.test(f)), "test fixtures are skipped");

if (fails.length) {
  for (const f of fails) console.log(`FAIL ${f}`);
  process.exit(1);
}
console.log("sound-slots-selftest: OK");
