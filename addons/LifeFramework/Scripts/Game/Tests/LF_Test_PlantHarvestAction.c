// red-proof: observed 2026-11-19 with `tools\cli test --no-build --red LF_Test_PlantHarvestAction,...`: `RED-OK farming/plant-harvest-action (fails with the hook on, 21 assertions)`; the hook leaves the plant unripe, so the ripe harvest is refused and pays nothing.
//! farming/plant-harvest-action (#3177): Harvest and Inspect on the plant itself go through the player
//! controller's request with the plant id only (LF_PlantHarvestRpc.Serve, the server half of
//! RpcAsk_LF_PlantHarvest). Every stage prefab the merged crop catalogue names carries one HARVEST and one
//! INSPECT action (the pack leg's everon/plant-stage-actions checks the cabbage stages by name). An
//! unripe plant gives nothing and is refused with its stage words; a ripe plant within reach gives the
//! yield and removes the row, once: a second request finds no row. A plant out of reach and an unknown
//! id are refused.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PlantHarvestAction : LF_Test
{
	protected static const string OWNER = "plant-harvest-action-gardener";
	protected static const ResourceName SEED = "{F77BE55D8D3AEBA3}Prefabs/Items/Seeds/CabbageSeeds.et";
	protected static const ResourceName CABBAGE = "{9FEBBF75B1D525EE}Prefabs/Items/Food/Cabbage.et";
	protected static const ResourceName VISUAL = "{B8FD99C47D856E7F}Prefabs/Vegetation/Crops/FarmPlot_CabbageVisual.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "farming/plant-harvest-action";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		CheckCatalogueStages(ctx);

		array<ref LF_PlantRow> kept = LF_Plants.ExportAll();
		LF_Plants.ResetStatics();
		LF_Plants.SetCatalogue(TestCatalogue());

		IEntity gardener = LF_TestCharacterFixture.Spawn(ctx);
		IEntity faraway = LF_TestCharacterFixture.Spawn(ctx);
		if (gardener && faraway)
			Check(ctx, gardener, faraway);

		LF_TestCharacterFixture.Delete(gardener);
		LF_TestCharacterFixture.Delete(faraway);
		LF_Plants.ResetStatics();
		LF_Plants.ApplyAll(kept);
	}

	//------------------------------------------------------------------------------------------------
	//! Each stage prefab of each catalogued crop carries the actions a gardener uses on the plant.
	protected void CheckCatalogueStages(LF_TestContext ctx)
	{
		LF_CropCatalogue catalogue = LF_CropCatalogue.Get();
		if (!catalogue || !catalogue.m_aCrops)
			return;

		foreach (LF_CropRow crop : catalogue.m_aCrops)
		{
			if (!crop || !crop.m_aStagePrefabs)
				continue;

			foreach (ResourceName stage : crop.m_aStagePrefabs)
				CheckStageActions(ctx, stage, crop.m_sCropId + " stage " + stage);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A stage prefab spawned locally (as LF_Plants draws it) carries one HARVEST and one INSPECT.
	static void CheckStageActions(LF_TestContext ctx, ResourceName prefab, string label)
	{
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
		{
			ctx.Fail(label + " does not load");
			return;
		}

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		IEntity visual = GetGame().SpawnEntityPrefabLocal(res, GetGame().GetWorld(), params);
		if (!visual)
		{
			ctx.Fail(label + " does not spawn");
			return;
		}

		int harvest;
		int inspect;
		ActionsManagerComponent actions = ActionsManagerComponent.Cast(visual.FindComponent(ActionsManagerComponent));
		if (actions)
		{
			array<BaseUserAction> list = {};
			actions.GetActionsList(list);
			foreach (BaseUserAction base : list)
			{
				LF_PlantAction plant = LF_PlantAction.Cast(base);
				if (plant && plant.GetMode() == LF_EPlantActionMode.HARVEST)
					harvest++;
				else if (plant && plant.GetMode() == LF_EPlantActionMode.INSPECT)
					inspect++;
			}
		}

		ctx.Equal(1, harvest, label + " carries one HARVEST action");
		ctx.Equal(1, inspect, label + " carries one INSPECT action");
		SCR_EntityHelper.DeleteEntityAndChildren(visual);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_CropCatalogue TestCatalogue()
	{
		LF_CropRow row = new LF_CropRow();
		row.m_sCropId = "plant-action-test-cabbage";
		row.m_SeedPrefab = SEED;
		row.m_HarvestPrefab = CABBAGE;
		row.m_iGrowthSeconds = 600;
		row.m_iWaterMinSeconds = 0;
		row.m_iWaterMaxSeconds = 0;
		row.m_iLifetimeAfterRipe = 1800;
		row.m_iYield = 3;
		row.m_bLegal = true;
		row.m_aStagePrefabs = {VISUAL, VISUAL};

		LF_CropCatalogue catalogue = new LF_CropCatalogue();
		catalogue.m_aCrops = {row};
		return catalogue;
	}

	//------------------------------------------------------------------------------------------------
	protected void Check(LF_TestContext ctx, IEntity gardener, IEntity faraway)
	{
		vector spot = gardener.GetOrigin() + "1 0 0";
		faraway.SetOrigin(gardener.GetOrigin() + "0 0 12");
		string plantId;
		LF_EPlantPlace place;
		ctx.Equal(1, LF_InventoryUtils.AddAmount(gardener, SEED, 1, true), "the gardener gets a seed");
		LF_EPlantResult planted = LF_Plants.PlantAs(gardener, OWNER, SEED, spot, plantId, place);
		if (LF_Zones.IsSafe(spot))
		{
			// The fixture stands in a safe zone on this map: planting is refused there, and that is all this run can see.
			ctx.Equal(LF_EPlantPlace.SAFE_ZONE, place, "a safe zone refuses planting");
			return;
		}

		ctx.Equal(LF_EPlantResult.OK, planted, "the seed is planted");
		if (planted != LF_EPlantResult.OK)
			return;

		string spoken;
		int harvest = LF_EPlantActionMode.HARVEST;
		int inspect = LF_EPlantActionMode.INSPECT;

		// Unripe: nothing given, refused with its stage words, the row stays.
		ctx.Equal(LF_EPlantResult.NOT_RIPE, LF_PlantHarvestRpc.Serve(gardener, plantId, harvest, spoken), "an unripe plant is refused");
		ctx.EqualStr("#LF-Plant_Stage_Sprout", spoken, "the refusal says its stage in words");
		ctx.Equal(0, LF_InventoryUtils.GetAmount(gardener, CABBAGE), "an unripe plant gives nothing");
		ctx.Equal(1, LF_Plants.Count(), "the unripe plant stays");
		ctx.Equal(LF_EPlantResult.OK, LF_PlantHarvestRpc.Serve(gardener, plantId, inspect, spoken), "inspect within reach answers");

		if (!RedProof())
			LF_Plants.AdvanceClock(700);

		// Out of reach and unknown: refused, the row stays.
		ctx.Equal(LF_EPlantResult.TOO_FAR, LF_PlantHarvestRpc.Serve(faraway, plantId, harvest, spoken), "a plant out of reach is refused");
		ctx.Equal(0, LF_InventoryUtils.GetAmount(faraway, CABBAGE), "out of reach gives nothing");
		ctx.Equal(LF_EPlantResult.NO_PLANT, LF_PlantHarvestRpc.Serve(gardener, "no-such-plant", harvest, spoken), "an unknown plant id is refused");
		ctx.Equal(LF_EPlantResult.BAD_CALLER, LF_PlantHarvestRpc.Serve(gardener, plantId, LF_EPlantActionMode.PLANT, spoken), "a request in another mode is refused");
		ctx.Equal(1, LF_Plants.Count(), "refusals leave the ripe plant");

		// Ripe and within reach: the yield, the row gone, and a second request pays nothing.
		ctx.Equal(LF_EPlantResult.OK, LF_PlantHarvestRpc.Serve(gardener, plantId, harvest, spoken), "a ripe plant within reach is harvested");
		int carried = LF_InventoryUtils.GetAmount(gardener, CABBAGE);
		int got = carried + CountLoose(gardener.GetOrigin());
		ctx.Equal(3, got, "the harvester gets the yield, carried or at their feet");
		ctx.Equal(0, LF_Plants.Count(), "the row is gone");
		ctx.Equal(LF_EPlantResult.NO_PLANT, LF_PlantHarvestRpc.Serve(gardener, plantId, harvest, spoken), "a second harvest of the same plant is refused");
		ctx.Equal(carried, LF_InventoryUtils.GetAmount(gardener, CABBAGE), "the second request pays nothing");
	}

	//------------------------------------------------------------------------------------------------
	protected int m_iLoose;
	protected ref array<IEntity> m_aLoose = {};

	//------------------------------------------------------------------------------------------------
	//! Cabbages lying loose within 3 m of position; they are deleted once counted, so no prop is left behind.
	protected int CountLoose(vector position)
	{
		m_iLoose = 0;
		m_aLoose.Clear();
		GetGame().GetWorld().QueryEntitiesBySphere(position, 3, AddLoose, null, EQueryEntitiesFlags.ALL);
		foreach (IEntity loose : m_aLoose)
			SCR_EntityHelper.DeleteEntityAndChildren(loose);

		m_aLoose.Clear();
		return m_iLoose;
	}

	//------------------------------------------------------------------------------------------------
	protected bool AddLoose(IEntity entity)
	{
		if (entity && !entity.GetParent() && LF_Utils.SamePrefab(LF_Utils.GetPrefabName(entity), CABBAGE))
		{
			m_iLoose++;
			m_aLoose.Insert(entity);
		}

		return true;
	}
}
