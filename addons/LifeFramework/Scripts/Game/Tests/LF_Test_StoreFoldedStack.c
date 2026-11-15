// red-proof: observed 2026-11-15 with `tools\cli test --no-build --red LF_Test_StoreStackRules,LF_Test_StoreFoldedStack`: RED-OK store/folded-stack (the hook turns the pile into lone slots, stack 0) failed `- copy 1 stands 0.0198059 m above the counter (wanted 0.06)`, `- the restored copy 1 is adopted into its own slot, not deleted and respawned` and `- copy 2 dropped into the gap`; green with the hook off.

// tier: WORLD
//! Folded clothes in a pile on a store counter (the clothing store). A line with piles of four stands
//! its bottom copy on the counter and the others at their written height above it, turned by the
//! slot's yaw. A restart that finds the restored copies adopts each one into its own slot and deletes
//! none (the bottom slot must not swallow the pile). A copy taken from the middle leaves the stock
//! once and the copies above close the gap. Deletes what it spawns.
class LF_Test_StoreFoldedStack : LF_Test
{
	static const ResourceName TROUSERS = "{6925E6C63EBCF623}Prefabs/Characters/Uniforms/Pants_Trousers_01/Pants_Trousers_01_DarkBrown.et";
	//! Height between two folded copies of the pile.
	static const float STEP = 0.04;
	static const float YAW = 30;

	override string GetName()
	{
		return "store/folded-stack";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity storeCounter = GetGame().GetWorld().FindEntityByName(LF_Test_StoreShelf.STORE_COUNTER);
		ctx.NotNull(storeCounter, "the general store counter resolves in the DebugWorld");
		if (!storeCounter)
			return;

		LF_StoreComponent store = LF_Component<LF_StoreComponent>.Find(storeCounter);
		ctx.NotNull(store, "the counter carries LF_StoreComponent");
		if (!store)
			return;

		store.FirstFill();

		// A counter of its own to pile on, out of everyone's way.
		vector tableAt = "46 300 48";
		IEntity table = LF_Utils.SpawnEntityPrefab(LF_Test_StoreSnapToBoard.COUNTER, tableAt);
		ctx.NotNull(table, "a counter spawns for the pile");
		if (!table)
			return;

		float top = tableAt[1] + LF_Test_StoreSnapToBoard.COUNTER_TOP;
		string slots;
		for (int k = 0; k < 4; k++)
		{
			vector local = storeCounter.CoordToLocal(tableAt + Vector(0, LF_Test_StoreSnapToBoard.COUNTER_TOP + 0.05 + k * STEP, -0.15));
			if (k > 0)
				slots += " | ";

			slots += string.Format("%1 %2 %3 %4", local[0], local[1], local[2], YAW);
		}

		int stack = 0;
		if (!RedProof())
			stack = 4;

		store.AddTestLine(TROUSERS, 4, 4, slots, stack);
		ctx.Equal(4, store.GetShelfCount(TROUSERS), "four copies on the pile");

		// The pile: bottom on the counter, each copy one step higher, all turned by the slot yaw.
		array<EntityID> ids = {};
		float wantYaw = storeCounter.GetYawPitchRoll()[0] + YAW;
		for (int i = 0; i < 4; i++)
		{
			IEntity copy = store.GetShelfItem(TROUSERS, i);
			if (!copy)
			{
				ids.Insert(EntityID.INVALID);
				continue;
			}

			ids.Insert(copy.GetID());
			float rise = copy.GetOrigin()[1] - top;
			ctx.True(Math.AbsFloat(rise - (0.02 + i * STEP)) < 0.01, string.Format("copy %1 stands %2 m above the counter (wanted %3)", i, rise, 0.02 + i * STEP));
			ctx.True(AngleOff(copy.GetYawPitchRoll()[0], wantYaw) < 1, string.Format("copy %1 is turned by the slot yaw (yaw %2, wanted %3)", i, copy.GetYawPitchRoll()[0], wantYaw));
		}

		// A restart: the engine restored the four copies and the store knows none of them. Each is
		// adopted into its own slot; none is deleted and none is spawned anew.
		store.ForgetShelf(TROUSERS);
		store.ForceStock(TROUSERS, 4);
		ctx.Equal(4, store.GetShelfCount(TROUSERS), "after the restart the pile still has four");
		for (int r = 0; r < 4; r++)
		{
			IEntity again = store.GetShelfItem(TROUSERS, r);
			ctx.True(again && again.GetID() == ids[r], string.Format("the restored copy %1 is adopted into its own slot, not deleted and respawned", r));
		}

		// A shopper takes the second copy from the bottom: the stock drops once, the two above drop down.
		IEntity middle = store.GetShelfItem(TROUSERS, 1);
		ctx.NotNull(middle, "the second copy is on the pile");
		if (middle)
		{
			middle.SetOrigin(tableAt + "3 0 3");
			ctx.Equal(1, store.DetectTaken(), "the store notices one copy taken");
			ctx.Equal(3, store.GetStock(TROUSERS), "the stock is down by one");
			ctx.Equal(3, store.GetShelfCount(TROUSERS), "three copies are left on the pile");
			ctx.True(store.GetShelfItem(TROUSERS, 3) == null, "the top of the pile is empty");
			IEntity bottom = store.GetShelfItem(TROUSERS, 0);
			ctx.True(bottom && bottom.GetID() == ids[0], "the bottom copy never moved");
			for (int c = 1; c < 3; c++)
			{
				IEntity closed = store.GetShelfItem(TROUSERS, c);
				ctx.NotNull(closed, string.Format("slot %1 is filled again", c));
				if (closed)
				{
					float riseAfter = closed.GetOrigin()[1] - top;
					ctx.True(Math.AbsFloat(riseAfter - (0.02 + c * STEP)) < 0.01, string.Format("copy %1 dropped into the gap (%2 m above the counter)", c, riseAfter));
				}
			}

			SCR_EntityHelper.DeleteEntityAndChildren(middle);
		}

		store.RemoveTestLine(TROUSERS);
		LF_Stores.ClearUnpaid();
		SCR_EntityHelper.DeleteEntityAndChildren(table);
	}

	//------------------------------------------------------------------------------------------------
	//! A pile of four on the top of a CounterShop_01 store counter, in the counter's own space. The
	//! restart proof (LF_Test_E2E_SaveState) stacks it on the general store counter.
	static string CounterPile()
	{
		string slots;
		for (int k = 0; k < 4; k++)
		{
			if (k > 0)
				slots += " | ";

			slots += string.Format("0.4 %1 -0.15 %2", LF_Test_StoreSnapToBoard.COUNTER_TOP + 0.05 + k * STEP, YAW);
		}

		return slots;
	}

	//------------------------------------------------------------------------------------------------
	protected static float AngleOff(float a, float b)
	{
		float off = Math.AbsFloat(a - b);
		while (off > 360)
			off -= 360;

		return Math.Min(off, 360 - off);
	}
}
