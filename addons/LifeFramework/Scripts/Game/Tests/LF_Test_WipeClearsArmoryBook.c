// red-proof: RED-OK 2026-11-17 `tools/cli test --no-build --red LF_Test_WipeClearsArmoryBook` (log test-20261117-220549): with the apply skipped, "a gear wipe clears the officer's outstanding serials (expected 0, got 2)" and "every officer's serials (expected 0, got 1)". Green: `--only` (log test-20261117-220614), log line "keepCarriedGear off: 3 outstanding armory serial(s) cleared" then 0 on the re-apply.
// tier: LOGIC
//! persistence/wipe-armory-book (#2621): a gear wipe (keepCarriedGear off) takes the issued kits, so
//! the armory book hands back every officer's outstanding serials; the 2 h issue cooldown stays as
//! saved. A kit kept in the police duty locker is still a gun, so its serial stays out. With
//! keepCarriedGear on the serials stay. Applying twice is idempotent.
class LF_Test_WipeClearsArmoryBook : LF_Test
{
	protected static const string UID = "test-wipe-armory-book";
	protected static const string OTHER_UID = "test-wipe-armory-book-2";
	protected static const string LOCKER_UID = "test-wipe-armory-book-3";
	protected static const string CHARACTER = "test-wipe-armory-book-char";
	protected static const string LOCKER_CHARACTER = "test-wipe-armory-book-char-3";
	protected static const string LOCKER_SERIAL = "LF-M9-0004";
	protected static const int T0 = 1800000000;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "persistence/wipe-armory-book";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_StorageSnapshotRecord> snapshots = LF_StorageSnapshot.ExportAll();
		array<ref LF_ArmoryKitRow> books = LF_ArmoryKits.ExportAll();

		RunKept(ctx);
		RunWiped(ctx);

		LF_StorageSnapshot.ApplyAll(snapshots);
		LF_ArmoryKits.ApplyAll(books);
		LF_ConfigService.Reset();
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_PersistenceConfig Keep(bool carriedGear)
	{
		string flag = "false";
		if (carriedGear)
			flag = "true";

		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"persistence\":{\"keepCarriedGear\":" + flag + "}}", "test");
		return LF_PersistenceConfig.ResolveKeep();
	}

	//------------------------------------------------------------------------------------------------
	//! Two officers with kits out: two serials and one serial.
	protected static void Seed()
	{
		LF_ArmoryKits.ApplyAll(null);
		array<string> serials = {"LF-M9-0001", "LF-M16-0002"};
		LF_ArmoryKits.RecordIssue(UID, T0, serials);
		array<string> other = {"LF-M9-0003"};
		LF_ArmoryKits.RecordIssue(OTHER_UID, T0 + 60, other);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunKept(LF_TestContext ctx)
	{
		Seed();
		LF_PersistenceConfig keep = Keep(true);
		ctx.True(keep.m_bKeepCarriedGear, "the config turns keepCarriedGear on");
		LF_PersistenceKeep.ApplyAfterLoad(null, keep);
		ctx.Equal(2, LF_ArmoryKits.Outstanding(UID), "keepCarriedGear on keeps the officer's two serials out");
		ctx.Equal(1, LF_ArmoryKits.Outstanding(OTHER_UID), "keepCarriedGear on keeps the second officer's serial out");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunWiped(LF_TestContext ctx)
	{
		Seed();
		// The first officer saved on duty: the kit rode in the carried snapshot, which the wipe drops.
		LF_StorageSnapshotRecord carried = LF_StorageSnapshotRecord.Create(CHARACTER, LF_StorageSnapshot.SLOT_CARRIED);
		LF_StorageItemRecord carriedGun = LF_StorageItemRecord.Create(LF_TestPoliceKit.M9_PREFAB, 1, -1, 0, 0);
		carriedGun.m_sSerial = "LF-M9-0001";
		carried.m_aItems.Insert(carriedGun);
		LF_StorageSnapshot.Set(carried);

		// A third officer saved off duty: the kit sits in the police duty locker, which the wipe keeps.
		array<string> lockerSerials = {LOCKER_SERIAL};
		LF_ArmoryKits.RecordIssue(LOCKER_UID, T0, lockerSerials);
		LF_StorageSnapshotRecord locker = LF_StorageSnapshotRecord.Create(LOCKER_CHARACTER, LF_DutyLocker.SLOT_POLICE);
		LF_StorageItemRecord lockerGun = LF_StorageItemRecord.Create(LF_TestPoliceKit.M9_PREFAB, 1, -1, 0, 0);
		lockerGun.m_sSerial = LOCKER_SERIAL;
		locker.m_aItems.Insert(lockerGun);
		LF_StorageSnapshot.Set(locker);

		ctx.Equal(2, LF_ArmoryKits.Outstanding(UID), "the officer starts with two serials out");
		LF_PersistenceConfig keep = Keep(false);
		ctx.False(keep.m_bKeepCarriedGear, "the config turns keepCarriedGear off");

		if (!RedProof())
			LF_PersistenceKeep.ApplyAfterLoad(null, keep);

		ctx.Equal(0, LF_ArmoryKits.Outstanding(UID), "a gear wipe clears the officer's outstanding serials");
		ctx.Equal(0, LF_ArmoryKits.Outstanding(OTHER_UID), "a gear wipe clears every officer's serials");
		ctx.True(LF_ArmoryKits.IsOutstanding(LOCKER_UID, LOCKER_SERIAL), "a kit kept in the police duty locker stays on the book");
		ctx.Equal(T0, LF_ArmoryKits.LastIssueUnix(UID), "the issue time is unchanged");
		ctx.Equal(T0 + 60, LF_ArmoryKits.LastIssueUnix(OTHER_UID), "the second officer's issue time is unchanged");
		ctx.True(LF_ArmoryKits.CooldownLeft(UID, T0 + 60) > 0, "the cooldown still runs after the wipe");

		// Idempotent: the live-session re-apply clears nothing more and keeps the times.
		LF_PersistenceKeep.ApplyAfterLoad(null, keep);
		ctx.Equal(0, LF_ArmoryKits.Outstanding(UID), "a second apply leaves the book clear");
		ctx.Equal(1, LF_ArmoryKits.Outstanding(LOCKER_UID), "a second apply keeps the locker kit on the book");
		ctx.Equal(T0, LF_ArmoryKits.LastIssueUnix(UID), "a second apply keeps the issue time");

		LF_StorageSnapshot.Clear(LOCKER_CHARACTER, LF_DutyLocker.SLOT_POLICE);
	}
}
