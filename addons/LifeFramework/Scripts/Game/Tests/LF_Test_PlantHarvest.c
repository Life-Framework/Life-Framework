// red-proof: Observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_PlantHarvest,LF_Test_PlantPersist`: `RED PROOF OK (all 2 listed test(s) fail with the hook on)`, planting skipped. Also red for real on the first green run: a fixture character has no player UID, so planting was refused as BAD_CALLER; the test plants through PlantAs with a key.
// tier: WORLD
//! farming/plant-harvest (#2664): planting takes one seed and makes a plant row; the same spot again is
//! too close; watering before the window is refused and inside it resets the thirst; an unripe plant
//! cannot be harvested; a second character with no water cannot water but CAN harvest the ripe crop
//! (theft), gets the yield, and the row and its mesh are gone. A plant left dry dies and is cleared
//! with nothing given.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PlantHarvest : LF_Test
{
	protected static const string OWNER = "plant-harvest-gardener";
	protected static const ResourceName SEED = "{F77BE55D8D3AEBA3}Prefabs/Items/Seeds/CabbageSeeds.et";
	protected static const ResourceName CABBAGE = "{9FEBBF75B1D525EE}Prefabs/Items/Food/Cabbage.et";
	protected static const ResourceName VISUAL = "{B8FD99C47D856E7F}Prefabs/Vegetation/Crops/FarmPlot_CabbageVisual.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "farming/plant-harvest";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_PlantRow> kept = LF_Plants.ExportAll();
		LF_Plants.ResetStatics();
		LF_Plants.SetCatalogue(TestCatalogue());

		IEntity gardener = LF_TestCharacterFixture.Spawn(ctx);
		IEntity thief = LF_TestCharacterFixture.Spawn(ctx);
		if (gardener && thief)
			Check(ctx, gardener, thief);

		LF_TestCharacterFixture.Delete(gardener);
		LF_TestCharacterFixture.Delete(thief);
		LF_Plants.ResetStatics();
		LF_Plants.ApplyAll(kept);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_CropCatalogue TestCatalogue()
	{
		LF_CropRow row = new LF_CropRow();
		row.m_sCropId = "plant-test-cabbage";
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
	protected void Check(LF_TestContext ctx, IEntity gardener, IEntity thief)
	{
		vector spot = gardener.GetOrigin() + "1 0 0";
		thief.SetOrigin(gardener.GetOrigin() + "0 0 1");
		bool drawn = RplSession.Mode() != RplMode.Dedicated;
		ctx.Equal(2, LF_InventoryUtils.AddAmount(gardener, SEED, 2, true), "the gardener gets two seeds");
		ctx.Equal(1, LF_InventoryUtils.AddAmount(gardener, LF_Plants.WATER_BOTTLE, 1, true), "the gardener gets a water bottle");

		string plantId;
		LF_EPlantPlace place;
		LF_EPlantResult planted = LF_EPlantResult.NOT_SERVER;
		if (!RedProof())
			planted = LF_Plants.PlantAs(gardener, OWNER, SEED, spot, plantId, place);

		if (LF_Zones.IsSafe(spot))
		{
			// The fixture stands in a safe zone on this map: planting is refused there, and that is all this run can see.
			ctx.Equal(LF_EPlantPlace.SAFE_ZONE, place, "a safe zone refuses planting");
			return;
		}

		ctx.Equal(LF_EPlantResult.OK, planted, "the seed is planted");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(gardener, SEED), "planting took one seed");
		ctx.Equal(1, LF_Plants.Count(), "one plant row");
		ctx.Equal(1, LF_Plants.CountByOwner(OWNER), "the plant is the gardener's");
		LF_PlantRow row = LF_Plants.Find(plantId);
		ctx.NotNull(row, "the row is found by its id");
		if (!row)
			return;

		ctx.EqualStr("plant-test-cabbage", row.m_sCropId, "the row grows the seed's crop");
		if (drawn)
			ctx.Equal(1, LF_Plants.VisualCount(), "a listen host draws the plant");

		string otherId;
		ctx.Equal(LF_EPlantResult.PLACE_REFUSED, LF_Plants.PlantAs(gardener, OWNER, SEED, spot, otherId, place), "the same spot again is refused");
		ctx.Equal(LF_EPlantPlace.TOO_CLOSE, place, "refused as too close");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(gardener, SEED), "a refused planting takes no seed");

		ctx.Equal(LF_EPlantResult.TOO_EARLY, LF_Plants.Water(gardener, plantId), "watering at once is too early");
		LF_Plants.AdvanceClock(100);
		ctx.Equal(LF_EPlantResult.OK, LF_Plants.Water(gardener, plantId), "watering inside the window");
		LF_Plants.AdvanceClock(200);
		ctx.Equal(LF_EPlantResult.NO_WATER, LF_Plants.Water(thief, plantId), "no water container, no watering");
		ctx.Equal(LF_EPlantResult.NOT_RIPE, LF_Plants.Harvest(thief, plantId), "an unripe plant is not harvested");
		ctx.Equal(LF_EPlantResult.OK, LF_Plants.Water(gardener, plantId), "the second watering");
		LF_Plants.AdvanceClock(200);
		ctx.Equal(LF_EPlantResult.OK, LF_Plants.Water(gardener, plantId), "the third watering");
		LF_Plants.AdvanceClock(200);

		string stageKey;
		string waterKey;
		ctx.True(LF_Plants.Describe(plantId, stageKey, waterKey), "the plant can be described");
		ctx.EqualStr("#LF-Plant_Stage_Ripe", stageKey, "inspect says ripe");

		ctx.Equal(LF_EPlantResult.OK, LF_Plants.Harvest(thief, plantId), "another character harvests the ripe crop");
		// A full inventory drops the rest at the feet, so count both.
		ctx.Equal(3, LF_InventoryUtils.GetAmount(thief, CABBAGE) + CountLoose(thief.GetOrigin()), "the harvester gets the yield, carried or at their feet");
		ctx.Equal(0, LF_Plants.Count(), "the row is gone");
		ctx.Equal(0, LF_Plants.VisualCount(), "the mesh is gone");
		ctx.Equal(LF_EPlantResult.NO_PLANT, LF_Plants.Harvest(gardener, plantId), "a harvested plant is not there twice");

		// A plant left dry dies and is cleared with nothing given.
		string dryId;
		ctx.Equal(LF_EPlantResult.OK, LF_Plants.PlantAs(gardener, OWNER, SEED, spot, dryId, place), "the second seed is planted");
		LF_Plants.AdvanceClock(300);
		ctx.True(LF_Plants.Describe(dryId, stageKey, waterKey), "the dry plant can be described");
		ctx.EqualStr("#LF-Plant_Stage_Dead", stageKey, "inspect says dead");
		ctx.Equal(LF_EPlantResult.NOT_GROWING, LF_Plants.Water(gardener, dryId), "a dead plant takes no water");
		ctx.Equal(LF_EPlantResult.CLEARED, LF_Plants.Harvest(gardener, dryId), "a dead plant is cleared");
		ctx.Equal(0, LF_InventoryUtils.GetAmount(gardener, CABBAGE), "a dead plant gives nothing");
		ctx.Equal(0, LF_Plants.Count(), "the dead plant's row is gone");
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
