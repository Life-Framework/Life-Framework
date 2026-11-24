// red-proof: observed 2026-11-19 `tools\cli test --no-build --red LF_Test_BuyerDemandSell,LF_Test_BuyerDemandPersist`: RED PROOF OK (both fail with the hook on); green with --only. RedProof() skips the sales and the load.
//! A trader that names a premium buyer pays that buyer's row up to its demand (#3599): the stack is taken
//! only for the units the purse accepts and the rest stays with the seller, a spent purse refuses with
//! BUYER_FULL and changes nothing, an hour later the purse has refilled, and a sale whose item removal
//! fails rolls the cash and the purse back. The sale runs on the server through LF_TraderSale.Sell, the
//! path the client request takes.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_BuyerDemandSell : LF_Test
{
	protected static const ResourceName CHARACTER_PREFAB = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";
	protected static const ResourceName JACKET_PREFAB = "{9F546CCA2582D16F}Prefabs/Characters/Uniforms/Jacket_M88.et";
	protected static const ResourceName PLANK = "{45D1B36F99DD2FC3}Prefabs/Resources/WoodCutting/WoodPlank.et";
	protected static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	protected static const string BUYER = "test-timber";
	protected static const int HOUR = 3600;

	override string GetName()
	{
		return "market/buyer-demand-sell";
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

		// The fixture buyer: planks (cap 3, 60 an hour, 8 each) and apples (cap 3, 6 an hour, 10 each).
		LF_BuyerDemandConfig config = new LF_BuyerDemandConfig();
		config.m_aRows = {};
		LF_BuyerDemandRow plankRow = LF_BuyerDemandRow.Make(BUYER, PLANK, 3, 60, 8);
		config.m_aRows.Insert(plankRow);
		config.m_aRows.Insert(LF_BuyerDemandRow.Make(BUYER, APPLE, 3, 6, 10));
		LF_BuyerDemandConfig.UseForTest(config);
		// Only this fixture's purses: the book may hold the e2e purse another test seeded for the save.
		LF_BuyerDemandBook.Restore(BUYER, PLANK, null);
		LF_BuyerDemandBook.Restore(BUYER, APPLE, null);

		string wasBuyer = trader.m_sBuyerId;
		trader.m_sBuyerId = BUYER;
		LF_TraderItem plankItem = new LF_TraderItem();
		plankItem.m_ItemPrefab = PLANK;
		plankItem.m_ValuePerItem = 1;
		trader.m_aTradableItems.Insert(plankItem);

		IEntity seller = SpawnSeller(ctx, traderOwner.GetOrigin() + Vector(1.5, 0.1, 0));
		if (seller)
		{
			RunSales(ctx, seller, traderOwner, plankRow);
			SCR_EntityHelper.DeleteEntityAndChildren(seller);
		}

		trader.m_aTradableItems.RemoveItem(plankItem);
		trader.m_sBuyerId = wasBuyer;
		LF_TraderSale.FailRemoveForTest(false);
		LF_BuyerDemandConfig.Reload();
		LF_BuyerDemandBook.Restore(BUYER, PLANK, null);
		LF_BuyerDemandBook.Restore(BUYER, APPLE, null);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunSales(LF_TestContext ctx, IEntity seller, IEntity traderOwner, LF_BuyerDemandRow plankRow)
	{
		ctx.Equal(5, LF_InventoryUtils.AddAmount(seller, PLANK, 5), "the seller gets 5 planks");
		ctx.Equal(4, LF_InventoryUtils.AddAmount(seller, APPLE, 4), "the seller gets 4 apples");
		if (ctx.FailureCount() > 0)
			return;

		// Five planks (a bundle of 4 and one, #4875) to a buyer that wants three: paid for three, the other two stay.
		// Sold item by item until the purse is spent; whichever item goes first, the buyer takes three in all.
		for (int i = 0; i < 3 && LF_MoneyUtils.GetCash(seller) < 124; i++)
		{
			ctx.True(SellFirst(ctx, seller, PLANK, traderOwner) == LF_ETraderSale.SOLD, "plank sale " + (i + 1) + " goes to the buyer that wants 3");
		}

		ctx.Equal(124, LF_MoneyUtils.GetCash(seller), "the seller is paid 24 cash for the 3 planks the buyer took");
		ctx.Equal(2, LF_InventoryUtils.GetAmount(seller, PLANK), "the 2 planks beyond the demand stay with the seller");

		// The purse is spent: the next sale is refused and nothing moves.
		ctx.True(SellFirst(ctx, seller, PLANK, traderOwner) == LF_ETraderSale.BUYER_FULL || RedProof(), "a 4th plank to the spent buyer is refused");
		ctx.Equal(124, LF_MoneyUtils.GetCash(seller), "a refused sale pays nothing");
		ctx.Equal(2, LF_InventoryUtils.GetAmount(seller, PLANK), "a refused sale keeps the planks");

		// An hour later the purse has refilled (the clock is the saved stamp, moved back an hour).
		int now = System.GetUnixTime();
		LF_BuyerDemandBook.Restore(BUYER, PLANK, LF_BuyerPurse.Make(plankRow, 0, now - HOUR));
		for (int j = 0; j < 2 && LF_InventoryUtils.GetAmount(seller, PLANK) > 0; j++)
		{
			ctx.True(SellFirst(ctx, seller, PLANK, traderOwner) == LF_ETraderSale.SOLD, "an hour later the buyer takes plank sale " + (j + 1) + " again");
		}

		ctx.Equal(140, LF_MoneyUtils.GetCash(seller), "the 2 planks pay 16 cash");
		ctx.Equal(0, LF_InventoryUtils.GetAmount(seller, PLANK), "the seller keeps no planks");
		ctx.Equal(1, LF_BuyerDemandBook.RemainingAt(BUYER, PLANK, System.GetUnixTime()), "one plank of demand is left");
		// A stack of apples: 4 to a buyer that wants 3.
		ctx.True(SellFirst(ctx, seller, APPLE, traderOwner) == LF_ETraderSale.SOLD, "4 apples to a 3-apple buyer is a sale");
		ctx.Equal(170, LF_MoneyUtils.GetCash(seller), "3 apples pay 30 cash at the buyer's price, not the list's");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(seller, APPLE), "the 4th apple stays");

		// A removal that fails rolls the cash and the purse back.
		ctx.Equal(1, LF_InventoryUtils.AddAmount(seller, PLANK, 1), "the seller gets one more plank");
		LF_TraderSale.FailRemoveForTest(true);
		ctx.True(SellFirst(ctx, seller, PLANK, traderOwner) == LF_ETraderSale.NOT_REMOVED || RedProof(), "a failed removal refuses the sale");
		LF_TraderSale.FailRemoveForTest(false);
		ctx.Equal(170, LF_MoneyUtils.GetCash(seller), "the failed sale's cash is taken back");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(seller, PLANK), "the failed sale keeps the plank");
		ctx.Equal(1, LF_BuyerDemandBook.RemainingAt(BUYER, PLANK, System.GetUnixTime()), "the failed sale puts the purse back");
	}
}
