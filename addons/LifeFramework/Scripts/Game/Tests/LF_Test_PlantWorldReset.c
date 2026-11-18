// red-proof: Observed 2026-11-18 on the unfixed step: `farming/plant-world-reset` failed "the farm reset step clears every plant row" (the step only emptied plots) before LF_Plants.ClearAll existed.
//! farming/plant-world-reset (#2820): the world reset's farm step clears every planted crop, its mesh
//! and its replicated row, so a wipe leaves no plants behind and the next save writes none.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PlantWorldReset : LF_Test
{
	protected static const string OWNER = "plant-world-reset-gardener";
	protected static const ResourceName SEED = "{F77BE55D8D3AEBA3}Prefabs/Items/Seeds/CabbageSeeds.et";
	protected static const ResourceName CABBAGE = "{9FEBBF75B1D525EE}Prefabs/Items/Food/Cabbage.et";
	protected static const ResourceName VISUAL = "{B8FD99C47D856E7F}Prefabs/Vegetation/Crops/FarmPlot_CabbageVisual.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "farming/plant-world-reset";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_PlantRow> kept = LF_Plants.ExportAll();
		LF_Plants.ResetStatics();
		LF_Plants.SetCatalogue(TestCatalogue());

		IEntity gardener = LF_TestCharacterFixture.Spawn(ctx);
		if (gardener)
			Check(ctx, gardener);

		LF_TestCharacterFixture.Delete(gardener);
		LF_Plants.ResetStatics();
		LF_Plants.ApplyAll(kept);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_CropCatalogue TestCatalogue()
	{
		LF_CropRow row = new LF_CropRow();
		row.m_sCropId = "plant-reset-cabbage";
		row.m_SeedPrefab = SEED;
		row.m_HarvestPrefab = CABBAGE;
		row.m_iGrowthSeconds = 600;
		row.m_iWaterMinSeconds = 60;
		row.m_iWaterMaxSeconds = 240;
		row.m_iLifetimeAfterRipe = 1800;
		row.m_iYield = 3;
		row.m_bLegal = true;
		row.m_aStagePrefabs = {VISUAL, VISUAL};

		LF_CropCatalogue catalogue = new LF_CropCatalogue();
		catalogue.m_aCrops = {row};
		return catalogue;
	}

	//------------------------------------------------------------------------------------------------
	protected void Check(LF_TestContext ctx, IEntity gardener)
	{
		vector origin = gardener.GetOrigin();
		ctx.Equal(2, LF_InventoryUtils.AddAmount(gardener, SEED, 2, true), "the gardener gets two seeds");

		string firstId;
		string secondId;
		LF_EPlantPlace place;
		LF_EPlantResult first = LF_Plants.PlantAs(gardener, OWNER, SEED, origin + "1 0 0", firstId, place);
		if (LF_Zones.IsSafe(origin + "1 0 0"))
		{
			// The fixture stands in a safe zone on this map: planting is refused there, and there is nothing to reset.
			ctx.Equal(LF_EPlantPlace.SAFE_ZONE, place, "a safe zone refuses planting");
			return;
		}

		LF_EPlantResult second = LF_Plants.PlantAs(gardener, OWNER, SEED, origin + "-1 0 0", secondId, place);
		ctx.Equal(LF_EPlantResult.OK, first, "the first seed is planted");
		ctx.Equal(LF_EPlantResult.OK, second, "the second seed is planted");
		ctx.Equal(2, LF_Plants.Count(), "two plant rows exist before the reset");
		bool drawn = RplSession.Mode() != RplMode.Dedicated;
		if (drawn)
			ctx.Equal(2, LF_Plants.VisualCount(), "a listen host draws both plants");

		LF_WorldReset_Farm step = new LF_WorldReset_Farm();
		int cleared = 0;
		if (!RedProof())
			cleared = step.Run();

		ctx.Equal(0, LF_Plants.Count(), "the farm reset step clears every plant row");
		ctx.Equal(0, LF_Plants.VisualCount(), "the farm reset step deletes every plant mesh");
		ctx.Equal(0, LF_Plants.ExportAll().Count(), "the next save exports no plants");
		ctx.Equal(0, LF_PlantReplication.Collect().Count(), "a late joiner is sent no plants");
		ctx.True(cleared >= 2, "the step reports the plants it cleared");
		ctx.Equal(LF_EPlantResult.NO_PLANT, LF_Plants.Harvest(gardener, firstId), "a cleared plant cannot be harvested");
	}
}
