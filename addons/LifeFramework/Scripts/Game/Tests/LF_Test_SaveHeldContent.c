// red-proof: persist/held-missing-content, observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_SaveHeldContent`:
// skipping LF_StorageSnapshot.CarryHeld in Set failed it with `- the capture still holds the missing canteen
// (expected 1, got 0)`, `- the capture has as many records as the save (expected 8, got 5)` and `- the returned
// canteen comes back inside the backpack as a water bottle, beside the first (expected 2, got 1)`. Restored; green.

//------------------------------------------------------------------------------------------------
//! Missing content is held, not dropped (#965). A carried inventory names a prefab the build no
//! longer has (inside the backpack) and one at the root with an apple inside it. Applying it skips
//! them; a fresh capture still holds them, once, in the right container; a second capture adds no
//! copy; when an alias brings the content back, the next load restores it into the backpack.
// tier: WORLD
class LF_Test_SaveHeldContent : LF_Test
{
	protected static const string CHARACTER_ID = "held-content-character";
	static const ResourceName GONE_IN_BACKPACK = "{0E2EA11A500000D1}Prefabs/Items/Drinks/Canteen_Gone.et";
	static const ResourceName GONE_AT_ROOT = "{0E2EA11A500000D2}Prefabs/Items/Equipment/Satchel_Gone.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "persist/held-missing-content";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_SaveIds.ResetStatics();
		array<ref LF_StorageSnapshotRecord> before = LF_StorageSnapshot.ExportAll();

		IEntity source = LF_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!source || ctx.FailureCount() > 0)
		{
			LF_TestCharacterFixture.Delete(source);
			return;
		}

		// The save as an older build wrote it: the real inventory plus two items this build lacks.
		LF_StorageSnapshotRecord saved = LF_StorageSnapshot.Capture(CHARACTER_ID, LF_StorageSnapshot.SLOT_CARRIED, source);
		LF_TestCharacterFixture.Delete(source);
		ctx.NotNull(saved, "the loaded character is captured");
		if (!saved)
			return;

		int backpack = FindPrefab(saved, LF_TestCarriedInventoryKit.BACKPACK_PREFAB);
		ctx.True(backpack >= 0, "the capture holds the backpack");
		saved.m_aItems.Insert(LF_StorageItemRecord.Create(GONE_IN_BACKPACK, 1, backpack, 0, 5));
		saved.m_aItems.Insert(LF_StorageItemRecord.Create(GONE_AT_ROOT, 1, -1, 0, 7));
		saved.m_aItems.Insert(LF_StorageItemRecord.Create(LF_TestCarriedInventoryKit.APPLE_PREFAB, 2, saved.m_aItems.Count() - 1, 0, 0));
		int savedCount = saved.m_aItems.Count();
		LF_StorageSnapshot.Set(saved);

		// The restart: the character spawns with what loads, the rest is skipped.
		IEntity restored = LF_TestCharacterFixture.Spawn(ctx);
		if (!restored)
		{
			LF_StorageSnapshot.ApplyAll(before);
			return;
		}

		LF_StorageSnapshot.Apply(restored, saved);
		ctx.Equal(3, LF_InventoryUtils.GetAmount(restored, LF_TestCarriedInventoryKit.APPLE_PREFAB), "3 apples come back (the 2 inside the missing satchel do not)");
		ctx.True(LF_SaveIds.WasMissing(LF_SaveIds.KIND_PREFAB, GONE_IN_BACKPACK), "the missing canteen is logged");

		// The next save point captures the live character: the missing items are held, not dropped.
		ctx.True(LF_StorageSnapshot.CaptureCarried(CHARACTER_ID, restored), "the restored character is captured");
		LF_StorageSnapshotRecord held = LF_StorageSnapshot.Get(CHARACTER_ID, LF_StorageSnapshot.SLOT_CARRIED);
		ctx.NotNull(held, "the capture is stored");
		if (held)
		{
			ctx.Equal(1, held.CountPrefab(GONE_IN_BACKPACK), "the capture still holds the missing canteen");
			ctx.Equal(1, held.CountPrefab(GONE_AT_ROOT), "the capture still holds the missing satchel");
			ctx.Equal(5, held.CountPrefab(LF_TestCarriedInventoryKit.APPLE_PREFAB), "the satchel's 2 apples are held with it (3 live + 2)");
			int canteen = FindPrefab(held, GONE_IN_BACKPACK);
			int satchel = FindPrefab(held, GONE_AT_ROOT);
			int heldBackpack = FindPrefab(held, LF_TestCarriedInventoryKit.BACKPACK_PREFAB);
			if (canteen >= 0 && satchel >= 0)
			{
				ctx.Equal(heldBackpack, held.m_aItems[canteen].m_iParent, "the held canteen stays inside the backpack");
				ctx.Equal(-1, held.m_aItems[satchel].m_iParent, "the held satchel stays at the root");
				ctx.Equal(satchel, held.m_aItems[held.m_aItems.Count() - 1].m_iParent, "the held apples stay inside the satchel");
			}

			ctx.Equal(savedCount, held.m_aItems.Count(), "the capture has as many records as the save");
		}

		// A second save point adds no copy.
		ctx.True(LF_StorageSnapshot.CaptureCarried(CHARACTER_ID, restored), "the character is captured again");
		LF_StorageSnapshotRecord again = LF_StorageSnapshot.Get(CHARACTER_ID, LF_StorageSnapshot.SLOT_CARRIED);
		if (again)
		{
			ctx.Equal(1, again.CountPrefab(GONE_IN_BACKPACK), "a second capture holds the canteen once");
			ctx.Equal(savedCount, again.m_aItems.Count(), "a second capture keeps the record count");
		}

		LF_TestCharacterFixture.Delete(restored);

		// The content returns under a new id: the next load resolves the held record and restores it.
		ctx.True(LF_SaveIds.Add(LF_SaveIds.KIND_PREFAB, GONE_IN_BACKPACK, LF_TestCarriedInventoryKit.WATER_PREFAB), "a pack adds the canteen's alias");
		array<ref LF_StorageSnapshotRecord> reload = {};
		if (again)
			reload.Insert(again);
		LF_SaveIds.ResolveSnapshots(reload);
		IEntity back = LF_TestCharacterFixture.Spawn(ctx);
		if (back && again)
		{
			LF_StorageSnapshot.Apply(back, again);
			ctx.Equal(2, LF_TestCarriedInventoryKit.CountInside(back, LF_TestCarriedInventoryKit.BACKPACK_PREFAB, LF_TestCarriedInventoryKit.WATER_PREFAB), "the returned canteen comes back inside the backpack as a water bottle, beside the first");
			ctx.Equal(1, again.CountPrefab(GONE_AT_ROOT), "the satchel with no alias is still held");
		}

		LF_TestCharacterFixture.Delete(back);
		LF_StorageSnapshot.ApplyAll(before);
		LF_SaveIds.ResetStatics();
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
