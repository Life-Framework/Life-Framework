// red-proof: 2026-11-18 `tools/cli test --no-build --only LF_Test_MarkedCashWipe` on the unfixed LF_PersistenceKeep (log test-20261118-050554): FAIL "keepCash off strips the carried marked notes (expected 0, got 300)", civilian stash 50, duty kit 25. Fixed (StripMarkedNotes): green.
//! Marked notes and the keepCash switch (#2850, lead decision): dirty money is money, so a wipe with
//! keepCash off strips marked notes from every slot it strips cash from; with keepCash on they stay.
//! The test puts back the snapshot store it touched.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MarkedCashWipe : LF_Test
{
	protected static const string OWNER = "test-marked-cash-wipe";
	protected static const string APPLE = "test-marked-cash-wipe-apple";

	override string GetName()
	{
		return "laundering/marked-cash-wipe";
	}

	override void Run(LF_TestContext ctx)
	{
		ResourceName marked = LF_MarkedCash.Prefab();
		ResourceName cash = LF_MoneyUtils.CashPrefab();
		ctx.False(marked.IsEmpty(), "the markedcash slot resolves a prefab");
		if (marked.IsEmpty())
			return;

		array<ref LF_StorageSnapshotRecord> saved = LF_StorageSnapshot.ExportAll();

		Seed(marked, cash);
		LF_PersistenceKeep.ApplyAfterLoad(null, Keep(string.Empty));
		ctx.Equal(300, LF_StorageSnapshot.Get(OWNER, LF_StorageSnapshot.SLOT_CARRIED).CountPrefab(marked), "keepCash on keeps the carried marked notes");

		Seed(marked, cash);
		string section = "{\"keepCash\":false}";
		if (RedProof())
			section = string.Empty;

		LF_PersistenceKeep.ApplyAfterLoad(null, Keep(section));
		LF_StorageSnapshotRecord carried = LF_StorageSnapshot.Get(OWNER, LF_StorageSnapshot.SLOT_CARRIED);
		ctx.NotNull(carried, "the carried snapshot stays");
		if (carried)
		{
			ctx.Equal(0, carried.CountPrefab(marked), "keepCash off strips the carried marked notes");
			ctx.Equal(0, carried.CountPrefab(cash), "and the carried cash");
			ctx.Equal(2, carried.CountPrefab(APPLE), "other items stay");
		}

		ctx.Equal(0, LF_StorageSnapshot.Get(OWNER, LF_DutyLocker.SLOT_CIVILIAN).CountPrefab(marked), "the civilian stash loses its marked notes");
		ctx.Equal(0, LF_StorageSnapshot.Get(OWNER, LF_DutyLocker.SLOT_POLICE).CountPrefab(marked), "a duty kit loses its marked notes");

		LF_StorageSnapshot.ApplyAll(saved);
		LF_ConfigService.Reset();
	}

	//------------------------------------------------------------------------------------------------
	protected static void Seed(ResourceName marked, ResourceName cash)
	{
		LF_StorageSnapshotRecord carried = LF_StorageSnapshotRecord.Create(OWNER, LF_StorageSnapshot.SLOT_CARRIED);
		carried.m_aItems.Insert(LF_StorageItemRecord.Create(marked, 300, -1, 0, 0));
		carried.m_aItems.Insert(LF_StorageItemRecord.Create(cash, 40, -1, 0, 1));
		carried.m_aItems.Insert(LF_StorageItemRecord.Create(APPLE, 2, -1, 0, 2));
		LF_StorageSnapshot.Set(carried);

		LF_StorageSnapshotRecord stash = LF_StorageSnapshotRecord.Create(OWNER, LF_DutyLocker.SLOT_CIVILIAN);
		stash.m_aItems.Insert(LF_StorageItemRecord.Create(marked, 50, -1, 0, 0));
		LF_StorageSnapshot.Set(stash);

		LF_StorageSnapshotRecord police = LF_StorageSnapshotRecord.Create(OWNER, LF_DutyLocker.SLOT_POLICE);
		police.m_aItems.Insert(LF_StorageItemRecord.Create(marked, 25, -1, 0, 0));
		LF_StorageSnapshot.Set(police);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_PersistenceConfig Keep(string persistenceSection)
	{
		LF_ConfigService.Reset();
		if (!persistenceSection.IsEmpty())
			LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"persistence\":" + persistenceSection + "}", "test");
		return LF_PersistenceConfig.ResolveKeep();
	}
}
