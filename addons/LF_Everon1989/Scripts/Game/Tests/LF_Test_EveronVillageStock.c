// red-proof (cafe and general store loop check): 2026-11-18 with the cafe and general store apple rows at $2, `tools\cli test --no-build --only LF_Test_EveronVillageStock` failed 4 lines (cafe and general_store: `Apple.et sells for 2 and buys back for 4` and `sells for 2 and the market pays up to 5`); at $6 it passes.
// red-proof: observed 2026-11-18 (#3218): on main (no supermarket, hardware, seed_feed, newsagent or mechanic stock) `tools\cli test --no-build --only LF_Test_EveronVillageStock` failed on every `has a stock under the pack header` line.
//! #3218: the new Saint-Philippe village shops each have a stock in the pack's shop stock overlay.
//! Checked against the merged pack config (pack leg): each stock id loads with the items the card names,
//! every prefab resolves, no item is sold for less than the store pays back (no money loop), and a good on
//! the market curve is never sold cheaper than the market's ceiling (no shop-to-trader loop, #3121).
//! The hardware counter copies the general store's rows (same prefab, same price) while both are open.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronVillageStock : LF_Test
{
	protected static const ResourceName WATER = "{A231A5F8D479B5DC}Prefabs/Items/Drinks/WaterBottle.et";
	protected static const ResourceName JUICE = "{1DBD99E6347DE1AD}Prefabs/Items/Drinks/AppleJuice.et";
	protected static const ResourceName BEER = "{4CBB1F9A04D15772}Prefabs/Items/Drinks/BeerBottle.et";
	protected static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	protected static const ResourceName CABBAGE = "{9FEBBF75B1D525EE}Prefabs/Items/Food/Cabbage.et";
	protected static const ResourceName PLUM = "{C3E35D690317B5BE}Prefabs/Items/Food/Plum.et";
	protected static const ResourceName CRACKERS = "{8F2C06D1F757C40F}Prefabs/Items/Food/LF_Crackers.et";
	protected static const ResourceName DRIED_MEAT = "{486317A3719FD166}Prefabs/Items/Food/LF_DriedMeat.et";
	protected static const ResourceName ETOOL = "{0619993AE6CDBF85}Prefabs/Items/Equipment/Accessories/ETool_ALICE/ETool_ALICE.et";
	protected static const ResourceName AXE = "{9CAC68BA551417A4}Prefabs/Tools/Axe/Axe.et";
	protected static const ResourceName GRAVEL = "{344090890C63F4CA}Prefabs/Items/Resources/GravelBag.et";
	protected static const ResourceName CEMENT = "{D58674633332DC86}Prefabs/Items/Resources/CementBag.et";
	protected static const ResourceName BRICK = "{0D5DA590E223A195}Prefabs/Items/Resources/Brick.et";
	protected static const ResourceName PLANK = "{45D1B36F99DD2FC3}Prefabs/Resources/WoodCutting/WoodPlank.et";
	protected static const ResourceName SANDBAG = "{5B10C0DE7A11E030}Prefabs/Items/Resources/Sandbag.et";
	protected static const ResourceName FLASHLIGHT = "{575EA58E67448C2A}Prefabs/Items/Equipment/Flashlights/Flashlight_Soviet_01/Flashlight_Soviet_01.et";
	protected static const ResourceName RADIO = "{D58E740DC2168D92}Prefabs/Items/Comms/LF_HandsetRadio.et";
	protected static const ResourceName SEEDS = "{F77BE55D8D3AEBA3}Prefabs/Items/Seeds/CabbageSeeds.et";
	protected static const ResourceName BAIT = "{53E0B27FA119A866}Prefabs/Items/Fishing/LF_BaitTin.et";
	protected static const ResourceName ROD = "{0327968D17D249FD}Prefabs/Items/Fishing/LF_FishingRod.et";
	protected static const ResourceName MAP = "{A5EDB92E7A42913E}Prefabs/Items/LF_TouristMap.et";
	protected static const ResourceName PARTS = "{7A94C1E05D3B2F68}Prefabs/Items/Resources/LF_CarParts.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/village-stock";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ShopManagerComponent manager = LF_ShopManagerComponent.GetInstance();
		ctx.NotNull(manager, "the shop manager exists");
		if (!manager)
			return;

		CheckStock(ctx, manager, "supermarket", {WATER, JUICE, BEER, APPLE, CABBAGE, PLUM, CRACKERS, DRIED_MEAT});
		CheckStock(ctx, manager, "hardware", {ETOOL, AXE, GRAVEL, CEMENT, BRICK, PLANK, SANDBAG, FLASHLIGHT, RADIO});
		CheckStock(ctx, manager, "seed_feed", {SEEDS, BAIT, ROD, WATER});
		CheckStock(ctx, manager, "newsagent", {MAP, WATER});
		CheckStock(ctx, manager, "mechanic", {PARTS});

		CheckHardwareCopiesGeneralStore(ctx);

		//! The cafe menu and the general store were priced apart from the market curve: an apple was $2 at both
		//! while the trader paid up to $5. Every row of both stocks gets the same no-loop checks.
		CheckNoLoops(ctx, manager, "cafe");
		CheckNoLoops(ctx, manager, "general_store");
	}

	//------------------------------------------------------------------------------------------------
	//! Every row of one stock: no buy-back and no market-ceiling loop (CheckItem's checks, without a fixed item list).
	protected void CheckNoLoops(LF_TestContext ctx, LF_ShopManagerComponent manager, string id)
	{
		LF_ShopStock stock = LF_ShopStockConfig.Get().Find(id);
		ctx.NotNull(stock, string.Format("the %1 has a stock under the pack header", id));
		if (!stock)
			return;

		foreach (LF_ShopStockItem row : stock.m_aItems)
		{
			if (row)
				CheckLoopFree(ctx, manager, id, row);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckStock(LF_TestContext ctx, LF_ShopManagerComponent manager, string id, array<ResourceName> wanted)
	{
		string lookup = id;
		if (RedProof())
			lookup = id + "_missing";

		LF_ShopStock stock = LF_ShopStockConfig.Get().Find(lookup);
		ctx.NotNull(stock, string.Format("the %1 has a stock under the pack header", id));
		if (!stock)
			return;

		ctx.True(stock.m_aItems && stock.m_aItems.Count() == wanted.Count(), string.Format("the %1 stock lists %2 items", id, wanted.Count()));
		foreach (ResourceName prefab : wanted)
		{
			CheckItem(ctx, manager, id, stock, prefab);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckItem(LF_TestContext ctx, LF_ShopManagerComponent manager, string id, LF_ShopStock stock, ResourceName prefab)
	{
		Resource resource = Resource.Load(prefab);
		ctx.True(resource && resource.IsValid(), string.Format("%1: %2 loads", id, prefab));

		LF_ShopStockItem row = stock.FindItem(prefab);
		ctx.NotNull(row, string.Format("%1: %2 is in the stock", id, prefab));
		if (!row)
			return;

		ctx.True(row.m_iPrice > 0, string.Format("%1: %2 has a price (got %3)", id, prefab, row.m_iPrice));
		CheckLoopFree(ctx, manager, id, row);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckLoopFree(LF_TestContext ctx, LF_ShopManagerComponent manager, string id, LF_ShopStockItem row)
	{
		ResourceName prefab = row.m_Prefab;

		//! An item outside the shop catalogue (the tourist map) is never bought back, so it has no sell price.
		if (manager.GetItem(prefab))
		{
			int buyPrice = 0;
			int sellPrice = 0;
			bool hidden = true;
			manager.GetPrices(prefab, buyPrice, sellPrice, hidden);
			ctx.True(row.m_iPrice >= sellPrice, string.Format("%1: %2 sells for %3 and buys back for %4: no profit loop", id, prefab, row.m_iPrice, sellPrice));
		}

		//! The market pays up to its ceiling for a curved good; the shop never sells it for less.
		LF_MarketGoodDef curve = LF_MarketBook.Curved(prefab);
		if (curve)
			ctx.True(row.m_iPrice >= curve.m_iCeiling, string.Format("%1: %2 sells for %3 and the market pays up to %4", id, prefab, row.m_iPrice, curve.m_iCeiling));
	}

	//------------------------------------------------------------------------------------------------
	//! Until the general store's hardware rows move out (#3276), both counters charge the same.
	protected void CheckHardwareCopiesGeneralStore(LF_TestContext ctx)
	{
		LF_ShopStock hardware = LF_ShopStockConfig.Get().Find("hardware");
		LF_ShopStock general = LF_ShopStockConfig.Get().Find("general_store");
		ctx.NotNull(general, "the general store has a stock");
		if (!hardware || !general)
			return;

		foreach (LF_ShopStockItem row : general.m_aItems)
		{
			if (!row)
				continue;

			LF_ShopStockItem copy = hardware.FindItem(row.m_Prefab);
			if (!copy)
				continue;

			ctx.Equal(row.m_iPrice, copy.m_iPrice,string.Format("hardware charges what the general store charges for %1", row.m_Prefab));
		}
	}
}
