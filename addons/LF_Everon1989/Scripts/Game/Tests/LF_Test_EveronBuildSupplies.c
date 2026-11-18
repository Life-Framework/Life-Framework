// red-proof: observed 2026-11-18 (#3171): on main (no gravel, cement, brick, plank, axe or seed rows in the general store stock) `tools\cli test --no-build --only LF_Test_EveronBuildSupplies` failed on every `is in the general store stock` line.
//! #3171: the Everon 1989 general store sells what a Saint-Philippe plot owner needs: the cottage's
//! building materials (`LF_Property.conf`: gravel, cement, planks, bricks), an axe for lumber and cabbage
//! seeds for the farm. Checked against the merged pack config (pack leg): each prefab loads, has a stock row
//! and a visible buy price, is never sold back for more than it costs, is never bought by the trader for
//! more than the store sells it (#3121), and the priced materials of the whole cottage cost $800 to $1400.
//! The door is not a shop item, so it is not in the total.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronBuildSupplies : LF_Test
{
	static const ResourceName GRAVEL = "{344090890C63F4CA}Prefabs/Items/Resources/GravelBag.et";
	static const ResourceName CEMENT = "{D58674633332DC86}Prefabs/Items/Resources/CementBag.et";
	static const ResourceName BRICK = "{0D5DA590E223A195}Prefabs/Items/Resources/Brick.et";
	static const ResourceName PLANK = "{45D1B36F99DD2FC3}Prefabs/Resources/WoodCutting/WoodPlank.et";
	static const ResourceName AXE = "{9CAC68BA551417A4}Prefabs/Tools/Axe/Axe.et";
	static const ResourceName SEEDS = "{F77BE55D8D3AEBA3}Prefabs/Items/Seeds/CabbageSeeds.et";
	static const ResourceName PACK_TRADER = "{3A9F6E21C47D8B71}Prefabs/Trader/LF_UniversalTrader_Everon1989.et";
	protected static const string STOCK = "general_store";
	protected static const int TOTAL_MIN = 800;
	protected static const int TOTAL_MAX = 1400;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/build-supplies";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ShopManagerComponent manager = LF_ShopManagerComponent.GetInstance();
		ctx.NotNull(manager, "the shop manager exists");
		LF_ShopStock stock = LF_ShopStockConfig.Get().Find(STOCK);
		ctx.NotNull(stock, "the general store has a stock under the pack header");
		if (!manager || !stock)
			return;

		array<ResourceName> goods = {GRAVEL, CEMENT, BRICK, PLANK, AXE, SEEDS};
		array<string> labels = {"gravel bag", "cement bag", "brick", "plank", "axe", "cabbage seeds"};
		foreach (int i, ResourceName prefab : goods)
		{
			CheckGood(ctx, manager, stock, labels[i], prefab);
		}

		CheckTraderLoop(ctx, manager, goods);
		CheckCottageTotal(ctx, manager);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckGood(LF_TestContext ctx, LF_ShopManagerComponent manager, LF_ShopStock stock, string label, ResourceName prefab)
	{
		Resource resource = Resource.Load(prefab);
		ctx.True(resource && resource.IsValid(), string.Format("the %1 prefab loads", label));

		ResourceName probe = prefab;
		if (RedProof())
			probe = "";

		LF_ShopStockItem row = stock.FindItem(probe);
		ctx.NotNull(row, string.Format("the %1 is in the general store stock", label));
		if (!row)
			return;

		int buyPrice = 0;
		int sellPrice = 0;
		bool hidden = true;
		manager.GetPrices(prefab, buyPrice, sellPrice, hidden);
		ctx.True(buyPrice > 0, string.Format("the %1 has a buy price (got %2)", label, buyPrice));
		ctx.False(hidden, string.Format("the %1 is not hidden from the shop", label));
		ctx.True(row.m_iPrice > 0, string.Format("the %1 has a stock price (got %2)", label, row.m_iPrice));
		ctx.True(buyPrice > sellPrice, string.Format("the %1 costs %2 and sells back for %3: no profit loop", label, buyPrice, sellPrice));
	}

	//------------------------------------------------------------------------------------------------
	//! #3121: no trader pays more for a good than the store charges for it.
	protected void CheckTraderLoop(LF_TestContext ctx, LF_ShopManagerComponent manager, array<ResourceName> goods)
	{
		float groundY = GetGame().GetWorld().GetSurfaceY(340, 100);
		IEntity traderEntity = LF_Utils.SpawnEntityPrefab(PACK_TRADER, Vector(340, groundY, 100));
		ctx.NotNull(traderEntity, "the Everon trader spawned");
		if (!traderEntity)
			return;

		LF_TraderManagerComponent trader = LF_Component<LF_TraderManagerComponent>.Find(traderEntity);
		ctx.NotNull(trader, "the Everon trader carries the trader manager");
		if (trader && trader.m_aTradableItems)
		{
			foreach (LF_TraderItem row : trader.m_aTradableItems)
			{
				if (!row)
					continue;

				foreach (ResourceName good : goods)
				{
					if (!LF_Utils.SamePrefab(row.m_ItemPrefab, good))
						continue;

					int buyPrice = 0;
					int sellPrice = 0;
					bool hidden = true;
					manager.GetPrices(good, buyPrice, sellPrice, hidden);
					ctx.True(buyPrice >= row.m_ValuePerItem, string.Format("%1: the store charges %2 and the trader pays %3", good, buyPrice, row.m_ValuePerItem));
				}
			}
		}

		SCR_EntityHelper.DeleteEntityAndChildren(traderEntity);
	}

	//------------------------------------------------------------------------------------------------
	//! The cottage's stage requirements (LF_Property.conf), priced at the shop, for every good the shop lists.
	protected void CheckCottageTotal(LF_TestContext ctx, LF_ShopManagerComponent manager)
	{
		LF_PropertyConfig config = LF_PropertyConfig.Get();
		ctx.NotNull(config, "the property config loads");
		if (!config || !config.m_aTiers)
			return;

		int total = 0;
		foreach (LF_PropertyTierDef tier : config.m_aTiers)
		{
			if (!tier || tier.m_sTierId != "cottage" || !tier.m_aStages)
				continue;

			foreach (LF_PropertyStageDef stage : tier.m_aStages)
			{
				if (!stage || !stage.m_aRequirements)
					continue;

				foreach (LF_PropertyStageReqDef req : stage.m_aRequirements)
				{
					if (!req || !manager.GetItem(req.m_Prefab))
						continue;

					int buyPrice = 0;
					int sellPrice = 0;
					bool hidden = true;
					manager.GetPrices(req.m_Prefab, buyPrice, sellPrice, hidden);
					total += buyPrice * req.m_iCount;
				}
			}
		}

		ctx.True(total >= TOTAL_MIN && total <= TOTAL_MAX, string.Format("the cottage's shop materials cost %1 (want %2 to %3)", total, TOTAL_MIN, TOTAL_MAX));
	}
}
