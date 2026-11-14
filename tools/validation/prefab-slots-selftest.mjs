// Selftest for validate-prefab-slots.mjs (#964): one planted defect per check must fail, and a clean
// core file with a pack overlay must pass.
// inputs: tools/**
import { CORE_FILE, checkSlots } from "./validate-prefab-slots.mjs";

const fails = [];
const expect = (cond, msg) => { if (!cond) fails.push(msg); };

const conf = (slots) => `LF_PrefabSlotsConfig {
 m_aSlots {
${slots.join("\n")}
 }
}
`;
const slot = (id, prefab, op) => `  LF_PrefabSlot "{AAAAAAAAAAAAAAA1}" {
${op ? `   m_sOp "${op}"\n` : ""}${id === null ? "" : `   m_sSlot "${id}"\n`}${prefab ? `   m_Prefab "${prefab}"\n` : ""}   m_aRequires {
    "InventoryItemComponent"
   }
  }`;

const CASH = "{5439738849229352}Prefabs/Items/Currencies/MoneyStack.et";
const BASE = "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et";
const ctx = {
  addonMeta: (p) => (p === "Prefabs/Items/Currencies/MoneyStack.et" ? "5439738849229352" : p.startsWith("Prefabs/Items/LF_") ? null : undefined),
  basePaths: new Set(["Prefabs/Weapons/Handguns/M9/Handgun_M9.et"]),
};
const script = { file: "Money.c", text: 'static const string SLOT_CASH = "cash";\nLF_PrefabSlots.Get(SLOT_CASH);' };
const pack = "addons/LF_Pack/Configs/Slots.conf";

const clean = checkSlots(
  [{ file: CORE_FILE, text: conf([slot("cash", CASH), slot("gun", BASE)]) }, { file: pack, text: conf([slot("cash", BASE, "patch"), slot("gun", null, "remove")]) }],
  [script], ctx);
expect(clean.fails.length === 0, `clean files pass: ${clean.fails.join("; ")}`);
expect(clean.files === 2 && clean.slots === 2, "two files, two core slots");

const bad = checkSlots(
  [
    { file: CORE_FILE, text: conf([slot("cash", "{0000000000000001}Prefabs/Items/Currencies/MoneyStack.et"), slot("cash", CASH), slot("bag", null)]) },
    { file: pack, text: conf([slot("cash", "{0000000000000002}Prefabs/Items/LF_Missing.et", "patch"), slot("gun", "{0000000000000003}Prefabs/Nope/Gone.et", "patch")]) },
    { file: "addons/LifeFramework/Configs/Tests/Fixture.conf", text: conf([slot("cash", "{0000000000000004}Prefabs/Items/LF_Bad.et")]) },
  ],
  [script, { file: "Card.c", text: 'static const string SLOT = "id_card";\nLF_PrefabSlots.Get(SLOT);' }], ctx);
const has = (re, msg) => expect(bad.fails.some((f) => re.test(f)), `${msg}: ${bad.fails.join(" | ")}`);
has(/has GUID 5439738849229352 in its \.meta, the slot says 0000000000000001/, "a wrong GUID fails");
has(/lists this slot twice/, "a duplicate core id fails");
has(/slot bag: a core slot needs m_Prefab/, "a core slot with no prefab fails");
has(/LF_Missing\.et is an addon path with no \.meta/, "a missing addon prefab fails");
has(/Gone\.et is neither an addon resource nor a base game prefab/, "a missing base game prefab fails");
has(/Card\.c: asks for slot 'id_card'/, "a script slot the core file lacks fails");
expect(!bad.fails.some((f) => /Fixture\.conf/.test(f)), "test fixtures are skipped");

if (fails.length) {
  for (const f of fails) console.log(`FAIL ${f}`);
  process.exit(1);
}
console.log("prefab-slots-selftest: OK");
