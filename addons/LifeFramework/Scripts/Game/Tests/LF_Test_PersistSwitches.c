// red-proof: StripPrefabFrom without the parent remap failed "the apple in the pouch follows the pouch to its new place (expected 2, got 4)"
// tier: LOGIC
//! Persistence switches (#689): the owner's `persistence.keep*` keys, and what each switch resets
//! in the stores a load just filled (LF_PersistenceKeep.ApplyAfterLoad). Cash, carried gear, jail
//! and licences here; survival and wanted, which live on the game mode, ride the real save path in
//! persistence/switches. The test puts back every store it touched.
class LF_Test_PersistSwitches : LF_Test
{
	protected static const string OWNER_A = "test-persist-switch-a";
	protected static const string OWNER_B = "test-persist-switch-b";
	protected static const string DUTY_SLOT = "police_duty";
	protected static const string APPLE = "test-persist-switch-apple";
	protected static const string BACKPACK = "test-persist-switch-backpack";
	protected static const string POUCH = "test-persist-switch-pouch";

	override string GetName()
	{
		return "persistence/switch-config";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		RunConfig(ctx);

		array<ref LF_StorageSnapshotRecord> snapshots = LF_StorageSnapshot.ExportAll();
		array<ref LF_JailRecord> sentences = LF_Jail.ExportAll();

		RunAllOn(ctx);
		RunCash(ctx);
		RunCarriedGear(ctx);
		RunJail(ctx);
		RunLicences(ctx);

		LF_Progression.Forget(OWNER_A);
		LF_Progression.Forget(OWNER_B);
		LF_StorageSnapshot.ApplyAll(snapshots);
		LF_Jail.ApplyAll(sentences);
		LF_ConfigService.Reset();
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_PersistenceConfig Keep(string persistenceSection)
	{
		LF_ConfigService.Reset();
		if (!persistenceSection.IsEmpty())
			LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"persistence\":" + persistenceSection + "}", "test");
		return LF_PersistenceConfig.ResolveKeep();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunConfig(LF_TestContext ctx)
	{
		LF_PersistenceConfig keep = Keep(string.Empty);
		ctx.True(keep.m_bKeepCash && keep.m_bKeepCarriedGear && keep.m_bKeepSurvival, "with no owner section cash, gear and survival persist");
		ctx.True(keep.m_bKeepWanted && keep.m_bKeepJail && keep.m_bKeepLicences, "with no owner section wanted, jail and licences persist");

		keep = Keep("{\"keepCash\":false,\"keepSurvival\":false,\"keepJail\":false}");
		ctx.False(keep.m_bKeepCash, "the owner turns cash on hand off");
		ctx.False(keep.m_bKeepSurvival, "the owner turns survival off");
		ctx.False(keep.m_bKeepJail, "the owner turns jail off");
		ctx.True(keep.m_bKeepCarriedGear && keep.m_bKeepWanted && keep.m_bKeepLicences, "the switches the owner left out stay on");

		keep = Keep("{\"keepCarriedGear\":false,\"keepWanted\":false,\"keepLicences\":false,\"autosaveInterval\":120}");
		ctx.False(keep.m_bKeepCarriedGear, "the owner turns carried gear off");
		ctx.False(keep.m_bKeepWanted, "the owner turns wanted off");
		ctx.False(keep.m_bKeepLicences, "the owner turns licences off");

		keep = Keep("{\"keepCash\":\"no\"}");
		ctx.True(keep.m_bKeepCash, "a switch that is not a bool is refused and stays on");
	}

	//------------------------------------------------------------------------------------------------
	//! Owner A carries a backpack holding cash, an apple and a pouch with an apple in it, and loose
	//! cash; the duty locker holds cash.
	protected static void SeedSnapshots(ResourceName cash)
	{
		LF_StorageSnapshotRecord carried = LF_StorageSnapshotRecord.Create(OWNER_A, LF_StorageSnapshot.SLOT_CARRIED);
		carried.m_aItems.Insert(LF_StorageItemRecord.Create(BACKPACK, 1, -1, 0, 3));
		carried.m_aItems.Insert(LF_StorageItemRecord.Create(cash, 50, 0, 0, 0));
		carried.m_aItems.Insert(LF_StorageItemRecord.Create(APPLE, 2, 0, 0, 1));
		carried.m_aItems.Insert(LF_StorageItemRecord.Create(cash, 20, -1, 1, 2));
		carried.m_aItems.Insert(LF_StorageItemRecord.Create(POUCH, 1, 0, 0, 2));
		carried.m_aItems.Insert(LF_StorageItemRecord.Create(APPLE, 1, 4, 0, 0));
		LF_StorageSnapshot.Set(carried);

		LF_StorageSnapshotRecord duty = LF_StorageSnapshotRecord.Create(OWNER_A, DUTY_SLOT);
		duty.m_aItems.Insert(LF_StorageItemRecord.Create(cash, 30, -1, 0, 0));
		LF_StorageSnapshot.Set(duty);

		LF_StorageSnapshotRecord other = LF_StorageSnapshotRecord.Create(OWNER_B, LF_StorageSnapshot.SLOT_CARRIED);
		other.m_aItems.Insert(LF_StorageItemRecord.Create(cash, 5, -1, 0, 0));
		LF_StorageSnapshot.Set(other);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunAllOn(LF_TestContext ctx)
	{
		ResourceName cash = LF_MoneyUtils.CashPrefab();
		SeedSnapshots(cash);
		LF_Jail.ApplyAll({LF_JailRecord.Create(OWNER_A, 120, true)});
		LF_Progression.NoteLicenses(OWNER_A, {LF_ELicenseType.FARMER_FIELD});

		ctx.Equal(0, LF_PersistenceKeep.ApplyAfterLoad(null, Keep(string.Empty)), "with every switch on nothing is reset");
		ctx.Equal(70, LF_StorageSnapshot.Get(OWNER_A, LF_StorageSnapshot.SLOT_CARRIED).CountPrefab(cash), "the carried cash stays");
		ctx.True(LF_Jail.IsServing(OWNER_A), "the sentence stays");
		array<LF_ELicenseType> licences = {};
		ctx.Equal(1, LF_Progression.GetLicenses(OWNER_A, licences), "the licence stays");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunCash(LF_TestContext ctx)
	{
		ResourceName cash = LF_MoneyUtils.CashPrefab();
		ctx.False(cash.IsEmpty(), "the cash prefab slot resolves");
		SeedSnapshots(cash);

		ctx.Equal(1, LF_PersistenceKeep.ApplyAfterLoad(null, Keep("{\"keepCash\":false}")), "keepCash off resets one field");
		LF_StorageSnapshotRecord carried = LF_StorageSnapshot.Get(OWNER_A, LF_StorageSnapshot.SLOT_CARRIED);
		ctx.NotNull(carried, "the carried snapshot stays");
		if (!carried)
			return;

		ctx.Equal(0, carried.CountPrefab(cash), "the carried cash is gone");
		ctx.Equal(4, carried.m_aItems.Count(), "the backpack, the pouch and both apples stay");
		ctx.Equal(3, carried.CountPrefab(APPLE), "with their quantities");
		ctx.Equal(0, carried.m_aItems[1].m_iParent, "the first apple still sits in the backpack");
		ctx.Equal(0, carried.m_aItems[2].m_iParent, "the pouch, moved up two places, still sits in the backpack");
		ctx.Equal(2, carried.m_aItems[3].m_iParent, "the apple in the pouch follows the pouch to its new place");
		ctx.Equal(0, LF_StorageSnapshot.Get(OWNER_B, LF_StorageSnapshot.SLOT_CARRIED).CountPrefab(cash), "every character's carried cash is gone");
		ctx.Equal(30, LF_StorageSnapshot.Get(OWNER_A, DUTY_SLOT).CountPrefab(cash), "a locker snapshot keeps its cash");

		ctx.Equal(0, LF_StorageSnapshot.StripPrefab(LF_StorageSnapshot.SLOT_CARRIED, cash), "a second pass removes nothing");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunCarriedGear(LF_TestContext ctx)
	{
		SeedSnapshots(LF_MoneyUtils.CashPrefab());
		ctx.Equal(1, LF_PersistenceKeep.ApplyAfterLoad(null, Keep("{\"keepCarriedGear\":false,\"keepCash\":false}")), "keepCarriedGear off resets one field (cash rides with it)");
		ctx.False(LF_StorageSnapshot.Has(OWNER_A, LF_StorageSnapshot.SLOT_CARRIED), "the carried snapshot is gone");
		ctx.False(LF_StorageSnapshot.Has(OWNER_B, LF_StorageSnapshot.SLOT_CARRIED), "every character's carried snapshot is gone");
		ctx.True(LF_StorageSnapshot.Has(OWNER_A, DUTY_SLOT), "a locker snapshot stays");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunJail(LF_TestContext ctx)
	{
		LF_Jail.ApplyAll({LF_JailRecord.Create(OWNER_A, 120, true), LF_JailRecord.Create(OWNER_B, 60, true)});
		ctx.True(LF_Jail.IsServing(OWNER_A), "owner A serves a sentence");
		ctx.Equal(1, LF_PersistenceKeep.ApplyAfterLoad(null, Keep("{\"keepJail\":false}")), "keepJail off resets one field");
		ctx.False(LF_Jail.IsServing(OWNER_A), "owner A is free");
		ctx.False(LF_Jail.IsServing(OWNER_B), "owner B is free");
		ctx.Equal(0, LF_Jail.ExportAll().Count(), "no sentence is left to save");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunLicences(LF_TestContext ctx)
	{
		LF_Progression.NoteLicenses(OWNER_A, {LF_ELicenseType.FARMER_FIELD, LF_ELicenseType.MINER_IRON});
		LF_Progression.NoteLicenses(OWNER_B, {LF_ELicenseType.MINER_COAL});
		ctx.Equal(1, LF_PersistenceKeep.ApplyAfterLoad(null, Keep("{\"keepLicences\":false}")), "keepLicences off resets one field");
		array<LF_ELicenseType> licences = {};
		ctx.Equal(0, LF_Progression.GetLicenses(OWNER_A, licences), "owner A holds no earned licence");
		ctx.Equal(0, LF_Progression.GetLicenses(OWNER_B, licences), "owner B holds no earned licence");
		ctx.NotNull(LF_Progression.Get(OWNER_A), "the rest of the progression record stays");
	}
}
