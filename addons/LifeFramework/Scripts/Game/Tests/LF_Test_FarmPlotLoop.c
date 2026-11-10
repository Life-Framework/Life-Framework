// red-proof: observed 2026-11-10 with `tools\cli test --no-build --only LF_Test_FarmPlotLoop`: the inclusive stage
// boundary failed `- half way it is half grown (expected 1, got 0)`, and a harvest that did not empty
// the plot failed `- the plot is empty again` and `- the second packet plants it again (expected 0, got 2)`. Restored; green.

// tier: WORLD
//! The farm loop on a placed DebugWorld plot (#614), through the same server calls the plot action
//! makes: no seed packet refuses, a packet plants and is used up, a planted plot refuses a second
//! packet and an early harvest, growth walks sprouts -> half grown -> ripe, the harvest gives three
//! cabbages and empties the plot, and a farmer across the field is refused. The plot's crop slots
//! lie on the bed. The test puts the plot back the way it found it.
class LF_Test_FarmPlotLoop : LF_Test
{
	protected static const string PLOT_KEY = "farm_plot_1";
	protected static const ResourceName SEEDS = "{F77BE55D8D3AEBA3}Prefabs/Items/Seeds/CabbageSeeds.et";
	protected static const ResourceName CABBAGE = "{9FEBBF75B1D525EE}Prefabs/Items/Food/Cabbage.et";

	override string GetName()
	{
		return "farm/plot-loop";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_FarmPlotComponent plot = LF_FarmPlots.Find(PLOT_KEY);
		ctx.NotNull(plot, "DebugWorld farm plot 1 is registered by its key");
		if (!plot)
			return;

		ctx.True(LF_FarmPlots.Count() >= 3, "the farm has three plots");
		ctx.EqualStr(SEEDS, plot.GetSeedPrefab(), "the plot takes cabbage seed packets");
		ctx.EqualStr(CABBAGE, plot.GetHarvestPrefab(), "the plot gives cabbages");
		ctx.True(Resource.Load(SEEDS).IsValid(), "the seed packet prefab loads");
		CheckSlotsOnBed(ctx, plot);

		LF_FarmPlotRow before = plot.Export();
		plot.Import(null);

		IEntity farmer = LF_TestCharacterFixture.Spawn(ctx);
		if (!farmer)
		{
			plot.Import(before);
			return;
		}

		farmer.SetOrigin(plot.GetOwner().GetOrigin() + "1.5 0 0");

		ctx.True(plot.IsEmpty(), "the plot starts empty");
		ctx.Equal(LF_EFarmPlot.NO_SEEDS, plot.ServerPlant(farmer), "no seed packet, nothing is planted");
		ctx.True(plot.IsEmpty(), "and the plot stays empty");

		ctx.Equal(2, LF_InventoryUtils.AddAmount(farmer, SEEDS, 2), "the farmer carries two seed packets");
		ctx.Equal(LF_EFarmPlot.OK, plot.ServerPlant(farmer), "a seed packet plants the plot");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(farmer, SEEDS), "one packet is used up");
		ctx.Equal(0, plot.GetStage(), "the crop starts as sprouts");
		ctx.EqualStr("#LF-Farm_Stage_Sprout", plot.GetStageName(), "the stage is named sprouts");
		ctx.Equal(LF_EFarmPlot.NOT_EMPTY, plot.ServerPlant(farmer), "a planted plot takes no second packet");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(farmer, SEEDS), "and the packet is kept");
		ctx.Equal(LF_EFarmPlot.NOT_RIPE, plot.ServerHarvest(farmer), "sprouts cannot be harvested");

		float toRipe = LF_FarmPlotRules.SecondsToLastStage(plot.GetStages());
		ctx.True(toRipe > 0, "the crop needs time to ripen");
		plot.ServerAdvance(toRipe / 2);
		ctx.Equal(1, plot.GetStage(), "half way it is half grown");
		ctx.False(plot.IsRipe(), "half grown is not ripe");
		ctx.Equal(LF_EFarmPlot.NOT_RIPE, plot.ServerHarvest(farmer), "half grown cannot be harvested");

		plot.ServerAdvance(toRipe / 2);
		ctx.Equal(2, plot.GetStage(), "after the growing time it is ripe");
		ctx.True(plot.IsRipe(), "the plot reads ripe");

		farmer.SetOrigin(plot.GetOwner().GetOrigin() + "30 0 0");
		ctx.Equal(LF_EFarmPlot.TOO_FAR, plot.ServerHarvest(farmer), "a farmer across the field is refused");
		ctx.True(plot.IsRipe(), "and the crop stays");

		farmer.SetOrigin(plot.GetOwner().GetOrigin() + "1.5 0 0");
		ctx.Equal(LF_EFarmPlot.OK, plot.ServerHarvest(farmer), "the ripe crop is harvested");
		ctx.Equal(3, HarvestCount(farmer), "the harvest is three cabbages, carried or at the farmer's feet");
		ctx.True(plot.IsEmpty(), "the plot is empty again");
		ctx.Equal(LF_EFarmPlot.NOT_RIPE, plot.ServerHarvest(farmer), "an empty plot gives nothing");
		ctx.Equal(3, HarvestCount(farmer), "still three cabbages");
		ctx.Equal(LF_EFarmPlot.OK, plot.ServerPlant(farmer), "the second packet plants it again");

		LF_TestCharacterFixture.Delete(farmer);
		foreach (EntityID looseId : s_aLoose)
			LF_TestCharacterFixture.Delete(GetGame().GetWorld().FindEntityByID(looseId));
		s_aLoose.Clear();
		plot.Import(before);
	}

	//------------------------------------------------------------------------------------------------
	//! Cabbages the farmer carries plus loose ones dropped at their feet (a full jacket drops the rest).
	protected int HarvestCount(IEntity farmer)
	{
		s_aLoose.Clear();
		GetGame().GetWorld().QueryEntitiesBySphere(farmer.GetOrigin(), 3, CollectLooseCabbage, null, EQueryEntitiesFlags.ALL);
		return LF_InventoryUtils.GetAmount(farmer, CABBAGE) + s_aLoose.Count();
	}

	protected static ref array<EntityID> s_aLoose = {};

	//------------------------------------------------------------------------------------------------
	protected static bool CollectLooseCabbage(IEntity entity)
	{
		if (entity && LF_Utils.GetPrefabName(entity) == CABBAGE && LF_InventoryUtils.GetStorageHierachyRoot(entity) == entity)
			s_aLoose.Insert(entity.GetID());

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Every crop slot stands on the bed mesh (inside its footprint, above its floor).
	protected void CheckSlotsOnBed(LF_TestContext ctx, LF_FarmPlotComponent plot)
	{
		vector mins, maxs;
		plot.GetOwner().GetBounds(mins, maxs);
		LF_Debug.Info(LF_FarmPlots.FEATURE, string.Format("plot %1 bed bounds %2 .. %3", plot.GetPlotKey(), mins, maxs));

		array<vector> slots = new array<vector>();
		plot.GetSlots(slots);
		ctx.True(slots.Count() >= 4, "the bed has at least four crop slots");
		foreach (vector slot : slots)
		{
			bool inside = slot[0] >= mins[0] && slot[0] <= maxs[0] && slot[2] >= mins[2] && slot[2] <= maxs[2];
			ctx.True(inside, string.Format("crop slot %1 lies on the bed (%2 .. %3)", slot, mins, maxs));
		}
	}
}
