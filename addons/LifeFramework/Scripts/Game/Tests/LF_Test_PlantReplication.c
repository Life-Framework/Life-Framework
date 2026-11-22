// red-proof: with `tools\cli test --no-build --red LF_Test_PlantReplication` the RedProof() hook skips the planting, so the carrier table stays empty and `one planted crop is in the carrier table` fails; before LF_PlantReplication existed the test did not compile.
//! farming/plant-replication (#2819): a dedicated server draws no plants, so the carrier table
//! (LF_PlantReplication) holds one id, crop, position and stage per planted crop. Planting adds a row,
//! growth moves its stage, harvest removes it. A late joiner's rows (Collect) applied on a fresh client
//! make one local mesh per row, a stage change redraws it, and a removal deletes it. A row for a crop the
//! client does not know draws nothing and is kept out of the mirror.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PlantReplication : LF_Test
{
	protected static const string OWNER = "plant-replication-gardener";
	protected static const ResourceName SEED = "{F77BE55D8D3AEBA3}Prefabs/Items/Seeds/CabbageSeeds.et";
	protected static const ResourceName CABBAGE = "{9FEBBF75B1D525EE}Prefabs/Items/Food/Cabbage.et";
	protected static const ResourceName VISUAL = "{B8FD99C47D856E7F}Prefabs/Vegetation/Crops/FarmPlot_CabbageVisual.et";
	protected static const string CROP = "plant-repl-cabbage";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "farming/plant-replication";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_PlantRow> kept = LF_Plants.ExportAll();
		LF_Plants.ResetStatics();
		LF_PlantReplication.ResetStatics();
		LF_Plants.SetCatalogue(TestCatalogue());

		IEntity gardener = LF_TestCharacterFixture.Spawn(ctx);
		if (gardener)
			Check(ctx, gardener);

		LF_TestCharacterFixture.Delete(gardener);
		LF_PlantReplication.ResetStatics();
		LF_Plants.ResetStatics();
		LF_Plants.ApplyAll(kept);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_CropCatalogue TestCatalogue()
	{
		LF_CropRow row = new LF_CropRow();
		row.m_sCropId = CROP;
		row.m_SeedPrefab = SEED;
		row.m_HarvestPrefab = CABBAGE;
		row.m_iGrowthSeconds = 600;
		row.m_iWaterMinSeconds = 0;
		row.m_iWaterMaxSeconds = 0;	// needs no water, so it grows alone
		row.m_iLifetimeAfterRipe = 0;
		row.m_iYield = 3;
		row.m_bLegal = true;
		row.m_aStagePrefabs = {VISUAL, VISUAL, VISUAL};

		LF_CropCatalogue catalogue = new LF_CropCatalogue();
		catalogue.m_aCrops = {row};
		return catalogue;
	}

	//------------------------------------------------------------------------------------------------
	protected void Check(LF_TestContext ctx, IEntity gardener)
	{
		vector spot = gardener.GetOrigin() + "1 0 0";
		ctx.Equal(1, LF_InventoryUtils.AddAmount(gardener, SEED, 1, true), "the gardener gets a seed");

		string plantId;
		LF_EPlantPlace place;
		LF_EPlantResult planted = LF_EPlantResult.NOT_SERVER;
		if (!RedProof())
			planted = LF_Plants.PlantAs(gardener, OWNER, SEED, spot, plantId, place);

		if (LF_Zones.IsSafe(spot))
		{
			ctx.Equal(LF_EPlantPlace.SAFE_ZONE, place, "a safe zone refuses planting");
			return;
		}

		ctx.Equal(LF_EPlantResult.OK, planted, "the seed is planted");
		array<ref LF_PlantViewRow> table = LF_PlantReplication.Collect();
		ctx.Equal(1, table.Count(), "one planted crop is in the carrier table");
		if (table.Count() != 1)
			return;

		LF_PlantRow row = LF_Plants.Find(plantId);
		ctx.NotNull(row, "the server row exists");
		ctx.EqualStr(plantId, table[0].m_sPlantId, "the table row carries the plant id");
		ctx.EqualStr(CROP, table[0].m_sCropId, "the table row carries the crop");
		ctx.Equal(0, table[0].m_iStage, "a fresh plant shows its first stage");
		if (row)
			ctx.True(vector.Distance(row.m_vPosition, table[0].m_vPosition) < 0.01, "the table row carries the position");

		// A late joiner: a fresh client mirror takes the whole table and draws one mesh per row.
		LF_PlantReplication.ResetClient();
		ctx.Equal(1, LF_PlantReplication.ClientApplyAll(table), "the joiner applies one row");
		ctx.Equal(1, LF_PlantReplication.ClientCount(), "the joiner's mirror holds the plant");
		ctx.Equal(1, LF_PlantReplication.ClientVisualCount(), "the joiner draws one local mesh");
		ctx.Equal(0, LF_PlantReplication.ClientStageOf(plantId), "the joiner shows the first stage");

		// Growth: the server table moves to the last stage, and the client redraws on the change.
		LF_Plants.AdvanceClock(600);
		table = LF_PlantReplication.Collect();
		ctx.Equal(1, table.Count(), "still one planted crop in the table");
		if (table.Count() == 1)
		{
			ctx.Equal(2, table[0].m_iStage, "a ripe plant shows the last stage");
			LF_PlantReplication.ClientUpsert(table[0]);
			ctx.Equal(2, LF_PlantReplication.ClientStageOf(plantId), "the client shows the ripe stage");
			ctx.Equal(1, LF_PlantReplication.ClientVisualCount(), "a stage change redraws, never duplicates");
		}

		// A row of a crop this client does not know draws nothing and is not kept.
		LF_PlantViewRow stranger = new LF_PlantViewRow();
		stranger.m_sPlantId = "plant-repl-stranger";
		stranger.m_sCropId = "no-such-crop";
		stranger.m_vPosition = spot + "2 0 0";
		array<ref LF_PlantViewRow> strangers = {stranger};
		ctx.Equal(0, LF_PlantReplication.ClientApplyAll(strangers), "an unknown crop is not applied");
		ctx.Equal(0, LF_PlantReplication.ClientVisualCount(), "an applied table replaces the old meshes");
		LF_PlantReplication.ClientApplyAll(table);

		// Harvest: the row leaves the table, and the client deletes its mesh.
		ctx.Equal(LF_EPlantResult.OK, LF_Plants.Harvest(gardener, plantId), "the ripe crop is harvested");
		// A yield that does not fit the pockets lands at the gardener's feet, at the gate: delete it, or a
		// cabbage stands on the gate spawn point and spawn/point-floor finds no room there.
		DeleteLoose(gardener.GetOrigin());
		ctx.Equal(0, LF_PlantReplication.Collect().Count(), "the harvested plant leaves the table");
		LF_PlantReplication.ClientRemove(plantId);
		ctx.Equal(0, LF_PlantReplication.ClientCount(), "the client mirror drops it");
		ctx.Equal(0, LF_PlantReplication.ClientVisualCount(), "the client deletes the mesh");
		LF_PlantReplication.ResetClient();
	}

	//------------------------------------------------------------------------------------------------
	protected ref array<IEntity> m_aLoose = {};

	//------------------------------------------------------------------------------------------------
	//! Deletes the cabbages lying loose within 3 m of position (the harvest's overflow).
	protected void DeleteLoose(vector position)
	{
		m_aLoose.Clear();
		GetGame().GetWorld().QueryEntitiesBySphere(position, 3, AddLoose, null, EQueryEntitiesFlags.ALL);
		foreach (IEntity loose : m_aLoose)
			SCR_EntityHelper.DeleteEntityAndChildren(loose);

		m_aLoose.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected bool AddLoose(IEntity entity)
	{
		if (entity && !entity.GetParent() && LF_Utils.SamePrefab(LF_Utils.GetPrefabName(entity), CABBAGE))
			m_aLoose.Insert(entity);

		return true;
	}
}
