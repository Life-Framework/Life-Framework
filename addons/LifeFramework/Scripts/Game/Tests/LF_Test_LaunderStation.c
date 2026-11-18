// red-proof: 2026-11-18 `tools\cli test --no-build --red LF_Test_LaunderStation`: RED PROOF OK, 17 red with the load skipped: `the load took all 1000 marked notes (expected 0, got 1000)`, `site heat rose by one batch: 0`. Green: 31 assertions. Review (#2893): a cuffed load took all 1000 notes (expected 12 RESTRAINED, got 0 OK) before the LF_CuffEnforcement check; green after.
//! The laundering station (#2853): a character with 1000 marked notes loads a batch, the wash runs
//! its batch time, and the same character takes out 700 cash with 0 marked left. The ledger books
//! launder-in and launder-out; a batch left mid-wash waits for its owner only; a character at the
//! daily cap is refused in words; the station's chain site warms by the configured heat per batch.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_LaunderStation : LF_Test
{
	protected static const ResourceName STATION = "{978FC469D81E785A}Prefabs/Props/Laundering/LF_LaunderStation.et";
	protected static const string SITE = "test-launder-site";
	protected static const string KEY_A = "test-launder-a";
	protected static const string KEY_B = "test-launder-b";
	protected static const int HEAT_UNITS = 10;

	override string GetName()
	{
		return "laundering/station";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity a = LF_TestCharacterFixture.Spawn(ctx);
		IEntity b = LF_TestCharacterFixture.Spawn(ctx);
		IEntity stationEntity;
		if (a)
			stationEntity = LF_Utils.SpawnEntityPrefab(STATION, a.GetOrigin() + "30 0 0");

		LF_LaunderStationComponent station;
		if (stationEntity)
			station = LF_Component<LF_LaunderStationComponent>.Find(stationEntity);

		ctx.NotNull(station, "the station prefab spawns with its component");
		if (a && b && station)
			RunStation(ctx, a, b, station);

		LF_MoneyUtils.SetUidForTests(null, "");
		LF_LaunderStationComponent.ResetStatics();
		LF_LaunderRules.ResetStatics();
		LF_LaunderConfig.ResetStatics();
		LF_ChainSites.Remove(SITE);
		LF_TestCharacterFixture.Delete(a);
		LF_TestCharacterFixture.Delete(b);
		if (stationEntity)
			SCR_EntityHelper.DeleteEntityAndChildren(stationEntity);
	}

	protected void RunStation(LF_TestContext ctx, IEntity a, IEntity b, LF_LaunderStationComponent station)
	{
		LF_LaunderConfig config = new LF_LaunderConfig();
		config.m_iCutPct = 30;
		config.m_iBatchMax = 2000;
		config.m_iBatchSeconds = 60;
		config.m_iDailyCap = 10000;
		LF_LaunderConfig.Install(config);
		LF_LaunderRules.ResetStatics();
		LF_Ledger.EnsureSubscribed();

		vector spot = station.GetOwner().GetOrigin();
		station.Configure(SITE, "cider", HEAT_UNITS);
		LF_ChainSites.Register(SITE, "cider", spot);
		LF_ChainSites.ClearHeat(SITE);
		LF_LaunderStationComponent.SetKeyForTests(a, KEY_A);
		LF_LaunderStationComponent.SetKeyForTests(b, KEY_B);
		LF_MoneyUtils.SetUidForTests(a, KEY_A);

		ctx.Equal(1000, LF_MarkedCash.Add(a, 1000, "test-robbery"), "A carries 1000 marked notes");
		ctx.Equal(500, LF_MarkedCash.Add(b, 500, "test-robbery"), "B carries 500 marked notes");
		int cashA = LF_MoneyUtils.GetCash(a);
		int cashB = LF_MoneyUtils.GetCash(b);
		int ledgerBefore = LF_Ledger.Count();

		// Far away: refused, nothing taken.
		ctx.Equal(LF_ELaunderStation.TOO_FAR, station.ServerLoad(a, 1000), "loading from out of reach is refused");
		ctx.Equal(1000, LF_MarkedCash.Count(a), "a refused load takes nothing");

		a.SetOrigin(spot + "1 0 0");
		b.SetOrigin(spot + "-1 0 0");

		// B is at the daily cap: refused in words, keeps the notes.
		LF_LaunderRules.Record(KEY_B, 10000, System.GetUnixTime());
		LF_ELaunderStation capped = station.ServerLoad(b, 1000);
		ctx.Equal(LF_ELaunderStation.DAILY_CAP, capped, "a character at the daily cap is refused");
		ctx.True(LF_LaunderStationComponent.RefusalKey(capped) == "#LF-Launder_DailyCap", "the cap refusal has words");
		ctx.Equal(500, LF_MarkedCash.Count(b), "the capped character keeps the notes");

		// A cuffed character cannot hand over notes (review of #2893): refused, nothing taken.
		SCR_ChimeraCharacter chimeraA = SCR_ChimeraCharacter.Cast(a);
		if (chimeraA)
		{
			chimeraA.LF_SetRestrained(true);
			ctx.Equal(LF_ELaunderStation.RESTRAINED, station.ServerLoad(a, 990), "a cuffed character's load is refused");
			ctx.Equal(1000, LF_MarkedCash.Count(a), "the cuffed character keeps the notes");
			chimeraA.LF_SetRestrained(false);
		}

		// A loads: notes leave first, the site warms by one batch of heat.
		LF_ChainDef chain = LF_ChainCatalog.FindChain("cider");
		ctx.NotNull(chain, "the cider chain carries the heat settings");
		if (!RedProof())
			ctx.Equal(LF_ELaunderStation.OK, station.ServerLoad(a, 1000), "A loads the batch");

		ctx.Equal(0, LF_MarkedCash.Count(a), "the load took all 1000 marked notes");
		ctx.Equal(1000, station.GetHeldMarked(), "the station holds the 1000");
		ctx.Equal(LF_LaunderStationComponent.STATE_WASHING, station.GetState(1001), "the batch is washing");
		if (chain && chain.m_Heat)
		{
			float expected = Math.Min(chain.m_Heat.m_fPerWork * HEAT_UNITS, chain.m_Heat.m_fMax);
			ctx.True(Math.AbsFloat(LF_ChainSites.GetHeat(SITE, 1000) - expected) < 0.01, string.Format("site heat rose by one batch: %1", LF_ChainSites.GetHeat(SITE, 1000)));
		}

		ctx.Equal(LF_ELaunderStation.BUSY, station.ServerLoad(b, 1002), "a washing station takes no second batch");
		ctx.Equal(LF_ELaunderStation.TOO_FAST, station.ServerCollect(a, 1000.3), "asking again faster than a hold is refused");

		// A walks away mid-wash: the batch waits in the station.
		a.SetOrigin(spot + "40 0 0");
		ctx.Equal(LF_ELaunderStation.TOO_FAR, station.ServerCollect(a, 1010), "collecting from afar is refused");
		ctx.Equal(1000, station.GetHeldMarked(), "the notes stay in the station while A is away");

		// B cannot take A's batch, washing or washed.
		ctx.Equal(LF_ELaunderStation.NOT_YOURS, station.ServerCollect(b, 1070), "another character cannot collect the batch");
		ctx.Equal(cashB, LF_MoneyUtils.GetCash(b), "B got no cash");

		a.SetOrigin(spot + "1 0 0");
		ctx.Equal(LF_ELaunderStation.NOT_READY, station.ServerCollect(a, 1030), "the batch is not washed before its time");
		ctx.Equal(LF_LaunderStationComponent.STATE_READY, station.GetState(1060), "the wash ticks to done after the batch time");
		ctx.Equal(LF_ELaunderStation.OK, station.ServerCollect(a, 1070), "A collects the washed batch");
		ctx.Equal(cashA + 700, LF_MoneyUtils.GetCash(a), "A has 700 more cash");
		ctx.Equal(0, LF_MarkedCash.Count(a), "A has 0 marked");
		ctx.Equal(LF_LaunderStationComponent.STATE_EMPTY, station.GetState(1071), "the station is empty again");
		ctx.Equal(LF_ELaunderStation.EMPTY, station.ServerCollect(a, 1080), "an empty station pays nothing");
		ctx.Equal(cashA + 700, LF_MoneyUtils.GetCash(a), "a second collect pays nothing");

		// The ledger: launder-in on the marked account, launder-out on cash.
		ctx.True(LF_Ledger.Count() >= ledgerBefore + 2, "the ledger booked the wash");
		array<ref LF_LedgerEntry> entries = {};
		LF_Ledger.Last(KEY_A, "", 10, entries);
		bool sawIn;
		bool sawOut;
		foreach (LF_LedgerEntry entry : entries)
		{
			if (entry.m_sReason == LF_LaunderStationComponent.REASON_IN && entry.m_sAccount == LF_Ledger.ACCOUNT_MARKED && entry.m_iDelta == -1000)
				sawIn = true;
			if (entry.m_sReason == LF_LaunderStationComponent.REASON_OUT && entry.m_sAccount == LF_Ledger.ACCOUNT_CASH && entry.m_iDelta == 700)
				sawOut = true;
		}

		ctx.True(sawIn, "the ledger books launder-in: 1000 marked notes in");
		ctx.True(sawOut, "the ledger books launder-out: 700 cash out");
	}
}
