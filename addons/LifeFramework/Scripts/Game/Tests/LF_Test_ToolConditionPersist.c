// red-proof: observed 2026-11-24 in the #4821 batched `--red` run: RED-OK persist/tool-condition, "the restored pickaxe keeps its condition (expected 37.5, got 100)" with the section read skipped. Green with `--only`.
//! The "toolcondition" section (#4821): a worn pickaxe in a carried snapshot is written in the section,
//! read back onto the snapshot after the carried payload, applied onto a fresh body at the same
//! condition; loading twice ends the same; a row that no longer matches its snapshot is skipped; a save
//! from before the section loads every tool at full. The restart check is LF_Test_AaToolConditionRestart.
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_ToolConditionPersist : LF_Test
{
	static const ResourceName PICKAXE = "{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et";
	static const string CHARACTER_ID = "tool-condition-persist";
	static const float WORN = 37.5; // lf-fixed: a distinctive condition the round trip must keep

	override string GetName()
	{
		return "persist/tool-condition";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		LF_PersistenceComponent persistence;
		if (gameMode)
			persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));

		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

#ifdef LF_E2E_LOAD_BOOT
		return; // the restart check is LF_Test_AaToolConditionRestart
#endif

		ctx.NotNull(LF_SaveSections.Get(LF_ToolConditionSavePayload.NAME), "toolcondition is a registered save section");
		ctx.Equal(0, LF_PersistenceComponentSerializer.PayloadSinceVersion(LF_ToolConditionSavePayload.NAME), "toolcondition is not a legacy positional payload");

		array<ref LF_StorageSnapshotRecord> before = LF_StorageSnapshot.ExportAll();
		IEntity source = LF_TestCharacterFixture.Spawn(ctx);
		if (!source)
			return;

		IEntity pickEntity = LF_Utils.SpawnEntityPrefab(PICKAXE, source.GetOrigin() + "2 0.5 0");
		LF_ToolConditionComponent pick = LF_ToolConditionComponent.Of(pickEntity);
		InventoryStorageManagerComponent manager = LF_InventoryUtils.GetResponsibleStorageManager(source);
		ctx.True(pick && manager && manager.TryInsertItem(pickEntity), "the pickaxe is carried");
		if (!pick)
		{
			LF_TestCharacterFixture.Delete(pickEntity);
			LF_TestCharacterFixture.Delete(source);
			return;
		}

		pick.SetCondition(WORN);
		ctx.True(LF_StorageSnapshot.CaptureCarried(CHARACTER_ID, source), "the character is captured");
		LF_TestCharacterFixture.Delete(source);

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(LF_SaveSections.OBJECT_PREFIX + LF_ToolConditionSavePayload.NAME), "the save carries the toolcondition section");

		// Restart: the session forgets the snapshots; the carried payload brings them back with no condition.
		LF_StorageSnapshot.ApplyAll(before);
		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "carried");
		// RedProof(): the section read is skipped, so the tool comes back at -1 and loads at full (`--red`).
		if (!RedProof())
			ReadSection(loadContext, ctx);

		LF_StorageSnapshotRecord loaded = LF_StorageSnapshot.Get(CHARACTER_ID, LF_StorageSnapshot.SLOT_CARRIED);
		ctx.NotNull(loaded, "the snapshot is back");
		if (loaded)
		{
			IEntity restored = LF_TestCharacterFixture.Spawn(ctx);
			if (restored)
			{
				LF_StorageSnapshot.Apply(restored, loaded);
				LF_ToolConditionComponent back = LF_ToolConditionComponent.InUse(restored, LF_ToolBench.KIND_PICKAXE);
				ctx.NotNull(back, "the pickaxe is back");
				if (back)
					ctx.EqualFloat(WORN, back.GetCondition(), 0.01, "the restored pickaxe keeps its condition");

				LF_TestCharacterFixture.Delete(restored);
			}

			// Loading the section twice ends the same.
			JsonLoadContext again = new JsonLoadContext(false);
			again.LoadFromString(saved);
			ReadSection(again, ctx);
			int index = ToolIndex(loaded);
			ctx.True(index >= 0, "the snapshot lists the pickaxe");
			if (index >= 0)
				ctx.EqualFloat(WORN, loaded.m_aItems[index].m_fToolCondition, 0.01, "loading twice keeps the same condition");
		}

		// A row for a snapshot whose item list changed is skipped.
		array<ref LF_StorageToolConditionRecord> stale = {};
		LF_StorageToolConditionRecord row = new LF_StorageToolConditionRecord();
		row.m_sOwnerKey = CHARACTER_ID;
		row.m_sSlot = LF_StorageSnapshot.SLOT_CARRIED;
		row.m_aConditions = {1.0};
		stale.Insert(row);
		ctx.Equal(0, LF_StorageSnapshot.ApplyToolConditions(stale), "a stale row matches nothing");

		// A save from before the section: the carried tool loads at full.
		JsonSaveContext oldContext = new JsonSaveContext(false);
		array<string> omit = {LF_ToolConditionSavePayload.NAME};
		LF_PersistenceComponentSerializer.SerializeStoreOmitting(persistence, oldContext, omit);
		string oldSave = oldContext.SaveToString();
		ctx.False(oldSave.Contains(LF_SaveSections.OBJECT_PREFIX + LF_ToolConditionSavePayload.NAME), "the older save has no toolcondition section");
		LF_StorageSnapshot.ApplyAll(before);
		JsonLoadContext oldLoad = new JsonLoadContext(false);
		oldLoad.LoadFromString(oldSave);
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, oldLoad, "carried");
		LF_ToolConditionSavePayload absent = new LF_ToolConditionSavePayload();
		absent.OnAbsent();
		LF_StorageSnapshotRecord old = LF_StorageSnapshot.Get(CHARACTER_ID, LF_StorageSnapshot.SLOT_CARRIED);
		if (old)
		{
			int oldIndex = ToolIndex(old);
			if (oldIndex >= 0)
				ctx.True(old.m_aItems[oldIndex].m_fToolCondition < 0, "an older save names no condition, so the tool loads at full");
		}

		LF_StorageSnapshot.ApplyAll(before);
	}

	//------------------------------------------------------------------------------------------------
	protected static int ToolIndex(LF_StorageSnapshotRecord record)
	{
		for (int i = 0; i < record.m_aItems.Count(); i++)
		{
			if (record.m_aItems[i] && LF_Utils.SamePrefab(record.m_aItems[i].m_sPrefab, PICKAXE))
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	protected static void ReadSection(notnull JsonLoadContext context, LF_TestContext ctx)
	{
		LF_SavePayload section = LF_SaveSections.Get(LF_ToolConditionSavePayload.NAME);
		if (!section)
			return;

		ctx.True(context.StartObject(LF_SaveSections.OBJECT_PREFIX + LF_ToolConditionSavePayload.NAME), "the toolcondition section opens");
		int savedVersion;
		context.ReadValue(LF_SaveSections.VERSION_KEY, savedVersion);
		ctx.Equal(section.Version(), savedVersion, "the section carries its own version");
		ctx.True(section.ReadSection(context, savedVersion), "the toolcondition section reads");
		context.EndObject();
	}
}
