// red-proof: first run on the unfixed LF_Cleanup (2026-11-24): `--only LF_Test_StoreShelfCleanup` failed `a shelf apple survives a sweep long after every player left` and `exactly the dropped apple is swept`; with the shelf guard in LF_Cleanup.Sweep it passes. `--red` makes the shelf apple count as taken first, so the same checks fail.

//! The world cleanup sweep must never delete shelf stock (#4819). A shelf copy is a loose item to
//! LF_Cleanup, so on an idle island server every sweep (300 s, 900 s without a player within 50 m)
//! deleted the supermarket and clothing store shelves and the stores refilled them one unit per tick.
//! A copy the store owns on its shelf is kept; a unit a shopper took off the shelf and dropped
//! loose is swept like any other item. Deletes what it spawns and puts the stock back.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_StoreShelfCleanup : LF_Test
{
	static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	//! The grid fixture the store tests read (Grid_shops_till.layer).
	static const string GRID_STORE = "Grid_shops_till_Store";
	protected static const int HOUR_MS = 3600000;	// lf-fixed: test clock, longer than every shipped age

	override string GetName()
	{
		return "store/shelf-cleanup";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity counter = GetGame().GetWorld().FindEntityByName(GRID_STORE);
		ctx.NotNull(counter, "the general store counter resolves in the GridWorld");
		if (!counter)
			return;

		LF_StoreComponent store = LF_Component<LF_StoreComponent>.Find(counter);
		ctx.NotNull(store, "the counter carries LF_StoreComponent");
		if (!store)
			return;

		LF_Cleanup.ResetStatics();
		ctx.NotNull(LF_CleanupConfig.Get(), "the cleanup config loads");
		if (!LF_CleanupConfig.Get())
			return;

		store.FirstFill();
		store.ForceStock(APPLE, 6);
		ctx.True(store.GetShelfCount(APPLE) >= 3, "at least three apples stand on the shelf");
		IEntity shelfApple = store.GetShelfItem(APPLE, 1);
		IEntity taken = store.GetShelfItem(APPLE, 0);
		ctx.NotNull(shelfApple, "slot 1 holds an apple");
		ctx.NotNull(taken, "slot 0 holds an apple");
		if (!shelfApple || !taken)
		{
			LF_Cleanup.ResetStatics();
			return;
		}

		EntityID shelfId = shelfApple.GetID();
		EntityID takenId = taken.GetID();

		// A shopper takes slot 0 off the shelf and drops it loose, well away from the shelf.
		taken.SetOrigin(counter.GetOrigin() + "0 0.3 6");
		taken.Update();
		if (RedProof())
		{
			// The proof: the shelf copy is no longer the store's, so the sweep sees an ordinary loose item.
			shelfApple.SetOrigin(counter.GetOrigin() + "0 0.3 -6");
			shelfApple.Update();
		}

		store.DetectTaken();
		LF_Stores.ClearUnpaid();
		ctx.False(store.IsShelfItem(taken), "the dropped apple is no longer a shelf item");

		// No player anywhere, the clock two hours on: every loose item has aged past every rule.
		array<IEntity> entities = {shelfApple, taken};
		array<vector> players = {};
		ctx.Equal(0, LF_Cleanup.Sweep(entities, players, 0), "a fresh sweep removes nothing: nothing has aged");
		int removed = LF_Cleanup.Sweep(entities, players, HOUR_MS * 2);

		ctx.True(GetGame().GetWorld().FindEntityByID(shelfId) != null, "a shelf apple survives a sweep long after every player left");
		ctx.True(GetGame().GetWorld().FindEntityByID(takenId) == null, "the apple a shopper dropped loose is swept");
		ctx.Equal(1, removed, "exactly the dropped apple is swept");

		LF_Cleanup.ResetStatics();
		LF_Stores.ClearUnpaid();
		store.ForceStock(APPLE, 6);
	}
}
