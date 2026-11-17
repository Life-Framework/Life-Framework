// red-proof: Observed 2026-11-17 with `tools\cli test --no-build --red LF_Test_HouseRegistryPersist,LF_Test_HouseCarriedKeyPersist`: `RED-OK houses/carried-key-persist` with the itemkeys read skipped, `- the restored key opens the same house (expected 'carried-key-house', got '')`. Green with the read.
// tier: PERSISTENCE
//! houses/carried-key-persist (#2410): a carried house key keeps the house it opens across a save. The
//! snapshot capture reads the binding, the "itemkeys" payload round trips it beside the carried
//! snapshot (whose own format holds no binding), and the restore binds the respawned key again.
//! Across a restart: `e2e/pass2-verify-load` (`tools\cli wt e2e`) carries a bound key on its carrier.
class LF_Test_HouseCarriedKeyPersist : LF_Test
{
	protected static const ResourceName KEY = "{3735389521255DBE}Prefabs/Items/HouseKeyItem.et";
	protected static const string HOUSE = "carried-key-house";
	protected static const string OWNER_KEY = "carried-key-character";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "houses/carried-key-persist";
	}

	//------------------------------------------------------------------------------------------------
	//! \return The house the first key in the snapshot opens, "" when it holds none.
	protected static string KeyOf(LF_StorageSnapshotRecord record)
	{
		if (!record || !record.m_aItems)
			return "";

		foreach (LF_StorageItemRecord item : record.m_aItems)
		{
			if (item && !item.m_sHouseKey.IsEmpty())
				return item.m_sHouseKey;
		}

		return "";
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
		// The restart path is e2e/pass2-verify-load: the e2e carrier carries a bound house key.
		ctx.Pass("the carried house key restart check is e2e/pass2-verify-load");
		return;
#endif

		ctx.True(LF_PersistenceComponentSerializer.PayloadSinceVersion("itemkeys") > 0, "itemkeys is a registered payload");

		IEntity carrier = LF_TestCharacterFixture.Spawn(ctx);
		IEntity key = LF_Utils.SpawnEntityPrefab(KEY, "68 0 68");
		IEntity restored = LF_TestCharacterFixture.Spawn(ctx);
		ctx.NotNull(key, "a house key spawns");
		if (carrier && key && restored)
			RunKey(ctx, persistence, carrier, key, restored);

		LF_StorageSnapshot.Clear(OWNER_KEY, LF_StorageSnapshot.SLOT_CARRIED);
		LF_TestCharacterFixture.Delete(carrier);
		LF_TestCharacterFixture.Delete(restored);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunKey(LF_TestContext ctx, LF_PersistenceComponent persistence, IEntity carrier, IEntity key, IEntity restored)
	{
		LF_KeyComponent keyComponent = LF_Component<LF_KeyComponent>.Find(key);
		ctx.NotNull(keyComponent, "the key carries LF_KeyComponent");
		if (!keyComponent)
			return;

		keyComponent.SetHouseIdentifier(HOUSE);
		InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(carrier);
		ctx.True(storage && storage.TryInsertItem(key, EStoragePurpose.PURPOSE_DEPOSIT), "the key goes into the carrier's pockets");

		LF_StorageSnapshotRecord record = LF_StorageSnapshot.Capture(OWNER_KEY, LF_StorageSnapshot.SLOT_CARRIED, carrier);
		ctx.NotNull(record, "the carried inventory is captured");
		if (!record)
			return;

		LF_StorageSnapshot.Set(record);
		ctx.EqualStr(HOUSE, KeyOf(record), "the capture reads the key's house");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.WritePersistencePayload(persistence, saveContext, "itemkeys");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(HOUSE), "the save carries the key's house");

		// A fresh load of the carried payload: its format holds no binding.
		LF_StorageSnapshotRecord stored = LF_StorageSnapshot.Get(OWNER_KEY, LF_StorageSnapshot.SLOT_CARRIED);
		foreach (LF_StorageItemRecord item : stored.m_aItems)
		{
			if (item)
				item.m_sHouseKey = "";
		}

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		if (!RedProof())
			LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "itemkeys");

		ctx.EqualStr(HOUSE, KeyOf(LF_StorageSnapshot.Get(OWNER_KEY, LF_StorageSnapshot.SLOT_CARRIED)), "the stored key opens its house after the load");

		// The restore binds the respawned key.
		LF_StorageSnapshot.Apply(restored, LF_StorageSnapshot.Get(OWNER_KEY, LF_StorageSnapshot.SLOT_CARRIED));
		SCR_InventoryStorageManagerComponent manager = LF_Component<SCR_InventoryStorageManagerComponent>.Find(restored);
		string bound;
		if (manager)
		{
			array<IEntity> keys = LF_InventoryUtils.FindItemsByPrefab(manager, KEY);
			foreach (IEntity found : keys)
			{
				LF_KeyComponent foundKey = LF_Component<LF_KeyComponent>.Find(found);
				if (foundKey)
					bound = foundKey.GetHouseIdentifier();
			}
		}

		ctx.EqualStr(HOUSE, bound, "the restored key opens the same house");
	}
}
