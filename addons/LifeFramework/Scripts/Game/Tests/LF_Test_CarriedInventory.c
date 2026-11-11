// red-proof: persist/carried-roundtrip, observed 2026-11-08 with `tools\cli test --no-build --tier all`:
// commenting out SetQuantity in LF_StorageSnapshot.SpawnInto failed it with
// `[LFTEST] FAIL persist/carried-roundtrip`, `- 237 cash after the apply (expected 237, got 100)`,
// `- 3 apples after the apply (...) (expected 3, got 1)`. Restored; green.
// red-proof: persist/carried-death-no-dupe, observed 2026-11-08 with `tools\cli test --no-build --tier all`:
// removing the IsAlive guard in LF_StorageSnapshot.CaptureCarried failed it with
// `[LFTEST] FAIL persist/carried-death-no-dupe`, `- a dead body is not captured`,
// `- no snapshot exists for the dead character`. Restored; green.

//------------------------------------------------------------------------------------------------
//! Shared helpers for the carried-inventory tests.
class LF_TestCarriedInventoryKit
{
	static const ResourceName MONEY_PREFAB = "{5439738849229352}Prefabs/Items/Currencies/MoneyStack.et";
	static const ResourceName APPLE_PREFAB = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	static const ResourceName BACKPACK_PREFAB = "{06B68C58B72EAAC6}Prefabs/Items/Equipment/Backpacks/Backpack_ALICE_Medium.et";
	static const ResourceName WATER_PREFAB = "{A231A5F8D479B5DC}Prefabs/Items/Drinks/WaterBottle.et";
	static const ResourceName MISSING_PREFAB = "{0000000000000002}Prefabs/Items/Food/Missing.et";

	//------------------------------------------------------------------------------------------------
	//! Fixture character (jacket) plus a backpack holding one water bottle, 237 cash and 3 apples.
	static IEntity SpawnLoaded(LF_TestContext ctx)
	{
		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return null;

		InventoryStorageManagerComponent manager = LF_InventoryUtils.GetResponsibleStorageManager(character);
		IEntity backpack = LF_Utils.SpawnEntityPrefab(BACKPACK_PREFAB, character.GetOrigin());
		ctx.True(backpack != null, "the backpack spawns");
		if (!backpack)
			return character;

		ctx.True(manager.TryInsertItem(backpack, EStoragePurpose.PURPOSE_LOADOUT_PROXY), "the backpack is worn");
		BaseInventoryStorageComponent backpackStorage = BaseInventoryStorageComponent.Cast(backpack.FindComponent(BaseInventoryStorageComponent));
		IEntity water = LF_Utils.SpawnEntityPrefab(WATER_PREFAB, character.GetOrigin());
		ctx.True(water != null && backpackStorage != null, "the water bottle and the backpack storage resolve");
		if (water && backpackStorage)
			ctx.True(manager.TryInsertItemInStorage(water, backpackStorage), "the water bottle goes inside the backpack");

		ctx.Equal(237, LF_MoneyUtils.AddCash(character, 237), "237 cash added");
		ctx.Equal(3, LF_InventoryUtils.AddAmount(character, APPLE_PREFAB, 3), "3 apples added");
		return character;
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many items of the prefab sit directly inside a storage of the first carried
	//!         entity of containerPrefab.
	static int CountInside(IEntity character, ResourceName containerPrefab, ResourceName prefab)
	{
		InventoryStorageManagerComponent manager = LF_InventoryUtils.GetResponsibleStorageManager(character);
		if (!manager)
			return -1;

		array<IEntity> containers = LF_InventoryUtils.FindItemsByPrefab(manager, containerPrefab);
		if (containers.IsEmpty())
		{
			array<IEntity> all = {};
			manager.GetItems(all, EStoragePurpose.PURPOSE_ANY);
			foreach (IEntity candidate : all)
			{
				if (LF_Utils.GetPrefabName(candidate) == containerPrefab)
					containers.Insert(candidate);
			}
		}

		if (containers.IsEmpty())
			return -1;

		array<Managed> storages = {};
		containers[0].FindComponents(BaseInventoryStorageComponent, storages);
		int count;
		foreach (Managed component : storages)
		{
			BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(component);
			if (!storage)
				continue;

			array<IEntity> items = {};
			storage.GetAll(items);
			foreach (IEntity item : items)
			{
				if (LF_Utils.GetPrefabName(item) == prefab)
					count++;
			}
		}

		return count;
	}
}

//------------------------------------------------------------------------------------------------
// tier: PERSISTENCE
class LF_Test_CarriedInventoryRoundtrip : LF_Test
{
	protected static const string CHARACTER_ID = "carried-roundtrip-character";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "persist/carried-roundtrip";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		ctx.NotNull(gameMode, "the DebugWorld game mode resolves");
		if (!gameMode)
			return;

		LF_PersistenceComponent persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));
		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

		array<ref LF_StorageSnapshotRecord> before = LF_StorageSnapshot.ExportAll();

		IEntity source = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!source || ctx.FailureCount() > 0)
		{
			LF_TestCharacterFixture.Delete(source);
			LF_StorageSnapshot.ApplyAll(before);
			return;
		}

		// Export.
		ctx.True(LF_StorageSnapshot.CaptureCarried(CHARACTER_ID, source), "the living character is captured");
		LF_StorageSnapshotRecord captured = LF_StorageSnapshot.Get(CHARACTER_ID, LF_StorageSnapshot.SLOT_CARRIED);
		ctx.NotNull(captured, "the carried snapshot is stored under the character id");
		if (captured)
		{
			ctx.Equal(237, captured.CountPrefab(LF_TestCarriedInventoryKit.MONEY_PREFAB), "the snapshot holds 237 cash");
			ctx.Equal(3, captured.CountPrefab(LF_TestCarriedInventoryKit.APPLE_PREFAB), "the snapshot holds 3 apples");
			ctx.Equal(1, captured.CountPrefab(LF_TestCarriedInventoryKit.WATER_PREFAB), "the snapshot holds the nested water bottle");
		}
		LF_TestCharacterFixture.Delete(source);

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(CHARACTER_ID), "the save carries the carried payload");

		// Wipe: the restarted session has no snapshot.
		LF_StorageSnapshot.ApplyAll(before);
		ctx.False(LF_StorageSnapshot.Has(CHARACTER_ID, LF_StorageSnapshot.SLOT_CARRIED), "the wiped session has no snapshot");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		int version;
		loadContext.ReadValue("version", version);
		ctx.Equal(24, version, "the save is written at version 24");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "carried");
		LF_StorageSnapshotRecord loaded = LF_StorageSnapshot.Get(CHARACTER_ID, LF_StorageSnapshot.SLOT_CARRIED);
		ctx.NotNull(loaded, "the snapshot is back after the load");
		if (!loaded)
		{
			LF_StorageSnapshot.ApplyAll(before);
			return;
		}

		// A prefab that no longer exists is skipped with everything inside it.
		loaded.m_aItems.Insert(LF_StorageItemRecord.Create(LF_TestCarriedInventoryKit.MISSING_PREFAB, 1, -1, 0, 0));
		loaded.m_aItems.Insert(LF_StorageItemRecord.Create(LF_TestCarriedInventoryKit.APPLE_PREFAB, 5, loaded.m_aItems.Count() - 1, 0, 0));

		// Apply onto a fresh character that already wears the fixture jacket.
		IEntity restored = LF_TestCharacterFixture.Spawn(ctx);
		if (!restored)
		{
			LF_StorageSnapshot.ApplyAll(before);
			return;
		}

		ctx.True(LF_StorageSnapshot.Apply(restored, loaded) > 0, "the first apply spawns the missing items");
		ctx.Equal(237, LF_MoneyUtils.GetCash(restored), "237 cash after the apply");
		ctx.Equal(3, LF_InventoryUtils.GetAmount(restored, LF_TestCarriedInventoryKit.APPLE_PREFAB), "3 apples after the apply (the missing prefab's contents are skipped)");
		ctx.Equal(1, LF_TestCarriedInventoryKit.CountInside(restored, LF_TestCarriedInventoryKit.BACKPACK_PREFAB, LF_TestCarriedInventoryKit.WATER_PREFAB), "the water bottle is back inside the backpack");

		// Apply twice leaves the same count.
		ctx.Equal(0, LF_StorageSnapshot.Apply(restored, loaded), "a second apply spawns nothing");
		ctx.Equal(237, LF_MoneyUtils.GetCash(restored), "237 cash after the second apply");
		ctx.Equal(3, LF_InventoryUtils.GetAmount(restored, LF_TestCarriedInventoryKit.APPLE_PREFAB), "3 apples after the second apply");
		ctx.Equal(1, LF_TestCarriedInventoryKit.CountInside(restored, LF_TestCarriedInventoryKit.BACKPACK_PREFAB, LF_TestCarriedInventoryKit.WATER_PREFAB), "one water bottle after the second apply");

		LF_TestCharacterFixture.Delete(restored);
		LF_StorageSnapshot.ApplyAll(before);
	}
}

//------------------------------------------------------------------------------------------------
// tier: WORLD
class LF_Test_CarriedInventoryDeathNoDupe : LF_Test
{
	protected static const string CHARACTER_ID = "carried-death-character";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "persist/carried-death-no-dupe";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_StorageSnapshotRecord> before = LF_StorageSnapshot.ExportAll();

		IEntity character = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!character || ctx.FailureCount() > 0)
		{
			LF_TestCharacterFixture.Delete(character);
			LF_StorageSnapshot.ApplyAll(before);
			return;
		}

		ctx.True(LF_StorageSnapshot.CaptureCarried(CHARACTER_ID, character), "the living character is captured (autosave)");
		ctx.True(LF_StorageSnapshot.Has(CHARACTER_ID, LF_StorageSnapshot.SLOT_CARRIED), "the snapshot exists before the death");

		// The death hook (LF_SpawnLogic.OnPlayerKilled_S) drops the account's snapshot.
		LF_StorageSnapshot.OnCharacterDied(CHARACTER_ID);
		ctx.False(LF_StorageSnapshot.Has(CHARACTER_ID, LF_StorageSnapshot.SLOT_CARRIED), "death drops the carried snapshot");

		// A save point between death and respawn must not capture the body either.
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(character.FindComponent(SCR_CharacterDamageManagerComponent));
		ctx.NotNull(damage, "the character has a damage manager to die by");
		if (damage)
			damage.Kill(Instigator.CreateInstigator(null));

		ctx.False(LF_StorageSnapshot.IsAlive(character), "the killed character reads dead");
		ctx.False(LF_StorageSnapshot.CaptureCarried(CHARACTER_ID, character), "a dead body is not captured");
		ctx.False(LF_StorageSnapshot.Has(CHARACTER_ID, LF_StorageSnapshot.SLOT_CARRIED), "no snapshot exists for the dead character");

		bool inExport;
		foreach (LF_StorageSnapshotRecord record : LF_StorageSnapshot.ExportAll())
		{
			if (record && record.m_sOwnerKey == CHARACTER_ID)
				inExport = true;
		}
		ctx.False(inExport, "the save payload carries nothing for the dead character, so a restart grants the starter kit");

		// The body keeps what it carried.
		ctx.Equal(237, LF_MoneyUtils.GetCash(character), "the body keeps its 237 cash");

		LF_TestCharacterFixture.Delete(character);
		LF_StorageSnapshot.ApplyAll(before);
	}
}
