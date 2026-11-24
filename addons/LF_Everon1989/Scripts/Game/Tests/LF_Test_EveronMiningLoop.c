// red-proof: 2026-11-22 `tools\cli test --no-build --red LF_Test_EveronMiningLoop`: RED PROOF OK. RedProof() asks the gate as a buyer outside the miner job, skips the rock and furnace spawns, the sign lookup and the POI row, and reads the buyer rows without the pack layer, so every check below fails.
//! everon/mining-loop: under the Everon 1989 header (the pack leg), the one mining loop a new player runs on the island
//! (Coalman's Brow iron mine) holds end to end. A MINER at level 1 can buy the pickaxe at the miners' supply till (and a
//! player outside the job is sent to the desk first); the iron rock prefab is an ore node on the `iron` row, which wants
//! the pickaxe, files under MINING (MINER XP) and regrows; the pack mine furnace carries the `furnace-iron-mine` action, a
//! pack recipe with no level gate that turns 3 iron nuggets into 1 iron bar; the iron buyer at the mine takes the bar on the
//! `hardware` demand row and the harbour warehouse has a curve for it; the mine's sign and its map label resolve.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronMiningLoop : LF_Test
{
	protected static const string OUTLET = "miners_supply";
	protected static const string JOB = "MINER";
	protected static const ResourceName PICKAXE = "{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et";
	//! The till lists the worn rung as the entry pickaxe ($150, MINER level 1); a bare Pickaxe row beside it was a duplicate (#4680).
	protected static const ResourceName PICKAXE_WORN = "{D9D9C619E82ECEA7}Prefabs/Tools/Tiers/LF_Tool_Pickaxe_Worn.et";
	protected static const ResourceName IRON_ROCK = "{2C571D6F7C05E095}Prefabs/Resources/Mining/IronOre.et";
	protected static const ResourceName FURNACE = "{CF27D083C01F1C36}Prefabs/Processor/LF_MineFurnace.et";
	protected static const ResourceName NUGGET = "{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et";
	protected static const ResourceName BAR = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";
	protected static const ResourceName IRON_BUYER = "{CDE1C29873CECBC0}Prefabs/Trader/LF_IronBuyer_Everon1989.et";
	protected static const string BUYER_ID = "coalmans_brow_mine";
	protected static const string NODE = "iron";
	protected static const string MACHINE = "furnace-iron-mine";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/mining-loop";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ItemCatalogueConfig.Reload();
		LF_JobsConfig.Reload();
		LF_ResourceChainCatalog.Reload();

		CheckPickaxe(ctx);
		CheckRock(ctx);
		CheckFurnace(ctx);
		CheckBuyer(ctx);
		CheckSignAndMap(ctx);

		LF_ItemCatalogueConfig.Reload();
		LF_BuyerDemandConfig.Reload();
		LF_MarketConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	//! Step 1: the desk gives the job, the till sells the pickaxe to a level 1 miner.
	protected void CheckPickaxe(LF_TestContext ctx)
	{
		LF_ItemCatalogueConfig catalogue = LF_ItemCatalogueConfig.Get();
		ctx.NotNull(catalogue, "the item catalogue loads under the pack header");
		if (!catalogue)
			return;

		LF_CatalogueItem item;
		LF_OutletOffer offer = LF_ItemCatalogue.OfferFor(catalogue, OUTLET, PICKAXE_WORN, item);
		ctx.NotNull(offer, "the miners' supply till sells the pickaxe");
		if (!offer)
			return;

		LF_Test_CatalogueBuyer newMiner = new LF_Test_CatalogueBuyer();
		if (!RedProof())
			newMiner.m_mJobLevels.Insert(JOB, 1);

		string missing;
		ctx.Equal(LF_EOutletGate.OPEN, LF_ItemCatalogue.Check(offer, newMiner, missing), "a new miner (level 1) can buy the pickaxe: " + missing);

		LF_Test_CatalogueBuyer stranger = new LF_Test_CatalogueBuyer();
		ctx.Equal(LF_EOutletGate.NEEDS_JOB, LF_ItemCatalogue.Check(offer, stranger, missing), "a player outside the miner job is sent to the desk first");
		if (item)
			ctx.True(item.m_iPrice > 0 && item.m_iPrice <= 1000, string.Format("the pickaxe ($%1) fits a new player's starting bank", item.m_iPrice));
	}

	//------------------------------------------------------------------------------------------------
	//! Step 2: the rock is an iron ore node that wants the pickaxe, pays MINER XP and grows back.
	protected void CheckRock(LF_TestContext ctx)
	{
		IEntity rock;
		if (!RedProof())
			rock = LF_Utils.SpawnEntityPrefab(IRON_ROCK, Vector(0, 500, 0));

		ctx.NotNull(rock, "the iron rock prefab spawns");
		if (rock)
		{
			LF_OreNodeComponent ore = LF_OreNodeComponent.Cast(rock.FindComponent(LF_OreNodeComponent));
			ctx.NotNull(ore, "the iron rock is an ore node");
			if (ore)
				ctx.EqualStr(NODE, ore.GetNodeId(), "the iron rock mines the iron row");

			SCR_EntityHelper.DeleteEntityAndChildren(rock);
		}

		LF_ResourceNodeDef row = LF_ResourceChainCatalog.FindNode(NODE);
		ctx.NotNull(row, "the iron node row exists");
		if (!row)
			return;

		ctx.True(LF_ResourceChainCatalog.IsNodeUsable(row), "the iron node row is usable");
		ctx.True(LF_Utils.SamePrefab(row.m_ItemPrefab, NUGGET), "the iron rock drops iron nuggets");
		ctx.True(LF_GatherTools.Accepts(row, PICKAXE), "the iron rock takes the pickaxe");
		ctx.True(row.m_iMaxGathers > 0, "the iron rock is spent after a few pieces");
		ctx.True(row.m_fCooldownMs > 0, "a spent iron rock grows back");

		string action = LF_JobLevelSources.ActionForFamily(LF_GatherTools.JobFamily(row));
		LF_JobsConfig jobs = LF_JobsConfig.Get();
		LF_JobXpAwardDef award;
		if (jobs)
			award = jobs.FindAward(action);

		ctx.NotNull(award, "a piece of iron ore pays job XP (" + action + ")");
		if (award)
		{
			ctx.EqualStr(JOB, award.m_sJob, "the iron XP goes to the miner job");
			ctx.True(award.m_iXp > 0, "the iron XP is more than nothing");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Step 3: the mine furnace smelts 3 nuggets into 1 bar for any miner (the level 4 smelter does 2 with coal).
	protected void CheckFurnace(LF_TestContext ctx)
	{
		LF_ResourceMachineDef machine = LF_ResourceChainCatalog.FindMachine(MACHINE);
		ctx.NotNull(machine, "the furnace-iron-mine machine row (the pack's) exists");
		if (machine)
		{
			ctx.True(LF_ResourceChainCatalog.IsMachineUsable(machine), "the furnace-iron-mine recipe is usable");
			ctx.True(machine.m_sLevelJob.IsEmpty() || machine.m_iMinLevel <= 1, "a level 1 miner may run the furnace");
			ctx.True(machine.m_aInputs && machine.m_aInputs.Count() == 1 && LF_Utils.SamePrefab(machine.m_aInputs[0].m_InputPrefab, NUGGET) && machine.m_aInputs[0].m_iInputAmount == 3, "the mine furnace takes 3 iron nuggets a bar and no coal");
			ctx.True(machine.m_aOutputs && machine.m_aOutputs.Count() == 1 && LF_Utils.SamePrefab(machine.m_aOutputs[0].m_OutputPrefab, BAR), "the furnace makes the iron bar");
		}

		IEntity furnace;
		if (!RedProof())
			furnace = LF_Utils.SpawnEntityPrefab(FURNACE, Vector(4, 500, 0));

		ctx.NotNull(furnace, "the mine furnace prefab spawns");
		if (!furnace)
			return;

		bool found;
		ActionsManagerComponent actions = ActionsManagerComponent.Cast(furnace.FindComponent(ActionsManagerComponent));
		if (actions)
		{
			array<BaseUserAction> list = {};
			actions.GetActionsList(list);
			foreach (BaseUserAction base : list)
			{
				LF_ProcessAction process = LF_ProcessAction.Cast(base);
				if (process && process.m_sMachineId == MACHINE)
					found = true;
			}
		}

		ctx.True(found, "the mine furnace carries the furnace-iron-mine action");
		SCR_EntityHelper.DeleteEntityAndChildren(furnace);
	}

	//------------------------------------------------------------------------------------------------
	//! Step 4: somebody pays for the bar.
	protected void CheckBuyer(LF_TestContext ctx)
	{
		if (RedProof())
		{
			LF_MissionConfigPackSource none = new LF_MissionConfigPackSource();
			none.SetHeader(new LF_MissionHeader());
			LF_ConfigService.SetPackSource(none);
		}

		LF_BuyerDemandConfig.Reload();
		LF_MarketConfig.Reload();
		LF_BuyerDemandConfig demand = LF_BuyerDemandConfig.Get();
		LF_MarketConfig market = LF_MarketConfig.Get();
		if (RedProof())
			LF_ConfigService.Reset();

		ctx.NotNull(demand, "the buyer demand config loads under the pack header");
		ctx.NotNull(market, "the market config loads under the pack header");
		if (!demand || !market)
			return;

		LF_BuyerDemandRow row = demand.Find(BUYER_ID, BAR);
		ctx.NotNull(row, "the iron buyer's demand row (" + BUYER_ID + ") buys the iron bar");
		if (row)
		{
			ctx.True(row.m_iPrice > 0 && row.m_iCap > 0 && row.m_iRefillPerHour > 0, string.Format("the iron buyer pays $%1 a bar for %2 bars, %3 more an hour", row.m_iPrice, row.m_iCap, row.m_iRefillPerHour));
			ctx.True(row.Problem().IsEmpty(), "the iron buyer's row is usable: " + row.Problem());
		}

		LF_MarketGoodDef curve = market.FindGood(BAR);
		ctx.NotNull(curve, "the harbour warehouse has a wholesale curve for the iron bar");
		if (curve)
			ctx.True(curve.m_iFloor > 0, "the wholesale iron bar price never falls to nothing");

		// The counter at the mine: a trader that takes the bar and pays that row.
		IEntity buyer;
		if (!RedProof())
			buyer = LF_Utils.SpawnEntityPrefab(IRON_BUYER, Vector(8, 500, 0));

		ctx.NotNull(buyer, "the mine's iron buyer prefab spawns");
		if (!buyer)
			return;

		LF_TraderManagerComponent trader = LF_TraderManagerComponent.Cast(buyer.FindComponent(LF_TraderManagerComponent));
		ctx.NotNull(trader, "the iron buyer is a trader");
		if (trader)
		{
			ctx.EqualStr(BUYER_ID, trader.m_sBuyerId, "the iron buyer pays the " + BUYER_ID + " demand row");
			ctx.True(LF_TraderManagerComponent.GetUnitValue(trader.m_aTradableItems, BAR) > 0, "the iron buyer lists the iron bar");
			ctx.Equal(0, LF_TraderManagerComponent.GetUnitValue(trader.m_aTradableItems, NUGGET), "the iron buyer does not take raw ore (it must be smelted)");
		}

		SCR_EntityHelper.DeleteEntityAndChildren(buyer);
	}

	//------------------------------------------------------------------------------------------------
	//! The mine is findable: its sign reads, its map label names it.
	protected void CheckSignAndMap(LF_TestContext ctx)
	{
		LF_SignDef sign;
		if (!RedProof())
			sign = LF_SignCatalog.Find("coalmans-brow-mine");

		ctx.NotNull(sign, "the mine plate is in the sign catalog");
		if (sign)
		{
			ctx.True(LF_SignCatalog.ResolveTitle(sign.m_sTitle) != "", "the mine plate has a title to print");
			ctx.True(LF_SignCatalog.Resolve(sign.m_sHow) != sign.m_sHow, "the mine plate's how-to resolves to the pack's string");
		}

		LF_MapPoiConfig.Reload();
		LF_MapPoiConfig config = LF_MapPoiConfig.Get();
		ctx.NotNull(config, "the POI table loads under the pack");
		if (!config)
			return;

		LF_MapPoiKind slotRow;
		if (!RedProof())
			slotRow = config.FindSlotRow("Workplace", "coalmans-brow.mine");

		ctx.NotNull(slotRow, "the mine's workplace has its own map row");
		if (slotRow)
		{
			ctx.EqualStr("poi_Mine", slotRow.m_sIcon, "the mine draws the mine pictogram");
			string caption = WidgetManager.Translate(slotRow.m_sLabel);
			ctx.True(caption != slotRow.m_sLabel && !caption.IsEmpty(), "the mine's map name is in the string table");
		}

		LF_MapPoiKind work = config.Find("Workplace");
		ctx.True(work && work.m_eVisibility == LF_EMapPoiVisibility.PUBLIC, "workplaces show on every player's map");
	}
}

