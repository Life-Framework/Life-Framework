// red-proof: loading APPLE_PREFAB in place of TOMATO_ITEM in the first assertion failed it with
// `[LFTEST]   - the removed tomato prefab no longer exists` (observed 2026-11-10, `test --no-build --only LF_Test_RemovedTomatoSkip`). Restored; green.
// tier: WORLD
class LF_Test_RemovedTomatoSkip : LF_Test
{
	protected static const ResourceName TOMATO_ITEM = "{0815D91FDF997A0A}Prefabs/Items/Food/Tomato.et";
	protected static const ResourceName TOMATO_SEED = "{97F71E1D9830AE26}Prefabs/Items/Seeds/SeedTomato.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "persist/removed-tomato-skipped";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Resource tomato = Resource.Load(TOMATO_ITEM);
		ctx.True(!tomato || !tomato.IsValid(), "the removed tomato prefab no longer exists");
		Resource seed = Resource.Load(TOMATO_SEED);
		ctx.True(!seed || !seed.IsValid(), "the removed tomato seed prefab no longer exists");

		// A save from before the removal: a tomato, a seed and an apple in the carried snapshot.
		LF_StorageSnapshotRecord record = LF_StorageSnapshotRecord.Create("removed-tomato-character", LF_StorageSnapshot.SLOT_CARRIED);
		record.m_aItems.Insert(LF_StorageItemRecord.Create(TOMATO_ITEM, 4, -1, 0, 0));
		record.m_aItems.Insert(LF_StorageItemRecord.Create(TOMATO_SEED, 1, -1, 0, 0));
		record.m_aItems.Insert(LF_StorageItemRecord.Create(LF_TestCarriedInventoryKit.APPLE_PREFAB, 2, -1, 0, 0));

		IEntity restored = LF_TestCharacterFixture.Spawn(ctx);
		if (!restored)
			return;

		LF_StorageSnapshot.Apply(restored, record);
		ctx.Equal(2, LF_InventoryUtils.GetAmount(restored, LF_TestCarriedInventoryKit.APPLE_PREFAB), "the apple in the old save still loads");
		LF_TestCharacterFixture.Delete(restored);
	}
}
