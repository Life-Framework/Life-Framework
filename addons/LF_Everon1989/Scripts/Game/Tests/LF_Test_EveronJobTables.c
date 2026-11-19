// red-proof: 2026-11-18 `tools\cli test --no-build --red LF_Test_EveronJobTables`: RED PROOF OK (RedProof() reads no gates, looks for the pickaxe in the stock and for an outlet no pack names). Green with the overlays in the pack leg.
//! everon/job-tables (#2710, slice of #1018): under the Everon 1989 header (the pack leg of `tools\cli test`),
//! every pack job has a level table with one unlock per level (a track whose thresholds make levels 2..N, and
//! exactly one licence gate on each of those levels), the hardware store stock has the shovel from a base-game
//! item, and the pickaxe is sold at the miners' supply outlet only to a MINER past the first level.
//! Medic ranks wait on a pack overlay for `LF_MedicConfig` (the core has none yet), so they are not read here.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronJobTables : LF_Test
{
	protected static const string OUTLET = "miners_supply";
	protected static const string STOCK = "hardware";
	protected static const ResourceName SHOVEL = "{0619993AE6CDBF85}Prefabs/Items/Equipment/Accessories/ETool_ALICE/ETool_ALICE.et";
	protected static const ResourceName PICKAXE = "{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et";

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

			ctx.Equal(topLevel - 1, unlockedAt.Count(), string.Format("%1 has one unlock per level above the first (levels 2 to %2)", job, topLevel));
			for (int level = 2; level <= topLevel; level++)
			{
				int onThisLevel = 0;
				foreach (int at : unlockedAt)
				{
					if (at == level)
						onThisLevel++;
				}

				ctx.Equal(1, onThisLevel, string.Format("%1 level %2 unlocks exactly one thing", job, level));
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
		foreach (LF_OutletOffer offer : offers)
		{
			LF_CatalogueItem item = LF_ItemCatalogue.Item(catalogue, offer.m_sItem);
			if (item && item.m_Prefab == PICKAXE)
				pickaxeOffer = offer;
		}

		ctx.NotNull(pickaxeOffer, "the miners' supply outlet sells the pickaxe");
		if (!pickaxeOffer)
			return;

		ctx.EqualStr("MINER", pickaxeOffer.m_sJob, "the pickaxe row is gated on the MINER job");
		ctx.True(pickaxeOffer.m_iMinRank >= 2, "the pickaxe is sold only past the first miner level");
	}
}
