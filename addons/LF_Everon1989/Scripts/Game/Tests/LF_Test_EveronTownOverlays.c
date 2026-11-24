// red-proof: 2026-11-22, `test --no-build --only LF_Test_EveronTownOverlays --world "{1054CC8646357708}Missions/LF_PlaytestTown.conf"` on the unchanged 20-overlay header FAILED (bus fare 10 not 15, bail 100 not 150, no POI rows, no runs, no clerk quest row); green on the 87-overlay header.
//! everon/town-overlays (TP-6a, #4259): the playtest town mission loads the island's config overlays, so the
//! systems the town cards place (bus stops, the onboarding route, mining, the postal run, mannequins, map POIs
//! and runs, the welcome guide, jail and bail) find the pack's tables there, not the neutral core defaults.
//! Run it on the town's header with `test --no-build --only LF_Test_EveronTownOverlays --world
//! "{1054CC8646357708}Missions/LF_PlaytestTown.conf"` (`--town` boots bare DebugWorld, no header). In the pack leg
//! the same checks hold under the flagship header. That the town header lists every island overlay but the
//! island-only ones is `validate-pack-missions`.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronTownOverlays : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/town-overlays";
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckKind(LF_TestContext ctx, LF_MapPoiConfig poi, string kind)
	{
		LF_MapPoiKind row = poi.Find(kind);
		ctx.NotNull(row, "the town map has a POI row for " + kind);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckRun(LF_TestContext ctx, LF_MapRunConfig runs, string id)
	{
		LF_MapRun run;
		foreach (LF_MapRun candidate : runs.m_aRuns)
		{
			if (candidate.m_sId == id)
				run = candidate;
		}
		ctx.NotNull(run, "the town has the " + id + " run");
		if (run)
			ctx.True(run.StopCount() >= 2, id + " has at least two stops");
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the run has a mission header");
		if (!header)
			return;
		if (RedProof())
			header.m_aLF_ConfigOverlays = new array<ResourceName>();

		// Bus stops: the pack's fares over the core's 10/5/40.
		LF_BusStopConfig.Reload();
		LF_BusStopConfig bus = LF_BusStopConfig.CreateDefault();
		if (!RedProof())
			bus = LF_BusStopConfig.Get();
		ctx.NotNull(bus, "the bus fares load");
		if (bus)
			ctx.Equal(15, bus.m_iBaseFare, "bus: the pack's base fare applies");

		// Jail and bail: the pack's bail per minute over the core's 100.
		LF_JailBreakConfig.Reload();
		LF_JailBreakConfig jail = LF_JailBreakConfig.Get();
		ctx.NotNull(jail, "the jail config loads");
		if (jail)
		{
			ctx.Equal(150, jail.m_iBailPerMinute, "jail: the pack's bail per minute applies");
			ctx.Equal(500, jail.m_iBailMinimum, "jail: the pack's minimum bail applies");
		}

		// Map POIs for the ATM-adjacent civic and shop places, the postal depot, the taxi rank and the board.
		LF_MapPoiConfig.Reload();
		LF_MapPoiConfig poi = LF_MapPoiConfig.Get();
		ctx.NotNull(poi, "the POI table loads");
		if (poi)
		{
			array<string> kinds = {"Clerk", "StoreCounter", "ClothingCounter", "GunCounter", "ParcelDepot", "TaxiRank", "TownBoard", "HostelBed", "FuelTill", "StoreCounter/hardware", "mine"};
			foreach (string kind : kinds)
			{
				CheckKind(ctx, poi, kind);
			}
		}

		// Run chains: the hours 0 to 2 route.
		LF_MapRunConfig.ResetStatics();
		LF_MapRunConfig runs = LF_MapRunConfig.Get();
		ctx.NotNull(runs, "the run table loads");
		if (runs)
		{
			CheckRun(ctx, runs, "mining");
			CheckRun(ctx, runs, "apples");
			CheckRun(ctx, runs, "postal");
		}

		// Onboarding: the clerk's quest chain and the welcome pack.
		LF_QuestGiverTable.ResetStatics();
		LF_QuestGiverConfig givers = LF_QuestGiverTable.Get();
		ctx.NotNull(givers, "the quest giver table loads");
		if (givers)
			ctx.NotNull(givers.Find("clerk"), "the clerk gives the arrival route");
		LF_WelcomeGiftTable.ResetStatics();
		array<ResourceName> gift = {};
		LF_WelcomeGiftTable.ItemsOf("arrival_register", gift);
		ctx.True(gift.Count() > 0, "the arrival papers hand the welcome pack");

		// Welcome guide: the handbook's parcel page and the miner's pickaxe row (mining, postal).
		LF_Handbook.Reload();
		ctx.NotNull(LF_Handbook.Find("parcel"), "the F3 handbook has the parcel page");
		LF_ItemCatalogueConfig.Reload();
		LF_ItemCatalogueConfig catalogue = LF_ItemCatalogueConfig.Get();
		ctx.NotNull(catalogue, "the item catalogue loads");
		if (catalogue)
		{
			ctx.NotNull(catalogue.FindItem("tool_pickaxe_worn"), "mining: the pickaxe is priced");
			ctx.NotNull(catalogue.FindItem("jacket_denim_brown"), "mannequins: the denim jacket is priced");
		}

		LF_BusStopConfig.Reload();
		LF_JailBreakConfig.Reload();
		LF_MapPoiConfig.Reload();
		LF_MapRunConfig.ResetStatics();
		LF_QuestGiverTable.ResetStatics();
		LF_WelcomeGiftTable.ResetStatics();
		LF_Handbook.Reload();
		LF_ItemCatalogueConfig.Reload();
	}
}
