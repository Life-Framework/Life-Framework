// red-proof: Observed 2026-11-16 with `tools\cli test --no-build --red LF_Test_CarriedStolenPersist,LF_Test_DebugMoneyTopUpHold`: RED-OK persist/carried-stolen, `- the restored drill is still stolen` failed with the Mark skipped. Green with `--only`.
// tier: PERSISTENCE
//! #1855: a carried item flagged stolen (LF_StolenGoods) stays stolen through the real save path (the
//! "itemstolen" payload, save version 35): captured from the live item, written, wiped, read back,
//! applied onto a fresh character. Before, the flag was an EntityID in a session set and a restart
//! laundered the item. A row that no longer matches its snapshot is skipped.
class LF_Test_CarriedStolenPersist : LF_Test
{
	protected static const string CHARACTER_ID = "carried-stolen-character";

	override string GetName()
	{
		return "persist/carried-stolen";
	}

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
		if (!source)
			return;

		ctx.Equal(1, LF_InventoryUtils.AddAmount(source, LF_Bank.DRILL_PREFAB, 1), "a drill is carried");
		InventoryStorageManagerComponent manager = LF_InventoryUtils.GetResponsibleStorageManager(source);
		array<IEntity> found = LF_InventoryUtils.FindItemsByPrefab(manager, LF_Bank.DRILL_PREFAB);
		ctx.Equal(1, found.Count(), "the drill is found");
		if (found.IsEmpty())
		{
			LF_TestCharacterFixture.Delete(source);
			return;
		}

		if (!RedProof())
			LF_StolenGoods.Mark(found[0]);

		ctx.True(LF_StolenGoods.IsStolen(found[0]), "the drill is flagged stolen");
		ctx.True(LF_StorageSnapshot.CaptureCarried(CHARACTER_ID, source), "the character is captured");
		LF_TestCharacterFixture.Delete(source);

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();

		LF_StorageSnapshot.ApplyAll(before);
		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		int version;
		loadContext.ReadValue("version", version);
		ctx.Equal(LF_TestSaveVersion.CURRENT, version, "the save is written at the current version (itemstolen is payload 35)");
		ctx.Equal(35, LF_PersistenceComponentSerializer.PayloadSinceVersion("itemstolen"), "itemstolen is the 35th payload");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "carried");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "itemstolen");

		LF_StorageSnapshotRecord loaded = LF_StorageSnapshot.Get(CHARACTER_ID, LF_StorageSnapshot.SLOT_CARRIED);
		ctx.NotNull(loaded, "the snapshot is back");
		if (loaded)
		{
			IEntity restored = LF_TestCharacterFixture.Spawn(ctx);
			if (restored)
			{
				LF_StorageSnapshot.Apply(restored, loaded);
				InventoryStorageManagerComponent restoredManager = LF_InventoryUtils.GetResponsibleStorageManager(restored);
				array<IEntity> drills = LF_InventoryUtils.FindItemsByPrefab(restoredManager, LF_Bank.DRILL_PREFAB);
				ctx.Equal(1, drills.Count(), "the drill is back");
				if (!drills.IsEmpty())
					ctx.True(LF_StolenGoods.IsStolen(drills[0]), "the restored drill is still stolen");

				LF_TestCharacterFixture.Delete(restored);
			}
		}

		// A row whose snapshot no longer matches is skipped, not applied to the wrong item.
		array<ref LF_StorageItemStolenRecord> stale = {};
		LF_StorageItemStolenRecord row = new LF_StorageItemStolenRecord();
		row.m_sOwnerKey = CHARACTER_ID;
		row.m_sSlot = LF_StorageSnapshot.SLOT_CARRIED;
		row.m_iItemCount = 999;
		row.m_aStolen.Insert(0);
		stale.Insert(row);
		ctx.Equal(0, LF_StorageSnapshot.ApplyItemStolen(stale), "a stale row matches nothing");

		LF_StorageSnapshot.ApplyAll(before);
	}
}
