// red-proof: observed 2026-11-18 with `tools\cli test --no-build --only LF_Test_StoreShelfNotSaved` and the three LF_StoreShelfSave hooks in LF_StoreComponent commented out: failed `- shelf apple 0..5 is not in the world save` (6 lines); hooks back, green. RedProof() re-tracks each copy and untracks the taken unit.

//! Shelf copies are not written to the world save (#3127). The store's stock count rides the
//! "storestock" payload, and the store spawns its shelf from it after a restart, so a shelf copy the
//! engine also saved came back twice and was deleted at every boot. Every stocked copy is untracked by
//! the PersistenceSystem (so the world save has no record of it), the store's stock is in the save, and
//! a unit a shopper takes off the shelf is tracked again (a carried or dropped item is still saved).
//! Deletes what it spawns and puts the stock back.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_StoreShelfNotSaved : LF_Test
{
	override string GetName()
	{
		return "persist/store-shelf-not-saved";
	}

	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		ctx.NotNull(gameMode, "the GridWorld game mode resolves");
		if (!gameMode)
			return;

		LF_PersistenceComponent persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));
		IEntity counter = GetGame().GetWorld().FindEntityByName(LF_Test_StoreShelf.GRID_STORE);
		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		ctx.NotNull(counter, "the general store counter resolves");
		ctx.True(LF_StoreShelfSave.HasSystem(), "the world has a persistence system");
		if (!persistence || !counter || !LF_StoreShelfSave.HasSystem())
			return;

		LF_StoreComponent store = LF_Component<LF_StoreComponent>.Find(counter);
		ctx.NotNull(store, "the counter carries LF_StoreComponent");
		if (!store)
			return;

		ResourceName apple = LF_Test_StoreShelf.APPLE;
		store.FirstFill();
		int original = store.GetStock(apple);
		// Fresh copies: an empty line filled again spawns every shelf copy now.
		store.ForceStock(apple, 0);
		store.ForceStock(apple, 6);
		int shelf = store.GetShelfCount(apple);
		ctx.True(shelf >= 3, "at least three apples stand on the shelf");
		if (ctx.FailureCount() > 0)
		{
			store.ForceStock(apple, original);
			return;
		}

		// No stocked copy is tracked, so the world save writes no record of it.
		int checkedItems;
		for (int i = 0; i < shelf + 2; i++)
		{
			IEntity item = store.GetShelfItem(apple, i);
			if (!item)
				continue;

			if (RedProof())
				LF_StoreShelfSave.Release(item);

			checkedItems++;
			ctx.False(LF_StoreShelfSave.IsSaved(item), string.Format("shelf apple %1 is not in the world save", i));
		}

		ctx.Equal(shelf, checkedItems, "every shelf apple was checked");

		// The stock count is what the save keeps of the shelf.
		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(store.GetStoreKey()), "the save carries the store's stock");

		// A unit a shopper took and dropped away from the shelf is saved as before.
		IEntity taken = store.GetShelfItem(apple, 0);
		ctx.NotNull(taken, "slot 0 holds an apple");
		if (taken)
		{
			taken.SetOrigin(counter.GetOrigin() + "0 0.3 6");
			taken.Update();
			store.DetectTaken();
			ctx.False(store.IsShelfItem(taken), "the dropped apple is no longer a shelf item");
			if (RedProof())
				LF_StoreShelfSave.Hold(taken);

			ctx.True(LF_StoreShelfSave.IsSaved(taken), "the dropped apple is in the world save again");
			LF_Stores.ClearUnpaid();
			SCR_EntityHelper.DeleteEntityAndChildren(taken);
		}

		store.ForceStock(apple, original);
	}
}
