// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_Systems,LF_Test_SystemsKeepData`:
// without the Farming check in LF_FarmPlots.AdvanceAll, systems/keep-data failed
// `- nothing grows while Farming is off (expected 0, got 1)` and `- with its 40 seconds (expected 40, got 70)`
// (the save written while off carried growth). Restored; green.
// tier: PERSISTENCE
//! Systems (#960): a system that is off keeps its saved data, so turning it on again restores it.
//! In one boot, with Farming off: a planted crop does not grow, rides the real save while off,
//! comes back from the "farmplots" payload while off, and grows again once Farming is on.
//!
//! Across a restart (`tools\cli wt e2e`): the save boot (LF_E2E_HOST_STOP) writes an owner
//! config.json with Farming off into the fresh profile and leaves a saved crop, so the shutdown save
//! is written with Farming off. The reload boot (LF_E2E_LOAD_BOOT) starts with Farming off from
//! that file, finds the crop loaded, deletes the file, turns Farming on and finds the crop still
//! there. A normal suite run compiles neither branch and writes no file.
class LF_Test_SystemsKeepData : LF_Test
{
	static const string E2E_PLOT = "e2e-systems-farm";
	static const int E2E_SECONDS = 45;
	static const string E2E_OWNER = "{\"schemaVersion\":1,\"systems\":{\"switches\":{\"Farming\":{\"enabled\":false}}}}";

	override string GetName()
	{
		return "systems/keep-data";
	}

	//------------------------------------------------------------------------------------------------
	protected void Owner(string json)
	{
		LF_ConfigService.Reset();
		if (!json.IsEmpty())
			LF_ConfigService.LoadOwnerString(json, "test");
		LF_Systems.Reload();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The saved row for a plot key, or null.
	protected static LF_FarmPlotRow RowOf(string plotKey)
	{
		array<ref LF_FarmPlotRow> rows = LF_FarmPlots.ExportAll();
		foreach (LF_FarmPlotRow row : rows)
		{
			if (row && row.m_sPlotKey == plotKey)
				return row;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
#ifdef LF_E2E_LOAD_BOOT
		RunReloadBoot(ctx);
#else
		RunRoundTrip(ctx);
	#ifdef LF_E2E_HOST_STOP
		SeedSaveBoot(ctx);
	#endif
#endif
	}

	//------------------------------------------------------------------------------------------------
	//! One boot: off, save, wipe, load, on.
	protected void RunRoundTrip(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		LF_PersistenceComponent persistence;
		if (gameMode)
			persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));
		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		LF_FarmPlotComponent plot = LF_FarmPlots.Find("farm_plot_1");
		ctx.NotNull(plot, "farm plot 1 is registered");
		if (!persistence || !plot)
			return;

		array<ref LF_FarmPlotRow> before = LF_FarmPlots.ExportAll();

		Owner("{\"schemaVersion\":1,\"systems\":{\"switches\":{\"Farming\":{\"enabled\":false}}}}");
		ctx.False(LF_Systems.IsEnabled(LF_FarmPlots.SYSTEM), "the owner turns Farming off");

		LF_FarmPlotRow planted = new LF_FarmPlotRow();
		planted.m_sPlotKey = "farm_plot_1";
		planted.m_sCropId = plot.GetCropId();
		planted.m_iGrownSeconds = 40;
		plot.Import(planted);
		ctx.Equal(0, LF_FarmPlots.AdvanceAll(30), "nothing grows while Farming is off");
		ctx.EqualFloat(40, plot.GetGrownSeconds(), 0.001, "the crop keeps its 40 seconds");

		// The real save path, written while the system is off.
		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes with Farming off");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains("farm_plot_1"), "the save written while Farming is off carries the crop");

		// A restart: the plot comes back empty, and the payload is read while the system is still off.
		LF_FarmPlots.ApplyAll(null);
		ctx.True(plot.IsEmpty(), "the restarted plot is empty");
		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		int version;
		loadContext.ReadValue("version", version);
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "farmplots");
		ctx.False(plot.IsEmpty(), "the crop loads while Farming is off");
		ctx.EqualFloat(40, plot.GetGrownSeconds(), 0.001, "with its 40 seconds");

		// On again: the same crop, growing.
		Owner(string.Empty);
		ctx.True(LF_Systems.IsEnabled(LF_FarmPlots.SYSTEM), "Farming is on again");
		ctx.EqualFloat(40, plot.GetGrownSeconds(), 0.001, "turning it on finds the crop as it was");
		ctx.Equal(1, LF_FarmPlots.AdvanceAll(5), "and it grows again");
		ctx.EqualFloat(45, plot.GetGrownSeconds(), 0.001, "by the time that passed while on");

		LF_FarmPlots.ApplyAll(before);
	}

	//------------------------------------------------------------------------------------------------
	//! The e2e save boot: Farming off from a real owner file, and a crop for the save.
	protected void SeedSaveBoot(LF_TestContext ctx)
	{
		ctx.False(FileIO.FileExists(LF_ConfigService.OWNER_PATH), "the save boot's fresh profile has no owner config");
		FileHandle handle = FileIO.OpenFile(LF_ConfigService.OWNER_PATH, FileMode.WRITE);
		ctx.True(handle != null, "the save boot writes an owner config.json with Farming off");
		if (!handle)
			return;

		handle.WriteLine(E2E_OWNER);
		handle.Close();
		LF_ConfigService.Reset();
		LF_Systems.Reload();
		ctx.False(LF_Systems.IsEnabled(LF_FarmPlots.SYSTEM), "the owner file turns Farming off before the shutdown save");
		ctx.True(LF_Systems.WhyOff(LF_FarmPlots.SYSTEM).Contains("owner config"), "and the reason names the owner config");

		array<ref LF_FarmPlotRow> rows = LF_FarmPlots.ExportAll();
		LF_FarmPlotRow row = new LF_FarmPlotRow();
		row.m_sPlotKey = E2E_PLOT;
		row.m_sCropId = "cabbage";
		row.m_iGrownSeconds = E2E_SECONDS;
		rows.Insert(row);
		LF_FarmPlots.ApplyAll(rows);
		LF_FarmPlotRow seeded = RowOf(E2E_PLOT);
		ctx.True(seeded != null, "e2e a saved crop waits for the shutdown save, written with Farming off");
	}

	//------------------------------------------------------------------------------------------------
	//! The e2e reload boot: off from the file, the crop loaded, then on and the crop still there.
	protected void RunReloadBoot(LF_TestContext ctx)
	{
		ctx.True(FileIO.FileExists(LF_ConfigService.OWNER_PATH), "e2e the owner config.json survived the restart");
		LF_ConfigService.Reset();
		LF_Systems.Reload();
		ctx.False(LF_Systems.IsEnabled(LF_FarmPlots.SYSTEM), "e2e Farming is off after the restart, from the owner file");

		LF_FarmPlotRow loaded = RowOf(E2E_PLOT);
		ctx.True(loaded != null, "e2e the crop saved while Farming was off is loaded while it is off");
		if (loaded)
			ctx.Equal(E2E_SECONDS, loaded.m_iGrownSeconds, "e2e with its growth");

		// The owner turns Farming back on.
		FileIO.DeleteFile(LF_ConfigService.OWNER_PATH);
		LF_ConfigService.Reset();
		LF_Systems.Reload();
		ctx.True(LF_Systems.IsEnabled(LF_FarmPlots.SYSTEM), "e2e Farming is on once the owner file no longer turns it off");
		LF_FarmPlotRow back = RowOf(E2E_PLOT);
		ctx.True(back != null, "e2e turning Farming on restores the crop");
		if (back)
			ctx.Equal(E2E_SECONDS, back.m_iGrownSeconds, "e2e with the growth it was saved with");

		// Leave the reload boot's store as the other pass-2 tests expect it.
		array<ref LF_FarmPlotRow> rows = LF_FarmPlots.ExportAll();
		for (int i = rows.Count() - 1; i >= 0; i--)
		{
			if (rows[i] && rows[i].m_sPlotKey == E2E_PLOT)
				rows.Remove(i);
		}
		LF_FarmPlots.ApplyAll(rows);
	}
}
