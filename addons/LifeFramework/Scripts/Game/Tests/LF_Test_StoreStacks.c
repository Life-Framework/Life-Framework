// red-proof: observed 2026-11-10 with `tools\cli test --no-build --local --only LF_Test_StoreStacks`: LF_Stores.HoldsApart returning false failed `- the shelf apple was not merged away into the stack`, `- the shelf apple is tagged unpaid (expected 1, got 0)` and `- the till charges it (expected 0, got 1)`; restored, green.

// tier: WORLD
//! A shopper who already carries apples takes a shelf apple (#202): the quantity merge must not fold it
//! into their stack, because that would delete the item carrying the unpaid tag and make it free. The
//! shelf apple stays its own item, tagged unpaid, and the till charges it. Deletes what it spawns.
class LF_Test_StoreStacks : LF_Test
{
	override string GetName()
	{
		return "store/stack-merge";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity counter = GetGame().GetWorld().FindEntityByName(LF_Test_StoreShelf.STORE_COUNTER);
		ctx.NotNull(counter, "the general store counter resolves in the DebugWorld");
		if (!counter)
			return;

		LF_StoreComponent store = LF_Component<LF_StoreComponent>.Find(counter);
		ctx.NotNull(store, "the counter carries LF_StoreComponent");
		if (!store)
			return;

		store.FirstFill();
		ResourceName apple = LF_Test_StoreShelf.APPLE;
		int original = store.GetStock(apple);
		int stock0 = 6;
		store.ForceStock(apple, stock0);
		array<ref LF_StoreStockRow> saved = LF_Stores.ExportAll();
		LF_Stores.ClearUnpaid();

		IEntity shopper = LF_TestCharacterFixture.Spawn(ctx);
		if (!shopper)
			return;

		shopper.SetOrigin(counter.GetOrigin() + "0 0.2 1.2");
		ctx.Equal(1000, LF_MoneyUtils.AddCash(shopper, 1000), "the shopper carries cash");
		ctx.Equal(1, LF_InventoryUtils.AddAmount(shopper, apple, 1), "the shopper already carries an apple");
		InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(shopper);
		IEntity shelfApple = store.GetShelfItem(apple, 0);
		ctx.True(storage != null && shelfApple != null, "the shopper has an inventory and the shelf holds an apple");
		if (storage && shelfApple)
		{
			EntityID shelfId = shelfApple.GetID();
			ctx.True(storage.TryInsertItem(shelfApple), "the shelf apple goes into the pocket");
			ctx.True(GetGame().GetWorld().FindEntityByID(shelfId) != null, "the shelf apple was not merged away into the stack");
			store.DetectTaken();
			ctx.Equal(1, LF_Stores.UnpaidCount(), "the shelf apple is tagged unpaid");
			ctx.Equal(stock0 - 1, store.GetStock(apple), "and left the stock once");
			int paid;
			ctx.Equal(LF_EStorePay.PAID, LF_Stores.Pay(store, shopper, paid), "the till charges it");
			ctx.Equal(2, LF_InventoryUtils.GetAmount(shopper, apple), "the shopper holds both apples");
		}

		LF_TestCharacterFixture.Delete(shopper);
		LF_Stores.DeleteLooseReceipt();
		LF_Stores.ClearUnpaid();
		LF_Stores.ApplyAll(saved);
		store.ForceStock(apple, stock0);
	}
}
