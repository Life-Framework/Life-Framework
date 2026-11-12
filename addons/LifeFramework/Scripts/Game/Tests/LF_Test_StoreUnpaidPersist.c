// red-proof: observed 2026-11-11 with `tools\cli test --no-build --only LF_Test_StoreUnpaidPersist`: LF_Stores.ExportUnpaid not writing the live tags failed
// `- both tags wait for the shopper (expected 2, got 0)` and `- the shopper owes the same after the restart (expected 16, got 0)`; restored, green.
// red-proof: observed 2026-11-11, same loop: a restored tag that never leaves its grace (the reviewer's logout exploit) failed
// `- after the grace, carrying the restored goods outside is shoplifting (expected 1, got 0)`; restored, green.

// tier: PERSISTENCE
//! Unpaid store tags ride the real save path (the "storeunpaid" payload, save version 25, #479): a
//! shopper with one unpaid apple in the pocket and one eaten in the shop is saved, the session forgets
//! the tags and the body, and the save loads. The tags wait under the shopper's character key; the
//! new body with its carried inventory back takes both (the pocket apple tagged again, the eaten one
//! still owed). A body that spawns away from the store is not a thief while the store's grace runs;
//! once it has been inside again, or the grace ran out, walking around outside with the goods is.
//! A second load binds the same tags, never doubled. A tag whose item or store is gone is dropped.
//! A version 24 save leaves the waiting tags alone. The test puts back what it found.
class LF_Test_StoreUnpaidPersist : LF_Test
{
	protected static const string SHOPPER_KEY = "store-unpaid-shopper";

	override string GetName()
	{
		return "persist/store-unpaid";
	}

	override void Run(LF_TestContext ctx)
	{
		// An older-format save loaded here replaces the organizations with record version 1 ones,
		// which drops rank masks only "orgranks" holds (#750a); put back what this test found.
		LF_TestLegacySnapshot keptOrgs = LF_TestLegacySnapshot.Take();
		RunKeptOrgs(ctx);
		keptOrgs.Restore();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunKeptOrgs(LF_TestContext ctx)
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

		// What the session holds now goes back at the end.
		store.FirstFill();
		ResourceName apple = LF_Test_StoreShelf.APPLE;
		string storeKey = store.GetStoreKey();
		int original = store.GetStock(apple);
		array<ref LF_StoreStockRow> stockBefore = LF_Stores.ExportAll();
		array<ref LF_StoreUnpaidItem> liveBefore = {};
		foreach (LF_StoreUnpaidItem live : LF_Stores.Unpaid())
			liveBefore.Insert(live);

		array<ref LF_StoreUnpaidRow> parkedBefore = LF_Stores.CopyParked();
		// The e2e save boot seeds wanted reports before this runs; the shoplift below must not lose them.
		array<ref LF_WantedReportRecord> reportsBefore = LF_WantedReports.ExportAll();
		LF_Stores.ClearUnpaid();
		LF_Stores.SetParked(null);
		store.ForceStock(apple, 6);

		vector inside = counter.GetOrigin() + "0 0.2 1.2";
		vector outside = counter.GetOrigin() + "0 0.2 40";
		IEntity shopper = LF_TestCharacterFixture.Spawn(ctx);
		IEntity back;
		if (shopper)
		{
			shopper.SetOrigin(inside);
			back = RunRoundTrip(ctx, persistence, store, shopper, inside, outside);
		}

		// Put the world back.
		LF_Dispatch.StopAllAlarms();
		LF_WantedReports.ApplyAll(reportsBefore);
		LF_TestCharacterFixture.Delete(back);
		LF_Stores.ClearUnpaid();
		foreach (LF_StoreUnpaidItem restore : liveBefore)
			LF_Stores.Unpaid().Insert(restore);

		LF_Stores.SetParked(parkedBefore);
		LF_Stores.ApplyAll(stockBefore);
		store.ForceStock(apple, original);
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes the shopper (the restart). \return The body that came back after the load (the caller deletes it).
	protected IEntity RunRoundTrip(LF_TestContext ctx, LF_PersistenceComponent persistence, LF_StoreComponent store, IEntity shopper, vector inside, vector outside)
	{
		ResourceName apple = LF_Test_StoreShelf.APPLE;
		string storeKey = store.GetStoreKey();
		InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(shopper);
		IEntity kept = store.GetShelfItem(apple, 0);
		IEntity eaten = store.GetShelfItem(apple, 1);
		ctx.True(storage != null && kept != null && eaten != null, "the shopper and two shelf apples resolve");
		if (!storage || !kept || !eaten)
		{
			LF_TestCharacterFixture.Delete(shopper);
			return null;
		}

		ctx.True(storage.TryInsertItem(kept), "the shopper pockets an apple");
		ctx.True(storage.TryInsertItem(eaten), "the shopper takes a second apple");
		store.DetectTaken();
		LF_Stores.RefreshAll();
		SCR_EntityHelper.DeleteEntityAndChildren(eaten);
		LF_Stores.RefreshAll();
		ctx.Equal(2, LF_Stores.UnpaidCount(), "one apple in the pocket and one eaten are unpaid");
		int owed = LF_Stores.OwedBy(storeKey, shopper);
		ctx.True(owed > 0, "the shopper owes for both apples");

		// The fixture body has no player; a player's tag carries its character key the same way.
		array<LF_StoreUnpaidItem> held = {};
		LF_Stores.CollectFor(storeKey, shopper, held);
		foreach (LF_StoreUnpaidItem record : held)
			record.m_sCarrierKey = SHOPPER_KEY;

		LF_StorageSnapshotRecord carried = LF_StorageSnapshot.Capture(SHOPPER_KEY, LF_StorageSnapshot.SLOT_CARRIED, shopper);
		ctx.NotNull(carried, "the shopper's carried inventory is captured");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(SHOPPER_KEY), "the save carries the shopper's key");

		// A restart: the session has no tags and no shopper.
		LF_Stores.ClearUnpaid();
		LF_Stores.SetParked(null);
		LF_TestCharacterFixture.Delete(shopper);

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		int version;
		loadContext.ReadValue("version", version);
		ctx.Equal(28, version, "the save is written at version 28 (storeunpaid is payload 25)");
		JsonLoadContext load = new JsonLoadContext(false);
		load.LoadFromString(saved);
		ctx.True(LF_PersistenceComponentSerializer.DeserializeStore(persistence, load), "the save loads");
		ctx.Equal(2, LF_Stores.ParkedCount(), "both tags wait for the shopper");
		ctx.Equal(0, LF_Stores.UnpaidCount(), "no item is tagged before the shopper is back");

		// The shopper comes back: a new body at a spawn point, the carried inventory applied.
		IEntity back = LF_TestCharacterFixture.Spawn(ctx);
		if (!back || !carried)
			return back;

		LF_StorageSnapshot.Apply(back, carried);
		ctx.Equal(2, LF_Stores.ReattachUnpaid(SHOPPER_KEY, back), "both tags go back on the shopper");
		ctx.Equal(0, LF_Stores.ParkedCount(), "nothing waits any more");
		ctx.Equal(owed, LF_Stores.OwedBy(storeKey, back), "the shopper owes the same after the restart");
		array<IEntity> pocket = LF_InventoryUtils.FindItemsByPrefab(LF_InventoryUtils.GetResponsibleStorageManager(back), apple);
		ctx.Equal(1, pocket.Count(), "the pocket apple is back");
		if (!pocket.IsEmpty())
			ctx.True(LF_Stores.HoldsApart(pocket[0]), "the pocket apple is tagged unpaid again");

		// The spawn point is not the store: no theft until the shopper has been inside again.
		back.SetOrigin(outside);
		ctx.Equal(0, LF_Stores.SweepExits(), "spawning away from the store is not shoplifting");
		ctx.Equal(2, LF_Stores.UnpaidCount(), "the restored tags stay");

		// A second load on the live session binds the same tags, never doubled.
		JsonLoadContext again = new JsonLoadContext(false);
		again.LoadFromString(saved);
		LF_PersistenceComponentSerializer.DeserializeStore(persistence, again);
		LF_Stores.ReattachUnpaid(SHOPPER_KEY, back);
		ctx.Equal(2, LF_Stores.UnpaidCount(), "a second load leaves two tags");
		ctx.Equal(owed, LF_Stores.OwedBy(storeKey, back), "and the same bill");

		// Back in the store and out again: now it is shoplifting.
		back.SetOrigin(inside);
		ctx.Equal(0, LF_Stores.SweepExits(), "inside the store with restored tags is browsing");
		back.SetOrigin(outside);
		ctx.Equal(1, LF_Stores.SweepExits(), "walking out with the restored tags is shoplifting");
		ctx.Equal(0, LF_Stores.UnpaidCount(), "the tags are closed");

		// Logging out inside the store is no way out: once the grace runs out, the spawn point is the street.
		JsonLoadContext third = new JsonLoadContext(false);
		third.LoadFromString(saved);
		LF_PersistenceComponentSerializer.DeserializeStore(persistence, third);
		ctx.Equal(2, LF_Stores.ReattachUnpaid(SHOPPER_KEY, back), "the saved tags go back on the shopper again");
		ctx.Equal(0, LF_Stores.SweepExits(), "within the grace the shopper may still come back and pay");
		ctx.True(store.GetRestoredGraceSeconds() > 0, "the store gives a restored shopper time to come back");
		foreach (LF_StoreUnpaidItem restored : LF_Stores.Unpaid())
			restored.m_fRestoredUntilMs = 0;

		ctx.Equal(1, LF_Stores.SweepExits(), "after the grace, carrying the restored goods outside is shoplifting");
		ctx.Equal(0, LF_Stores.UnpaidCount(), "and those tags are closed too");

		// A tag whose item is gone or whose store left the world is dropped, nothing else breaks.
		array<ref LF_StoreUnpaidRow> odd = {};
		odd.Insert(Row(storeKey, "{9FEBBF75B1D525EE}Prefabs/Items/Food/Cabbage.et", false));
		odd.Insert(Row("ghost_store_not_in_world", apple, true));
		LF_Stores.SetParked(odd);
		ctx.Equal(0, LF_Stores.ReattachUnpaid(SHOPPER_KEY, back), "a tag with no item or no store binds nothing");
		ctx.Equal(0, LF_Stores.ParkedCount(), "and does not wait forever");

		// A version 24 save has no tags: loading it leaves the waiting ones alone.
		array<ref LF_StoreUnpaidRow> waiting = {};
		waiting.Insert(Row(storeKey, apple, true));
		LF_Stores.SetParked(waiting);
		JsonSaveContext oldContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStoreAsVersion(persistence, oldContext, 24);
		JsonLoadContext oldLoad = new JsonLoadContext(false);
		oldLoad.LoadFromString(oldContext.SaveToString());
		LF_PersistenceComponentSerializer.DeserializeStore(persistence, oldLoad);
		ctx.Equal(1, LF_Stores.ParkedCount(), "a version 24 save leaves the waiting tag alone");
		return back;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_StoreUnpaidRow Row(string storeKey, string prefab, bool consumed)
	{
		LF_StoreUnpaidRow row = new LF_StoreUnpaidRow();
		row.m_sStoreKey = storeKey;
		row.m_sPrefab = prefab;
		row.m_iPrice = 5;
		row.m_sCarrierKey = SHOPPER_KEY;
		row.m_bConsumed = consumed;
		return row;
	}
}
