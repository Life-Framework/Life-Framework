// red-proof: observed 2026-11-10 with `tools\cli test --no-build --only LF_Test_StoreShoplift,...`: LF_Stores.Shoplift without LF_WantedReports.Identify failed `- the report names the thief`; in the same run Pay leaving the tags failed `- exactly one shopper is caught walking out (expected 1, got 2)`; restored, green.

// tier: WORLD
//! Walking out of the physical general store (#202): a shopper who carries an unpaid shelf item and
//! stands outside the boundary is a thief (the alarm sounds at the store, dispatch is called, the
//! report names them, the tags close); a shopper who paid walks out clean; an item eaten in the shop
//! stays on the bill; two shoppers are judged separately. Deletes what it spawns.
class LF_Test_StoreShoplift : LF_Test
{
	override string GetName()
	{
		return "store/shoplift";
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
		int stock0 = store.GetStock(apple);
		array<ref LF_StoreStockRow> saved = LF_Stores.ExportAll();
		LF_Stores.ClearUnpaid();
		LF_Dispatch.ResetStatics();
		LF_WantedReports.ResetStatics();

		IEntity thief = LF_TestCharacterFixture.Spawn(ctx);
		IEntity honest = LF_TestCharacterFixture.Spawn(ctx);
		if (!thief || !honest)
		{
			LF_TestCharacterFixture.Delete(thief);
			LF_TestCharacterFixture.Delete(honest);
			return;
		}

		vector inside = counter.GetOrigin() + "0 0.2 1.2";
		thief.SetOrigin(inside);
		honest.SetOrigin(inside + "0.5 0 0");
		ctx.Equal(1000, LF_MoneyUtils.AddCash(honest, 1000), "the honest shopper carries cash");

		InventoryStorageManagerComponent thiefStorage = LF_InventoryUtils.GetResponsibleStorageManager(thief);
		InventoryStorageManagerComponent honestStorage = LF_InventoryUtils.GetResponsibleStorageManager(honest);
		IEntity thiefApple = store.GetShelfItem(apple, 0);
		IEntity honestApple = store.GetShelfItem(apple, 1);
		ctx.True(thiefStorage != null && honestStorage != null && thiefApple != null && honestApple != null, "both shoppers and two shelf apples resolve");
		if (!thiefStorage || !honestStorage || !thiefApple || !honestApple)
		{
			LF_TestCharacterFixture.Delete(thief);
			LF_TestCharacterFixture.Delete(honest);
			return;
		}

		ctx.True(thiefStorage.TryInsertItem(thiefApple), "the thief pockets an apple");
		ctx.True(honestStorage.TryInsertItem(honestApple), "the honest shopper takes an apple");
		store.DetectTaken();
		LF_Stores.RefreshAll();
		ctx.Equal(2, LF_Stores.UnpaidCount(), "two apples are unpaid");

		// Inside the boundary nobody is a thief, however long they browse.
		ctx.Equal(0, LF_Stores.SweepExits(), "carrying unpaid goods inside the store is not shoplifting");

		int paid;
		ctx.Equal(LF_EStorePay.PAID, LF_Stores.Pay(store, honest, paid), "the honest shopper pays");

		// Both walk out of the boundary.
		vector outside = counter.GetOrigin() + "0 0.2 40";
		thief.SetOrigin(outside);
		honest.SetOrigin(outside + "0.5 0 0");
		ctx.False(store.Contains(thief.GetOrigin()), "the street is outside the boundary");
		int calls = LF_Dispatch.CallCount();
		int alarmsBefore = LF_Dispatch.ActiveAlarmCount();
		ctx.Equal(1, LF_Stores.SweepExits(), "exactly one shopper is caught walking out");
		string thiefKey = LF_Stores.ThiefKey(thief);
		string honestKey = LF_Stores.ThiefKey(honest);
		ctx.Equal(1, LF_WantedReports.CountFor(thiefKey), "the thief has a crime report");
		ctx.True(LF_WantedReports.IsIdentified(thiefKey), "the report names the thief");
		ctx.Equal(0, LF_WantedReports.CountFor(honestKey), "the shopper who paid has no report");
		ctx.Equal(calls + 1, LF_Dispatch.CallCount(), "dispatch was called once");
		ctx.True(LF_Dispatch.ActiveAlarmCount() > alarmsBefore, "the alarm sounds at the store");
		LF_DispatchRecord last = LF_Dispatch.Last();
		if (last)
			ctx.EqualStr(LF_WantedReports.OFFENCE_SHOPLIFTING, last.m_sOffence, "the call names shoplifting");

		ctx.Equal(0, LF_Stores.UnpaidCount(), "the tags are closed");
		ctx.Equal(0, LF_Stores.SweepExits(), "the same thief is not reported twice");

		// An item eaten in the shop stays on the bill: used up, still owed.
		IEntity eater = LF_TestCharacterFixture.Spawn(ctx);
		if (eater)
		{
			eater.SetOrigin(inside);
			IEntity eaten = store.GetShelfItem(apple, 2);
			InventoryStorageManagerComponent eaterStorage = LF_InventoryUtils.GetResponsibleStorageManager(eater);
			if (eaten && eaterStorage && eaterStorage.TryInsertItem(eaten))
			{
				store.DetectTaken();
				LF_Stores.RefreshAll();
				SCR_EntityHelper.DeleteEntityAndChildren(eaten);
				LF_Stores.RefreshAll();
				ctx.True(LF_Stores.OwedBy(store.GetStoreKey(), eater) > 0, "an apple used up in the shop is still owed");
				eater.SetOrigin(outside);
				ctx.Equal(1, LF_Stores.SweepExits(), "walking out owing for an eaten apple is shoplifting");
			}
			else
			{
				ctx.Fail("the third shelf apple could not be taken");
			}

			LF_TestCharacterFixture.Delete(eater);
		}

		// Put the world back.
		LF_Dispatch.StopAllAlarms();
		LF_Dispatch.ResetStatics();
		LF_WantedReports.ResetStatics();
		LF_TestCharacterFixture.Delete(thief);
		LF_TestCharacterFixture.Delete(honest);
		LF_Stores.ClearUnpaid();
		LF_Stores.ApplyAll(saved);
		store.ForceStock(apple, stock0);
	}
}
