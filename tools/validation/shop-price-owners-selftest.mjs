// Selftest for validate-shop-price-owners.mjs: one planted double owner per kind must fail, a pack row over
// a core row and a test fixture must pass.
// inputs: tools/**, agent-box/**, .githooks/**
import { checkOwners, parseShopRules } from "./validate-shop-price-owners.mjs";

const fails = [];
const expect = (cond, msg) => { if (!cond) fails.push(msg); };

const HAT = "{A22C976003AA5BA3}Prefabs/Characters/HeadGear/Hat_Ushanka_01/Hat_Ushanka_01_folded.et";
const shop = (rules) => `LF_ShopPricesConfig {
 m_aPriceRules {
${rules.map((r) => `  LF_ShopPriceRule "{AAAAAAAAAAAAAAA1}" {
${r.prefab ? `   m_Prefab "${r.prefab}"\n` : ""}${r.find ? `   m_sFind "${r.find}"\n` : ""}   m_iBuyPrice ${r.buy}
   m_iSellPrice 1
  }`).join("\n")}
 }
}
`;
const cat = (items, offers) => `LF_ItemCatalogueConfig {
 m_aItems {
${items.map((i) => `  LF_CatalogueItem "{AAAAAAAAAAAAAAA2}" {
   m_sId "${i.id}"
${i.price === undefined ? "" : `   m_iPrice ${i.price}\n`}  }`).join("\n")}
 }
 m_aOffers {
${offers.map((o) => `  LF_OutletOffer "{AAAAAAAAAAAAAAA3}" {
   m_sOutlet "${o.outlet}"
   m_sItem "${o.item}"
${o.removed ? "   m_bRemoved 1\n" : ""}  }`).join("\n")}
 }
}
`;
const P = "addons/LF_Pack/Configs/";
const CORE_SHOP = "addons/LifeFramework/Configs/Shop/ShopPrices.conf";
const CORE_CAT = "addons/LifeFramework/Configs/Catalogue/LF_ItemCatalogue.conf";

const keys = parseShopRules(shop([{ prefab: HAT, buy: 35 }, { find: "Prefabs/Items/Food/", buy: 8 }]));
expect(keys && keys.length === 2 && keys[0].key === "prefab:A22C976003AA5BA3" && keys[1].key === "Prefabs/Items/Food/", "rule keys follow LF_ShopPriceResolver.RuleKey");
expect(parseShopRules(cat([], [])) === null, "a catalogue is not a shop price file");

const two = checkOwners([
  { file: P + "Shop/A.conf", text: shop([{ prefab: HAT, buy: 38 }]) },
  { file: P + "Shop/B.conf", text: shop([{ prefab: HAT, buy: 35 }]) },
]);
expect(two.fails.length === 1 && /prefab:A22C976003AA5BA3.*A\.conf:3 and .*B\.conf:3/.test(two.fails[0]), "a prefab rule in two pack shop files fails, naming both");

const findTwice = checkOwners([
  { file: P + "Shop/A.conf", text: shop([{ find: "Prefabs/Items/Food/", buy: 8 }]) },
  { file: P + "Shop/B.conf", text: shop([{ find: "Prefabs/Items/Food/", buy: 9 }]) },
]);
expect(findTwice.fails.length === 1, "a find rule in two pack shop files fails");

const priceTwice = checkOwners([
  { file: P + "Catalogue/Gun.conf", text: cat([{ id: "rifle_vz58p", price: 1100 }], []) },
  { file: P + "Catalogue/Weapons.conf", text: cat([{ id: "rifle_vz58p", price: 1800 }], []) },
]);
expect(priceTwice.fails.length === 1 && /catalogue price of item rifle_vz58p/.test(priceTwice.fails[0]), "an item priced in two pack catalogue files fails");

const offerTwice = checkOwners([
  { file: P + "Catalogue/Gun.conf", text: cat([], [{ outlet: "gun_counter", item: "rifle_vz58p" }]) },
  { file: P + "Catalogue/Weapons.conf", text: cat([], [{ outlet: "gun_counter", item: "rifle_vz58p", removed: true }]) },
]);
expect(offerTwice.fails.length === 1 && /catalogue offer gun_counter\/rifle_vz58p/.test(offerTwice.fails[0]), "an offer added in one pack file and removed in another fails");

const overCore = checkOwners([
  { file: CORE_SHOP, text: shop([{ prefab: HAT, buy: 40 }]) },
  { file: P + "Shop/A.conf", text: shop([{ prefab: HAT, buy: 38 }]) },
  { file: CORE_CAT, text: cat([{ id: "pistol_m9", price: 500 }], [{ outlet: "gun_counter", item: "pistol_m9" }]) },
  { file: P + "Catalogue/Weapons.conf", text: cat([{ id: "pistol_m9", price: 450 }], [{ outlet: "gun_counter", item: "pistol_m9", removed: true }]) },
  { file: "addons/LifeFramework/Configs/Tests/LF_Test_X.conf", text: shop([{ prefab: HAT, buy: 1 }]) },
]);
expect(overCore.fails.length === 0, `a pack over core, and a test fixture, pass (got ${overCore.fails.join(" | ")})`);

const sameFile = checkOwners([{ file: P + "Catalogue/One.conf", text: cat([{ id: "a", price: 1 }], [{ outlet: "o", item: "a" }]) }]);
expect(sameFile.fails.length === 0, "one owner passes");

if (fails.length) {
  for (const f of fails) console.log(`FAIL ${f}`);
  process.exit(1);
}
console.log("shop-price-owners-selftest: OK (prefab and find rules, item price, offer, pack over core, test fixture)");
