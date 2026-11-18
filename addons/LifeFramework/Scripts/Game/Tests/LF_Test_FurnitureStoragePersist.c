// red-proof: observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_FurnitureStoragePersist` (batched run, RED PROOF OK): RED-OK (RedProof() skips the load's storage restore, so "the load restores the three items once" fails). Green otherwise.
//! housing/furniture-storage-save (PERSISTENCE): the contents of a placed wardrobe persist with the placement.
//! Seeded with 151 cash, an apple and a water bottle, captured, then the wardrobe is deleted and the saved
//! placement rows and storage snapshots are applied again (the load). The wardrobe stands again with the same
//! three items, a second load adds nothing, an empty one is picked up and its record goes. The restart path
//! itself (a real save and boot) is the manual step in the PR.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_FurnitureStoragePersist : LF_Test
{
	protected static const ResourceName WARDROBE = "{7416F34B901F7B83}Prefabs/Placeables/Furniture/Furniture_Wardrobe_Placed.et";
	protected static const ResourceName MONEY = "{5439738849229352}Prefabs/Items/Currencies/MoneyStack.et";
	protected static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	protected static const ResourceName WATER = "{A231A5F8D479B5DC}Prefabs/Items/Drinks/WaterBottle.et";
	protected static const string OWNER = "furniture-storage-persist-owner";
	protected static const vector SPOT = "100 0 90"; // inside the test annex (x/z 18..114), never on the town

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "housing/furniture-storage-save";
	}

	//------------------------------------------------------------------------------------------------
	protected static int ItemCount(IEntity placed)
	{
		BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(placed.FindComponent(BaseInventoryStorageComponent));
		if (!storage)
			return -1;

		array<IEntity> items = {};
		storage.GetAll(items, false);
		return items.Count();
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_StorageSnapshotRecord> snapshotsBefore = LF_StorageSnapshot.ExportAll();
		vector at = SPOT;
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		IEntity placed = LF_Utils.SpawnEntityPrefab(WARDROBE, at);
		ctx.True(placed != null, "the wardrobe spawns");
		if (!placed)
			return;

		string id = LF_PlacementSave.Record(OWNER, "Furniture_Wardrobe", placed);
		ctx.False(id.IsEmpty(), "the wardrobe is recorded as a permanent placement");
		string key = LF_PlacedStorage.KeyFor(id);
		LF_PersistentStorageComponent persistent = LF_PlacedStorage.ComponentOf(placed);
		ctx.True(persistent != null && LF_PersistentStorageComponent.Resolve(key) == placed, "the wardrobe registered under its placement key");
		if (!persistent || id.IsEmpty())
		{
			LF_TestCharacterFixture.Delete(placed);
			return;
		}

		ctx.Equal(151, LF_InventoryUtils.AddAmount(placed, MONEY, 151), "151 cash goes into the wardrobe");
		ctx.Equal(1, LF_InventoryUtils.AddAmount(placed, APPLE, 1), "an apple goes into the wardrobe");
		ctx.Equal(1, LF_InventoryUtils.AddAmount(placed, WATER, 1), "a water bottle goes into the wardrobe");
		ctx.Equal(3, ItemCount(placed), "the wardrobe holds three items");
		ctx.True(LF_PersistentStorageComponent.CaptureAllPlaced() > 0, "the save point captures the placed storages");
		LF_StorageSnapshotRecord record = LF_StorageSnapshot.Get(key, LF_PersistentStorageComponent.SLOT_PLACED);
		ctx.NotNull(record, "a snapshot is stored under furniture:<placement id>");
		if (record)
			ctx.Equal(3, record.m_aItems.Count(), "the snapshot lists the three items");

		array<ref LF_PlacementRow> rows = LF_PlacementSave.ExportAll();
		array<ref LF_StorageSnapshotRecord> saved = LF_StorageSnapshot.ExportAll();

		// The restart: the wardrobe is gone, the saved rows and snapshots are applied (the placement load spawns it).
		LF_TestCharacterFixture.Delete(placed);
		LF_StorageSnapshot.ApplyAll(saved);
		ctx.Equal(1, LF_PlacementSave.ApplyAll(rows), "the load stands the wardrobe again");
		IEntity again = LF_PlacementSave.LiveEntity(id);
		ctx.True(again != null && again != placed, "a new wardrobe entity stands for the placement");
		if (again)
		{
			ctx.True(LF_PersistentStorageComponent.Resolve(key) == again, "the new wardrobe registered under the same key");
			int spawned;
			if (!RedProof())
				spawned = LF_PersistentStorageComponent.RestoreAllPlaced();

			ctx.Equal(3, spawned, "the load restores the three items once");
			ctx.Equal(3, ItemCount(again), "three items are back in the wardrobe");
			ctx.Equal(0, LF_PersistentStorageComponent.RestoreAllPlaced(), "a second restore spawns nothing");
			ctx.Equal(3, ItemCount(again), "still three items after the second restore");

			// Emptied, it is picked up and its record goes with it.
			LF_StorageSnapshot.Apply(again, LF_StorageSnapshotRecord.Create("furniture-storage-test-empty", "empty"));
			ctx.Equal(0, ItemCount(again), "the wardrobe is empty");
			IEntity owner = LF_TestCharacterFixture.Spawn(ctx);
			if (owner)
			{
				LF_Houses.BindCharacter(owner, OWNER);
				owner.SetOrigin(again.GetOrigin() + "1.5 0 0");
				ctx.True(LF_PlacementPickup.PickUp(owner, again), "the empty wardrobe is picked up");
				ctx.False(LF_StorageSnapshot.Has(key, LF_PersistentStorageComponent.SLOT_PLACED), "its storage record goes with it");
				ctx.True(LF_PlacementSave.Find(id) == null, "its placement row goes with it");
			}

			LF_TestCharacterFixture.Delete(owner);
		}

		if (LF_PlacementSave.Find(id))
			LF_PlacementSave.Forget(id);

		LF_StorageSnapshot.Clear(key, LF_PersistentStorageComponent.SLOT_PLACED);
		LF_TestCharacterFixture.Delete(LF_PlacementSave.LiveEntity(id));
		LF_StorageSnapshot.ApplyAll(snapshotsBefore);
	}
}
