// red-proof: 2026-11-27 (#5074, no copper on the island): the main guard's suite failed `everon/job-tables` on "MINER level 3 unlocks exactly one thing" once the copper gate left the pack config; with the miner level 3 row expecting 0 it passes. 2026-11-18`tools\cli test --no-build --red LF_Test_EveronJobTables`: RED PROOF OK (RedProof() reads no gates, looks for the pickaxe in the stock and for an outlet no pack names). Green with the overlays in the pack leg. 2026-11-22 (pickaxe from level 1): with m_iMinRank 2 back, `--only LF_Test_EveronJobTables` FAILED "a new miner (level 1) can buy the pickaxe"; with 1 it passes. 2026-11-24 (#4680, duplicate pickaxe row): with the legacy `tool_pickaxe` offer back, `--only LF_Test_EveronJobTables` FAILED "the miners' till lists one $150 pickaxe row, not a duplicate"; with the offer removed it passes.
//! everon/job-tables (#2710, slice of #1018): under the Everon 1989 header (the pack leg of `tools\cli test`),
//! every pack job has a level table with one unlock per level (a track whose thresholds make levels 2..N, and
//! exactly one licence gate on each of those levels), the hardware store stock has the shovel from a base-game
//! item, and the pickaxe is sold at the miners' supply outlet to any MINER from the first level (a new miner has no
//! other way to earn MINER XP: only pickaxe gathers pay it).
//! Medic ranks wait on a pack overlay for `LF_MedicConfig` (the core has none yet), so they are not read here.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronJobTables : LF_Test
{
	protected static const string OUTLET = "miners_supply";
	protected static const string STOCK = "hardware";
	protected static const ResourceName SHOVEL = "{0619993AE6CDBF85}Prefabs/Items/Equipment/Accessories/ETool_ALICE/ETool_ALICE.et";
	protected static const ResourceName PICKAXE = "{D9D9C619E82ECEA7}Prefabs/Tools/Tiers/LF_Tool_Pickaxe_Worn.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/job-tables";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The leg boots under the pack header; layer the configs again so the pack overlays apply.
		LF_JobsConfig.Reload();
		LF_ItemCatalogueConfig.Reload();

		CheckJobTables(ctx);
		CheckShovel(ctx);
		CheckPickaxe(ctx);

		LF_JobsConfig.Reload();
		LF_ItemCatalogueConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckJobTables(LF_TestContext ctx)
	{
		LF_JobsConfig config = LF_JobsConfig.Get();
		ctx.NotNull(config, "the jobs config loads under the pack header");
		if (!config)
			return;

		array<string> jobs = {"FARMER", "MINER", "LUMBERJACK", "MEDIC"};
		foreach (string job : jobs)
		{
			LF_JobTrackDef track = config.FindTrack(job);
			ctx.NotNull(track, string.Format("%1 has a level table", job));
			if (!track || !track.m_aLevelThresholds)
				continue;

			int topLevel = track.m_aLevelThresholds.Count() + 1;
			array<int> unlockedAt = {};
			if (!RedProof())
			{
				foreach (LF_JobLicenceGateDef gate : config.m_aLicenceGates)
				{
					if (gate && gate.m_sJob == job)
						unlockedAt.Insert(gate.m_iMinLevel);
				}
			}

			// The tables run to level 30 (#4181); the unlock rows for levels 6 to 30 are W4's. Until they land the
			// gates cover the first levels: at most one per level, none past the table, and levels 2 to 4 (every
			// pack job has them) keep their one unlock each.
			int gatedTop = 4;
			if (topLevel < gatedTop)
				gatedTop = topLevel;

			for (int level = 2; level <= topLevel; level++)
			{
				int onThisLevel = 0;
				foreach (int at : unlockedAt)
				{
					if (at == level)
						onThisLevel++;
				}

				// The island has coal and iron only (#5074): the MINER_COPPER (level 3) and MINER_GOLD (level 6)
				// licences stay defined in the core but are not gated here, so miner level 3 unlocks nothing.
				if (job == "MINER" && level == 3)
					ctx.Equal(0, onThisLevel, "MINER level 3 unlocks nothing (no copper on the island)");
				else if (level <= gatedTop)
					ctx.Equal(1, onThisLevel, string.Format("%1 level %2 unlocks exactly one thing", job, level));
				else
					ctx.True(onThisLevel <= 1, string.Format("%1 level %2 unlocks at most one thing", job, level));
			}

			foreach (int gateLevel : unlockedAt)
			{
				ctx.True(gateLevel >= 2 && gateLevel <= topLevel, string.Format("%1 has a gate on level %2, inside its table", job, gateLevel));
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckShovel(LF_TestContext ctx)
	{
		LF_ShopStock stock = LF_ShopStockConfig.Get().Find(STOCK);
		ctx.NotNull(stock, "the hardware store has a stock under the pack header");
		if (!stock)
			return;

		ResourceName shovel = SHOVEL;
		if (RedProof())
			shovel = PICKAXE;

		LF_ShopStockItem row = stock.FindItem(shovel);
		ctx.NotNull(row, "the shovel (the vanilla entrenching tool) is in the hardware store stock (#3220)");
		if (row)
			ctx.True(row.m_iPrice > 0, "the shovel has a price");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckPickaxe(LF_TestContext ctx)
	{
		LF_ItemCatalogueConfig catalogue = LF_ItemCatalogueConfig.Get();
		ctx.NotNull(catalogue, "the item catalogue loads under the pack header");
		if (!catalogue)
			return;

		string outlet = OUTLET;
		if (RedProof())
			outlet = "clothing_rack";

		array<LF_OutletOffer> offers = {};
		LF_ItemCatalogue.Offers(catalogue, outlet, offers);

		LF_OutletOffer pickaxeOffer;
		int entryRows;
		foreach (LF_OutletOffer offer : offers)
		{
			LF_CatalogueItem item = LF_ItemCatalogue.Item(catalogue, offer.m_sItem);
			if (item && item.m_Prefab == PICKAXE)
				pickaxeOffer = offer;

			// One pickaxe row per price: the till once listed the plain Pickaxe and the worn one, both $150 (#4680).
			if (item && item.m_sId.StartsWith("tool_pickaxe") && item.m_iPrice == 150)
				entryRows++;
		}
		if (RedProof())
			entryRows = 2;
		if (outlet == OUTLET)
			ctx.Equal(1, entryRows, "the miners' till lists one $150 pickaxe row, not a duplicate");

		ctx.NotNull(pickaxeOffer, "the miners' supply outlet sells the pickaxe");
		if (!pickaxeOffer)
			return;

		ctx.EqualStr("MINER", pickaxeOffer.m_sJob, "the pickaxe row is gated on the MINER job");
		ctx.True(pickaxeOffer.m_iMinRank <= 1, "a new miner (level 1) can buy the pickaxe");
	}
}
