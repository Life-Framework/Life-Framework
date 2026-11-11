// red-proof: spawning BaseCrop (this test's first spawn) VME-crashed on the
// unguarded m_aCropStages.Count() in the stage-change path until LF_BaseCrop
// guarded the null stage list; that crash is the observed red. Point any other
// assertion at the wrong stage count (e.g. expect 4 stages on CabbageCrop) and
// it fails on the next `tools\cli test --tier all`.
// tier: WORLD
class LF_Test_CropPrefabs : LF_Test
{
	protected static const ResourceName BASE_CROP = "{64DFA805B1BF7F7D}Prefabs/Vegetation/Crops/BaseCrop.et";
	protected static const ResourceName CABBAGE_CROP = "{C4B2301E5D989B42}Prefabs/Vegetation/Crops/CabbageCrop.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "crop/prefab-stage-shape";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_BaseCrop SpawnCrop(LF_TestContext ctx, ResourceName path)
	{
		Resource res = Resource.Load(path);
		ctx.True(res.IsValid(), string.Format("crop prefab loads: %1", path));
		if (ctx.FailureCount() > 0)
			return null;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;

		IEntity entity = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.True(entity != null, string.Format("crop prefab spawns an entity: %1", path));
		if (ctx.FailureCount() > 0)
			return null;

		LF_BaseCrop crop = LF_BaseCrop.Cast(entity);
		ctx.True(crop != null, string.Format("spawned crop is an LF_BaseCrop: %1", path));
		return crop;
	}

	//------------------------------------------------------------------------------------------------
	protected void AssertStageShape(LF_TestContext ctx, LF_BaseCrop crop, int expectedCount, string expectedLastStage, string label)
	{
		array<ref LF_CropStage> stages = crop.GetCropStages();
		ctx.True(stages != null, string.Format("%1 carries a stage list", label));
		if (ctx.FailureCount() > 0)
			return;

		ctx.Equal(expectedCount, stages.Count(), string.Format("%1 stage count", label));

		for (int i = 0; i < stages.Count(); i++)
		{
			LF_CropStage stage = stages[i];
			ctx.True(stage != null, string.Format("%1 stage %2 instance is not null", label, i));
			if (!stage)
				continue;

			ctx.True(!stage.m_sStageName.IsEmpty(), string.Format("%1 stage %2 has a name", label, i));
			ctx.True(stage.GetDurationMs() > 0, string.Format("%1 stage %2 has a positive duration", label, i));
			ctx.True(stage.m_fStageScale > 0, string.Format("%1 stage %2 has a positive scale", label, i));
		}

		LF_CropStage lastStage = stages[stages.Count() - 1];
		ctx.EqualStr(expectedLastStage, lastStage.m_sStageName, string.Format("%1 last stage name", label));
		ctx.True(lastStage.m_bCanGather, string.Format("%1 last stage can gather", label));
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// Base crop is the abstract shape: no stages of its own, never gatherable
		LF_BaseCrop baseCrop = SpawnCrop(ctx, BASE_CROP);
		if (baseCrop)
		{
			array<ref LF_CropStage> baseStages = baseCrop.GetCropStages();
			ctx.True(baseStages == null || baseStages.Count() == 0, "base crop has no stages of its own");
			ctx.False(baseCrop.CanGather(), "base crop cannot gather");
			ctx.Equal(-1, baseCrop.GetSpawnItemsAtStage(), "base crop spawns no items");
		}

		// CabbageCrop: 3 stages, gather-only (no area spawner)
		LF_BaseCrop cabbageCrop = SpawnCrop(ctx, CABBAGE_CROP);
		if (cabbageCrop)
		{
			AssertStageShape(ctx, cabbageCrop, 3, "Ripe", "cabbage");
			ctx.Equal(-1, cabbageCrop.GetSpawnItemsAtStage(), "cabbage spawns no items");
			ctx.False(cabbageCrop.GetDeleteAfterFinalStage(), "cabbage stays after the final stage");
		}
	}
}