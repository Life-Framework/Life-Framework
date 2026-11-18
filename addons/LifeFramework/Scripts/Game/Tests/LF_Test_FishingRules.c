// red-proof: new test with no failing state on main (the rules did not exist). Batched hook run:
// `tools\cli test --no-build --red LF_Test_FishingRules` zeroes the strike window and slack in RedProof(), so the
// strike assertions fail.
//! Fishing rules (#2927): bite times stay inside min and max for any rod, bait and spot; a strike inside
//! window plus slack lands, before or after it misses; a pond never gives an ocean species (nor the sea a pond
//! one); an emptied spot falls back to the open-water table and gives small fish only; stock regrows on time;
//! casts per minute are limited; a bad row disables only itself; the core file ships valid.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_FishingRules : LF_Test
{
	static const ResourceName ROD = "{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et";
	static const ResourceName ROD_FAST = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	static const ResourceName ROD_SLOW = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";
	static const ResourceName BAIT = "{577A665F1662E7AD}Prefabs/Resources/Mining/CopperBar.et";
	static const ResourceName FISH_S = "{ACC9A10F0F20F5C4}Prefabs/Resources/Mining/GoldBar.et";
	static const ResourceName FISH_M = "{8F81BD0BF343CF49}Prefabs/Resources/Mining/GoldNugget.et";
	static const ResourceName FISH_L = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";

	override string GetName()
	{
		return "fishing/rules";
	}

	//------------------------------------------------------------------------------------------------
	protected array<ResourceName> Sizes()
	{
		array<ResourceName> sizes = {FISH_S, FISH_M, FISH_L};
		return sizes;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_FishingConfig MakeConfig()
	{
		LF_FishingConfig config = LF_FishingConfig.MakeEmpty();
		// RedProof: a strike that can never land (`--red`).
		if (RedProof())
		{
			config.m_fStrikeWindowSeconds = 0.01;
			config.m_fStrikeSlackSeconds = 0;
		}

		config.m_aRods.Insert(LF_FishingRodDef.Make(ROD, 20, 1, 0));
		config.m_aRods.Insert(LF_FishingRodDef.Make(ROD_FAST, 25, 100, 2));
		config.m_aRods.Insert(LF_FishingRodDef.Make(ROD_SLOW, 10, 0.1, 0));
		array<string> boost = {"perch"};
		config.m_aBaits.Insert(LF_FishingBaitDef.Make(BAIT, 1.5, boost));

		array<string> ocean = {"OCEAN"};
		array<string> pond = {"POND"};
		array<int> noHours = {};
		array<int> morning = {5, 6, 7, 8, 9};
		config.m_aSpecies.Insert(LF_FishSpeciesDef.Make("mackerel", Sizes(), ocean, noHours, 0));
		config.m_aSpecies.Insert(LF_FishSpeciesDef.Make("tuna", Sizes(), ocean, noHours, 3));
		config.m_aSpecies.Insert(LF_FishSpeciesDef.Make("perch", Sizes(), pond, noHours, 0));
		config.m_aSpecies.Insert(LF_FishSpeciesDef.Make("carp", Sizes(), pond, morning, 0));

		array<int> smallOnly = {1, 0, 0};
		array<int> mixed = {1, 1, 2};
		array<int> largeOnly = {0, 0, 1};
		config.m_aTables.Insert(LF_FishTableDef.Make("open_water", Weights("mackerel", 1, "perch", 1), smallOnly));
		config.m_aTables.Insert(LF_FishTableDef.Make("mixed", Weights4("mackerel", 1, "tuna", 1, "perch", 1, "carp", 1), mixed));
		config.m_aTables.Insert(LF_FishTableDef.Make("pondweights", Weights("perch", 1, "carp", 3), mixed));
		config.m_aTables.Insert(LF_FishTableDef.Make("baited", Weights("perch", 1, "carp", 1), mixed));
		config.m_aTables.Insert(LF_FishTableDef.Make("cove", Weights("tuna", 1, "tuna", 1), largeOnly));
		config.m_aTables.Insert(LF_FishTableDef.Make("tunaonly", Weights("tuna", 1, "tuna", 1), largeOnly));
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected array<ref LF_FishWeightDef> Weights(string a, int wa, string b, int wb)
	{
		array<ref LF_FishWeightDef> rows = {};
		rows.Insert(LF_FishWeightDef.Make(a, wa));
		rows.Insert(LF_FishWeightDef.Make(b, wb));
		return rows;
	}

	//------------------------------------------------------------------------------------------------
	protected array<ref LF_FishWeightDef> Weights4(string a, int wa, string b, int wb, string c, int wc, string d, int wd)
	{
		array<ref LF_FishWeightDef> rows = Weights(a, wa, b, wb);
		rows.Insert(LF_FishWeightDef.Make(c, wc));
		rows.Insert(LF_FishWeightDef.Make(d, wd));
		return rows;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_FishingConfig config = MakeConfig();
		ctx.Equal(0, config.Sanitize(), "the test config is valid");

		RunBite(ctx, config);
		RunStrike(ctx, config);
		RunWater(ctx, config);
		RunPicks(ctx, config);
		RunFallback(ctx, config);
		RunStock(ctx);
		RunCasts(ctx, config);
		RunRefused(ctx);
		RunShipped(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunBite(LF_TestContext ctx, LF_FishingConfig config)
	{
		LF_FishingRodDef rod = config.FindRod(ROD);
		LF_FishingRodDef fast = config.FindRod(ROD_FAST);
		LF_FishingRodDef slow = config.FindRod(ROD_SLOW);
		LF_FishingBaitDef bait = config.FindBait(BAIT);

		ctx.EqualFloat(15, LF_FishingRules.BiteSeconds(config, rod, null, false, 0), 0.001, "roll 0 is the shortest wait");
		ctx.EqualFloat(60, LF_FishingRules.BiteSeconds(config, rod, null, false, 1), 0.001, "roll 1 is the longest wait");
		ctx.EqualFloat(37.5, LF_FishingRules.BiteSeconds(config, rod, null, false, 0.5), 0.001, "roll 0.5 is the middle");
		ctx.EqualFloat(25, LF_FishingRules.BiteSeconds(config, null, bait, false, 0.5), 0.001, "a 1.5 times bait cuts 37.5 seconds to 25");
		ctx.True(LF_FishingRules.BiteSeconds(config, fast, bait, false, 1) < LF_FishingRules.BiteSeconds(config, rod, null, false, 1), "a faster rod and bait bite sooner");
		ctx.True(LF_FishingRules.BiteSeconds(config, rod, null, true, 0.5) > LF_FishingRules.BiteSeconds(config, rod, null, false, 0.5), "an emptied spot bites slower");

		// Whatever the rod, bait, spot and roll, the wait stays inside min and max.
		array<LF_FishingRodDef> rods = {rod, fast, slow, null};
		array<LF_FishingBaitDef> baits = {bait, null};
		int outside = 0;
		for (int r = 0; r < rods.Count(); r++)
		{
			for (int b = 0; b < baits.Count(); b++)
			{
				for (int empty = 0; empty < 2; empty++)
				{
					for (int step = 0; step <= 20; step++)
					{
						float seconds = LF_FishingRules.BiteSeconds(config, rods[r], baits[b], empty == 1, step / 20.0);
						if (seconds < config.m_fBiteMinSeconds || seconds > config.m_fBiteMaxSeconds)
							outside++;
					}
				}
			}
		}

		ctx.Equal(0, outside, "no bite time falls outside min and max");
		ctx.EqualFloat(15, LF_FishingRules.BiteSeconds(config, fast, bait, false, 0), 0.001, "a very fast rod is held at the minimum");
		ctx.EqualFloat(60, LF_FishingRules.BiteSeconds(config, slow, null, true, 1), 0.001, "a very slow rod at an emptied spot is held at the maximum");

		ctx.True(LF_FishingRules.InCastRange(rod, 20), "a cast at the rod's range is in range");
		ctx.False(LF_FishingRules.InCastRange(rod, 20.1), "a cast past it is out of range");
		ctx.False(LF_FishingRules.InCastRange(rod, -1), "a negative distance is refused");
		ctx.False(LF_FishingRules.InCastRange(null, 1), "no rod, no cast");
		ctx.False(LF_FishingRules.RodUsable(fast, 1), "a rod above the fisher's level is refused");
		ctx.True(LF_FishingRules.RodUsable(fast, 2), "a rod at the fisher's level is usable");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunStrike(LF_TestContext ctx, LF_FishingConfig config)
	{
		ctx.True(LF_FishingRules.Strike(config, -0.1) == LF_EFishStrike.EARLY, "a strike before the bite is early");
		ctx.True(LF_FishingRules.Strike(config, 0) == LF_EFishStrike.HIT, "a strike at the bite lands");
		ctx.True(LF_FishingRules.StrikeLands(config, 1.9), "a strike inside the window lands");
		ctx.True(LF_FishingRules.StrikeLands(config, 2.4), "a strike just past the window lands on the latency slack");
		ctx.True(LF_FishingRules.StrikeLands(config, 2.5), "the end of window plus slack still lands");
		ctx.True(LF_FishingRules.Strike(config, 2.6) == LF_EFishStrike.LATE, "a strike past window plus slack is late");
		ctx.False(LF_FishingRules.StrikeLands(config, -0.5), "an early strike does not land");
		ctx.False(LF_FishingRules.StrikeLands(config, 30), "a late strike does not land");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunWater(LF_TestContext ctx, LF_FishingConfig config)
	{
		LF_FishTableDef mixed = config.FindTable("mixed");
		int oceanInPond = 0;
		int pondInOcean = 0;
		int pondPicks = 0;
		for (int step = 0; step < 100; step++)
		{
			float roll = step / 100.0;
			LF_FishSpeciesDef inPond = LF_FishingRules.PickSpecies(config, mixed, "POND", 6, 5, null, roll);
			if (inPond)
			{
				pondPicks++;
				if (inPond.LivesIn("OCEAN"))
					oceanInPond++;
			}

			LF_FishSpeciesDef inSea = LF_FishingRules.PickSpecies(config, mixed, "OCEAN", 6, 5, null, roll);
			if (inSea && inSea.LivesIn("POND"))
				pondInOcean++;
		}

		ctx.True(pondPicks > 0, "a pond gives fish");
		ctx.Equal(0, oceanInPond, "a pond never gives an ocean species");
		ctx.Equal(0, pondInOcean, "the sea never gives a pond species");
		ctx.True(LF_FishingRules.PickSpecies(config, config.FindTable("tunaonly"), "POND", 12, 5, null, 0.5) == null, "a table with only ocean fish gives nothing in a pond");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunPicks(LF_TestContext ctx, LF_FishingConfig config)
	{
		LF_FishTableDef weighted = config.FindTable("pondweights");
		LF_FishSpeciesDef first = LF_FishingRules.PickSpecies(config, weighted, "POND", 6, 0, null, 0.1);
		LF_FishSpeciesDef second = LF_FishingRules.PickSpecies(config, weighted, "POND", 6, 0, null, 0.3);
		LF_FishSpeciesDef last = LF_FishingRules.PickSpecies(config, weighted, "POND", 6, 0, null, 0.999);
		ctx.True(first && first.m_sId == "perch", "a low roll picks the light entry");
		ctx.True(second && second.m_sId == "carp", "roll 0.3 picks the heavy entry (weights 1 and 3)");
		ctx.True(last && last.m_sId == "carp", "the top roll picks the last entry");

		// Hour: carp bites in the morning only.
		int carpAtNoon = 0;
		for (int step = 0; step < 50; step++)
		{
			LF_FishSpeciesDef noon = LF_FishingRules.PickSpecies(config, weighted, "POND", 12, 0, null, step / 50.0);
			if (noon && noon.m_sId == "carp")
				carpAtNoon++;
		}

		ctx.Equal(0, carpAtNoon, "a morning species never bites at noon");

		// Level: tuna needs level 3.
		LF_FishTableDef tunaOnly = config.FindTable("tunaonly");
		ctx.True(LF_FishingRules.PickSpecies(config, tunaOnly, "OCEAN", 12, 2, null, 0.5) == null, "a species above the fisher's level is out");
		LF_FishSpeciesDef tuna = LF_FishingRules.PickSpecies(config, tunaOnly, "OCEAN", 12, 3, null, 0.5);
		ctx.True(tuna && tuna.m_sId == "tuna", "at its level the species bites");

		// Bait boost: perch weighs double with its bait.
		LF_FishTableDef baited = config.FindTable("baited");
		LF_FishingBaitDef bait = config.FindBait(BAIT);
		LF_FishSpeciesDef plain = LF_FishingRules.PickSpecies(config, baited, "POND", 6, 0, null, 0.6);
		LF_FishSpeciesDef boosted = LF_FishingRules.PickSpecies(config, baited, "POND", 6, 0, bait, 0.6);
		ctx.True(plain && plain.m_sId == "carp", "without bait roll 0.6 lands on carp");
		ctx.True(boosted && boosted.m_sId == "perch", "the boosted bait turns the same roll into perch");

		// Sizes: weights 1, 1, 2.
		LF_FishTableDef mixed = config.FindTable("mixed");
		ctx.Equal(0, LF_FishingRules.PickSize(mixed, 0.1), "a low roll is small");
		ctx.Equal(1, LF_FishingRules.PickSize(mixed, 0.3), "roll 0.3 is medium");
		ctx.Equal(2, LF_FishingRules.PickSize(mixed, 0.6), "roll 0.6 is large");
		ctx.Equal(2, LF_FishingRules.PickSize(mixed, 0.99), "the top roll is large");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunFallback(LF_TestContext ctx, LF_FishingConfig config)
	{
		LF_FishCatch spot = LF_FishingRules.PickCatch(config, "cove", 5, "OCEAN", 12, 3, null, 0.5, 0.99);
		ctx.True(spot != null, "a stocked spot gives a catch");
		if (spot)
		{
			ctx.EqualStr("cove", spot.m_sTable, "a stocked spot rolls its own table");
			ctx.False(spot.m_bOpenWater, "it is not the open-water table");
			ctx.Equal(2, spot.m_iSize, "the cove's table gives large fish");
			ctx.EqualStr(FISH_L, spot.m_Prefab, "the large prefab is handed over");
		}

		LF_FishCatch empty = LF_FishingRules.PickCatch(config, "cove", 0, "OCEAN", 12, 3, null, 0.5, 0.99);
		ctx.True(empty != null, "an emptied spot still gives a catch");
		if (empty)
		{
			ctx.EqualStr("open_water", empty.m_sTable, "an emptied spot falls back to the open-water table");
			ctx.True(empty.m_bOpenWater, "the catch says so");
			ctx.Equal(0, empty.m_iSize, "open water gives small fish even on a high size roll");
			ctx.EqualStr("mackerel", empty.m_Species.m_sId, "the ocean species of the open-water table");
			ctx.EqualStr(FISH_S, empty.m_Prefab, "the small prefab is handed over");
		}

		LF_FishCatch noSpot = LF_FishingRules.PickCatch(config, "", 0, "POND", 12, 0, null, 0.5, 0.99);
		ctx.True(noSpot && noSpot.m_bOpenWater && noSpot.m_Species.LivesIn("POND"), "no spot uses the open-water table, pond fish in a pond");

		LF_FishCatch unknown = LF_FishingRules.PickCatch(config, "no_such_table", 9, "OCEAN", 12, 0, null, 0.5, 0.5);
		ctx.True(unknown && unknown.m_bOpenWater, "a spot table the config lacks falls back to open water");

		// The spot table has nothing for this water: open water answers instead of nothing.
		LF_FishCatch wrongWater = LF_FishingRules.PickCatch(config, "cove", 5, "POND", 12, 3, null, 0.5, 0.5);
		ctx.True(wrongWater && wrongWater.m_bOpenWater && wrongWater.m_Species.LivesIn("POND"), "a spot with no fish for this water falls back to open water");

		LF_FishingConfig bare = LF_FishingConfig.MakeEmpty();
		ctx.True(LF_FishingRules.PickCatch(bare, "", 0, "OCEAN", 12, 0, null, 0.5, 0.5) == null, "no open-water table means no catch, not a crash");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunStock(LF_TestContext ctx)
	{
		LF_GatherStock stock = LF_FishingRules.NewStock(3, 60, 0);
		ctx.Equal(3, LF_FishingRules.StockLeft(stock, 0), "a spot starts full");
		ctx.True(LF_FishingRules.TakeFish(stock, 0), "first fish");
		ctx.True(LF_FishingRules.TakeFish(stock, 0), "second fish");
		ctx.True(LF_FishingRules.TakeFish(stock, 0), "third fish");
		ctx.False(LF_FishingRules.TakeFish(stock, 0), "an empty spot gives no more");
		ctx.Equal(0, LF_FishingRules.StockLeft(stock, 59), "nothing has regrown after 59 seconds");
		ctx.Equal(1, LF_FishingRules.StockLeft(stock, 60), "one fish is back after 60 seconds");
		ctx.True(LF_FishingRules.TakeFish(stock, 60), "the regrown fish can be taken");
		ctx.Equal(0, LF_FishingRules.StockLeft(stock, 119), "the clock restarts from the last regrowth");
		ctx.Equal(1, LF_FishingRules.StockLeft(stock, 120), "the next one is back a minute later");
		ctx.Equal(3, LF_FishingRules.StockLeft(stock, 100000), "regrowth stops at the cap");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunCasts(LF_TestContext ctx, LF_FishingConfig config)
	{
		array<float> casts = {};
		int allowed = 0;
		for (int i = 0; i < 6; i++)
		{
			if (LF_FishingRules.TryCast(config, casts, i))
				allowed++;
		}

		ctx.Equal(6, allowed, "six casts fit in a minute");
		ctx.False(LF_FishingRules.TryCast(config, casts, 6), "the seventh in the same minute is refused");
		ctx.True(LF_FishingRules.TryCast(config, casts, 60), "a minute after the first cast one is free again");
		ctx.False(LF_FishingRules.TryCast(config, casts, 60.5), "and only the one");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunRefused(LF_TestContext ctx)
	{
		LF_FishingConfig config = LF_FishingConfig.MakeEmpty();
		array<string> ocean = {"OCEAN"};
		array<string> none = {};
		array<string> lake = {"LAKE"};
		array<int> noHours = {};
		array<int> badHour = {25};
		array<ResourceName> twoSizes = {FISH_S, FISH_M};
		array<int> sizes = {1, 1, 1};
		array<int> twoWeights = {1, 1};
		array<int> zeroWeights = {0, 0, 0};

		config.m_aRods.Insert(LF_FishingRodDef.Make(ROD, 20, 1, 0));
		config.m_aRods.Insert(LF_FishingRodDef.Make("", 20, 1, 0));
		config.m_aRods.Insert(LF_FishingRodDef.Make(ROD, 20, 1, 0));
		config.m_aRods.Insert(LF_FishingRodDef.Make(ROD_FAST, 20, 0, 0));
		config.m_aBaits.Insert(LF_FishingBaitDef.Make(BAIT, 1, null));
		config.m_aBaits.Insert(LF_FishingBaitDef.Make(ROD_SLOW, 0, null));

		config.m_aSpecies.Insert(LF_FishSpeciesDef.Make("tuna", Sizes(), ocean, noHours, 0));
		config.m_aSpecies.Insert(LF_FishSpeciesDef.Make("perch", Sizes(), ocean, noHours, 0));
		config.m_aSpecies.Insert(LF_FishSpeciesDef.Make("", Sizes(), ocean, noHours, 0));
		config.m_aSpecies.Insert(LF_FishSpeciesDef.Make("twosizes", twoSizes, ocean, noHours, 0));
		config.m_aSpecies.Insert(LF_FishSpeciesDef.Make("nowater", Sizes(), none, noHours, 0));
		config.m_aSpecies.Insert(LF_FishSpeciesDef.Make("lake", Sizes(), lake, noHours, 0));
		config.m_aSpecies.Insert(LF_FishSpeciesDef.Make("lateshift", Sizes(), ocean, badHour, 0));
		config.m_aSpecies.Insert(LF_FishSpeciesDef.Make("tuna", Sizes(), ocean, noHours, 0));

		// t1 keeps tuna and loses its ghost and its zero-weight entry; the rest are refused whole.
		array<ref LF_FishWeightDef> t1 = Weights("tuna", 1, "ghost", 1);
		t1.Insert(LF_FishWeightDef.Make("perch", 0));
		config.m_aTables.Insert(LF_FishTableDef.Make("t1", t1, sizes));
		config.m_aTables.Insert(LF_FishTableDef.Make("", Weights("tuna", 1, "tuna", 1), sizes));
		config.m_aTables.Insert(LF_FishTableDef.Make("twoweights", Weights("tuna", 1, "tuna", 1), twoWeights));
		config.m_aTables.Insert(LF_FishTableDef.Make("zeroweights", Weights("tuna", 1, "tuna", 1), zeroWeights));
		config.m_aTables.Insert(LF_FishTableDef.Make("ghostsonly", Weights("ghost", 1, "ghost2", 1), sizes));
		config.m_aTables.Insert(LF_FishTableDef.Make("t1", Weights("tuna", 1, "tuna", 1), sizes));

		// Rods 3 + baits 1 + species 6 + tables 9 (entries: ghost, zero weight, two ghosts; rows: five and the ghosts-only table).
		ctx.Equal(19, config.Sanitize(), "nineteen rows and entries are refused");
		ctx.Equal(1, config.m_aRods.Count(), "one rod survives");
		ctx.True(config.FindRod(ROD) != null, "the valid rod is kept");
		ctx.True(config.FindRod(ROD_FAST) == null, "a rod with no bite speed is not loaded");
		ctx.Equal(1, config.m_aBaits.Count(), "one bait survives");
		ctx.Equal(2, config.m_aSpecies.Count(), "two species survive: the others are refused one by one");
		ctx.True(config.FindSpecies("tuna") != null && config.FindSpecies("perch") != null, "the valid species are kept");
		ctx.True(config.FindSpecies("lake") == null && config.FindSpecies("lateshift") == null, "a bad water kind and a bad hour disable only their own species");
		ctx.Equal(1, config.m_aTables.Count(), "one table survives");

		LF_FishTableDef t1Kept = config.FindTable("t1");
		ctx.True(t1Kept != null, "the table with some bad entries is kept");
		if (t1Kept)
		{
			ctx.Equal(1, t1Kept.m_aSpecies.Count(), "its bad entries are dropped");
			ctx.EqualStr("tuna", t1Kept.m_aSpecies[0].m_sSpecies, "the good entry stays");
		}

		// Bad globals fall back to their defaults instead of breaking the config.
		LF_FishingConfig globals = LF_FishingConfig.MakeEmpty();
		globals.m_fBiteMinSeconds = 0;
		globals.m_fBiteMaxSeconds = 5;
		globals.m_fStrikeWindowSeconds = -1;
		globals.m_fStrikeSlackSeconds = -1;
		globals.m_iCastsPerMinute = 0;
		globals.m_fBaitBoost = 0.5;
		globals.m_fEmptySpotSlowdown = 0.5;
		globals.Sanitize();
		ctx.EqualFloat(15, globals.m_fBiteMinSeconds, 0.001, "a bad bite range falls back to 15");
		ctx.EqualFloat(60, globals.m_fBiteMaxSeconds, 0.001, "a bad bite range falls back to 60");
		ctx.EqualFloat(2, globals.m_fStrikeWindowSeconds, 0.001, "a bad window falls back to 2");
		ctx.EqualFloat(0.5, globals.m_fStrikeSlackSeconds, 0.001, "a bad slack falls back to 0.5");
		ctx.Equal(6, globals.m_iCastsPerMinute, "a bad cast limit falls back to 6");
		ctx.EqualFloat(2, globals.m_fBaitBoost, 0.001, "a bad bait boost falls back to 2");
		ctx.EqualFloat(1.5, globals.m_fEmptySpotSlowdown, 0.001, "a bad slowdown falls back to 1.5");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunShipped(LF_TestContext ctx)
	{
		LF_FishingConfig.Reload();
		LF_FishingConfig shipped = LF_FishingConfig.Get();
		ctx.True(shipped != null, "the core fishing config resolves");
		if (!shipped)
			return;

		ctx.Equal(1, shipped.m_aRods.Count(), "the core ships one rod and refuses none");
		ctx.Equal(1, shipped.m_aBaits.Count(), "one bait");
		ctx.Equal(6, shipped.m_aSpecies.Count(), "six species (the trout joined in #2995 so the grid pond gives a fish the counter buys)");
		ctx.Equal(3, shipped.m_aTables.Count(), "three tables");
		ctx.Equal(0, shipped.Sanitize(), "a second pass refuses nothing");

		LF_FishTableDef open = shipped.FindTable(shipped.m_sOpenWaterTable);
		ctx.True(open != null, "the open-water table exists");
		if (open)
			ctx.Equal(0, LF_FishingRules.PickSize(open, 0.999), "open water is small fish only");

		int wrong = 0;
		for (int hour = 0; hour < 24; hour += 3)
		{
			for (int step = 0; step < 20; step++)
			{
				float roll = step / 20.0;
				LF_FishCatch pond = LF_FishingRules.PickCatch(shipped, "pond", 5, "POND", hour, 3, null, roll, roll);
				if (!pond || pond.m_Species.LivesIn("OCEAN"))
					wrong++;

				LF_FishCatch sea = LF_FishingRules.PickCatch(shipped, "harbour", 5, "OCEAN", hour, 3, null, roll, roll);
				if (!sea || sea.m_Species.LivesIn("POND"))
					wrong++;
			}
		}

		ctx.Equal(0, wrong, "the shipped tables give pond fish in ponds and ocean fish in the sea at any hour");
		LF_FishingConfig.Reload();
	}
}
