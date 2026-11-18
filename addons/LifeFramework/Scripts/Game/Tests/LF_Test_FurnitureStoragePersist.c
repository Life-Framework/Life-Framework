// red-proof: observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_FurnitureStoragePersist` (batched run, RED PROOF OK): RED-OK (RedProof() skips the load's storage restore, so "the load restores the three items once" fails). Green otherwise.
//! housing/furniture-storage-save (PERSISTENCE): the contents of a placed wardrobe persist with the placement.
//! Seeded with 151 cash, an apple and a water bottle, captured, then the wardrobe is deleted and the saved
//! placement rows and storage snapshots are applied again (the load). The wardrobe stands again with the same
//! three items, a second load adds nothing, an empty one is picked up and its record goes. The restart path
//! itself is `tools\cli wt e2e` (SeedE2E / VerifyE2E): one chest, its two apples once, nothing loose.
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

		// Only this wardrobe's row: a loaded save (the e2e load boot) can hold other placements.
		array<ref LF_PlacementRow> rows = {};
		foreach (LF_PlacementRow exported : LF_PlacementSave.ExportAll())
		{
			if (exported && exported.m_sPlacementId == id)
				rows.Insert(exported);
		}

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

	//------------------------------------------------------------------------------------------------
	//! The e2e chest (LF_Test_E2E_SaveState pass 1 places it with two apples, pass 2 checks it after a real restart).
	protected static const ResourceName E2E_CHEST = "{BAFB1BD4358EDEFD}Prefabs/Placeables/Furniture/Furniture_Chest_Placed.et";
	protected static const string E2E_OWNER = "e2e-furniture-storage-owner";
	protected static const vector E2E_SPOT = "106 0 96"; // test annex

	//------------------------------------------------------------------------------------------------
	protected static string E2EPlacementId()
	{
		foreach (LF_PlacementRow row : LF_PlacementSave.ExportAll())
		{
			if (row && row.m_sOwnerKey == E2E_OWNER)
				return row.m_sPlacementId;
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e save boot: a placed chest holds two apples through the shutdown save.
	static void SeedE2E(LF_TestContext ctx)
	{
		vector at = E2E_SPOT;
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		IEntity placed = LF_Utils.SpawnEntityPrefab(E2E_CHEST, at);
		ctx.NotNull(placed, "e2e the furniture chest spawns");
		if (!placed)
			return;

		string id = LF_PlacementSave.Record(E2E_OWNER, "Furniture_Chest", placed);
		ctx.False(id.IsEmpty(), "e2e the chest is a permanent placement");
		ctx.Equal(2, LF_InventoryUtils.AddAmount(placed, APPLE, 2), "e2e two apples go into the chest");
		LF_TestDebrisProbe.Keep(placed);
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e reload boot: one chest stands for the placement and holds exactly the two apples; the engine's
	//! own save brought back no second chest and no loose or second copy of the apples.
	static void VerifyE2E(LF_TestContext ctx)
	{
		string id = E2EPlacementId();
		if (!LF_E2E.RequiresSeed("furniture chest (#2916)", !id.IsEmpty()))
			return;

		vector at = E2E_SPOT;
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		IEntity chest = LF_PlacementSave.LiveEntity(id);
		ctx.NotNull(chest, "e2e the furniture chest stands again after the restart");
		ctx.Equal(1, LF_Test_LockerContentsPersist.CountNear(at, 5, E2E_CHEST, false), "e2e exactly one chest stands: the engine did not restore a second one");
		ctx.Equal(0, LF_Test_LockerContentsPersist.CountNear(at, 5, APPLE, true), "e2e no apple from the chest came back loose");
		if (chest)
		{
			LF_PersistentStorageComponent.RestoreAllPlaced();
			ctx.Equal(2, LF_InventoryUtils.GetAmount(chest, APPLE), "e2e the chest holds exactly its two apples after the restart, none doubled");
		}

		LF_StorageSnapshot.Clear(LF_PlacedStorage.KeyFor(id), LF_PersistentStorageComponent.SLOT_PLACED);
		LF_PlacementSave.Forget(id);
		LF_TestCharacterFixture.Delete(chest);
	}
}
