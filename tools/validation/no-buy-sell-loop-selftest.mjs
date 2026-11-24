// Proves validate-no-buy-sell-loop (#4180) on a scratch addons tree: the cafe juice loop (stock $6, a pack
// inputs: tools/economy/**, tools/validation/**, tools/lib/**
// trader row $18) fails; each other way to buy or sell pairs up (a price-table shop item, a catalogue outlet
// with an offer, a premium buyer's demand row, a trader row that inherits its prefab from the parent's row of
// the same id, the market ceiling standing in for a curved good's trader row, the good-word perk on a trade
// good); things that are not loops stay clear (an extra item no shop lists, a catalogue item no outlet offers,
// a Configs/Tests overlay, a break-even row). The validator runs on the clean tree and on the loop tree.
//
// red-proof: 2026-11-24, with `sell.price <= buy.price` in findBuySellLoops flipped to `sell.price >= buy.price` this
// printed "FAIL clean: expected no loops, got buy-sell:AppleJuice +-2" and "FAIL juice: expected buy-sell:AppleJuice +12
// ..., got nothing" and exited 1; restored, it prints "no-buy-sell-loop-selftest: OK".
import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";
import { scratchDir } from "../lib/scratch.mjs";
import { findBuySellLoops, blocksOf } from "../economy/buy-sell-loops.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const fails = [];
const expect = (ok, msg) => { if (!ok) fails.push(msg); };

const G = {
  juice: "{1DBD99E6347DE1AD}Prefabs/Items/Drinks/AppleJuice.et",
  water: "{A231A5F8D479B5DC}Prefabs/Items/Drinks/WaterBottle.et",
  ore: "{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et",
  plank: "{45D1B36F99DD2FC3}Prefabs/Resources/WoodCutting/WoodPlank.et",
  apple: "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et",
  coat: "{1111111111111111}Prefabs/Characters/Coat.et",
  hat: "{2222222222222222}Prefabs/Characters/Hat.et",
  gem: "{3333333333333333}Prefabs/Items/Gem.et",
  test: "{4444444444444444}Prefabs/Items/TestOnly.et",
};

/** Writes one addons tree; `p` holds the prices that decide each case. */
function tree(p) {
  const root = scratchDir("lf-buysell-");
  const put = (rel, text) => {
    const f = join(root, "addons", rel);
    mkdirSync(dirname(f), { recursive: true });
    writeFileSync(f, text);
  };
  put("LifeFramework/Configs/Shop/ShopPrices.conf", `LF_ShopPricesConfig {
 m_iDefaultBuyPrice 50
 m_iDefaultSellPrice 25
 m_aPriceRules {
  LF_ShopPriceRule "{A000000000000001}" {
   m_sFind "Prefabs/Items/Drinks/"
   m_iBuyPrice 6
   m_iSellPrice 3
  }
 }
 m_aExtraItemPrefabs {
  "${G.gem}"
 }
}
`);
  put("Pack/Configs/Shop/LF_ShopPrices_Pack.conf", `LF_ShopPricesConfig {
 m_aPriceRules {
  LF_ShopPriceRule "{A000000000000002}" {
   m_Prefab "${G.water}"
   m_iBuyPrice ${p.tableWater}
   m_iSellPrice 2
  }
 }
}
`);
  put("Pack/Configs/Shop/LF_ShopStock_Pack.conf", `LF_ShopStockConfig {
 m_aStocks {
  LF_ShopStock "{B000000000000001}" {
   m_sId "cafe"
   m_aItems {
    LF_ShopStockItem "{B000000000000002}" {
     m_Prefab "${G.juice}"
     m_iPrice ${p.cafeJuice}
    }
    LF_ShopStockItem "{B000000000000003}" {
     m_Prefab "${G.ore}"
     m_iPrice ${p.ore}
    }
    LF_ShopStockItem "{B000000000000004}" {
     m_Prefab "${G.plank}"
     m_iPrice ${p.plank}
    }
    LF_ShopStockItem "{B000000000000005}" {
     m_Prefab "${G.apple}"
     m_iPrice ${p.apple}
    }
    LF_ShopStockItem "{B000000000000006}" {
     m_Prefab "${G.test}"
     m_iPrice 1
    }
   }
  }
 }
}
`);
  // The core trader names the ore row; the pack trader overrides only its value (the row id is the link).
  put("LifeFramework/Prefabs/Trader/Trader.et", `GenericEntity {
 components {
  LF_TraderManagerComponent "{C000000000000001}" {
   m_aTradableItems {
    LF_TraderItem "{C000000000000002}" {
     m_ItemPrefab "${G.ore}"
     m_ValuePerItem 1
    }
    LF_TraderItem "{C000000000000003}" {
     m_ItemPrefab "${G.apple}"
     m_ValuePerItem 99
    }
   }
  }
 }
}
`);
  put("Pack/Prefabs/Trader/PackTrader.et", `GenericEntity : "{D000000000000001}Prefabs/Trader/Trader.et" {
 components {
  LF_TraderManagerComponent "{C000000000000001}" {
   m_aTradableItems {
    LF_TraderItem "{C000000000000002}" {
     m_ValuePerItem ${p.oreTrader}
    }
    LF_TraderItem "{C000000000000004}" {
     m_ItemPrefab "${G.juice}"
     m_ValuePerItem 18
     m_bPosted 1
    }
   }
  }
 }
}
`);
  // A price-table shop in a layer sells water; the extra-item gem is listed by no shop.
  put("Pack/Worlds/Town/Shop.layer", `GenericEntity Shop {
 components {
  LF_ShopComponent "{E000000000000001}" {
   m_aShopItems {
    LF_ShopItem "{E000000000000002}" {
     m_ItemPrefab "${G.water}"
     m_iMaxQuantity 5
    }
   }
  }
 }
}
`);
  put("Pack/Configs/Market/LF_BuyerDemand_Pack.conf", `LF_BuyerDemandConfig {
 m_aRows {
  LF_BuyerDemandRow "{F000000000000001}" {
   m_sOp "add"
   m_sBuyer "hardware"
   m_sGood "${G.water}"
   m_iCap 10
   m_iPrice ${p.demandWater}
  }
 }
}
`);
  // The apple is curved: its trader row ($99) is not paid, the ceiling per unit is.
  put("Pack/Configs/Market/LF_Market_Pack.conf", `LF_MarketConfig {
 m_aGoods {
  LF_MarketGoodDef "{F100000000000001}" {
   m_sGood "${G.apple}"
   m_iCeiling ${p.appleCeiling}
   m_iLotSize 2
  }
 }
}
`);
  if (p.appleRemoved) put("Pack/Configs/Market/LF_Market_Pack2.conf", `LF_MarketConfig {
 m_aGoods {
  LF_MarketGoodDef "{F100000000000002}" {
   m_sOp "remove"
   m_sGood "${G.apple}"
  }
 }
}
`);
  put("LifeFramework/Configs/Catalogue/LF_ItemCatalogue.conf", `LF_ItemCatalogueConfig {
 m_aItems {
  LF_CatalogueItem "{F200000000000001}" {
   m_sId "coat"
   m_Prefab "${G.coat}"
   m_iPrice 40
   m_iSellPrice 20
  }
  LF_CatalogueItem "{F200000000000002}" {
   m_sId "hat"
   m_Prefab "${G.hat}"
   m_iPrice ${p.hatPrice}
   m_iSellPrice 20
  }
 }
 m_aOffers {
  LF_OutletOffer "{F200000000000003}" {
   m_sOutlet "rack"
   m_sItem "coat"
  }
 }
}
`);
  put("Pack/Configs/Shop/LF_ShopStock_Coat.conf", `LF_ShopStockConfig {
 m_aStocks {
  LF_ShopStock "{B100000000000001}" {
   m_sId "tailor"
   m_aItems {
    LF_ShopStockItem "{B100000000000002}" {
     m_Prefab "${G.coat}"
     m_iPrice ${p.tailorCoat}
    }
   }
  }
 }
}
`);
  put("LifeFramework/Configs/Tests/LF_Test_Overlay.conf", `LF_BuyerDemandConfig {
 m_aRows {
  LF_BuyerDemandRow "{F300000000000001}" {
   m_sBuyer "test"
   m_sGood "${G.test}"
   m_iPrice 500
  }
 }
}
`);
  put("LifeFramework/Scripts/Game/Feature/Jobs/LF_PerkPrice.c", `class LF_PerkPrice
{
	static const int GOOD_WORD_PERCENT = 10;
	static string TradeOfGood(ResourceName prefab)
	{
		string path = prefab;
		if (path.Contains("Prefabs/Resources/Mining/"))
			return "MINER";
		if (path.Contains("Prefabs/Resources/WoodCutting/"))
			return "LUMBERJACK";
		return string.Empty;
	}
}
`);
  return root;
}

// The clean prices: every buy at or above the best payout (the ore at 22 equals the perked 20 x 1.1: break-even is clear).
const CLEAN = { cafeJuice: 20, ore: 22, oreTrader: 20, plank: 9, tableWater: 10, demandWater: 10, apple: 5, appleCeiling: 10, tailorCoat: 20, hatPrice: 5 };
const clean = tree(CLEAN);
const none = findBuySellLoops(join(clean, "addons"));
expect(none.length === 0, `clean: expected no loops, got ${none.map((l) => `${l.id} +${l.margin}`).join(", ")}`);

// Each case moves one price; the loop it makes is the only one.
const cases = [
  ["juice", { cafeJuice: 6 }, "buy-sell:AppleJuice", 12, /stock "cafe".*trader row C000000000000004/],
  ["inherited trader row + good-word perk", { ore: 21 }, "buy-sell:IronNugget", 1, /trader row C000000000000002.*20 plus the 10% good-word perk/],
  ["premium buyer row vs price-table shop item", { demandWater: 11 }, "buy-sell:WaterBottle", 1, /the price table \(shop item in addons\/Pack\/Worlds\/Town\/Shop\.layer\).*buyer "hardware"/],
  ["market ceiling per unit, not the curved trader row", { apple: 4 }, "buy-sell:Apple", 1, /the market at its ceiling/],
  ["a removed curve hands the good back to its trader row", { appleRemoved: true }, "buy-sell:Apple", 94, /stock "cafe".*trader row C000000000000003/],
  ["catalogue outlet sell price", { tailorCoat: 19 }, "buy-sell:Coat", 1, /stock "tailor".*catalogue outlet item coat/],
];
for (const [label, change, id, margin, path] of cases) {
  const root = tree({ ...CLEAN, ...change });
  const loops = findBuySellLoops(join(root, "addons"));
  const hit = loops.find((l) => l.id === id);
  expect(loops.length === 1 && hit && hit.margin === margin && path.test(hit.path),
    `${label}: expected ${id} +${margin} matching ${path}, got ${loops.map((l) => `${l.id} +${l.margin}: ${l.path}`).join(" | ") || "nothing"}`);
}
// Not loops: the hat is offered by no outlet, so its $5 buy price is no way to buy it; the gem is only an extra item.
const notOffered = findBuySellLoops(join(tree({ ...CLEAN, hatPrice: 1 }), "addons"));
expect(notOffered.length === 0, `an un-offered catalogue item counted as buyable: ${notOffered.map((l) => l.id)}`);

// Block reader: a value holding braces does not end the block.
const blocks = blocksOf(`LF_TraderItem "{ab}" {\n m_ItemPrefab "{1DBD99E6347DE1AD}Prefabs/X.et"\n m_ValuePerItem 18\n}\n`, "LF_TraderItem");
expect(blocks.length === 1 && blocks[0].id === "AB" && blocks[0].fields.m_ValuePerItem === 18, `blocksOf: ${JSON.stringify(blocks)}`);

// The validator: exit 0 on the clean tree, 1 with the juice line on the loop tree.
const run = (root) => spawnSync(process.execPath, [join(here, "validate-no-buy-sell-loop.mjs"), "--root", root], { encoding: "utf8", windowsHide: true });
const ok = run(clean);
expect(ok.status === 0, `validator on the clean tree: exit ${ok.status}: ${ok.stdout}${ok.stderr}`);
const bad = run(tree({ ...CLEAN, cafeJuice: 6 }));
expect(bad.status === 1 && /FAIL buy-sell loop buy-sell:AppleJuice \(\+12 a unit\)/.test(bad.stdout), `validator on the juice tree: exit ${bad.status}: ${bad.stdout}${bad.stderr}`);

for (const f of fails) console.log(`FAIL ${f}`);
if (fails.length) process.exit(1);
console.log("no-buy-sell-loop-selftest: OK");
