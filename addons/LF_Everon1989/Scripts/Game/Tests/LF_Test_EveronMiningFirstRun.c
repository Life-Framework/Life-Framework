// red-proof: 2026-11-23 `node tools/cli.mjs test --no-build --red LF_Test_TillPaidSlip,LF_Test_EveronMiningFirstRun`: RED-OK everon/mining-first-run. RedProof() puts back the old run (3 ore, no bonus): "1 bars at $20 + $0 bonus - $150 pickaxe = $-130".
//! everon/mining-first-run (first-2h re-review N3): a new player who does the F2 mining run start to finish ends clearly
//! ahead. The run buys the pickaxe at the miners' supply till, mines the ore the ore step asks for, smelts it in the mine
//! furnace and sells the bars at the iron counter (the coalmans_brow_mine demand row), and the sell step pays the
//! first-run bonus. Bars times the bar price plus the bonus, less the pickaxe, is at least $100; the ore step asks for
//! whole bars that fit one furnace run and the buyer's purse; the F2 rate sits in the civilian entry band ($400 to $480 an
//! hour; `tools\cli economy` puts the starter loop with the dock trip at $447).
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronMiningFirstRun : LF_Test
{
	protected static const string ACTIVITY = "mining";
	protected static const string OUTLET = "miners_supply";
	protected static const string MACHINE = "furnace-iron-mine";
	protected static const string BUYER_ID = "coalmans_brow_mine";
	protected static const ResourceName PICKAXE = "{D9D9C619E82ECEA7}Prefabs/Tools/Tiers/LF_Tool_Pickaxe_Worn.et";
	protected static const ResourceName NUGGET = "{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et";
	protected static const ResourceName BAR = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";
	protected static const int MIN_NET = 100;
	protected static const int ENTRY_LOW = 400;
	protected static const int ENTRY_HIGH = 480;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/mining-first-run";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ActivityTable.ResetStatics();
		LF_ItemCatalogueConfig.Reload();
		LF_ResourceChainCatalog.Reload();
		LF_BuyerDemandConfig.Reload();

		Check(ctx);

		LF_ActivityTable.ResetStatics();
		LF_ItemCatalogueConfig.Reload();
		LF_BuyerDemandConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected void Check(LF_TestContext ctx)
	{
		LF_ActivityConfig activities = LF_ActivityTable.Get();
		ctx.NotNull(activities, "the activity table loads under the pack header");
		if (!activities)
			return;

		LF_ActivityDef mining = activities.Find(ACTIVITY);
		ctx.NotNull(mining, "the mining run is listed");
		if (!mining)
			return;

		int ore = 0;
		int bonus = 0;
		foreach (LF_ActivityStep step : mining.m_aSteps)
		{
			if (step.m_sStepId == "ore" && LF_Utils.SamePrefab(step.m_sCheckParam, NUGGET))
				ore = step.m_iCheckCount;

			if (step.m_sStepId == "sell")
				bonus = step.m_iRewardMoney;
		}

		if (RedProof())
		{
			ore = 3;
			bonus = 0;
		}

		LF_ItemCatalogueConfig catalogue = LF_ItemCatalogueConfig.Get();
		LF_CatalogueItem pickaxe;
		LF_OutletOffer offer;
		if (catalogue)
			offer = LF_ItemCatalogue.OfferFor(catalogue, OUTLET, PICKAXE, pickaxe);

		ctx.NotNull(pickaxe, "the miners' supply till sells the pickaxe");
		ctx.NotNull(offer, "the pickaxe has an offer at the miners' supply till");
		if (!pickaxe)
			return;

		LF_ResourceMachineDef furnace = LF_ResourceChainCatalog.FindMachine(MACHINE);
		ctx.NotNull(furnace, "the mine furnace recipe loads");
		if (!furnace || !furnace.m_aInputs || furnace.m_aInputs.Count() != 1 || !furnace.m_aOutputs || furnace.m_aOutputs.Count() != 1)
		{
			ctx.True(false, "the mine furnace has one input and one output");
			return;
		}

		int orePerBatch = furnace.m_aInputs[0].m_iInputAmount;
		int barsPerBatch = furnace.m_aOutputs[0].m_iOutputAmount;
		ctx.True(orePerBatch > 0 && barsPerBatch > 0, "the furnace recipe takes and gives something");
		if (orePerBatch <= 0 || barsPerBatch <= 0)
			return;

		ctx.Equal(0, ore % orePerBatch, string.Format("the ore step (%1 ore) asks for whole bars (%2 ore a bar)", ore, orePerBatch));
		int batches = ore / orePerBatch;
		ctx.True(furnace.m_iMaxBatches <= 0 || batches <= furnace.m_iMaxBatches, string.Format("the run's ore (%1 batches) fits one furnace run (%2)", batches, furnace.m_iMaxBatches));
		int bars = batches * barsPerBatch;

		LF_BuyerDemandConfig demand = LF_BuyerDemandConfig.Get();
		LF_BuyerDemandRow row;
		if (demand)
			row = demand.Find(BUYER_ID, BAR);

		ctx.NotNull(row, "the iron counter's demand row buys the bar");
		if (!row)
			return;

		ctx.True(row.m_iCap >= bars, string.Format("the iron counter's purse (%1 bars) takes the whole first run (%2 bars)", row.m_iCap, bars));

		int earned = bars * row.m_iPrice + bonus;
		int net = earned - pickaxe.m_iPrice;
		ctx.True(net >= MIN_NET, string.Format("the first run ends at least $%1 ahead: %2 bars at $%3 + $%4 bonus - $%5 pickaxe = $%6", MIN_NET, bars, row.m_iPrice, bonus, pickaxe.m_iPrice, net));
		ctx.True(mining.m_iPayPerHour >= ENTRY_LOW && mining.m_iPayPerHour <= ENTRY_HIGH, string.Format("F2 shows $%1/h, inside the civilian entry band ($%2 to $%3)", mining.m_iPayPerHour, ENTRY_LOW, ENTRY_HIGH));
	}
}
