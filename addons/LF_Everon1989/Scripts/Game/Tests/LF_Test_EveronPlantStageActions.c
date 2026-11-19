// red-proof: observed 2026-11-19 with `tools\cli test --no-build --red LF_Test_EveronPlantStageActions`: `RED PROOF OK`, `cabbage stage ...LF_CabbageStage3.et carries one HARVEST action (expected 1, got 0)`; the hook checks the bare mesh the stages inherit, which is what main shipped.
//! #3177 in game: under the Everon 1989 header (the pack leg of `tools\cli test`), every cabbage stage prefab
//! the merged crop catalogue names carries one HARVEST and one INSPECT action, so a gardener walking up to the
//! plant sees Harvest and Inspect on it. Red on main: the stage prefabs carried no actions.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronPlantStageActions : LF_Test
{
	protected static const ResourceName BARE_VISUAL = "{B8FD99C47D856E7F}Prefabs/Vegetation/Crops/FarmPlot_CabbageVisual.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/plant-stage-actions";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_CropCatalogue.Reload();
		LF_CropRow cabbage = LF_CropCatalogue.Find("cabbage");
		ctx.NotNull(cabbage, "the merged catalogue has cabbage");
		if (!cabbage)
			return;

		ctx.Equal(3, cabbage.m_aStagePrefabs.Count(), "cabbage has three stage prefabs");
		foreach (ResourceName stage : cabbage.m_aStagePrefabs)
		{
			// The red-proof hook checks the bare mesh the stages inherit, as main's stages were.
			ResourceName checked = stage;
			if (RedProof())
				checked = BARE_VISUAL;

			LF_Test_PlantHarvestAction.CheckStageActions(ctx, checked, "cabbage stage " + stage);
		}
	}
}
