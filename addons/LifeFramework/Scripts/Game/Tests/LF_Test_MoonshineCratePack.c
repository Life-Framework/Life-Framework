// red-proof: 2026-11-17 with `tools\cli test --no-build --only LF_Test_MoonshineCratePack` before the pack step and the station existed: `the moonshine chain has a pack step`, `the pack bench of the grid cell is registered` failed, `SUMMARY tier=only passed=0 failed=1`; with them in, green.

// tier: WORLD
//! The last step of the moonshine chain (#2441, Refs #2029): at a packing bench a set number of moonshine
//! bottles become one `LF_Contraband` crate, so the barn-to-pub convoy can carry a crate the chain made.
//! The step is checked in the shipped config; the bench of the grid cell `slice-2441` is driven through the
//! server methods the held actions call: loading takes the bottles, work finishes the batch, collecting
//! gives the crate. The bottle and the crate both stay chain goods for an officer's search (#419).
class LF_Test_MoonshineCratePack : LF_Test
{
	protected static const string BENCH = "Grid_slice_2441_Bench";
	protected static const string SITE = "Grid_slice_2441_Site";

	override string GetName()
	{
		return "chains/moonshine-crate-pack";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ChainCatalog.ResetStatics();
		LF_ChainCatalog.Build(LF_ResourceChainCatalog.GetConfig());

		LF_ChainDef chain = LF_ChainCatalog.FindChain("moonshine");
		ctx.NotNull(chain, "the moonshine chain resolves");
		if (!chain)
			return;

		LF_ChainStepDef pack = chain.FindStep("pack");
		ctx.NotNull(pack, "the moonshine chain has a pack step");
		if (!pack)
		{
			LF_ChainCatalog.ResetStatics();
			return;
		}

		ctx.Equal(3, chain.m_aSteps.Count(), "mash, distill, then pack");
		ctx.EqualStr("pack", chain.m_aSteps[2].m_sStepId, "packing is the last step");
		ctx.Equal(1, pack.m_aInputs.Count(), "the pack step takes one input");
		ctx.EqualStr(LF_Test_AlcoholDrinks.MOONSHINE, pack.m_aInputs[0].m_InputPrefab, "the pack step takes the moonshine bottle");
		int bottles = pack.m_aInputs[0].m_iInputAmount;
		ctx.True(bottles > 1, "a crate takes more than one bottle");
		ctx.Equal(1, pack.m_aOutputs.Count(), "the pack step makes one output");
		ctx.EqualStr(LF_Contraband.ITEM, pack.m_aOutputs[0].m_OutputPrefab, "the pack step makes the contraband crate");
		ctx.Equal(1, pack.m_aOutputs[0].m_iOutputAmount, "one crate per batch");
		ctx.Equal(LF_EChainGate.OK, LF_ChainGate.Evaluate(pack, false, 0, false), "packing has no gate");

		int bulk;
		ctx.True(LF_ChainSearch.IsGood(LF_Test_AlcoholDrinks.MOONSHINE, bulk), "the bottle stays chain goods for a search");
		ctx.True(LF_ChainSearch.IsGood(LF_Contraband.ITEM, bulk), "the crate is chain goods too");
		ctx.False(LF_ChainSearch.IsGood("{A231A5F8D479B5DC}Prefabs/Items/Drinks/WaterBottle.et", bulk), "the mash water is not");

		LF_ChainStationComponent bench = LF_ChainStations.Find(BENCH);
		ctx.NotNull(bench, "the pack bench of the grid cell is registered");
		if (bench)
		{
			ctx.EqualStr("moonshine", bench.GetChainId(), "the bench runs the moonshine chain");
			ctx.EqualStr("pack", bench.GetStepId(), "the bench runs the pack step");
			RunBench(ctx, bench, bottles);
		}

		LF_ChainCatalog.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunBench(LF_TestContext ctx, LF_ChainStationComponent bench, int bottles)
	{
		IEntity worker = LF_TestCharacterFixture.Spawn(ctx);
		if (!worker)
			return;

		InventoryStorageManagerComponent bag = LF_InventoryUtils.GetResponsibleStorageManager(worker);
		ctx.NotNull(bag, "the worker has an inventory");
		if (!bag)
		{
			LF_TestCharacterFixture.Delete(worker);
			return;
		}

		worker.SetOrigin(bench.GetOwner().GetOrigin() + "0.8 0 0");

		// One bottle short: nothing is taken.
		LF_InventoryUtils.AddAmount(bag, LF_Test_AlcoholDrinks.MOONSHINE, bottles - 1);
		ctx.Equal(LF_EChainStation.MISSING_INPUTS, bench.ServerLoad(worker, 1000), "too few bottles cannot be packed");
		ctx.Equal(bottles - 1, LF_InventoryUtils.GetAmount(bag, LF_Test_AlcoholDrinks.MOONSHINE), "a refused load takes nothing");

		// A full set, plus one spare that must stay.
		LF_InventoryUtils.AddAmount(bag, LF_Test_AlcoholDrinks.MOONSHINE, 2);
		ctx.Equal(bottles + 1, LF_InventoryUtils.GetAmount(bag, LF_Test_AlcoholDrinks.MOONSHINE), "the worker carries a set and a spare");
		int crates = LF_InventoryUtils.GetAmount(bag, LF_Contraband.ITEM);

		ctx.Equal(LF_EChainStation.OK, bench.ServerLoad(worker, 1000), "the worker loads the bench");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(bag, LF_Test_AlcoholDrinks.MOONSHINE), "the bottles are used up, the spare stays");
		ctx.Equal(crates, LF_InventoryUtils.GetAmount(bag, LF_Contraband.ITEM), "no crate before the work is done");

		float now = 1000;
		int guard = 0;
		while (bench.GetPercent() != LF_StationProgress.READY && guard < 200)
		{
			now += 2;
			guard++;
			bench.ServerWork(worker, now);
		}

		ctx.Equal(LF_StationProgress.READY, bench.GetPercent(), "the work finishes the batch");
		ctx.Equal(LF_EChainStation.OK, bench.ServerCollect(worker, now + 3), "the worker collects the crate");
		ctx.Equal(crates + 1, LF_InventoryUtils.GetAmount(bag, LF_Contraband.ITEM), "the worker holds the crate");
		ctx.Equal(LF_StationProgress.EMPTY, bench.GetPercent(), "a collected bench is empty again");

		bench.Import(null);
		LF_ChainSites.ClearHeat(SITE);
		LF_TestCharacterFixture.Delete(worker);
	}
}
