// red-proof: 2026-11-18 `tools\cli test --no-build --red LF_Test_LaunderStationPersist,LF_Test_LaunderStation`: RED PROOF OK, with the load skipped `A collects the restored batch (expected 0, got 3)` and `A got exactly the 700 owed (expected 700, got 0)`. Green: `--only` passed=3. Restart: `tools\cli wt e2e slice-2894` green; with VerifyE2E expecting E2E_CLEAN + 1 the reload boot failed laundering/persist (shutdown save wrote 1 row, the reload boot read it back), so the check is live.
//! A laundering batch rides the real save path (#2894, the "launder" save section): a batch loaded
//! mid-wash comes back after a save and load with its owner, its marked notes, the clean cash owed
//! and the wash time left; only the owner collects, exactly the owed cash. Loading twice gives the
//! same batch; a row whose station never appears waits, then is dropped with a warning; a washed
//! batch's forfeit clock survives the load; a save without the section leaves an empty station
//! empty. Across a restart (`tools\cli wt e2e`): every non-reload run leaves a batch for E2E_SITE as
//! a pending row; the reload boot gives it to a new station with that key.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_LaunderStationPersist : LF_Test
{
	protected static const ResourceName STATION = "{978FC469D81E785A}Prefabs/Props/Laundering/LF_LaunderStation.et";
	protected static const string SITE = "test-launder-persist";
	protected static const string GONE = "test-launder-gone";
	static const string E2E_SITE = "test-launder-e2e";
	static const string E2E_OWNER = "test-launder-e2e-owner";
	static const int E2E_MARKED = 400;
	static const int E2E_CLEAN = 280;
	protected static const string KEY_A = "test-launder-pa";
	protected static const string KEY_B = "test-launder-pb";

	override string GetName()
	{
		return "laundering/persist";
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
		// The reload boot: the e2e batch came back from the real save, when it carried the section.
		if (LF_E2E.RequiresSection(LF_LaunderSavePayload.NAME))
			VerifyE2E(ctx);

		return;
#endif

		ctx.NotNull(LF_SaveSections.Get(LF_LaunderSavePayload.NAME), "launder is a registered save section");

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
		{
			RunPersist(ctx, persistence, a, b, station);
			// Every non-reload run (the e2e save boot among them) leaves the e2e batch in a station
			// that is deleted below, so it waits as a pending row in the save.
			SeedE2E(ctx, station);
		}

		LF_MoneyUtils.SetUidForTests(null, "");
		LF_LaunderStationComponent.ResetStatics();
		LF_LaunderRules.ResetStatics();
		LF_LaunderConfig.ResetStatics();
		LF_ChainSites.Remove(SITE);
		LF_ChainSites.Remove(E2E_SITE);
		LF_TestCharacterFixture.Delete(a);
		LF_TestCharacterFixture.Delete(b);
		if (stationEntity)
			SCR_EntityHelper.DeleteEntityAndChildren(stationEntity);

		LF_LaunderStations.ForgetPending(SITE);
		LF_LaunderStations.ForgetPending(GONE);
		ctx.NotNull(LF_LaunderStations.FindPending(E2E_SITE), "the deleted station's e2e batch waits as a pending row");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunPersist(LF_TestContext ctx, LF_PersistenceComponent persistence, IEntity a, IEntity b, LF_LaunderStationComponent station)
	{
		LF_LaunderConfig config = new LF_LaunderConfig();
		config.m_iCutPct = 30;
		config.m_iBatchMax = 2000;
		config.m_iBatchSeconds = 60;
		config.m_iDailyCap = 10000;
		LF_LaunderConfig.Install(config);
		LF_LaunderRules.ResetStatics();

		vector spot = station.GetOwner().GetOrigin();
		station.Configure(SITE, "cider", 0);
		station.SetForfeitSeconds(7200);
		ctx.True(LF_LaunderStations.Find(SITE) == station, "the station is known by its site id");
		LF_LaunderStationComponent.SetKeyForTests(a, KEY_A);
		LF_LaunderStationComponent.SetKeyForTests(b, KEY_B);
		LF_MoneyUtils.SetUidForTests(a, KEY_A);
		a.SetOrigin(spot + "1 0 0");
		b.SetOrigin(spot + "-1 0 0");

		ctx.Equal(1000, LF_MarkedCash.Add(a, 1000, "test-robbery"), "A carries 1000 marked notes");
		int cashA = LF_MoneyUtils.GetCash(a);
		int cashB = LF_MoneyUtils.GetCash(b);

		// A loads mid-session at the real world clock (the section saves at it).
		float t0 = LF_LaunderStations.WorldNow();
		ctx.Equal(LF_ELaunderStation.OK, station.ServerLoad(a, t0), "A loads the batch");
		LF_LaunderStationRow live = station.Export(t0);
		ctx.NotNull(live, "the loaded station exports a row");
		int wash = 0;
		if (live)
			wash = live.m_iWashLeft;

		ctx.True(wash > 0, string.Format("the batch has wash time left: %1 s", wash));

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(LF_SaveSections.OBJECT_PREFIX + LF_LaunderSavePayload.NAME), "the save carries the launder section");
		ctx.Equal(0, LF_PersistenceComponentSerializer.PayloadSinceVersion(LF_LaunderSavePayload.NAME), "launder is not a legacy positional payload");

		// Restart: the station is empty.
		station.Import(null, t0);
		ctx.Equal(LF_LaunderStationComponent.STATE_EMPTY, station.GetState(t0), "the station is empty before the load");
		// RedProof(): the load is skipped, so every read-back assertion fails (`--red`).
		if (!RedProof())
			ReadSection(saved, ctx);

		CheckBatch(ctx, station, KEY_A, 1000, 700, "after the load");
		ctx.Equal(LF_LaunderStationComponent.STATE_WASHING, station.GetState(t0), "the batch is still washing after the load");
		ctx.Equal(LF_LaunderStationComponent.STATE_READY, station.GetState(t0 + wash + 1), "the batch is washed after the saved wash time");

		// Only the owner collects, exactly the owed cash.
		ctx.Equal(LF_ELaunderStation.NOT_YOURS, station.ServerCollect(b, t0 + wash + 5), "another character cannot collect the restored batch");
		ctx.Equal(cashB, LF_MoneyUtils.GetCash(b), "B got no cash");
		ctx.Equal(LF_ELaunderStation.OK, station.ServerCollect(a, t0 + wash + 5), "A collects the restored batch");
		ctx.Equal(cashA + 700, LF_MoneyUtils.GetCash(a), "A got exactly the 700 owed");
		ctx.Equal(LF_LaunderStationComponent.STATE_EMPTY, station.GetState(t0 + wash + 6), "the station is empty after the collect");

		// Loading twice replaces, never accumulates.
		ReadSection(saved, ctx);
		ReadSection(saved, ctx);
		CheckBatch(ctx, station, KEY_A, 1000, 700, "after a second load");
		station.Import(null, t0);

		// A row whose station never appears waits as a pending row, rides the next save, then is dropped.
		float now = LF_LaunderStations.WorldNow();
		array<ref LF_LaunderStationRow> rows = {};
		rows.Insert(LF_LaunderStationRow.Create(GONE, KEY_A, 50, 35, 0, 10));
		ctx.Equal(0, LF_LaunderStations.ApplyAll(rows, now), "no live station takes the row of a station that is gone");
		ctx.NotNull(LF_LaunderStations.FindPending(GONE), "the row waits for its station");
		ctx.Equal(1, LF_LaunderStations.ExportAll(now).Count(), "the waiting row is still saved");
		ctx.Equal(1, LF_LaunderStations.DropUnclaimed(), "the unclaimed row is dropped");
		ctx.True(!LF_LaunderStations.FindPending(GONE), "the dropped row is gone");
		ctx.Equal(0, LF_LaunderStations.ExportAll(now).Count(), "nothing is saved for a station that is gone");

		// The forfeit clock rides the save: a batch washed 7000 s ago, with 123 still owed after a short pay.
		rows = {};
		rows.Insert(LF_LaunderStationRow.Create(SITE, KEY_A, 100, 123, 0, 7000));
		ctx.Equal(1, LF_LaunderStations.ApplyAll(rows, now), "the washed batch goes to its station");
		CheckBatch(ctx, station, KEY_A, 100, 123, "a washed batch with a short-paid rest");
		LF_LaunderStationRow again = station.Export(now + 100);
		ctx.True(again && again.m_iReadySeconds == 7100 && again.m_iWashLeft == 0, "the export carries the time it has waited washed");
		ctx.Equal(LF_LaunderStationComponent.STATE_READY, station.GetState(now + 199), "still waiting 1 s before the 2 h window ends");
		ctx.Equal(LF_LaunderStationComponent.STATE_EMPTY, station.GetState(now + 201), "forfeited once the 2 h window ends");
		ctx.Equal(0, station.GetOwedClean(), "a forfeited batch owes nothing");
		int cashBefore = LF_MoneyUtils.GetCash(a);
		ctx.Equal(LF_ELaunderStation.EMPTY, station.ServerCollect(a, now + 205), "the owner's collect after the forfeit finds it empty");
		ctx.Equal(cashBefore, LF_MoneyUtils.GetCash(a), "nobody is paid for a forfeited batch");

		// A save without this section (an older save): an empty station stays empty.
		JsonSaveContext oldContext = new JsonSaveContext(false);
		array<string> omit = {LF_LaunderSavePayload.NAME};
		LF_PersistenceComponentSerializer.SerializeStoreOmitting(persistence, oldContext, omit);
		string oldSave = oldContext.SaveToString();
		ctx.False(oldSave.Contains(LF_SaveSections.OBJECT_PREFIX + LF_LaunderSavePayload.NAME), "the older save has no launder section");
		JsonLoadContext oldLoad = new JsonLoadContext(false);
		ctx.True(oldLoad.LoadFromString(oldSave), "the old save parses");
		LF_PersistenceComponentSerializer.DeserializeStore(persistence, oldLoad);
		ctx.Equal(LF_LaunderStationComponent.STATE_EMPTY, station.GetState(now + 210), "a pre-section save leaves the station empty");
		ctx.True(station.GetOwnerKey().IsEmpty(), "a pre-section save gives the station no owner");
	}

	//------------------------------------------------------------------------------------------------
	protected static void CheckBatch(LF_TestContext ctx, LF_LaunderStationComponent station, string owner, int marked, int clean, string when)
	{
		ctx.True(station.GetOwnerKey() == owner, string.Format("the batch belongs to %1 %2 (got '%3')", owner, when, station.GetOwnerKey()));
		ctx.Equal(marked, station.GetHeldMarked(), string.Format("the station holds the marked notes %1", when));
		ctx.Equal(clean, station.GetOwedClean(), string.Format("the station owes the clean cash %1", when));
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the launder section of a whole-store JSON save, as LF_SaveSections.ReadAll does.
	protected static void ReadSection(string saved, LF_TestContext ctx)
	{
		LF_SavePayload section = LF_SaveSections.Get(LF_LaunderSavePayload.NAME);
		if (!section)
			return;

		JsonLoadContext context = new JsonLoadContext(false);
		ctx.True(context.LoadFromString(saved), "the save parses");
		ctx.True(context.StartObject(LF_SaveSections.OBJECT_PREFIX + LF_LaunderSavePayload.NAME), "the launder section opens");
		int savedVersion;
		context.ReadValue(LF_SaveSections.VERSION_KEY, savedVersion);
		ctx.Equal(section.Version(), savedVersion, "the section carries its own version");
		ctx.True(section.ReadSection(context, savedVersion), "the launder section reads");
		context.EndObject();
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e save boot: the station takes the e2e key and batch; deleting it keeps the batch pending.
	static void SeedE2E(LF_TestContext ctx, LF_LaunderStationComponent station)
	{
		station.Configure(E2E_SITE, "cider", 0);
		float now = LF_LaunderStations.WorldNow();
		station.Import(LF_LaunderStationRow.Create(E2E_SITE, E2E_OWNER, E2E_MARKED, E2E_CLEAN, 30, 0), now);
		ctx.True(station.GetOwnerKey() == E2E_OWNER, "e2e batch is loaded before the save");
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e reload boot: a new station with the e2e key gets the saved batch.
	static void VerifyE2E(LF_TestContext ctx)
	{
		LF_LaunderStationRow pending = LF_LaunderStations.FindPending(E2E_SITE);
		ctx.NotNull(pending, "e2e batch is back after the restart, waiting for its station");
		IEntity stationEntity = LF_Utils.SpawnEntityPrefab(STATION, "0 0 0");
		LF_LaunderStationComponent station;
		if (stationEntity)
			station = LF_Component<LF_LaunderStationComponent>.Find(stationEntity);

		ctx.NotNull(station, "e2e station spawns");
		if (station)
		{
			station.Configure(E2E_SITE, "cider", 0);
			ctx.True(station.GetOwnerKey() == E2E_OWNER, string.Format("e2e batch owner survives the restart (got '%1')", station.GetOwnerKey()));
			ctx.Equal(E2E_MARKED, station.GetHeldMarked(), "e2e marked notes survive the restart");
			ctx.Equal(E2E_CLEAN, station.GetOwedClean(), "e2e clean owed survives the restart");
			LF_Debug.Info(LF_LaunderStationComponent.FEATURE, string.Format("e2e restart: station %1 holds %2's %3 marked for %4 clean", E2E_SITE, station.GetOwnerKey(), station.GetHeldMarked(), station.GetOwedClean()));
		}

		if (stationEntity)
			SCR_EntityHelper.DeleteEntityAndChildren(stationEntity);

		LF_LaunderStations.ForgetPending(E2E_SITE);
		LF_ChainSites.Remove(E2E_SITE);
	}
}
