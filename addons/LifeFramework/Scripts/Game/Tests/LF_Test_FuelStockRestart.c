// red-proof: observed 2026-11-19 `tools/cli test --no-build --red LF_Test_FuelStockRestart`: RED-OK, 38 assertions fail with the hook on (first: `station 'a' holds 650 litres after the load (expected 650, got 400)`); RedProof() skips the section load, so the read-back assertions fail. Green with `--only`: passed=2 failed=0.
//! Each fuel station's litres ride the real save path (#3329, the "fuelstock" save section): two stations at
//! different stock come back from a save with their own litres, a dry one still dry and a fresh delivery still
//! there; loading twice gives the same stocks; a saved id with no station in a config that lists its stations,
//! an empty id, a negative count and a duplicate are dropped and logged without breaking the rest; litres above
//! the capacity are cut to it; a save without the section opens every station at its starting litres. Across a
//! restart (`tools\cli wt e2e`): the save boot leaves one station at E2E_LITRES, the reload boot finds it there.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_FuelStockRestart : LF_Test
{
	//! A station id no pack uses; the table values (not a row) apply to it, so the reload boot's real config reads it.
	protected static const string E2E_ID = "lf-e2e-fuel-station";
	//! Litres the e2e save boot leaves in that station: below the core table capacity (2000).
	static const int E2E_LITRES = 777;

	override string GetName()
	{
		return "fuel/fuel-stock-restart";
	}

	//------------------------------------------------------------------------------------------------
	//! A config with the station rows `a` (capacity 1000, starts at 400) and `b` (capacity 300, starts at 300).
	protected static LF_FuelConfig MakeConfig()
	{
		LF_FuelConfig config = LF_FuelConfig.Make(true, 200, 100, 2000, 500, 200, 1);
		config.m_aStations = new array<ref LF_FuelStationRow>();
		config.m_aStations.Insert(MakeRow("a", 1000, 400));
		config.m_aStations.Insert(MakeRow("b", 300, 300));
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_FuelStationRow MakeRow(string id, int capacity, int start)
	{
		LF_FuelStationRow row = new LF_FuelStationRow();
		row.m_sId = id;
		row.m_iCapacity = capacity;
		row.m_iStartLitres = start;
		return row;
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
		// The reload boot: the e2e station came back from the real save, when it carried the section.
		if (LF_E2E.RequiresSection(LF_FuelStockSavePayload.NAME))
			VerifyE2E(ctx);

		return;
#endif

		ctx.NotNull(LF_SaveSections.Get(LF_FuelStockSavePayload.NAME), "fuelstock is a registered save section");
		ctx.Equal(0, LF_PersistenceComponentSerializer.PayloadSinceVersion(LF_FuelStockSavePayload.NAME), "fuelstock is not a legacy positional payload");
		LF_FuelConfig.UseForTest(MakeConfig());
		LF_FuelStationComponent.ResetStatics();

		// Two stations at different stock: a is half full after a delivery, b is dry.
		LF_FuelStationComponent.StockFor("a").m_iLitres = 650;
		LF_FuelStationComponent.StockFor("b").m_iLitres = 0;
		ctx.Equal(2, LF_FuelStationComponent.Stocks().Count(), "two stations are open");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(LF_SaveSections.OBJECT_PREFIX + LF_FuelStockSavePayload.NAME), "the save carries the fuelstock section");

		// A restart: every station opens at its starting litres again.
		LF_FuelStationComponent.ResetStatics();
		ctx.Equal(400, LF_FuelStationComponent.StockFor("a").m_iLitres, "a fresh boot opens a at its starting litres");
		LF_FuelStationComponent.ResetStatics();
		// RedProof(): the load is skipped, so every read-back assertion fails (`--red`).
		if (!RedProof())
			ReadSection(saved, ctx);

		CheckLitres(ctx, "a", 650, "after the load");
		CheckLitres(ctx, "b", 0, "after the load");
		ctx.Equal(1000, LF_FuelStationComponent.StockFor("a").m_iCapacity, "a keeps the capacity of its config row");
		ctx.Equal(2, LF_FuelStationComponent.Stocks().Count(), "the load opens the two saved stations");

		// Loading twice replaces, never accumulates; a station the save lacks is forgotten and opens fresh.
		LF_FuelStationComponent.StockFor("a").m_iLitres = 5;
		LF_FuelStationComponent.Stocks().Set("b", LF_FuelStock.Make(300, 77));
		ReadSection(saved, ctx);
		ReadSection(saved, ctx);
		CheckLitres(ctx, "a", 650, "after a second load");
		CheckLitres(ctx, "b", 0, "after a second load");

		// A saved id the config no longer lists is tolerated and dropped, the rest still load.
		array<ref LF_FuelStockRow> rows = {};
		rows.Insert(LF_FuelStockRow.Create("a", 123));
		rows.Insert(LF_FuelStockRow.Create("retired-station", 50));
		ctx.Equal(1, LF_FuelStockSavePayload.ApplyAll(rows, MakeConfig()), "a station with no config row is dropped, the other row loads");
		CheckLitres(ctx, "a", 123, "beside a retired station row");
		ctx.False(LF_FuelStationComponent.Stocks().Contains("retired-station"), "the retired station does not come back");

		// An empty id, a negative count and a duplicate are dropped; the first duplicate wins; litres above the capacity are cut.
		rows.Clear();
		rows.Insert(LF_FuelStockRow.Create("", 10));
		rows.Insert(LF_FuelStockRow.Create("a", -5));
		rows.Insert(LF_FuelStockRow.Create("b", 250));
		rows.Insert(LF_FuelStockRow.Create("b", 20));
		ctx.Equal(1, LF_FuelStockSavePayload.ApplyAll(rows, MakeConfig()), "only the one good row loads");
		CheckLitres(ctx, "b", 250, "from a duplicate row, the first one");
		rows.Clear();
		rows.Insert(LF_FuelStockRow.Create("b", 99999));
		LF_FuelStockSavePayload.ApplyAll(rows, MakeConfig());
		CheckLitres(ctx, "b", 300, "cut to its capacity");

		// A config with no station rows gives every id the table values, so the id is kept.
		rows.Clear();
		rows.Insert(LF_FuelStockRow.Create("anywhere", 321));
		ctx.Equal(1, LF_FuelStockSavePayload.ApplyAll(rows, LF_FuelConfig.Make(true, 200, 100, 2000, 500, 200, 1)), "a table-values config keeps any station id");
		CheckLitres(ctx, "anywhere", 321, "in a config with no rows");

		// A save without this section (an older save): every station opens at its starting litres.
		LF_FuelConfig.UseForTest(MakeConfig());
		LF_FuelStationComponent.ResetStatics();
		LF_FuelStationComponent.StockFor("a").m_iLitres = 650;
		JsonSaveContext oldContext = new JsonSaveContext(false);
		array<string> omit = {LF_FuelStockSavePayload.NAME};
		LF_PersistenceComponentSerializer.SerializeStoreOmitting(persistence, oldContext, omit);
		string oldSave = oldContext.SaveToString();
		ctx.False(oldSave.Contains(LF_SaveSections.OBJECT_PREFIX + LF_FuelStockSavePayload.NAME), "the older save has no fuelstock section");
		JsonLoadContext oldLoad = new JsonLoadContext(false);
		ctx.True(oldLoad.LoadFromString(oldSave), "the old save parses");
		// A restart, then the old save: nothing loads and the station opens at its starting litres on first use.
		LF_FuelStationComponent.ResetStatics();
		LF_PersistenceComponentSerializer.DeserializeStore(persistence, oldLoad);
		ctx.Equal(0, LF_FuelStationComponent.Stocks().Count(), "a pre-section save opens no station");
		CheckLitres(ctx, "a", 400, "from a pre-section save, at its starting litres");
		// An unrelated store load without the section leaves an open station as it is.
		LF_FuelStationComponent.StockFor("a").m_iLitres = 90;
		JsonLoadContext secondOldLoad = new JsonLoadContext(false);
		ctx.True(secondOldLoad.LoadFromString(oldSave), "the old save parses again");
		LF_PersistenceComponentSerializer.DeserializeStore(persistence, secondOldLoad);
		CheckLitres(ctx, "a", 90, "after another store load without the section");

		LF_FuelConfig.Reload();
		LF_FuelStationComponent.ResetStatics();

		// Every non-reload run (the e2e save boot among them) leaves one station at E2E_LITRES.
		SeedE2E(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected static void CheckLitres(LF_TestContext ctx, string id, int litres, string when)
	{
		ctx.Equal(litres, LF_FuelStationComponent.StockFor(id).m_iLitres, string.Format("station '%1' holds %2 litres %3", id, litres, when));
	}

	//------------------------------------------------------------------------------------------------
	//! Reads the fuelstock section of a whole-store JSON save, as LF_SaveSections.ReadAll does.
	protected static void ReadSection(string saved, LF_TestContext ctx)
	{
		LF_SavePayload section = LF_SaveSections.Get(LF_FuelStockSavePayload.NAME);
		if (!section)
			return;

		JsonLoadContext context = new JsonLoadContext(false);
		ctx.True(context.LoadFromString(saved), "the save parses");
		ctx.True(context.StartObject(LF_SaveSections.OBJECT_PREFIX + LF_FuelStockSavePayload.NAME), "the fuelstock section opens");
		int savedVersion;
		context.ReadValue(LF_SaveSections.VERSION_KEY, savedVersion);
		ctx.Equal(section.Version(), savedVersion, "the section carries its own version");
		ctx.True(section.ReadSection(context, savedVersion), "the fuelstock section reads");
		context.EndObject();
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e save boot: one station holds E2E_LITRES in the live stocks.
	static void SeedE2E(LF_TestContext ctx)
	{
		LF_FuelStationComponent.Stocks().Set(E2E_ID, LF_FuelStock.Make(2000, E2E_LITRES));
		ctx.Equal(E2E_LITRES, LF_FuelStationComponent.Stocks().Get(E2E_ID).m_iLitres, "the e2e station holds its litres before the save");
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e reload boot: the same station holds the same litres after the restart.
	static void VerifyE2E(LF_TestContext ctx)
	{
		LF_FuelStock stock;
		ctx.True(LF_FuelStationComponent.Stocks().Find(E2E_ID, stock), "the e2e station is still open after the restart");
		if (!stock)
			return;

		ctx.Equal(E2E_LITRES, stock.m_iLitres, "the e2e station keeps its litres across the restart");
		LF_Debug.Info(LF_FuelStockSavePayload.FEATURE, string.Format("e2e restart: station '%1' holds %2 litres", E2E_ID, stock.m_iLitres));
	}
}
