// red-proof: observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_PlantOpenGround`: `RED PROOF OK (all 1 listed test(s) fail with the hook on)`; the pure open case and the planting are skipped, so `open ground takes the seed (expected 0, got 1)` fails. The first green attempt was also red for real: the DebugWorld ground is an entity, so a plain "trace hit an entity" check refused every spot (fixed: only a hit above the terrain counts as cover).

//! farming/plant-open-ground (#2821, spec #809): a seed goes on open ground only. Refused when the planter
//! stands well above the terrain (a floor or a roof), when a trace from above the spot hits a building or
//! prop before the terrain, on a road surface and below sea level; accepted on plain ground. A refusal
//! takes no seed and has its own toast key.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PlantOpenGround : LF_Test
{
	protected static const string OWNER = "plant-open-ground-gardener";
	protected static const ResourceName SEED = "{F77BE55D8D3AEBA3}Prefabs/Items/Seeds/CabbageSeeds.et";
	protected static const ResourceName CABBAGE = "{9FEBBF75B1D525EE}Prefabs/Items/Food/Cabbage.et";
	protected static const ResourceName VISUAL = "{B8FD99C47D856E7F}Prefabs/Vegetation/Crops/FarmPlot_CabbageVisual.et";
	protected static const ResourceName VANILLA_BOX = "{2B1D0128ED660BE9}Prefabs/Props/Crates/BoxWooden_01.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "farming/plant-open-ground";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		CheckRules(ctx);

		array<ref LF_PlantRow> kept = LF_Plants.ExportAll();
		LF_Plants.ResetStatics();
		LF_Plants.SetCatalogue(TestCatalogue());

		IEntity gardener = LF_TestCharacterFixture.Spawn(ctx);
		if (gardener)
			CheckWorld(ctx, gardener);

		LF_TestCharacterFixture.Delete(gardener);
		LF_Plants.ResetStatics();
		LF_Plants.ApplyAll(kept);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_CropCatalogue TestCatalogue()
	{
		LF_CropRow row = new LF_CropRow();
		row.m_sCropId = "plant-open-ground-cabbage";
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
	//! The pure rule: each reason alone refuses, open ground passes.
	protected void CheckRules(LF_TestContext ctx)
	{
		LF_EPlantPlace open = LF_EPlantPlace.NOT_OPEN_GROUND;
		if (!RedProof())
			open = LF_PlantRules.OpenGround(0.1, 10, 0, false, "");

		ctx.True(open == LF_EPlantPlace.OK, "plain ground above the sea, nothing overhead, no road: open");
		ctx.True(LF_PlantRules.OpenGround(2.5, 10, 0, false, "") == LF_EPlantPlace.NOT_OPEN_GROUND, "a planter 2.5 m above the terrain is on a floor or a roof");
		ctx.True(LF_PlantRules.OpenGround(0.1, 10, 0, true, "") == LF_EPlantPlace.NOT_OPEN_GROUND, "a building or prop over the spot refuses");
		ctx.True(LF_PlantRules.OpenGround(0.1, 10, 0, false, "{ABCD}Common/Materials/Game/Asphalt.gamemat") == LF_EPlantPlace.NOT_OPEN_GROUND, "an asphalt road refuses");
		ctx.True(LF_PlantRules.OpenGround(0.1, -0.5, 0, false, "") == LF_EPlantPlace.NOT_OPEN_GROUND, "a spot below sea level refuses");
		ctx.True(LF_PlantRules.OpenGround(0.1, 10, 0, false, "Common/Materials/Game/Grass.gamemat") == LF_EPlantPlace.OK, "a grass surface is open");
		ctx.True(LF_PlantRules.IsHardSurface("Common/Materials/Game/Gravel_Road.gamemat"), "a gravel road is a hard surface");
		ctx.True(!LF_PlantRules.IsHardSurface(""), "an unknown surface is not called hard");
		ctx.EqualStr("#LF-Plant_NotOpenGround", LF_PlantRules.PlaceRefusalKey(LF_EPlantPlace.NOT_OPEN_GROUND), "the refusal has its own toast key");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckWorld(LF_TestContext ctx, IEntity gardener)
	{
		vector home = gardener.GetOrigin();
		vector spot = home + "1 0 0";
		ctx.Equal(3, LF_InventoryUtils.AddAmount(gardener, SEED, 3, true), "the gardener gets three seeds");

		if (LF_Zones.IsSafe(spot))
		{
			// The fixture stands in a safe zone on this map: the safe zone refuses first, so the ground rules cannot be seen here.
			return;
		}

		BaseWorld world = GetGame().GetWorld();
		string plantId;
		LF_EPlantPlace place;
		LF_EPlantResult result;

		// On a floor or a roof: the planter stands well above the terrain under them.
		gardener.SetOrigin(home + "0 2.5 0");
		result = LF_Plants.PlantAs(gardener, OWNER, SEED, spot, plantId, place);
		ctx.Equal(LF_EPlantResult.PLACE_REFUSED, result, "a planter well above the terrain is refused");
		ctx.True(place == LF_EPlantPlace.NOT_OPEN_GROUND, "refused as not open ground (standing high)");
		gardener.SetOrigin(home);

		// A prop over the spot: a trace from above hits it before the terrain.
		Resource boxResource = Resource.Load(VANILLA_BOX);
		ctx.True(boxResource.IsValid(), "the vanilla box prop loads");
		if (boxResource.IsValid())
		{
			EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			params.Transform[3] = spot + "0 3 0";
			IEntity box = GetGame().SpawnEntityPrefab(boxResource, world, params);
			ctx.True(box != null, "the box spawns over the spot");
			if (box)
			{
				result = LF_Plants.PlantAs(gardener, OWNER, SEED, spot, plantId, place);
				ctx.Equal(LF_EPlantResult.PLACE_REFUSED, result, "a spot under a prop is refused");
				ctx.True(place == LF_EPlantPlace.NOT_OPEN_GROUND, "refused as not open ground (covered)");
				SCR_EntityHelper.DeleteEntityAndChildren(box);
			}
		}

		ctx.Equal(3, LF_InventoryUtils.GetAmount(gardener, SEED), "a refused planting takes no seed");
		ctx.Equal(0, LF_Plants.Count(), "no plant row from a refusal");

		// Plain ground: accepted.
		result = LF_EPlantResult.NOT_SERVER;
		if (!RedProof())
			result = LF_Plants.PlantAs(gardener, OWNER, SEED, spot, plantId, place);

		ctx.Equal(LF_EPlantResult.OK, result, "open ground takes the seed");
		ctx.Equal(1, LF_Plants.Count(), "one plant row");
		ctx.Equal(2, LF_InventoryUtils.GetAmount(gardener, SEED), "planting took one seed");
	}
}
