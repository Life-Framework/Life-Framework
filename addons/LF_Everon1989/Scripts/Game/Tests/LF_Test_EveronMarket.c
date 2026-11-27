// red-proof (2026-11-19, #3823): on main with the scrapyard curves in the pack market, `--only everon/market` failed `no unexpected curve` and the trader-row lines for scrap metal, copper wire and rubber scrap; the goods are now in the expected set and skipped in the per-good loop (LF_Test_EveronScrapItems reads them).
// red-proof (2026-11-19, #3237): the copper bar curve is re-added and the four mining goods are skipped in the per-good loop (LF_Test_EveronMiningMarket reads them).
// red-proof (2026-11-27, no-copper-gold): the pack's `patch` row for the copper bar (LF_Market_Everon1989.conf) back in place fails `the pack removes the copper bar curve` and `no unexpected curve: ...CopperBar.et`; the `remove` row passes.
// red-proof: observed 2026-11-18 `tools\cli test --no-build --red LF_Test_EveronMarket` printed `RED PROOF OK`; with the pack layer removed (an empty header as the pack source) it failed e.g. `GoldBar.et base is today's sell price (expected 120, got 200)` and the market-on, copper-removed and eight-goods checks. `--only` then passed under the pack header. 2026-11-18 (#3028): the recovery check passes hundredths (it went hollow when Recover moved to hundredths); with a 1 h window instead of 24 h it failed `everon/market`, with 24 h it passes.
// red-proof (2026-11-19, #3622): with the granary grain curve (#3638) the old count of 11 failed on a `--tier all` run (found 12). The test now asserts the set of goods; `--red LF_Test_EveronMarket` printed RED PROOF OK and `--tier all` is green.
//! #2849 in game: under the Everon 1989 header (the pack leg of `tools\cli test`), the pack's market overlay
//! switches the market on and gives the eight goods the 1989 trader buys (apples, plums, cement, gravel, sand,
//! iron bars, planks, cabbages) a curve. Each curve is read against the pack trader's own price list: ceiling
//! and base are the trader's price, the floor is half of it (rounded up), a unit takes 2 percent of the base off,
//! and a floored good is back within 10 percent of its base after 24 real hours. The core copper and gold bar
//! curves are removed by the pack (the island has neither rock, so the price board lists neither).
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronMarket : LF_Test
{
	static const ResourceName OVERLAY = "{3A9F6E21C47D8B50}Configs/Market/LF_Market_Everon1989.conf";
	static const ResourceName TRADER = "{3A9F6E21C47D8B71}Prefabs/Trader/LF_UniversalTrader_Everon1989.et";
	static const ResourceName CORE_TRADER = "{5DDBFA4225659FFE}Prefabs/Trader/UniversalTrader.et";
	static const ResourceName COPPER_BAR = "{577A665F1662E7AD}Prefabs/Resources/Mining/CopperBar.et";
	static const ResourceName GOLD_BAR = "{ACC9A10F0F20F5C4}Prefabs/Resources/Mining/GoldBar.et";
	static const ResourceName WOOD_PLANK = "{45D1B36F99DD2FC3}Prefabs/Resources/WoodCutting/WoodPlank.et";
	static const ResourceName CABBAGE = "{9FEBBF75B1D525EE}Prefabs/Items/Food/Cabbage.et";
	static const ResourceName IRON_BAR = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";
	static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	static const ResourceName PLUM = "{C3E35D690317B5BE}Prefabs/Items/Food/Plum.et";
	static const ResourceName CEMENT = "{D58674633332DC86}Prefabs/Items/Resources/CementBag.et";
	static const ResourceName GRAVEL = "{344090890C63F4CA}Prefabs/Items/Resources/GravelBag.et";
	static const ResourceName SAND = "{5B10C0DE7A11E030}Prefabs/Items/Resources/Sandbag.et";
	static const int HOURS_24 = 86400;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/market";
	}

	//------------------------------------------------------------------------------------------------
	//! The `m_aTradableItems` rows of a trader prefab's LF_TraderManagerComponent, or null.
	static BaseContainerList TraderRows(ResourceName prefab)
	{
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return null;

		IEntitySource source = resource.GetResource().ToEntitySource();
		if (!source)
			return null;

		for (int i = 0, count = source.GetComponentCount(); i < count; i++)
		{
			IEntityComponentSource component = source.GetComponent(i);
			if (component.GetClassName() == "LF_TraderManagerComponent")
				return component.GetObjectArray("m_aTradableItems");
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! What the trader prefab pays for a good, or -1 when it does not list it.
	static int TraderPrice(BaseContainerList rows, ResourceName good)
	{
		for (int i = 0, count = rows.Count(); i < count; i++)
		{
			BaseContainer row = rows.Get(i);
			ResourceName prefab;
			row.Get("m_ItemPrefab", prefab);
			if (!LF_Utils.SamePrefab(prefab, good))
				continue;

			int value;
			row.Get("m_ValuePerItem", value);
			return value;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	static bool TraderBuys(ResourceName prefab, ResourceName good)
	{
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return false;

		IEntitySource source = resource.GetResource().ToEntitySource();
		if (!source)
			return false;

		for (int i = 0, count = source.GetComponentCount(); i < count; i++)
		{
			IEntityComponentSource component = source.GetComponent(i);
			if (component.GetClassName() != "LF_RestrictedInventoryStorageComponent")
				continue;

			array<ResourceName> tradable = {};
			component.Get("m_TradablePrefabs", tradable);
			foreach (ResourceName entry : tradable)
			{
				if (LF_Utils.SamePrefab(entry, good))
					return true;
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");
		if (header)
			ctx.True(header.m_aLF_ConfigOverlays && header.m_aLF_ConfigOverlays.Find(OVERLAY) != -1, "the header lists the Everon market overlay");

		// The act: the pack overlays layered over the core file.
		if (RedProof())
		{
			// No pack layer: an empty header as the pack source.
			LF_MissionConfigPackSource none = new LF_MissionConfigPackSource();
			none.SetHeader(new LF_MissionHeader());
			LF_ConfigService.SetPackSource(none);
		}

		LF_MarketConfig.Reload();
		LF_MarketConfig config = LF_MarketConfig.Get();
		if (RedProof())
		{
			LF_ConfigService.Reset();
			LF_MarketConfig.Reload();
		}

		ctx.NotNull(config, "the market config loads under the pack header");
		if (!config)
			return;

		ctx.True(config.m_bEnabled, "the pack switches the market on");
		ctx.True(config.FindGood(COPPER_BAR) == null, "the pack removes the copper bar curve: the island has no copper, so the price board does not list it");
		ctx.True(config.FindGood(GOLD_BAR) == null, "the pack removes the gold bar curve");
		// The set of goods, not a count: the eight the trader buys, the four lumber goods of the wholesale warehouse (#3269)
		// and the granary grain (#3622, its own lot curve, read by LF_Test_EveronGrainBuyers).
		// The honey jar and honeycomb (#3448) have their own slow curves, read by LF_Test_EveronHoney.
		array<ResourceName> expected = {IRON_BAR, APPLE, PLUM, CEMENT, GRAVEL, SAND, WOOD_PLANK, CABBAGE, LF_Test_EveronLumberBuyers.BOARD, LF_Test_EveronLumberBuyers.POST, LF_Test_EveronLumberBuyers.FIREWOOD, LF_Test_EveronGrainBuyers.GRAIN, LF_Test_EveronHoney.JAR, LF_Test_EveronHoney.COMB, LF_Test_EveronMiningMarket.COAL, LF_Test_EveronScrapItems.SCRAP_METAL, LF_Test_EveronScrapItems.COPPER_WIRE, LF_Test_EveronScrapItems.RUBBER_SCRAP};
		foreach (ResourceName want : expected)
			ctx.True(config.FindGood(want) != null, "a curve exists for " + want);
		foreach (LF_MarketGoodDef have : config.m_aGoods)
		{
			bool known;
			foreach (ResourceName want2 : expected)
			{
				if (LF_Utils.SamePrefab(have.m_sGood, want2))
					known = true;
			}
			ctx.True(known, "no unexpected curve: " + have.m_sGood);
		}

		BaseContainerList rows = TraderRows(TRADER);
		BaseContainerList coreRows = TraderRows(CORE_TRADER);
		ctx.True(rows != null, "the pack trader prefab lists its prices");
		ctx.True(coreRows != null, "the core trader prefab lists its prices");
		if (!rows || !coreRows)
			return;

		ctx.Equal(17, rows.Count(), "the pack trader lists 17 goods, each once (apple juice for the arrival route)");
		ctx.Equal(TraderPrice(rows, WOOD_PLANK), 6, "the pack trader buys planks at 6");
		ctx.Equal(TraderPrice(rows, CABBAGE), 4, "the pack trader buys cabbages at 4");
		ctx.Equal(-1, TraderPrice(coreRows, WOOD_PLANK), "the core trader list is untouched: no planks");
		ctx.Equal(-1, TraderPrice(coreRows, CABBAGE), "the core trader list is untouched: no cabbages");

		foreach (LF_MarketGoodDef good : config.m_aGoods)
		{
			// The lumber goods ride the wholesale curves of #3269 (own floor, drop and recovery); LF_Test_EveronLumberBuyers reads them.
			if (LF_Test_EveronLumberBuyers.IsLumberGood(good.m_sGood))
				continue;

			// The granary grain is priced per lot with its own curve (#3622); LF_Test_EveronGrainBuyers reads it.
			if (LF_Utils.SamePrefab(good.m_sGood, LF_Test_EveronGrainBuyers.GRAIN))
				continue;

			// The iron bar, cement bag and coal lump have their own mining curves (#3237); LF_Test_EveronMiningMarket reads them.
			if (LF_Test_EveronMiningMarket.IsMiningGood(good.m_sGood))
				continue;

			// Honey drops 6 percent a unit (#3448); LF_Test_EveronHoney reads it.
			if (LF_Test_EveronHoney.IsHoneyGood(good.m_sGood))
				continue;

			// The scrapyard goods are priced by the yard, not the trader list (#3823); LF_Test_EveronScrapItems reads them.
			if (LF_Test_EveronScrapItems.IsScrapGood(good.m_sGood))
				continue;

			string name = good.m_sGood;
			Resource prefab = Resource.Load(good.m_sGood);
			ctx.True(prefab != null && prefab.IsValid(), name + " resolves to a real prefab");

			int price = TraderPrice(rows, good.m_sGood);
			ctx.True(price > 0, name + " matches a pack trader list entry");
			ctx.True(TraderBuys(TRADER, good.m_sGood) || TraderBuys(CORE_TRADER, good.m_sGood), name + " is in the trader storage filter (the pack adds to the core one)");
			if (price <= 0)
				continue;

			ctx.Equal(price, good.m_iCeiling, name + " ceiling is today's sell price");
			ctx.Equal(price, good.m_iBase, name + " base is today's sell price");
			ctx.Equal((price + 1) / 2, good.m_iFloor, name + " floor is half the sell price, rounded up");
			ctx.Equal(2, good.m_iDropPerUnitPct, name + " drops 2 percent of the base per unit");
			ctx.True(good.m_iRecoverPctPerHour > 0, name + " recovers");

			// Recover speaks hundredths of a cash unit (#3028): start from the floor in hundredths and compare there.
			int backFine = config.Recover(good.m_sGood, good.FloorFine(), HOURS_24);
			ctx.True((good.BaseFine() - backFine) * 10 <= good.BaseFine(), string.Format("%1 floored at %2 is back to %3 hundredths of base %4 after 24 h (within 10 percent)", name, good.m_iFloor, backFine, good.BaseFine()));
			ctx.True(good.m_iFloor * 2 >= good.m_iBase, name + " floor is at least 0.5x the base");
			ctx.True(good.m_iBase <= good.m_iCeiling, name + " base is inside the ceiling");
		}
	}
}
