// red-proof: with the pack layer removed (an empty header as the pack source, the `RedProof()` hook) the recycle config has no recipe and the catalogue has no car parts, so every `has a recipe` and `has a buyer curve` line fails.
//! #3351: the pack's scrapyard recipes (docs/roadmap/m6-outline.md "Scrap and salvage"), read through the overlays
//! under the Everon 1989 header (the pack leg). Every recipe loads past the no-profit rule (parts worth less than
//! the item, #3347), uses no worn item or container as its input, and every part it gives back has a buyer row
//! with a price curve in the market, so a flooded yard pays less (#1016). A recipe's cash value is also kept
//! under the legal band's hour: one load of parts never pays a full hour of the fuel haul.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronScrapyard : LF_Test
{
	static const ResourceName OVERLAY = "{B474E10AF9B31DD1}Configs/Processing/LF_Recycle_Everon1989.conf";
	static const ResourceName CATALOGUE_OVERLAY = "{6A041BB311A4EB97}Configs/Processing/LF_Catalogue_Scrap_Everon1989.conf";

	//! The legal band's hour, below the fuel haul (`tools\cli economy`).
	static const int LEGAL_HOUR = 450;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/scrapyard";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");
		if (header && header.m_aLF_ConfigOverlays)
		{
			ctx.True(header.m_aLF_ConfigOverlays.Find(OVERLAY) != -1, "the header lists the recycle overlay");
			ctx.True(header.m_aLF_ConfigOverlays.Find(CATALOGUE_OVERLAY) != -1, "the header lists the scrap catalogue overlay");
		}
		else
		{
			ctx.Fail("the header lists no config overlays");
		}

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

		LF_RecycleRecipe carParts = recycle.Find("car_parts");
		ctx.NotNull(carParts, "car parts has a recipe");
		if (!carParts)
			return;

		// Every recipe passed the load check (Sanitize removed the rest), and none takes a worn item or a container.
		ctx.True(recycle.m_aRecipes.Count() > 0, "the pack lists recipes");
		foreach (LF_RecycleRecipe recipe : recycle.m_aRecipes)
		{
			ctx.EqualStr("", LF_RecycleRules.Problem(recipe, catalogue), string.Format("recipe %1 passes the no-profit rule", recipe.m_sItem));
			LF_CatalogueItem input = LF_ItemCatalogue.Item(catalogue, recipe.m_sItem);
			if (input)
				ctx.True(input.m_sCategory != "clothing" && input.m_sCategory != "weapon", string.Format("recipe %1 is not a worn item or a weapon", recipe.m_sItem));

			int worth = LF_RecycleRules.ReturnedValue(recipe, catalogue);
			ctx.True(worth > 0 && worth < LEGAL_HOUR, string.Format("recipe %1 gives back $%2, under the legal hour of $%3", recipe.m_sItem, worth, LEGAL_HOUR));

			// Every part has a buyer row with a price curve.
			foreach (LF_RecyclePart part : recipe.m_aParts)
			{
				LF_CatalogueItem partItem = LF_ItemCatalogue.Item(catalogue, part.m_sItem);
				ctx.NotNull(partItem, string.Format("part %1 is in the catalogue", part.m_sItem));
				if (!partItem)
					continue;

				LF_MarketGoodDef curve = market.FindGood(partItem.m_Prefab);
				ctx.NotNull(curve, string.Format("part %1 has a buyer curve", part.m_sItem));
				if (!curve)
					continue;

				ctx.True(curve.m_iDropPerUnitPct > 0, string.Format("part %1: a flooded yard pays less", part.m_sItem));
				ctx.True(curve.m_iFloor > 0 && curve.m_iFloor < curve.m_iCeiling, string.Format("part %1 has a floor under its ceiling", part.m_sItem));
			}
		}

		// Car parts at 50 %: 3 of the 6 iron bars come back.
		array<ref LF_RecycleReturn> back = {};
		LF_RecycleRules.Yield(recycle, "car_parts", back);
		ctx.Equal(1, back.Count(), "car parts return one kind of part");
		if (back.Count() > 0)
		{
			ctx.EqualStr("scrap_iron_bar", back[0].m_sItem, "the part is iron bar");
			ctx.Equal(3, back[0].m_iCount, "3 of the 6 iron bars come back at 50 %");
		}
	}
}
