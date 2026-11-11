// red-proof: observed 2026-11-10 with `tools\cli test --no-build --only LF_Test_FarmPlotsPersist`: a farmplots read
// that dropped the rows instead of applying them failed `- the crop is back half grown (expected 1, got -1)` and
// `- with the same 75 seconds of growth (expected 75, got 0)`. Restored; green.

// tier: PERSISTENCE
//! A growing crop rides the real save path (the "farmplots" payload, save version 23, #614): after
//! the store is wiped the plot is empty, after the payload is read it is back at the same growth and
//! stage and still ripens, and reading twice changes nothing. A saved crop whose plot is not in the
//! world waits and is kept, and a crop for a plot that now grows something else is dropped. The
//! test puts every plot back the way it found it.
class LF_Test_FarmPlotsPersist : LF_Test
{
	override string GetName()
	{
		return "persist/farm-plots-roundtrip";
	}

	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		ctx.NotNull(gameMode, "the DebugWorld game mode resolves");
		if (!gameMode)
			return;

		LF_PersistenceComponent persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));
		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		LF_FarmPlotComponent plot = LF_FarmPlots.Find("farm_plot_2");
		LF_FarmPlotComponent other = LF_FarmPlots.Find("farm_plot_3");
		ctx.NotNull(plot, "farm plot 2 is registered");
		ctx.NotNull(other, "farm plot 3 is registered");
		if (!persistence || !plot || !other)
			return;

		array<ref LF_FarmPlotRow> before = LF_FarmPlots.ExportAll();

		// Plot 2 was planted 75 seconds ago (half grown); plot 3 stays empty.
		LF_FarmPlots.ApplyAll(null);
		LF_FarmPlotRow planted = new LF_FarmPlotRow();
		planted.m_sPlotKey = "farm_plot_2";
		planted.m_sCropId = plot.GetCropId();
		planted.m_iGrownSeconds = 75;
		plot.Import(planted);
		ctx.Equal(1, plot.GetStage(), "the plot is half grown");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains("farm_plot_2"), "the save carries the planted plot");
		ctx.False(saved.Contains("farm_plot_3"), "an empty plot is not saved");

		// A restart: the world comes back with empty plots.
		LF_FarmPlots.ApplyAll(null);
		ctx.True(plot.IsEmpty(), "the restarted plot is empty");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		int version;
		loadContext.ReadValue("version", version);
		ctx.Equal(23, version, "the save is written at version 23 (farmplots is payload 23)");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "farmplots");
		ctx.Equal(1, plot.GetStage(), "the crop is back half grown");
		ctx.EqualFloat(75, plot.GetGrownSeconds(), 0.001, "with the same 75 seconds of growth");
		ctx.True(other.IsEmpty(), "the other plot is still empty");

		// Loading again converges (idempotent), and the crop still ripens.
		JsonLoadContext again = new JsonLoadContext(false);
		again.LoadFromString(saved);
		again.ReadValue("version", version);
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, again, "farmplots");
		ctx.EqualFloat(75, plot.GetGrownSeconds(), 0.001, "a second load leaves the same growth");
		ctx.Equal(1, LF_FarmPlots.ExportAll().Count(), "and exactly one saved crop");
		plot.ServerAdvance(LF_FarmPlotRules.SecondsToLastStage(plot.GetStages()));
		ctx.True(plot.IsRipe(), "the restored crop ripens");

		// A crop for a plot that is not in the world waits, and is kept across a save.
		array<ref LF_FarmPlotRow> ghosts = new array<ref LF_FarmPlotRow>();
		LF_FarmPlotRow ghost = new LF_FarmPlotRow();
		ghost.m_sPlotKey = "farm_plot_ghost";
		ghost.m_sCropId = "cabbage";
		ghost.m_iGrownSeconds = 30;
		ghosts.Insert(ghost);
		LF_FarmPlots.ApplyAll(ghosts);
		ctx.Equal(1, LF_FarmPlots.ExportAll().Count(), "a crop with no plot yet is kept");
		ctx.True(plot.IsEmpty(), "and does not touch the plots that exist");

		// A crop saved for a plot that now grows something else is dropped.
		LF_FarmPlotRow stale = new LF_FarmPlotRow();
		stale.m_sPlotKey = "farm_plot_2";
		stale.m_sCropId = "a-crop-that-was-removed";
		stale.m_iGrownSeconds = 40;
		plot.Import(stale);
		ctx.True(plot.IsEmpty(), "a crop for a changed plot is dropped");

		// Put the world back.
		LF_FarmPlots.ApplyAll(before);
	}
}
