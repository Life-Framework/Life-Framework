// red-proof: Observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_SiteRent`: `RED PROOF OK`, banking skipped failed `two hours held at 600/h banks 1200 (expected 1200, got 0)`, the store read skipped failed `the other site keeps its bank (load 2) (expected 450, got 0)`. Before the written version was raised, `--only` failed `the site keeps its bank (load 1) (expected 1200, got 0)`. Review of #2840 (2026-11-18): `--only LF_Test_SiteRent` on the unfixed LF_SiteRent failed `the old holder's half hour is logged under the old holder (expected 300, got 0)`, then passed with the holder-change flush.
// tier: PERSISTENCE
//! sites/site-rent (#2683): a held site banks rentPerHour pro rata each sweep, capped at
//! maxRentBanked (defaults 600/h, 3000); nothing banks while unheld, contested or changing hands; one
//! `site_rent` money line per hour of banking; the "siterent" save section round trips the bank through
//! the real store write and read, twice without change; a save without the section loads no bank.
//! Across a restart (`tools\cli wt e2e`): the save boot banks one site, the reload boot finds it.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_SiteRent : LF_Test
{
	//! No LF_SitesConfig entry: the default rules (600 per hour, 3000 cap).
	protected static const string SITE = "site-rent-test";
	protected static const string OTHER = "site-rent-other";
	protected static const string E2E_SITE = "site-rent-e2e";
	protected static const string ORG = "site-rent-org";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "sites/site-rent";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		LF_PersistenceComponent persistence;
		if (gameMode)
			persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));
		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

#ifdef LF_E2E_LOAD_BOOT
		if (!LF_SaveSections.BootHadSection(LF_SiteRentSavePayload.NAME))
		{
			// The guard's pre-move e2e writes pass 1 with a build before this section (#2812).
			LF_Debug.Info(LF_SiteRent.FEATURE, "e2e restart: the boot save has no siterent section, skipping the bank check");
			ctx.Equal(0, LF_SiteRent.BankedOf(E2E_SITE), "a save without the siterent section loads no bank");
			return;
		}

		ctx.Equal(1200, LF_SiteRent.BankedOf(E2E_SITE), "the bank is back after the restart");
		ctx.Equal(1800000000, LF_SiteRent.LastRentAtOf(E2E_SITE), "the last rent time is back after the restart");
		return;
#endif

		bool registered;
		foreach (LF_SavePayload section : LF_SaveSections.All())
		{
			if (section && section.Name() == LF_SiteRentSavePayload.NAME)
				registered = true;
		}

		ctx.True(registered, "siterent is a registered save section");
		array<ref LF_SiteRentRow> kept = LF_SiteRent.ExportAll();
		Banking(ctx);
		if (registered)
			RoundTrip(ctx, persistence);
		LF_SiteRent.ApplyAll(kept);

#ifdef LF_E2E_HOST_STOP
		// The save boot: the shutdown save carries one site with 1200 banked.
		array<ref LF_SiteRentRow> e2eRows = LF_SiteRent.ExportAll();
		e2eRows.Insert(LF_SiteRentRow.Create(E2E_SITE, 1200, 1800000000));
		LF_SiteRent.ApplyAll(e2eRows);
		ctx.Equal(1200, LF_SiteRent.BankedOf(E2E_SITE), "the save boot stored the e2e bank");
#endif
	}

	//------------------------------------------------------------------------------------------------
	//! Feeds `seconds` of 5 s sweeps for one site.
	protected static void Hold(string site, string holder, LF_ESiteHoldOutcome outcome, int seconds, int startUnix)
	{
		for (int t = 0; t < seconds; t += 5)
		{
			LF_SiteRent.Accrue(site, holder, outcome, 5, startUnix + t + 5);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Banking(LF_TestContext ctx)
	{
		LF_SiteRent.ResetStatics();
		LF_SiteSettings settings = LF_SitesConfig.For(SITE);
		ctx.Equal(600, settings.m_iRentPerHour, "the default rent is 600 per hour");
		ctx.Equal(3000, settings.m_iMaxRentBanked, "the default cap is 3000");

		LF_Test_SiteRentMoney money = new LF_Test_SiteRentMoney();
		LF_EventBus.Subscribe(LF_OnMoneyMoved, money);

		int start = 1800000000;
		if (!RedProof())
			Hold(SITE, ORG, LF_ESiteHoldOutcome.GAIN, 7200, start);
		ctx.Equal(1200, LF_SiteRent.BankedOf(SITE), "two hours held at 600/h banks 1200");
		ctx.Equal(start + 7200, LF_SiteRent.LastRentAtOf(SITE), "the last rent time is the last sweep");
		ctx.Equal(2, money.m_iLines, "one site_rent line per hour");
		ctx.Equal(1200, money.m_iAmount, "the lines carry what was banked");

		// A held site nobody stands on still banks (DECAY, NONE); a contest or a capture step does not.
		Hold(SITE, ORG, LF_ESiteHoldOutcome.CONTESTED, 3600, start + 7200);
		ctx.Equal(1200, LF_SiteRent.BankedOf(SITE), "nothing banks while contested");
		LF_SiteRent.Accrue(SITE, ORG, LF_ESiteHoldOutcome.CAPTURED, 5, start + 10805);
		ctx.Equal(1200, LF_SiteRent.BankedOf(SITE), "nothing banks on the capture step");
		Hold(OTHER, "", LF_ESiteHoldOutcome.NONE, 3600, start);
		ctx.Equal(0, LF_SiteRent.BankedOf(OTHER), "nothing banks while unheld");

		Hold(SITE, ORG, LF_ESiteHoldOutcome.DECAY, 36000, start + 10805);
		ctx.Equal(3000, LF_SiteRent.BankedOf(SITE), "the cap holds at 3000");
		ctx.Equal(3000, money.m_iAmount, "every banked unit was logged once");

		// A capture keeps the bank: the new holder banks onto what is there (capped).
		Hold(SITE, "site-rent-capturer", LF_ESiteHoldOutcome.GAIN, 3600, start + 50000);
		ctx.Equal(3000, LF_SiteRent.BankedOf(SITE), "the new holder takes the capped bank");

		LF_EventBus.Unsubscribe(LF_OnMoneyMoved, money);

		// A holder change mid-hour logs the old holder's unlogged rent under the old holder (review of #2840).
		LF_Test_SiteRentMoney byHolder = new LF_Test_SiteRentMoney();
		LF_EventBus.Subscribe(LF_OnMoneyMoved, byHolder);
		Hold(OTHER, ORG, LF_ESiteHoldOutcome.GAIN, 1800, start);
		LF_SiteRent.Accrue(OTHER, ORG, LF_ESiteHoldOutcome.CAPTURED, 5, start + 1805);
		Hold(OTHER, "site-rent-capturer", LF_ESiteHoldOutcome.GAIN, 3600, start + 1805);
		LF_EventBus.Unsubscribe(LF_OnMoneyMoved, byHolder);
		ctx.Equal(300, byHolder.AmountFor(ORG), "the old holder's half hour is logged under the old holder");
		ctx.Equal(600, byHolder.AmountFor("site-rent-capturer"), "the new holder's hour is logged under the new holder");
	}

	//------------------------------------------------------------------------------------------------
	protected void RoundTrip(LF_TestContext ctx, LF_PersistenceComponent persistence)
	{
		LF_SiteRent.ResetStatics();
		array<ref LF_SiteRentRow> rows = {};
		rows.Insert(LF_SiteRentRow.Create(SITE, 1200, 1800007200));
		rows.Insert(LF_SiteRentRow.Create(OTHER, 450, 1800003000));
		ctx.Equal(2, LF_SiteRent.ApplyAll(rows), "two banks before the save");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext);
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(SITE), "the save carries the banks");

		for (int pass = 1; pass <= 2; pass++)
		{
			string tag = string.Format(" (load %1)", pass);
			// The live banks empty before each load, so only the save can bring them back.
			LF_SiteRent.ResetStatics();
			JsonLoadContext loadContext = new JsonLoadContext(false);
			ctx.True(loadContext.LoadFromString(saved), "the save parses" + tag);
			if (!RedProof())
				LF_PersistenceComponentSerializer.DeserializeStore(persistence, loadContext);

			ctx.Equal(1200, LF_SiteRent.BankedOf(SITE), "the site keeps its bank" + tag);
			ctx.Equal(1800007200, LF_SiteRent.LastRentAtOf(SITE), "the site keeps its last rent time" + tag);
			ctx.Equal(450, LF_SiteRent.BankedOf(OTHER), "the other site keeps its bank" + tag);
		}

		// A second apply of the exported rows on a live table changes nothing.
		LF_SiteRent.ApplyAll(LF_SiteRent.ExportAll());
		ctx.Equal(1200, LF_SiteRent.BankedOf(SITE), "a second apply keeps the bank");
		ctx.Equal(2, LF_SiteRent.ExportAll().Count(), "a second apply keeps two banks");

		// A saved bank above the cap comes back at the cap.
		array<ref LF_SiteRentRow> over = {};
		over.Insert(LF_SiteRentRow.Create(SITE, 99999, 1800007200));
		LF_SiteRent.ApplyAll(over);
		ctx.Equal(3000, LF_SiteRent.BankedOf(SITE), "a bank above the cap loads at the cap");
		LF_SiteRent.ApplyAll(rows);

		// A save written before this section holds no bank.
		JsonSaveContext oldContext = new JsonSaveContext(false);
		array<string> omit = {LF_SiteRentSavePayload.NAME};
		LF_PersistenceComponentSerializer.SerializeStoreOmitting(persistence, oldContext, omit);
		string oldSave = oldContext.SaveToString();
		ctx.False(oldSave.Contains(OTHER), "the old save has no bank rows");
		LF_SiteRent.ResetStatics();
		JsonLoadContext oldLoad = new JsonLoadContext(false);
		ctx.True(oldLoad.LoadFromString(oldSave), "the old save parses");
		LF_PersistenceComponentSerializer.DeserializeStore(persistence, oldLoad);
		ctx.Equal(0, LF_SiteRent.BankedOf(SITE), "a save without the section loads no bank");
	}
}

//------------------------------------------------------------------------------------------------
//! Counts the `site_rent` money lines the test's sweeps publish.
class LF_Test_SiteRentMoney : LF_EventSubscriber
{
	int m_iLines;
	int m_iAmount;
	ref map<string, int> m_mByHolder = new map<string, int>();

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnMoneyMoved moved = LF_OnMoneyMoved.Cast(busEvent);
		if (!moved || moved.m_sSource != LF_SiteRent.SOURCE)
			return true;

		m_iLines++;
		m_iAmount += moved.m_iAmount;
		m_mByHolder.Set(moved.m_sPlayerUid, m_mByHolder.Get(moved.m_sPlayerUid) + moved.m_iAmount);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	int AmountFor(string holderOrgId)
	{
		return m_mByHolder.Get(holderOrgId);
	}
}
