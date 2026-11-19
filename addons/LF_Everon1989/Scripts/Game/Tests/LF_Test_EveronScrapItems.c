// red-proof: with the pack layer removed (an empty header as the pack source, the `RedProof()` hook) the catalogue, recycle and market configs hold none of the five scrap items, so every `is in the catalogue`, `has a recipe` and `has a buyer curve` line fails.
//! #3823: the scrapyard's loose items (Prefabs/Items/Scrap/) and what they break into, read through the overlays
//! under the Everon 1989 header (the pack leg). Each item is in the catalogue and its prefab loads; the worn hand
//! tool, the old radio and the rubber scrap each have a recipe that gives back scrap metal and copper wire at the
//! expected counts; every part has a buyer curve; and a yard fed at the assumed rate of inputs pays under the legal
//! band's hour (`tools\cli economy`, the fuel haul), so scrap is a side income and never the main job.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronScrapItems : LF_Test
{
	static const ResourceName SCRAP_METAL = "{EC59EAAEC5529F4A}Prefabs/Items/Scrap/LF_ScrapMetal.et";
	static const ResourceName COPPER_WIRE = "{8092D345F08C5F22}Prefabs/Items/Scrap/LF_CopperWireCoil.et";
	static const ResourceName RUBBER_SCRAP = "{0FC6120F0F62EFBA}Prefabs/Items/Scrap/LF_RubberScrap.et";

	//! The legal band's hour, below the fuel haul.
	static const int LEGAL_HOUR = 450;

	//! Inputs a player finds, hauls and benches in an hour: one every four minutes (walk, find, carry, work).
	static const int INPUTS_PER_HOUR = 15;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/scrap-items";
	}

	//------------------------------------------------------------------------------------------------
	//! The three goods the scrapyard buys on its own curves (Market/LF_Market_Scrap_Everon1989.conf); LF_Test_EveronMarket skips them.
	static bool IsScrapGood(ResourceName good)
	{
		return LF_Utils.SamePrefab(good, SCRAP_METAL) || LF_Utils.SamePrefab(good, COPPER_WIRE) || LF_Utils.SamePrefab(good, RUBBER_SCRAP);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		if (RedProof())
		{
			// No pack layer: an empty header as the pack source.
			LF_MissionConfigPackSource none = new LF_MissionConfigPackSource();
			none.SetHeader(new LF_MissionHeader());
			LF_ConfigService.SetPackSource(none);
		}

		LF_ItemCatalogueConfig.Reload();
		LF_RecycleConfig.Reload();
		LF_MarketConfig.Reload();
		LF_ItemCatalogueConfig catalogue = LF_ItemCatalogueConfig.Get();
		LF_RecycleConfig recycle = LF_RecycleConfig.Get();
		LF_MarketConfig market = LF_MarketConfig.Get();
		if (RedProof())
			LF_ConfigService.Reset();

		ctx.NotNull(catalogue, "the item catalogue loads under the pack header");
		ctx.NotNull(recycle, "the recycle config loads under the pack header");
		ctx.NotNull(market, "the market config loads under the pack header");
		if (!catalogue || !recycle || !market)
			return;

		// Every scrap item is catalogued and its prefab loads.
		array<string> ids = {"scrap_metal", "copper_wire_coil", "rubber_scrap", "worn_hand_tool", "old_radio"};
		foreach (string id : ids)
		{
			LF_CatalogueItem item = LF_ItemCatalogue.Item(catalogue, id);
			ctx.NotNull(item, string.Format("%1 is in the catalogue", id));
			if (!item)
				continue;

			ctx.True(item.m_iSellPrice <= item.m_iPrice, string.Format("%1 sells for no more than it costs", id));
			ctx.True(item.m_sCategory != "clothing" && item.m_sCategory != "weapon", string.Format("%1 is not a worn item or a weapon", id));
			Resource prefab = Resource.Load(item.m_Prefab);
			ctx.True(prefab && prefab.IsValid(), string.Format("%1 prefab resolves: %2", id, item.m_Prefab));
		}

		// Scrap metal and copper wire are the yard's goods: a buyer row, a floor under a ceiling, a flooded yard pays less.
		array<string> goods = {"scrap_metal", "copper_wire_coil", "rubber_scrap"};
		foreach (string good : goods)
		{
			LF_CatalogueItem goodItem = LF_ItemCatalogue.Item(catalogue, good);
			if (!goodItem)
				continue;

			LF_MarketGoodDef curve = market.FindGood(goodItem.m_Prefab);
			ctx.NotNull(curve, string.Format("%1 has a buyer curve", good));
			if (!curve)
				continue;

			ctx.True(curve.m_iDropPerUnitPct > 0, string.Format("%1: a flooded yard pays less", good));
			ctx.True(curve.m_iFloor > 0 && curve.m_iFloor < curve.m_iCeiling, string.Format("%1 has a floor under its ceiling", good));
			ctx.True(curve.m_iRecoverPctPerHour > 0, string.Format("%1: the price recovers over the hours", good));
		}

		// Worn hand tool: 6 scrap metal in it, 3 come back at 50 %.
		CheckRecipe(ctx, recycle, catalogue, market, "worn_hand_tool", "scrap_metal", 3, "", 0);
		// Old radio: 4 scrap metal and 2 copper wire coils in it, 2 and 1 come back.
		CheckRecipe(ctx, recycle, catalogue, market, "old_radio", "scrap_metal", 2, "copper_wire_coil", 1);
		// Rubber scrap: a steel bead, 1 scrap metal comes back.
		CheckRecipe(ctx, recycle, catalogue, market, "rubber_scrap", "scrap_metal", 1, "", 0);

		// Copper wire is a good, not a breakdown: nothing recycles into a loop.
		ctx.True(recycle.Find("copper_wire_coil") == null, "copper wire coil has no recipe");
		ctx.True(recycle.Find("scrap_metal") == null, "scrap metal has no recipe");
	}

	//------------------------------------------------------------------------------------------------
	//! One recipe: it exists, passes the no-profit rule, returns the counts, and its market value at the base
	//! price times the assumed inputs per hour stays under the legal hour.
	protected void CheckRecipe(LF_TestContext ctx, LF_RecycleConfig recycle, LF_ItemCatalogueConfig catalogue, LF_MarketConfig market, string item, string firstPart, int firstCount, string secondPart, int secondCount)
	{
		LF_RecycleRecipe recipe = recycle.Find(item);
		ctx.NotNull(recipe, string.Format("%1 has a recipe", item));
		if (!recipe)
			return;

		ctx.EqualStr("", LF_RecycleRules.Problem(recipe, catalogue), string.Format("recipe %1 passes the no-profit rule", item));

		array<ref LF_RecycleReturn> back = {};
		LF_RecycleRules.Yield(recycle, item, back);
		int lines = 1;
		if (!secondPart.IsEmpty())
			lines = 2;

		ctx.Equal(lines, back.Count(), string.Format("%1 returns %2 kinds of part", item, lines));
		if (back.Count() != lines)
			return;

		ctx.EqualStr(firstPart, back[0].m_sItem, string.Format("%1: the first part", item));
		ctx.Equal(firstCount, back[0].m_iCount, string.Format("%1: %2 of %3 come back at 50 %%", item, firstCount, firstPart));
		if (lines == 2)
		{
			ctx.EqualStr(secondPart, back[1].m_sItem, string.Format("%1: the second part", item));
			ctx.Equal(secondCount, back[1].m_iCount, string.Format("%1: %2 of %3 come back at 50 %%", item, secondCount, secondPart));
		}

		// What a trader pays for the parts at the base price, when nothing has been sold lately.
		int paid = 0;
		foreach (LF_RecycleReturn line : back)
		{
			LF_CatalogueItem partItem = LF_ItemCatalogue.Item(catalogue, line.m_sItem);
			if (!partItem)
				continue;

			LF_MarketGoodDef curve = market.FindGood(partItem.m_Prefab);
			if (curve)
				paid += curve.m_iBase * line.m_iCount;
		}

		ctx.True(paid > 0, string.Format("recipe %1 pays something at the yard", item));
		ctx.True(paid * INPUTS_PER_HOUR < LEGAL_HOUR, string.Format("recipe %1: $%2 a load at %3 loads an hour is under $%4", item, paid, INPUTS_PER_HOUR, LEGAL_HOUR));
	}
}
