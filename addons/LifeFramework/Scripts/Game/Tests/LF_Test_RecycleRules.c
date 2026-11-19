// red-proof: RED-OK processing/recycle-rules (observed 2026-11-19 `tools/cli test --no-build --red LF_Test_RecycleRules`: with the Yield call skipped the car door returns no scrap metal and the assertions on it fail)
//! Recycle rules (#3347): a car door gives back 2 of its 4 scrap metal at 50 %; a recipe worth 100 % or
//! more of the item is refused at load with the reason; an unknown item gives nothing. Pure logic.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_RecycleRules : LF_Test
{
	override string GetName()
	{
		return "processing/recycle-rules";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ItemCatalogueConfig Catalogue()
	{
		LF_ItemCatalogueConfig catalogue = new LF_ItemCatalogueConfig();
		catalogue.m_aItems = {};
		catalogue.m_aItems.Insert(LF_CatalogueItem.Make("car_door", "", "equipment", 40, 20, "legal"));
		catalogue.m_aItems.Insert(LF_CatalogueItem.Make("scrap_metal", "", "equipment", 10, 5, "legal"));
		return catalogue;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_RecycleRecipe Door(int percent)
	{
		array<ref LF_RecyclePart> parts = {};
		parts.Insert(LF_RecyclePart.Make("scrap_metal", 4));
		return LF_RecycleRecipe.Make("car_door", percent, parts);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ItemCatalogueConfig catalogue = Catalogue();

		// A car door gives back 2 of its 4 scrap metal at 50 %.
		LF_RecycleConfig config = new LF_RecycleConfig();
		config.m_aRecipes = {};
		config.m_aRecipes.Insert(Door(50));
		ctx.EqualStr("", LF_RecycleRules.Problem(config.m_aRecipes[0], catalogue), "a 50 % car door recipe is usable");
		ctx.Equal(0, config.Sanitize(catalogue), "a usable recipe survives the load check");

		array<ref LF_RecycleReturn> back = {};
		// Red-proof hook: with --red the yield is skipped, so the assertions on it must fail.
		if (!RedProof())
			LF_RecycleRules.Yield(config, "car_door", back);

		ctx.Equal(1, back.Count(), "a car door returns one kind of part");
		if (back.Count() > 0)
		{
			ctx.EqualStr("scrap_metal", back[0].m_sItem, "the part is scrap metal");
			ctx.Equal(2, back[0].m_iCount, "2 of the 4 scrap metal come back at 50 %");
		}

		ctx.Equal(1, LF_RecycleRules.ReturnCount(3, 50), "the return rounds down");
		ctx.Equal(0, LF_RecycleRules.ReturnCount(1, 50), "one part at 50 % gives nothing");
		ctx.Equal(4, LF_RecycleRules.ReturnCount(4, 100), "the return is never more than went in");
		ctx.Equal(4, LF_RecycleRules.ReturnCount(4, 250), "a percent over 100 still returns no more than went in");

		// A recipe worth 100 % or more is refused at load, with the reason.
		LF_RecycleRecipe full = Door(100);
		string reason = LF_RecycleRules.Problem(full, catalogue);
		ctx.False(reason.IsEmpty(), "a 100 % recipe is refused");
		ctx.True(reason.Contains("100"), "the reason names the return percent");
		ctx.False(LF_RecycleRules.Problem(Door(150), catalogue).IsEmpty(), "a recipe over 100 % is refused");

		// Under 100 % the value can still be a loop when the parts are dearer than the item: 4 scrap at 90 % = 3 scrap = 30 against 25.
		LF_ItemCatalogueConfig cheapDoor = Catalogue();
		cheapDoor.m_aItems[0].m_iPrice = 25;
		string loop = LF_RecycleRules.Problem(Door(90), cheapDoor);
		ctx.False(loop.IsEmpty(), "parts worth more than the item are refused under 100 %");
		ctx.True(loop.Contains("30"), "the reason names the value that came back");
		ctx.EqualStr("", LF_RecycleRules.Problem(Door(50), cheapDoor), "50 % of the same door is 20 against 25: usable");

		LF_RecycleConfig refused = new LF_RecycleConfig();
		refused.m_aRecipes = {};
		refused.m_aRecipes.Insert(Door(100));
		ctx.Equal(1, refused.Sanitize(catalogue), "the 100 % row is removed at load");
		ctx.Equal(0, refused.m_aRecipes.Count(), "nothing is left to recycle");

		LF_RecycleConfig twice = new LF_RecycleConfig();
		twice.m_aRecipes = {};
		twice.m_aRecipes.Insert(Door(50));
		twice.m_aRecipes.Insert(Door(40));
		ctx.Equal(1, twice.Sanitize(catalogue), "a repeated item keeps its first row");
		ctx.Equal(50, twice.m_aRecipes[0].m_iReturnPercent, "the first row is the one kept");

		// An unknown item gives nothing.
		array<ref LF_RecycleReturn> none = {};
		ctx.Equal(0, LF_RecycleRules.Yield(config, "no_such_item", none), "an unknown item gives nothing");
		ctx.Equal(0, none.Count(), "an unknown item adds no line");
		ctx.Equal(0, LF_RecycleRules.Yield(null, "car_door", none), "no config gives nothing");
		ctx.Equal(0, LF_RecycleRules.Yield(config, "", none), "an empty id gives nothing");

		// A recipe for an item or part the catalogue does not hold, or with no catalogue, is refused.
		LF_RecycleRecipe stray = Door(50);
		stray.m_sItem = "ghost_item";
		ctx.False(LF_RecycleRules.Problem(stray, catalogue).IsEmpty(), "an item missing from the catalogue is refused");
		ctx.False(LF_RecycleRules.Problem(Door(50), null).IsEmpty(), "with no catalogue every recipe is refused");
	}
}
