// Selftest for validate-item-catalogue.mjs (#988): a fixture catalogue with one planted defect per
// inputs: tools/**, agent-box/**, .githooks/**
// check must fail each, and a clean one must pass.
import { checkCatalogues, parseCatalogue } from "./validate-item-catalogue.mjs";

const fails = [];
const expect = (cond, msg) => { if (!cond) fails.push(msg); };

const conf = (items, offers) => `LF_ItemCatalogueConfig {
 m_aItems {
${items}
 }
 m_aOffers {
${offers}
 }
}
`;
const item = (id, prefab, price, sell) => `  LF_CatalogueItem "{AAAAAAAAAAAAAAA1}" {
   m_sId "${id}"
   m_Prefab "${prefab}"
${price === null ? "" : `   m_iPrice ${price}\n`}${sell === null ? "" : `   m_iSellPrice ${sell}\n`}  }`;
const offer = (outlet, it) => `  LF_OutletOffer "{AAAAAAAAAAAAAAA2}" {
   m_sOutlet "${outlet}"
   m_sItem "${it}"
  }`;

const PISTOL = "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et";
const HELMET = "{0000000000000001}Prefabs/Characters/HeadGear/Helmet_Rally_01/Helmet_Rally_01_red.et";
const ADDON = "{4FF102B4CE9B0ED3}Prefabs/Items/Fence/LF_Drill.et";
const ctx = {
  dlc: new Set(["Prefabs/Characters/HeadGear/Helmet_Rally_01/Helmet_Rally_01_red.et"]),
  addonMeta: (p) => (p === "Prefabs/Items/Fence/LF_Drill.et" ? "4FF102B4CE9B0ED3" : p === "Prefabs/Items/Gone.et" ? null : undefined),
  basePaths: new Set(["Prefabs/Weapons/Handguns/M9/Handgun_M9.et", "Prefabs/Characters/HeadGear/Helmet_Rally_01/Helmet_Rally_01_red.et"]),
};

const clean = conf([item("pistol", PISTOL, 500, 250), item("drill", ADDON, 2500, 0)].join("\n"), offer("gun_counter", "pistol"));
const parsed = parseCatalogue(clean);
expect(parsed && parsed.items.length === 2 && parsed.offers.length === 1, "the fixture parses into two items and one offer");
expect(parseCatalogue("LF_ShopPricesConfig {\n}\n") === null, "another root class is not a catalogue");
const ok = checkCatalogues([{ file: "clean.conf", text: clean }], ctx);
expect(ok.fails.length === 0, `a clean catalogue passes (got: ${ok.fails.join("; ")})`);

const run = (items, offers) => checkCatalogues([{ file: "bad.conf", text: conf(items, offers) }], ctx).fails.join("\n");
expect(/DLC-owned/.test(run(item("helmet", HELMET, 90, 45), offer("rack", "helmet"))), "a DLC-owned prefab fails");
expect(/profit loop/.test(run(item("pistol", PISTOL, 100, 400), "")), "a sell price above the price fails");
expect(/profit loop/.test(run(item("pistol", PISTOL, 40, null), "")), "a price below the default sell price fails");
expect(/no \.meta/.test(run(item("gone", "{1234567890ABCDEF}Prefabs/Items/Gone.et", 10, 5), "")), "an addon prefab with no meta fails");
expect(/GUID/.test(run(item("drill", "{1234567890ABCDEF}Prefabs/Items/Fence/LF_Drill.et", 10, 5), "")), "an addon prefab with the wrong GUID fails");
expect(/neither an addon resource nor a base game prefab/.test(run(item("x", "{1234567890ABCDEF}Prefabs/Weapons/Nope.et", 10, 5), "")), "a prefab nobody has fails");
expect(/no catalogue file lists item ghost/.test(run(item("pistol", PISTOL, 500, 250), offer("gun_counter", "ghost"))), "an offer for a missing item fails");
// Overlays merge by id over the core file: a patch of the sell price alone is judged against the core price.
const CORE = "addons/LifeFramework/Configs/Catalogue/LF_ItemCatalogue.conf";
const core = conf(item("pistol", PISTOL, 500, 250), offer("gun_counter", "pistol"));
const sellOnly = (sell) => conf(item("pistol", PISTOL, null, sell), "");
expect(checkCatalogues([{ file: "z-pack.conf", text: sellOnly(200) }, { file: CORE, text: core }], ctx).fails.length === 0, "a pack lowering the sell price below the core price passes");
expect(/profit loop/.test(checkCatalogues([{ file: CORE, text: core }, { file: "z-pack.conf", text: sellOnly(600) }], ctx).fails.join(" | ")), "a pack raising the sell price above the core price fails");
const noGame = checkCatalogues([{ file: "c.conf", text: clean }], { ...ctx, basePaths: null });
expect(noGame.fails.length === 0 && noGame.notes.length === 1, "without a game install the base game half is skipped with a note");

if (fails.length) {
  for (const f of fails) console.log(`FAIL ${f}`);
  process.exit(1);
}
console.log("item-catalogue-selftest: OK (DLC, profit loop, overlay merge, missing and mismatched prefab, unknown item, no-game skip)");
