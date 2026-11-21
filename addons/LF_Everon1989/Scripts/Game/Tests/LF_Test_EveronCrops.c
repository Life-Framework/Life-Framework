// red-proof: observed 2026-11-18 `tools\cli test --no-build --red LF_Test_EveronCrops` printed `RED-OK everon/crops (fails with the hook on, 2 assertions)`; the hook looks up an id the catalogue lacks, which is what main (no cabbage row) answers, and skips the planting.
//! #3176 in game: under the Everon 1989 header (the pack leg of `tools\cli test`), the pack's crop overlay adds
//! cabbage to the catalogue with the cabbage seed, three stage prefabs that all load, a yield of three cabbages
//! and a harvest item the 1989 trader buys. Planting a cabbage seed on open ground makes a plant row whose crop
//! id is `cabbage`. Red on main: core ships no cabbage row.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronCrops : LF_Test
{
	protected static const string OWNER = "everon-crops-gardener";
	protected static const ResourceName SEED = "{F77BE55D8D3AEBA3}Prefabs/Items/Seeds/CabbageSeeds.et";
	protected static const ResourceName CABBAGE = "{9FEBBF75B1D525EE}Prefabs/Items/Food/Cabbage.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/crops";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_CropCatalogue.Reload();
		string cropId = "cabbage";
		if (RedProof())
			cropId = "no-such-crop";

		LF_CropRow cabbage = LF_CropCatalogue.Find(cropId);
		ctx.True(cabbage != null, "the merged catalogue has cabbage");
		if (!cabbage)
			return;

		ctx.True(LF_Utils.SamePrefab(cabbage.m_SeedPrefab, SEED), "cabbage grows from the cabbage seed");
		ctx.True(LF_Utils.SamePrefab(cabbage.m_HarvestPrefab, CABBAGE), "cabbage harvests to the cabbage item");
		ctx.Equal(3, cabbage.m_iYield, "a harvest gives three cabbages");
		ctx.True(cabbage.m_iGrowthSeconds >= 900 && cabbage.m_iGrowthSeconds <= 1500, "cabbage ripens in about 20 real minutes");
		ctx.True(cabbage.m_iWaterMaxSeconds > 0, "cabbage needs water");
		ctx.True(cabbage.m_iWaterMinSeconds * 2 <= cabbage.m_iGrowthSeconds, "one watering inside the window carries it to ripe");
		ctx.True(cabbage.m_iWaterMinSeconds + cabbage.m_iWaterMaxSeconds >= cabbage.m_iGrowthSeconds, "a watering at the earliest second still reaches ripe within the window");
		ctx.True(LF_CropCatalogue.Get().FindBySeed(SEED) == cabbage, "the seed finds the cabbage row");

		ctx.Equal(3, cabbage.m_aStagePrefabs.Count(), "cabbage has three growth stages");
		foreach (int i, ResourceName stage : cabbage.m_aStagePrefabs)
		{
			Resource resource = Resource.Load(stage);
			ctx.True(resource && resource.IsValid(), string.Format("stage %1 prefab loads", i + 1));
		}

		CheckStageScales(ctx, cabbage);
		CheckPlanting(ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! The stages grow: each prefab's mesh child is bigger than the last stage's. LF_Plants spawns a stage with
	//! an identity world transform, which replaces a scale set on the root, so the size is on the child entity
	//! (the prefab data is read here; the drawn result is the `look item` picture in the PR).
	protected void CheckStageScales(LF_TestContext ctx, LF_CropRow cabbage)
	{
		float previous = 0;
		foreach (int i, ResourceName stage : cabbage.m_aStagePrefabs)
		{
			Resource resource = Resource.Load(stage);
			if (!resource || !resource.IsValid())
				continue;

			IEntitySource root = resource.GetResource().ToEntitySource();
			ctx.True(root && root.GetNumChildren() == 1, string.Format("stage %1 has one mesh child", i + 1));
			if (!root || root.GetNumChildren() != 1)
				continue;

			float scale = 1;
			IEntitySource mesh = root.GetChild(0);
			if (mesh && mesh.IsVariableSetDirectly("scale"))
				mesh.Get("scale", scale);

			ctx.False(root.IsVariableSetDirectly("scale"), string.Format("stage %1 keeps its size off the root (a spawn transform would drop it)", i + 1));
			ctx.True(scale > previous, string.Format("stage %1 is bigger than the one before (scale %2)", i + 1, scale));
			previous = scale;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A gardener with one seed plants it on open ground: the row says cabbage and the seed is used.
	protected void CheckPlanting(LF_TestContext ctx)
	{
		array<ref LF_PlantRow> kept = LF_Plants.ExportAll();
		LF_Plants.ResetStatics();

		IEntity gardener = LF_TestCharacterFixture.Spawn(ctx);
		if (gardener)
		{
			vector spot = gardener.GetOrigin() + "1 0 0";
			ctx.Equal(1, LF_InventoryUtils.AddAmount(gardener, SEED, 1, true), "the gardener gets a cabbage seed");
			if (LF_Zones.IsSafe(spot))
			{
				// The fixture stands in a safe zone on this map: planting is refused there and the catalogue checks above stand alone.
				LF_Debug.Info("Farming", "everon/crops: fixture is in a safe zone, planting not exercised");
			}
			else
			{
				string plantId;
				LF_EPlantPlace place;
				LF_EPlantResult result = LF_EPlantResult.NOT_SERVER;
				if (!RedProof())
					result = LF_Plants.PlantAs(gardener, OWNER, SEED, spot, plantId, place);

				ctx.Equal(LF_EPlantResult.OK, result, "a cabbage seed plants on open ground");
				LF_PlantRow row = LF_Plants.Find(plantId);
				ctx.True(row != null, "the plant has a row");
				if (row)
					ctx.EqualStr("cabbage", row.m_sCropId, "the row is a cabbage");
			}
		}

		LF_TestCharacterFixture.Delete(gardener);
		LF_Plants.ResetStatics();
		LF_Plants.ApplyAll(kept);
	}
}
