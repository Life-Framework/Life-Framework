// red-proof: e2e/content-ids, observed 2026-11-14 with `tools\cli wt e2e save-stable-ids --content-ids`:
// without LF_SaveIds.ResolveSnapshots in the "carried" read, boot 1 stayed green and boot 2 failed with
// `- e2e content ids: no record keeps the renamed id (expected 0, got 4)` and
// `- e2e content ids: 7 apples in the inventory after the restart (expected 7, got 3)` (the renamed apples were
// held as missing instead). Restored; all three boots green.

//------------------------------------------------------------------------------------------------
//! The restart proof of #965, three boots (`tools\cli wt e2e <slug> --content-ids`):
//! 1. Save: a carried inventory as an older build wrote it, with 4 apples (in the backpack) under an
//!    id a pack has since renamed and a canteen (also in the backpack) whose content it removed.
//! 2. Restart with the rename alias (LF_E2E_IDS_RENAMED): the apples load as apples, the canteen is
//!    logged and held; the inventory is applied to a character and captured again (a save point),
//!    and the canteen is still held when the server stops.
//! 3. Restart with the content back (LF_E2E_IDS_RESTORED aliases the canteen to the water bottle):
//!    it comes back inside the backpack.
//! Runs only under LF_E2E_CONTENT_IDS; a normal suite run passes it untouched.
// tier: PERSISTENCE
class LF_Test_E2E_ContentIds : LF_Test
{
	static const string OWNER = "e2e-content-ids-character";
	//! The apple as an older pack named it (alias in Configs/Tests/LF_E2E_SaveAliases_Renamed.conf).
	static const ResourceName OLD_APPLE = "{0E2EA11A5000000A}Prefabs/Items/Food/Apple_1988.et";
	//! Content the pack removed (alias back in Configs/Tests/LF_E2E_SaveAliases_Restored.conf).
	static const ResourceName REMOVED = "{0E2EA11A5000000B}Prefabs/Items/Drinks/Canteen_Removed.et";
	static const string MARKER_SEEDED = "e2e content ids seeded";
	static const string MARKER_HELD = "e2e content ids held";
	static const string MARKER_RESTORED = "e2e content ids restored";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "e2e/content-ids";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
#ifdef LF_E2E_CONTENT_IDS
#ifdef LF_E2E_LOAD_BOOT
#ifdef LF_E2E_IDS_RESTORED
		VerifyRestored(ctx);
#else
		VerifyRenamedAndHold(ctx);
#endif
#else
		Seed(ctx);
#endif
#else
		ctx.Pass("runs only under tools\\cli wt e2e <slug> --content-ids");
#endif
	}

	//------------------------------------------------------------------------------------------------
	//! Boot 1: the save an older build would have written.
	protected void Seed(LF_TestContext ctx)
	{
		IEntity source = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!source)
			return;

		LF_StorageSnapshotRecord record = LF_StorageSnapshot.Capture(OWNER, LF_StorageSnapshot.SLOT_CARRIED, source);
		LF_TestCharacterFixture.Delete(source);
		ctx.NotNull(record, "e2e content ids: the loaded character is captured");
		if (!record)
			return;

		int backpack = FindPrefab(record, LF_TestCarriedInventoryKit.BACKPACK_PREFAB);
		ctx.True(backpack >= 0, "e2e content ids: the capture holds the backpack");
		record.m_aItems.Insert(LF_StorageItemRecord.Create(OLD_APPLE, 4, backpack, 0, -1));
		record.m_aItems.Insert(LF_StorageItemRecord.Create(REMOVED, 1, backpack, 0, -1));
		LF_StorageSnapshot.Set(record);
		ctx.Equal(4, LF_StorageSnapshot.Get(OWNER, LF_StorageSnapshot.SLOT_CARRIED).CountPrefab(OLD_APPLE), "e2e content ids: the save holds 4 apples under the old id");
		PrintFormat("[LF_Tests] %1: %2 records under %3", MARKER_SEEDED, record.m_aItems.Count(), OWNER);
	}

	//------------------------------------------------------------------------------------------------
	//! Boot 2: the rename alias is in; the removed canteen has none.
	protected void VerifyRenamedAndHold(LF_TestContext ctx)
	{
		LF_StorageSnapshotRecord loaded = LF_StorageSnapshot.Get(OWNER, LF_StorageSnapshot.SLOT_CARRIED);
		ctx.NotNull(loaded, "e2e content ids: the carried inventory loaded after the restart");
		if (!loaded)
			return;

		ctx.Equal(0, loaded.CountPrefab(OLD_APPLE), "e2e content ids: no record keeps the renamed id");
		ctx.Equal(7, loaded.CountPrefab(LF_TestCarriedInventoryKit.APPLE_PREFAB), "e2e content ids: the 4 renamed apples load as apples beside the 3");
		ctx.Equal(1, loaded.CountPrefab(REMOVED), "e2e content ids: the removed canteen is held in the save");
		ctx.True(LF_SaveIds.WasMissing(LF_SaveIds.KIND_PREFAB, REMOVED), "e2e content ids: the removed canteen was logged at the load");

		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return;

		LF_StorageSnapshot.Apply(character, loaded);
		ctx.Equal(7, LF_InventoryUtils.GetAmount(character, LF_TestCarriedInventoryKit.APPLE_PREFAB), "e2e content ids: 7 apples in the inventory after the restart");
		ctx.Equal(1, LF_TestCarriedInventoryKit.CountInside(character, LF_TestCarriedInventoryKit.BACKPACK_PREFAB, LF_TestCarriedInventoryKit.WATER_PREFAB), "e2e content ids: only the one water bottle is in the backpack");

		// A save point: the live character is captured; the canteen it never got is held.
		ctx.True(LF_StorageSnapshot.CaptureCarried(OWNER, character), "e2e content ids: the character is captured again");
		LF_StorageSnapshotRecord recaptured = LF_StorageSnapshot.Get(OWNER, LF_StorageSnapshot.SLOT_CARRIED);
		LF_TestCharacterFixture.Delete(character);
		if (!recaptured)
			return;

		ctx.Equal(1, recaptured.CountPrefab(REMOVED), "e2e content ids: the capture still holds the removed canteen");
		ctx.Equal(7, recaptured.CountPrefab(LF_TestCarriedInventoryKit.APPLE_PREFAB), "e2e content ids: the capture holds 7 apples under the current id");
		int canteen = FindPrefab(recaptured, REMOVED);
		int backpack = FindPrefab(recaptured, LF_TestCarriedInventoryKit.BACKPACK_PREFAB);
		if (canteen >= 0)
			ctx.Equal(backpack, recaptured.m_aItems[canteen].m_iParent, "e2e content ids: the held canteen is still inside the backpack");

		PrintFormat("[LF_Tests] %1: %2 apples, %3 canteen held", MARKER_HELD, recaptured.CountPrefab(LF_TestCarriedInventoryKit.APPLE_PREFAB), recaptured.CountPrefab(REMOVED));
	}

	//------------------------------------------------------------------------------------------------
	//! Boot 3: the content is back (aliased to the water bottle).
	protected void VerifyRestored(LF_TestContext ctx)
	{
		LF_StorageSnapshotRecord loaded = LF_StorageSnapshot.Get(OWNER, LF_StorageSnapshot.SLOT_CARRIED);
		ctx.NotNull(loaded, "e2e content ids: the carried inventory loaded after the second restart");
		if (!loaded)
			return;

		ctx.Equal(0, loaded.CountPrefab(REMOVED), "e2e content ids: the held canteen resolved");
		ctx.Equal(2, loaded.CountPrefab(LF_TestCarriedInventoryKit.WATER_PREFAB), "e2e content ids: the canteen loads as the second water bottle");
		ctx.Equal(7, loaded.CountPrefab(LF_TestCarriedInventoryKit.APPLE_PREFAB), "e2e content ids: the apples are still 7");

		IEntity character = LF_TestCharacterFixture.Spawn(ctx);
		if (!character)
			return;

		LF_StorageSnapshot.Apply(character, loaded);
		int inside = LF_TestCarriedInventoryKit.CountInside(character, LF_TestCarriedInventoryKit.BACKPACK_PREFAB, LF_TestCarriedInventoryKit.WATER_PREFAB);
		ctx.Equal(2, inside, "e2e content ids: the restored canteen is back inside the backpack");
		ctx.Equal(7, LF_InventoryUtils.GetAmount(character, LF_TestCarriedInventoryKit.APPLE_PREFAB), "e2e content ids: 7 apples after the second restart");
		LF_TestCharacterFixture.Delete(character);
		if (ctx.FailureCount() == 0)
			PrintFormat("[LF_Tests] %1: %2 water bottles in the backpack", MARKER_RESTORED, inside);
	}

	//------------------------------------------------------------------------------------------------
	protected static int FindPrefab(LF_StorageSnapshotRecord record, ResourceName prefab)
	{
		for (int index = 0; index < record.m_aItems.Count(); index++)
		{
			LF_StorageItemRecord item = record.m_aItems[index];
			if (item && LF_Utils.SamePrefab(item.m_sPrefab, prefab))
				return index;
		}

		return -1;
	}
}
