// red-proof: red first, 2026-11-19: before the pack files existed every line failed (no kitchen-honey machine, no farm_beehive offer, no jar curve). `--red` swaps the pack source for an empty header, so the same lines fail.
//! #3448: the 1989 honey, read through the overlays under the Everon 1989 header (the pack leg). The kitchen
//! turns 2 honeycombs into 1 honey jar (`kitchen-honey`, the LF_ProcessAction recipe row), the seed and feed
//! store (`farm_supply`) sells the beehive kit for $120, and the jar sells on a market curve from $8 down to a
//! $4 floor. Slow money: a week of one hive pays less than a day of full-time parcel running.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronHoney : LF_Test
{
	static const ResourceName HONEY_OVERLAY = "{0E53EE3367DBDED3}Configs/Farming/LF_Honey_Everon1989.conf";
	static const ResourceName MARKET_OVERLAY = "{696C5C44DC198A3C}Configs/Market/LF_Market_Honey_Everon1989.conf";
	static const ResourceName JAR = "{0CBDB38CA152E98C}Prefabs/Items/Farming/LF_HoneyJar.et";
	static const ResourceName KIT = "{DA8DBB9B5A97E39D}Prefabs/Items/Farming/LF_BeehiveKit.et";
	static const ResourceName COMB = "{9DC40F97DF4CCFC4}Prefabs/Items/Food/LF_Honeycomb.et";
	static const int NOW = 1800000480;	// lf-fixed: a unix time past 2^24, where float maths loses seconds
	static const int GAME_DAYS = 7;	// lf-fixed: the week the earnings check runs over
	static const int WORK_HOURS = 8;	// lf-fixed: a full-time job day
	static const int PARCEL_RUNS_PER_HOUR = 10;	// lf-fixed: economy tool, a parcel run takes about 6 minutes

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/honey";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");
		if (header)
		{
			ctx.True(header.m_aLF_ConfigOverlays && header.m_aLF_ConfigOverlays.Find(HONEY_OVERLAY) != -1, "the header lists the honey chain overlay");
			ctx.True(header.m_aLF_ConfigOverlays && header.m_aLF_ConfigOverlays.Find(MARKET_OVERLAY) != -1, "the header lists the honey market overlay");
		}

		if (RedProof())
		{
			// No pack layer: an empty header as the pack source.
			LF_MissionConfigPackSource none = new LF_MissionConfigPackSource();
			none.SetHeader(new LF_MissionHeader());
			LF_ConfigService.SetPackSource(none);
		}

		Reload();
		CheckPrefabs(ctx);
		CheckRecipe(ctx);
		CheckHiveStock(ctx);
		int jarsPerWeek = CheckHiveWeek(ctx);
		CheckBuyer(ctx, jarsPerWeek);

		if (RedProof())
			LF_ConfigService.Reset();
		Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected void Reload()
	{
		LF_ResourceChainCatalog.Reload();
		LF_ItemCatalogueConfig.Reload();
		LF_MarketConfig.Reload();
		LF_JobsConfig.Reload();
		LF_MarketBook.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckPrefabs(LF_TestContext ctx)
	{
		Resource jar = Resource.Load(JAR);
		ctx.True(jar && jar.IsValid(), "the honey jar resolves to a real prefab");
		Resource kit = Resource.Load(KIT);
		ctx.True(kit && kit.IsValid(), "the beehive kit resolves to a real prefab");
		Resource comb = Resource.Load(COMB);
		ctx.True(comb && comb.IsValid(), "the honeycomb the recipe takes resolves");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckRecipe(LF_TestContext ctx)
	{
		LF_ResourceMachineDef machine = LF_ResourceChainCatalog.FindMachine("kitchen-honey");
		ctx.NotNull(machine, "the kitchen has a honey recipe");
		if (!machine)
			return;

		ctx.True(LF_ResourceChainCatalog.IsMachineUsable(machine), "the honey recipe is usable");
		ctx.Equal(1, machine.m_aInputs.Count(), "one input");
		ctx.EqualStr(COMB, (string)machine.m_aInputs[0].m_InputPrefab, "the input is honeycomb");
		ctx.Equal(2, machine.m_aInputs[0].m_iInputAmount, "two combs make a batch");
		ctx.Equal(1, machine.m_aOutputs.Count(), "one output");
		ctx.EqualStr(JAR, (string)machine.m_aOutputs[0].m_OutputPrefab, "the output is a honey jar");
		ctx.Equal(1, machine.m_aOutputs[0].m_iOutputAmount, "a batch gives one jar");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckHiveStock(LF_TestContext ctx)
	{
		LF_ItemCatalogueConfig catalogue = LF_ItemCatalogueConfig.Get();
		ctx.NotNull(catalogue, "the catalogue loads");
		if (!catalogue)
			return;

		array<LF_OutletOffer> offers = {};
		LF_ItemCatalogue.Offers(catalogue, "farm_supply", offers);

		LF_CatalogueItem hive = null;
		foreach (LF_OutletOffer offer : offers)
		{
			if (offer && offer.m_sItem == "farm_beehive")
				hive = LF_ItemCatalogue.Item(catalogue, offer.m_sItem);
		}

		ctx.NotNull(hive, "the seed and feed counter sells the beehive kit");
		if (!hive)
			return;

		ctx.EqualStr(KIT, (string)hive.m_Prefab, "the offer is the beehive kit");
		ctx.Equal(120, hive.m_iPrice, "a hive costs $120");
	}

	//------------------------------------------------------------------------------------------------
	//! \return Jars one hive makes in a game week: the daytime hours of seven days, a comb per configured
	//! hours, two combs a jar.
	protected int CheckHiveWeek(LF_TestContext ctx)
	{
		LF_HiveConfig hives = LF_HiveConfig.Get();
		ctx.NotNull(hives, "the hive config loads");
		if (!hives)
			return 0;

		float dayHours = hives.m_fNightStartHour - hives.m_fNightEndHour;
		int combs = Math.Floor(dayHours * GAME_DAYS / hives.m_fHoursPerComb);
		int jars = combs / 2;
		ctx.True(jars > 0, string.Format("one hive makes jars in a week (%1 combs, %2 jars)", combs, jars));
		return jars;
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckBuyer(LF_TestContext ctx, int jarsPerWeek)
	{
		LF_MarketConfig market = LF_MarketConfig.Get();
		ctx.NotNull(market, "the market config loads under the pack header");
		if (!market)
			return;

		LF_MarketGoodDef curve = market.FindGood(JAR);
		ctx.NotNull(curve, "honey jars have a market curve");
		if (!curve)
			return;

		ctx.EqualStr("", curve.Problem(), "the jar curve is valid");
		ctx.Equal(8, curve.m_iBase, "a jar starts at $8");
		ctx.Equal(4, curve.m_iFloor, "the floor is $4");
		ctx.True(curve.m_iDropPerUnitPct > 0, "selling jars drops the price");
		ctx.True(curve.m_iRecoverPctPerHour > 0, "the price recovers");

		int week = LF_MarketBook.QuoteAt(JAR, jarsPerWeek, NOW);
		ctx.True(week > 0, string.Format("a week of jars sells for something ($%1)", week));

		// A day of full-time parcel running at the pack's pay.
		int pay = 0;
		LF_JobsConfig jobs = LF_JobsConfig.Get();
		ctx.NotNull(jobs, "the jobs config loads");
		if (jobs && jobs.m_aRunPays)
		{
			foreach (LF_JobRunPayDef row : jobs.m_aRunPays)
			{
				if (row && row.m_sJob == "PARCEL")
					pay = row.m_iPay;
			}
		}

		int jobDay = pay * PARCEL_RUNS_PER_HOUR * WORK_HOURS;
		ctx.True(jobDay > 0, "a parcel run pays");
		ctx.True(week < jobDay, string.Format("a week of one hive ($%1) pays less than a day of full-time work ($%2)", week, jobDay));
	}
}
