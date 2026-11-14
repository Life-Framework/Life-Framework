// red-proof: observed 2026-11-10 with `tools\cli test --no-build --only LF_Test_StorePersist,...`: LF_StoreComponent.ImportRow not applying the saved count failed `- the saved stock is back (expected 3, got 6)`; restored, green.

// tier: PERSISTENCE
//! The store stock rides the real save path (the "storestock" payload, save version 21): after a
//! restart the shelf holds the stock that was left, not a fresh full one, a second load changes
//! nothing and never doubles the shelf, a saved stock for an item the store no longer sells is
//! dropped, and a stock saved for a store that is not in the world waits and is kept. The test puts
//! the store back the way it found it.
class LF_Test_StorePersist : LF_Test
{
	override string GetName()
	{
		return "persist/store-stock";
	}

	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		ctx.NotNull(gameMode, "the DebugWorld game mode resolves");
		if (!gameMode)
			return;

		LF_PersistenceComponent persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));
		IEntity counter = GetGame().GetWorld().FindEntityByName(LF_Test_StoreShelf.STORE_COUNTER);
		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		ctx.NotNull(counter, "the general store counter resolves");
		if (!persistence || !counter)
			return;

		LF_StoreComponent store = LF_Component<LF_StoreComponent>.Find(counter);
		ctx.NotNull(store, "the counter carries LF_StoreComponent");
		if (!store)
			return;

		store.FirstFill();
		ResourceName apple = LF_Test_StoreShelf.APPLE;
		array<ref LF_StoreStockRow> before = LF_Stores.ExportAll();
		int original = store.GetStock(apple);
		int stock0 = 6;
		store.ForceStock(apple, stock0);
		ctx.Equal(stock0, store.GetStock(apple), "the test starts from a full apple line");

		// Three apples left the store: a restart must keep it that way.
		int left = stock0 - 3;
		store.ForceStock(apple, left);
		ctx.Equal(left, store.GetStock(apple), "the stock is down");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(store.GetStoreKey()), "the save carries the store's key");

		// A restart: the world comes back full.
		store.ForceStock(apple, stock0);
		ctx.Equal(stock0, store.GetStock(apple), "the restarted store is full");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		int version;
		loadContext.ReadValue("version", version);
		ctx.Equal(31, version, "the save is written at version 31 (storestock is payload 21)");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "storestock");
		ctx.Equal(left, store.GetStock(apple), "the saved stock is back");
		int shelf = store.GetShelfCount(apple);
		ctx.True(shelf <= left, "the shelf does not show more than the stock");
		ctx.True(shelf > 0, "the shelf still shows apples");

		// Loading again converges: same stock, same shelf, nothing doubled.
		JsonLoadContext again = new JsonLoadContext(false);
		again.LoadFromString(saved);
		again.ReadValue("version", version);
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, again, "storestock");
		ctx.Equal(left, store.GetStock(apple), "a second load leaves the same stock");
		ctx.Equal(shelf, store.GetShelfCount(apple), "and the same shelf");

		// A row for an item the store no longer sells is dropped; a row for a missing store waits.
		array<ref LF_StoreStockRow> odd = new array<ref LF_StoreStockRow>();
		LF_StoreStockRow stale = new LF_StoreStockRow();
		stale.m_sStoreKey = store.GetStoreKey();
		stale.m_sPrefab = "{00000000DEADBEEF}Prefabs/Items/Removed/Nothing.et";
		stale.m_iStock = 3;
		odd.Insert(stale);
		LF_StoreStockRow ghost = new LF_StoreStockRow();
		ghost.m_sStoreKey = "ghost_store_not_in_world";
		ghost.m_sPrefab = apple;
		ghost.m_iStock = 2;
		odd.Insert(ghost);
		LF_Stores.ApplyAll(odd);
		ctx.Equal(left, store.GetStock(apple), "a stale row leaves the live stock alone");
		bool kept;
		array<ref LF_StoreStockRow> exported = LF_Stores.ExportAll();
		foreach (LF_StoreStockRow row : exported)
		{
			if (row.m_sStoreKey == "ghost_store_not_in_world")
				kept = true;
		}

		ctx.True(kept, "a stock for a store that is not in the world waits and is saved again");

		// Put the world back.
		LF_Stores.ApplyAll(before);
		store.ForceStock(apple, original);
	}
}
