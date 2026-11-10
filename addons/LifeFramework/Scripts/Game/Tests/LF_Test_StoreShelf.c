// red-proof: observed 2026-11-10 with `tools\cli test --no-build --only LF_Test_StoreShelf,...`: LF_Stores.Pay not removing the paid tags failed `- the tag is cleared (expected 0, got 1)` and `- a second visit to the till has nothing to charge (expected 1, got 0)`; restored, green.

// tier: WORLD
//! The physical general store (#202) on the DebugWorld counter: a shelf item taken by hand leaves the
//! stock once and is tagged unpaid; a price-list sale of the same line leaves it once more (never the
//! same unit twice, even before the pickup was noticed); an empty line refuses; the till charges
//! exactly what is carried unpaid, clears the tags and hands over a receipt; a short purse pays for
//! none of it. Deletes what it spawns and puts the stock back.
class LF_Test_StoreShelf : LF_Test
{
	static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	static const string STORE_COUNTER = "Shop_GeneralStore_Debug";

	override string GetName()
	{
		return "store/shelf-till";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity counter = GetGame().GetWorld().FindEntityByName(STORE_COUNTER);
		ctx.NotNull(counter, "the general store counter resolves in the DebugWorld");
		if (!counter)
			return;

		LF_StoreComponent store = LF_Component<LF_StoreComponent>.Find(counter);
		LF_ShopComponent shop = LF_Component<LF_ShopComponent>.Find(counter);
		ctx.NotNull(store, "the counter carries LF_StoreComponent");
		ctx.NotNull(shop, "the counter keeps its price list");
		if (!store || !shop)
			return;

		store.FirstFill();
		ctx.True(store.HasLine(APPLE), "the store stocks apples");
		int stock0 = store.GetStock(APPLE);
		int price = shop.GetBuyPrice(APPLE);
		ctx.True(stock0 >= 4, "the apple line starts with enough for the test");
		ctx.True(price > 0, "an apple has a price");
		ctx.True(store.GetShelfCount(APPLE) > 0, "apples stand on the shelf");
		if (ctx.FailureCount() > 0)
			return;

		array<ref LF_StoreStockRow> saved = LF_Stores.ExportAll();
		LF_Stores.ClearUnpaid();

		IEntity buyer = LF_TestCharacterFixture.Spawn(ctx);
		if (!buyer)
			return;

		buyer.SetOrigin(counter.GetOrigin() + "0 0.2 1.2");
		ctx.Equal(1000, LF_MoneyUtils.AddCash(buyer, 1000), "the buyer carries cash");
		InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(buyer);
		ctx.NotNull(storage, "the buyer has an inventory");
		IEntity shelfApple = store.GetShelfItem(APPLE, 0);
		ctx.NotNull(shelfApple, "slot 0 of the apple line holds a real item");
		if (!storage || !shelfApple)
		{
			LF_TestCharacterFixture.Delete(buyer);
			return;
		}

		// Pickup: the buyer takes the item off the shelf by hand.
		EntityID shelfId = shelfApple.GetID();
		ctx.True(storage.TryInsertItem(shelfApple), "the apple goes into the buyer's pocket");

		// A price-list sale lands BEFORE the server noticed the pickup: still one unit each.
		ctx.True(shop.BuyItem(APPLE, 1, buyer), "the price list sells an apple");
		ctx.Equal(stock0 - 2, store.GetStock(APPLE), "the picked apple and the sold apple each left the stock once");
		ctx.True(GetGame().GetWorld().FindEntityByID(shelfId) != null, "the sale did not delete the apple the buyer picked");
		ctx.Equal(2, LF_InventoryUtils.GetAmount(buyer, APPLE), "the buyer holds exactly two apples");
		ctx.True(store.GetShelfCount(APPLE) <= store.GetStock(APPLE), "the shelf never shows more than the stock");
		ctx.Equal(1, LF_Stores.UnpaidCount(), "one apple is tagged unpaid, the bought one is not");
		ctx.Equal(price, LF_Stores.OwedBy(store.GetStoreKey(), buyer), "the buyer owes one apple");

		// An empty line refuses.
		store.ForceStock(APPLE, 0);
		int charged;
		ctx.Equal(LF_EShopRefusal.OUT_OF_STOCK, shop.Buy(APPLE, 1, buyer, charged), "an empty line is out of stock");
		ctx.Equal(0, store.GetShelfCount(APPLE), "an empty line has an empty shelf");
		ctx.Equal(0, store.GetStock(APPLE), "a refused sale takes nothing from the stock");
		store.ForceStock(APPLE, stock0 - 2);

		// The till: pays exactly what is carried unpaid, once.
		int cashBefore = LF_MoneyUtils.GetCash(buyer);
		int paid;
		ctx.Equal(LF_EStorePay.PAID, LF_Stores.Pay(store, buyer, paid), "the till takes the payment");
		ctx.Equal(price, paid, "the till charged one apple");
		ctx.Equal(cashBefore - price, LF_MoneyUtils.GetCash(buyer), "the cash left the pocket");
		ctx.Equal(0, LF_Stores.UnpaidCount(), "the tag is cleared");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(buyer, LF_Stores.RECEIPT_PREFAB), "the buyer holds a receipt");
		ctx.Equal(2, LF_InventoryUtils.GetAmount(buyer, APPLE), "the apples stay with the buyer");
		ctx.Equal(LF_EStorePay.NOTHING, LF_Stores.Pay(store, buyer, paid), "a second visit to the till has nothing to charge");

		// A short purse pays for none of it, and the tag stays.
		IEntity poor = LF_TestCharacterFixture.Spawn(ctx);
		if (poor)
		{
			poor.SetOrigin(counter.GetOrigin() + "0 0.2 1.6");
			IEntity poorApple = store.GetShelfItem(APPLE, 1);
			InventoryStorageManagerComponent poorStorage = LF_InventoryUtils.GetResponsibleStorageManager(poor);
			ctx.NotNull(poorApple, "slot 1 holds a second apple");
			if (poorApple && poorStorage)
			{
				ctx.True(poorStorage.TryInsertItem(poorApple), "the second shopper takes an apple");
				ctx.Equal(LF_EStorePay.NOT_ENOUGH_CASH, LF_Stores.Pay(store, poor, paid), "no cash, no payment");
				ctx.Equal(1, LF_Stores.UnpaidCount(), "the apple is still tagged unpaid");
				ctx.Equal(0, LF_InventoryUtils.GetAmount(poor, LF_Stores.RECEIPT_PREFAB), "no receipt for no payment");
			}

			LF_TestCharacterFixture.Delete(poor);
		}

		// Put the world back.
		LF_TestCharacterFixture.Delete(buyer);
		LF_Stores.ClearUnpaid();
		LF_Stores.ApplyAll(saved);
		store.ForceStock(APPLE, stock0);
	}
}
