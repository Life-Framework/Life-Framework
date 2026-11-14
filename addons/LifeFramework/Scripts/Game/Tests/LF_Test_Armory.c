// red-proof: Observed 2026-11-10 with `tools/cli test --no-build --only LF_Test_Armory`: not spending a kit in LF_Bank.PayHeist failed `crime/armory` with `- one kit left in stock (expected 1, got 2)`, `- the stock is spent (expected 0, got 2)`, `- out of stock: refused`. Restored; green.
// tier: WORLD
//! The police armory store (#275): breaching it without the keycard is refused and files nothing.
//! With the keycard it pays its weapons once, each stamped with a book serial that names the thief,
//! and files the loud report. It pays again only after a police repair, the loot cooldown and
//! stock; an empty store refuses until the refill brings a kit back.
class LF_Test_Armory : LF_Test
{
	override string GetName()
	{
		return "crime/armory";
	}

	override void Run(LF_TestContext ctx)
	{
		string uid = "armory-a";
		string storeId = "armory-store-test";

		IEntity robber = LF_TestCharacterFixture.Spawn(ctx);
		if (!robber)
			return;

		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(robber);
		IEntity pack = LF_Utils.SpawnEntityPrefab("{06B68C58B72EAAC6}Prefabs/Items/Equipment/Backpacks/Backpack_ALICE_Medium.et", robber.GetOrigin());
		ctx.True(pack != null && inventory.TryInsertItem(pack, EStoragePurpose.PURPOSE_LOADOUT_PROXY), "the robber wears a pack");

		ResourceName m9 = "{C95F0E5226F71550}Prefabs/Items/Police/LF_ArmoryM9.et";
		LF_Bank.Configure(storeId, 1, 1, 600, 0, 0);
		LF_Bank.ConfigureTools(storeId, "keycard,charge", 0);
		LF_Bank.ConfigureHeist(storeId, LF_BankLoot.KIND_WEAPONS, m9 + "," + m9, LF_WantedReports.OFFENCE_ARMORY, true, 2, 1800);

		LF_HeistListener listener = new LF_HeistListener();
		LF_EventBus.Subscribe(LF_OnHeistLoot, listener);

		ctx.True(LF_Breach.IsLoud(storeId), "the armory sounds the loud alarm");
		ctx.False(LF_Bank.Rob(storeId, uid, robber), "no keycard: refused");
		ctx.False(LF_Breach.IsOpen(storeId), "no keycard: the store stays shut");
		ctx.Equal(0, LF_Breach.GetPoliceNotifyCount(storeId), "no keycard: nothing is filed");
		ctx.Equal(0, listener.m_iCount, "no keycard: no loot event");
		ctx.Equal(2, LF_Bank.GetStock(storeId), "no keycard: the stock is untouched");

		ctx.True(LF_InventoryUtils.AddItem(inventory, LF_Bank.KEYCARD_PREFAB), "a keycard is handed over");
		int calls = LF_Dispatch.CallCount();
		ctx.True(LF_Bank.Rob(storeId, uid, robber), "with the keycard the store opens");
		ctx.True(LF_Breach.IsOpen(storeId), "the store is open");
		ctx.Equal(1, LF_Breach.GetPoliceNotifyCount(storeId), "the breach is reported once");
		ctx.Equal(calls + 1, LF_Dispatch.CallCount(), "dispatch took one call");
		ctx.Equal(1, listener.m_iCount, "one loot event");
		ctx.EqualStr(LF_BankLoot.KIND_WEAPONS, listener.m_sKind, "the loot is weapons");
		ctx.Equal(2, listener.m_iAmount, "both weapons spawned");
		ctx.Equal(1, LF_Bank.GetStock(storeId), "one kit left in stock");

		array<string> serials = {};
		listener.m_sSerials.Split(",", serials, true);
		ctx.Equal(2, serials.Count(), "both weapons carry a serial");
		foreach (string serial : serials)
		{
			LF_FenceSerialRow row = LF_FenceSerials.Find(serial);
			ctx.NotNull(row, "the serial is in the book");
			if (row)
				ctx.EqualStr(uid, row.m_sBuyerUid, "the serial names the thief");
		}

		ctx.False(LF_Bank.Rob(storeId, uid, robber), "an open store pays nothing again");
		ctx.Equal(1, listener.m_iCount, "still one loot event");

		LF_Breach.Reset(storeId);
		ctx.False(LF_Bank.Rob(storeId, uid, robber), "repaired but inside the loot cooldown: refused");
		LF_Bank.Advance(storeId, 601);
		ctx.True(LF_Bank.Rob(storeId, uid, robber), "after the cooldown it pays again");
		ctx.Equal(2, listener.m_iCount, "a second loot event");
		ctx.Equal(0, LF_Bank.GetStock(storeId), "the stock is spent");

		LF_Breach.Reset(storeId);
		LF_Bank.Advance(storeId, 601);
		ctx.False(LF_Bank.Rob(storeId, uid, robber), "out of stock: refused");
		LF_Bank.Advance(storeId, 1800);
		ctx.True(LF_Bank.GetStock(storeId) >= 1, "the refill brought a kit back");
		ctx.True(LF_Bank.Rob(storeId, uid, robber), "restocked: it pays again");
		ctx.Equal(3, listener.m_iCount, "a third loot event");

		LF_EventBus.Unsubscribe(LF_OnHeistLoot, listener);

		// Leave the world as found: the weapons lying at the store and the robber with its pack and
		// keycard are removed (debugworld/entity-collision scans the world after this test).
		LF_Bank.Restock(storeId);
		LF_TestCharacterFixture.Delete(robber);
	}
}
