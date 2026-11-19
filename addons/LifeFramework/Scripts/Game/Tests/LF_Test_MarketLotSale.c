// red-proof: observed 2026-11-19 `tools/cli test --no-build --red LF_Test_MarketLotSale,<two more>`: RED PROOF OK on both legs (every listed test fails with the hook on); RedProof() skips the sales, so the paid and left-over lines fail. Green with `--only`: passed.
//! A price per lot through the real sale path (#3622): a trader that buys a curved good priced per 3 units takes
//! whole lots of the stack (7 apples sell 6, the 7th stays), pays per lot, and refuses a stack below one lot with
//! BELOW_LOT and no cash moved; a premium buyer row with a lot does the same (4 apples sell 3). The sale runs on
//! the server through LF_TraderSale.Sell, the path the client request takes.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_MarketLotSale : LF_Test
{
	protected static const ResourceName CHARACTER_PREFAB = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";
	protected static const ResourceName JACKET_PREFAB = "{9F546CCA2582D16F}Prefabs/Characters/Uniforms/Jacket_M88.et";
	protected static const ResourceName PLANK = "{45D1B36F99DD2FC3}Prefabs/Resources/WoodCutting/WoodPlank.et";
	protected static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	protected static const string BUYER = "test-timber";
	protected static const int HOUR = 3600;

	override string GetName()
	{
		return "market/lot-sale";
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnSeller(LF_TestContext ctx, vector position)
	{
		Resource res = Resource.Load(CHARACTER_PREFAB);
		ctx.True(res.IsValid(), "character prefab loads");
		if (!res.IsValid())
			return null;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(params.Transform);
		params.Transform[3] = position;

		IEntity character = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.NotNull(character, "seller character spawns");
		if (!character)
			return null;

		InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(character);
		IEntity jacket = LF_Utils.SpawnEntityPrefab(JACKET_PREFAB, character.GetOrigin());
		ctx.NotNull(storage, "seller has an inventory manager");
		ctx.NotNull(jacket, "jacket spawns");
		if (!storage || !jacket)
			return null;

		ctx.True(storage.TryInsertItem(jacket, EStoragePurpose.PURPOSE_LOADOUT_PROXY), "jacket goes on the seller, giving it pockets");
		ctx.Equal(100, LF_MoneyUtils.AddCash(character, 100), "seller starts with 100 cash");
		return character;
	}

	//------------------------------------------------------------------------------------------------
	//! Sells the first stack of `prefab` the seller carries to the trader, as a client request would.
	//! Under the red-proof hook the sale is skipped.
	protected LF_ETraderSale SellFirst(LF_TestContext ctx, IEntity seller, ResourceName prefab, IEntity traderOwner)
	{
		InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(seller);
		array<IEntity> items = LF_InventoryUtils.FindItemsByPrefab(storage, prefab);
		ctx.True(items.Count() > 0, "the seller carries " + prefab);
		if (items.Count() == 0 || RedProof())
			return LF_ETraderSale.NOT_SERVER;

		return LF_TraderSale.Sell(seller, items[0], traderOwner, true);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity traderOwner = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.APPLE_TRADE_CONTAINER);
		ctx.NotNull(traderOwner, "Annex_AppleTradeContainer resolves in the DebugWorld");
		if (!traderOwner)
			return;

		LF_TraderManagerComponent trader = LF_Component<LF_TraderManagerComponent>.Find(traderOwner);
		ctx.NotNull(trader, "the container carries the trader manager");
		if (!trader)
			return;

		string wasBuyer = trader.m_sBuyerId;
		IEntity seller = SpawnSeller(ctx, traderOwner.GetOrigin() + Vector(1.5, 0.1, 0));
		if (seller)
		{
			RunCurve(ctx, seller, traderOwner, trader);
			RunBuyer(ctx, seller, traderOwner, trader);
			SCR_EntityHelper.DeleteEntityAndChildren(seller);
		}

		trader.m_sBuyerId = wasBuyer;
		LF_MarketConfig.Reload();
		LF_MarketBook.ResetStatics();
		LF_BuyerDemandConfig.Reload();
		LF_BuyerDemandBook.Restore(BUYER, APPLE, null);
	}

	//------------------------------------------------------------------------------------------------
	//! A curved apple priced $4 a lot of 3, no drop: 7 apples sell 6 for 8 and the 7th stays; one apple is BELOW_LOT.
	protected void RunCurve(LF_TestContext ctx, IEntity seller, IEntity traderOwner, LF_TraderManagerComponent trader)
	{
		LF_MarketConfig market = new LF_MarketConfig();
		market.m_bEnabled = true;
		market.m_iMaxRecoveryHours = 24;
		market.m_aGoods = {};
		market.m_aGoods.Insert(LF_MarketGoodDef.Make(APPLE, 4, 4, 4, 0, 0, 3));
		LF_MarketConfig.UseForTest(market);
		LF_MarketBook.ResetStatics();
		trader.m_sBuyerId = "";

		ctx.Equal(7, LF_InventoryUtils.AddAmount(seller, APPLE, 7), "the seller gets 7 apples");
		if (ctx.FailureCount() > 0)
			return;

		int before = LF_MoneyUtils.GetCash(seller);
		ctx.True(SellFirst(ctx, seller, APPLE, traderOwner) == LF_ETraderSale.SOLD, "7 apples at a lot of 3 are a sale");
		ctx.Equal(before + 8, LF_MoneyUtils.GetCash(seller), "two lots at $4 pay 8");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(seller, APPLE), "the part lot, one apple, stays with the seller");

		ctx.True(SellFirst(ctx, seller, APPLE, traderOwner) == LF_ETraderSale.BELOW_LOT || RedProof(), "one apple is below one lot of 3");
		ctx.Equal(before + 8, LF_MoneyUtils.GetCash(seller), "a stack below a lot pays nothing");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(seller, APPLE), "and keeps the apple");
		LF_InventoryUtils.RemoveAmount(seller, APPLE, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! A premium buyer row with a lot of 3 ($2 a lot, a purse of 9): 4 apples sell 3 for 2 and the 4th stays.
	protected void RunBuyer(LF_TestContext ctx, IEntity seller, IEntity traderOwner, LF_TraderManagerComponent trader)
	{
		LF_MarketConfig.Reload();
		LF_BuyerDemandConfig config = new LF_BuyerDemandConfig();
		config.m_aRows = {};
		config.m_aRows.Insert(LF_BuyerDemandRow.Make(BUYER, APPLE, 9, 0, 2, "", 3));
		LF_BuyerDemandConfig.UseForTest(config);
		LF_BuyerDemandBook.Restore(BUYER, APPLE, null);
		trader.m_sBuyerId = BUYER;

		ctx.Equal(4, LF_InventoryUtils.AddAmount(seller, APPLE, 4), "the seller gets 4 apples");
		if (ctx.FailureCount() > 0)
			return;

		int before = LF_MoneyUtils.GetCash(seller);
		ctx.True(SellFirst(ctx, seller, APPLE, traderOwner) == LF_ETraderSale.SOLD, "4 apples at a lot of 3 are a sale");
		ctx.Equal(before + 2, LF_MoneyUtils.GetCash(seller), "one lot at $2 pays 2");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(seller, APPLE), "the part lot stays with the seller");
		ctx.Equal(6, LF_BuyerDemandBook.RemainingAt(BUYER, APPLE, System.GetUnixTime()), "the purse counts apples: 9 less 3");

		ctx.True(SellFirst(ctx, seller, APPLE, traderOwner) == LF_ETraderSale.BELOW_LOT || RedProof(), "one apple is below one lot of 3");
		ctx.Equal(before + 2, LF_MoneyUtils.GetCash(seller), "a stack below a lot pays nothing");
		ctx.Equal(6, LF_BuyerDemandBook.RemainingAt(BUYER, APPLE, System.GetUnixTime()), "and spends none of the purse");
	}
}
