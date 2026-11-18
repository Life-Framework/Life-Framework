// red-proof: run with `tools\cli test --no-build --red LF_Test_StoreShelfGarbage`; RedProof() hands every shelf copy back to the garbage system (LF_StoreGarbage.Release), so the "not tracked" and "insert is refused" checks fail.

//! The garbage system must never delete shelf stock (#3117). The vanilla system tracks a loose item and
//! deletes it when its lifetime (1200 s) is up; on an idle dedicated server every store lost all of its
//! stock 20 minutes after boot. A stocked shelf copy is blacklisted from the system: it is not tracked,
//! an insert request is refused (so an inventory event or a drop cannot re-track it), and a copy placed
//! again by a later fill or a pile closing up is held too. A unit a shopper takes off the shelf and puts
//! on the ground goes back under the normal rules. Deletes what it spawns and puts the stock back.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_StoreShelfGarbage : LF_Test
{
	static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	//! The grid fixture the store tests read (Grid_shops_till.layer).
	static const string GRID_STORE = "Grid_shops_till_Store";

	override string GetName()
	{
		return "store/shelf-garbage";
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

		store.FirstFill();
		ctx.True(store.HasLine(APPLE), "the store stocks apples");
		store.ForceStock(APPLE, 6);
		int shelf = store.GetShelfCount(APPLE);
		ctx.True(shelf >= 3, "at least three apples stand on the shelf");
		if (ctx.FailureCount() > 0)
			return;

		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		ctx.True(world && world.GetGarbageSystem() != null, "the world has a garbage system to exclude the shelf from");
		if (!world || !world.GetGarbageSystem())
			return;

		// Every stocked copy is out of the garbage system, and the system refuses it.
		int checkedItems;
		for (int i = 0; i < shelf + 2; i++)
		{
			IEntity item = store.GetShelfItem(APPLE, i);
			if (!item)
				continue;

			if (RedProof())
				LF_StoreGarbage.Release(item);

			checkedItems++;
			ctx.False(LF_StoreGarbage.IsTracked(item), string.Format("shelf apple %1 is not tracked by the garbage system", i));
			ctx.False(LF_StoreGarbage.TryInsert(item), string.Format("the garbage system refuses shelf apple %1", i));
		}

		ctx.Equal(shelf, checkedItems, "every shelf apple was checked");

		// A shelf the store refills after a restock holds the new copies too.
		store.ForceStock(APPLE, 0);
		store.ForceStock(APPLE, 6);
		for (int r = 0; r < store.GetShelfCount(APPLE) + 2; r++)
		{
			IEntity again = store.GetShelfItem(APPLE, r);
			if (again)
				ctx.False(LF_StoreGarbage.IsTracked(again), string.Format("refilled apple %1 is not tracked", r));
		}

		// A unit a shopper took and dropped away from the shelf is under the normal rules again.
		IEntity taken = store.GetShelfItem(APPLE, 0);
		ctx.NotNull(taken, "slot 0 holds an apple");
		if (taken)
		{
			taken.SetOrigin(counter.GetOrigin() + "0 0.3 6");
			taken.Update();
			store.DetectTaken();
			ctx.False(store.IsShelfItem(taken), "the dropped apple is no longer a shelf item");
			ctx.True(LF_StoreGarbage.IsTracked(taken), "the dropped apple is tracked by the garbage system again");
			LF_Stores.ClearUnpaid();
			SCR_EntityHelper.DeleteEntityAndChildren(taken);
		}

		store.ForceStock(APPLE, 6);
	}
}
