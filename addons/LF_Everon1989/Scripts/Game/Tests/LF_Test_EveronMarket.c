// red-proof: observed 2026-11-18 `tools\cli test --no-build --red LF_Test_EveronMarket` printed `RED PROOF OK`; with the pack layer removed (an empty header as the pack source) it failed e.g. `GoldBar.et base is today's sell price (expected 120, got 200)` and the market-on, copper-removed and eight-goods checks. `--only` then passed under the pack header. 2026-11-18 (#3028): the recovery check passes hundredths (it went hollow when Recover moved to hundredths); with a 1 h window instead of 24 h it failed `everon/market`, with 24 h it passes.
//! #2849 in game: under the Everon 1989 header (the pack leg of `tools\cli test`), the pack's market overlay
//! switches the market on and gives the eight goods the 1989 trader buys (apples, plums, cement, gravel, sand,
//! iron bars, planks, cabbages) a curve. Each curve is read against the pack trader's own price list: ceiling
//! and base are the trader's price, the floor is half of it (rounded up), a unit takes 2 percent of the base off,
//! and a floored good is back within 10 percent of its base after 24 real hours. The core copper and gold bar
//! curves are removed by the pack.
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
	static const int HOURS_24 = 86400;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/market";
	}

	//------------------------------------------------------------------------------------------------
	//! The `m_aTradableItems` rows of a trader prefab's LF_TraderManagerComponent, or null.
	protected static BaseContainerList TraderRows(ResourceName prefab)
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
	protected static int TraderPrice(BaseContainerList rows, ResourceName good)
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
	protected static bool TraderBuys(ResourceName prefab, ResourceName good)
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
		ctx.True(config.FindGood(COPPER_BAR) == null, "the pack removes the copper bar curve");
		ctx.True(config.FindGood(GOLD_BAR) == null, "the pack removes the gold bar curve");
		ctx.Equal(8, config.m_aGoods.Count(), "eight goods ride a curve");

		BaseContainerList rows = TraderRows(TRADER);
		BaseContainerList coreRows = TraderRows(CORE_TRADER);
		ctx.True(rows != null, "the pack trader prefab lists its prices");
		ctx.True(coreRows != null, "the core trader prefab lists its prices");
		if (!rows || !coreRows)
			return;

		ctx.Equal(14, rows.Count(), "the pack trader lists 14 goods, each once");
		ctx.Equal(TraderPrice(rows, WOOD_PLANK), 6, "the pack trader buys planks at 6");
		ctx.Equal(TraderPrice(rows, CABBAGE), 4, "the pack trader buys cabbages at 4");
		ctx.Equal(-1, TraderPrice(coreRows, WOOD_PLANK), "the core trader list is untouched: no planks");
		ctx.Equal(-1, TraderPrice(coreRows, CABBAGE), "the core trader list is untouched: no cabbages");

		foreach (LF_MarketGoodDef good : config.m_aGoods)
		{
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
