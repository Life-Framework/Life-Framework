// red-proof: Observed 2026-11-10 with `--only LF_Test_CarriedSerialPersist`: skipping RestoreSerial in LF_StorageSnapshot.SetSerial failed `- the restored drill keeps its serial (expected 'LF-4242', got '')`. Restored; green.
// tier: PERSISTENCE
//! A stamped fence tool carried by a character keeps its serial through the real save path (the
//! "itemserials" payload, save version 22, #470): captured from the live item, written, wiped, read
//! back, applied onto a fresh character. A row that no longer matches its snapshot is skipped.
class LF_Test_CarriedSerialPersist : LF_Test
{
	protected static const string CHARACTER_ID = "carried-serial-character";
	protected static const string SERIAL = "LF-4242";

	override string GetName()
	{
		return "persist/carried-serial";
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
		LF_SerialComponent stamp;
		if (!found.IsEmpty())
			stamp = LF_SerialComponent.Cast(found[0].FindComponent(LF_SerialComponent));

		ctx.NotNull(stamp, "the drill carries a serial component");
		if (!stamp)
		{
			LF_TestCharacterFixture.Delete(source);
			return;
		}

		ctx.True(stamp.SetSerial(SERIAL), "the drill is stamped");
		ctx.True(LF_StorageSnapshot.CaptureCarried(CHARACTER_ID, source), "the character is captured");
		LF_TestCharacterFixture.Delete(source);

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(SERIAL), "the save carries the serial");

		LF_StorageSnapshot.ApplyAll(before);
		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		int version;
		loadContext.ReadValue("version", version);
		ctx.Equal(27, version, "the save is written at version 27 (itemserials is payload 22)");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "carried");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "itemstate");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "itemserials");

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
				{
					LF_SerialComponent back = LF_SerialComponent.Cast(drills[0].FindComponent(LF_SerialComponent));
					ctx.NotNull(back, "the restored drill has a serial component");
					if (back)
						ctx.EqualStr(SERIAL, back.GetSerial(), "the restored drill keeps its serial");
				}

				LF_TestCharacterFixture.Delete(restored);
			}
		}

		// A row whose snapshot no longer matches is skipped, not applied to the wrong item.
		array<ref LF_StorageItemSerialRecord> stale = {};
		LF_StorageItemSerialRecord row = new LF_StorageItemSerialRecord();
		row.m_sOwnerKey = CHARACTER_ID;
		row.m_sSlot = LF_StorageSnapshot.SLOT_CARRIED;
		row.m_aSerials.Insert("a");
		row.m_aSerials.Insert("b");
		row.m_aSerials.Insert("c");
		row.m_aSerials.Insert("d");
		stale.Insert(row);
		ctx.Equal(0, LF_StorageSnapshot.ApplyItemSerials(stale), "a stale row matches nothing");

		LF_StorageSnapshot.ApplyAll(before);
	}
}
