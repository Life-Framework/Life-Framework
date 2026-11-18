// red-proof: 2026-11-18 `tools/cli test --no-build --red LF_Test_DutyPayRankChange,LF_Test_ArmoryDemotionStrip`: RED PROOF OK; with the strip and the online demotion skipped the rifle stays on the officer and its serial on the book. Green with `--only`.
//! police/armory-demotion-strip (#2874): a senior officer carrying the issued rifle (a senior-only
//! armory issue) is demoted to patrol by an administrator. Online, the rifle comes off them at once and
//! its serial leaves the armory book; offline, it stays until they spawn, when the spawn path takes it.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ArmoryDemotionStrip : LF_Test
{
	protected static const string ADMIN = "demotion-strip-admin";
	protected static const string OFFICER_UID = "demotion-strip-officer";
	protected static const string NOBODY_ROSTER = "$profile:LifeFramework/test-demotion-strip-no-staff.txt";
	protected static const ResourceName ISSUED_RIFLE = "{1E06C1BB38AFB556}Prefabs/Items/Police/LF_ArmoryM16.et";
	protected static const int T0 = 1800000000;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "police/armory-demotion-strip";
	}

	//------------------------------------------------------------------------------------------------
	//! Senior, on duty, issued the rifle and booked for it. \return The rifle's serial.
	protected static string IssueRifle(LF_TestContext ctx, LF_PlayerAccount account, IEntity officer)
	{
		account.SetPoliceRank(LF_PoliceRanks.SENIOR);
		string missing;
		ctx.True(LF_PoliceRanks.IssueItem(OFFICER_UID, officer, "rifle_m16a2", missing), "the senior officer is issued the rifle");
		string serial = LF_Test_ArmoryKitTurnIn.SerialOf(officer);
		ctx.False(serial.IsEmpty(), "the issued rifle carries a serial");
		array<string> kit = {serial};
		LF_ArmoryKits.RecordIssue(OFFICER_UID, T0, kit);
		return serial;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		LF_ItemCatalogueConfig.Reload();
		LF_PoliceApplicationStaff.SetStaffPath(NOBODY_ROSTER);

		ctx.False(LF_PoliceRanks.RankMayHoldIssued(LF_PoliceRanks.PATROL, ISSUED_RIFLE), "patrol may not hold the issued rifle");
		ctx.True(LF_PoliceRanks.RankMayHoldIssued(LF_PoliceRanks.SENIOR, ISSUED_RIFLE), "senior may hold the issued rifle");
		ctx.True(LF_PoliceRanks.RankMayHoldIssued(LF_PoliceRanks.PATROL, LF_PoliceRanks.SPIKE_STRIP), "an item no armory offer issues is not rank kit");

		IEntity officer = LF_TestCharacterFixture.Spawn(ctx);
		if (!officer)
			return;

		array<ref LF_ArmoryKitRow> booksBefore = LF_ArmoryKits.ExportAll();
		array<ref LF_FenceSerialRow> serialsBefore = LF_FenceSerials.ExportAll();
		LF_ArmoryKits.ApplyAll(null);

		LF_PlayerAccount account = LF_TestPoliceKit.Account(OFFICER_UID, true);
		account.SetFaction(LF_Faction.POLICE);
		account.SetOnDuty(true);

		// Offline: no body is bound to the account, so the demotion cannot reach the rifle yet.
		string serial = IssueRifle(ctx, account, officer);
		ctx.EqualStr("", LF_PoliceRankAdmin.SetRank(ADMIN, true, OFFICER_UID, LF_PoliceRanks.PATROL), "an administrator demotes the offline officer");
		ctx.EqualStr(serial, LF_Test_ArmoryKitTurnIn.SerialOf(officer), "an offline officer's rifle waits for their spawn");

		// Their spawn: the spawn path takes the rifle.
		int taken = 0;
		if (!RedProof())
			taken = LF_PoliceRanks.StripKitAboveRank(OFFICER_UID, officer);
		ctx.Equal(1, taken, "the spawn path takes the rifle");
		ctx.EqualStr("", LF_Test_ArmoryKitTurnIn.SerialOf(officer), "the officer no longer carries the rifle after the spawn");
		ctx.Equal(0, LF_ArmoryKits.Outstanding(OFFICER_UID), "the rifle's serial leaves the book");
		ctx.Equal(0, LF_PoliceRanks.StripKitAboveRank(OFFICER_UID, officer), "a second pass takes nothing");

		// Online: a bound body loses the rifle in the demotion itself.
		LF_ArmoryKits.ApplyAll(null);
		LF_PoliceRanks.BindOfficer(officer, OFFICER_UID);
		IssueRifle(ctx, account, officer);
		string refusal = "not run";
		if (!RedProof())
			refusal = LF_PoliceRankAdmin.SetRank(ADMIN, true, OFFICER_UID, LF_PoliceRanks.PATROL);
		ctx.EqualStr("", refusal, "an administrator demotes the online officer");
		ctx.EqualStr("", LF_Test_ArmoryKitTurnIn.SerialOf(officer), "the online officer loses the rifle at the demotion");
		ctx.Equal(0, LF_ArmoryKits.Outstanding(OFFICER_UID), "the online officer's serial leaves the book");
		ctx.True(LF_PoliceRanks.GetLastError().Contains("kit above rank"), "the log names the kit taken");

		// A promotion keeps the kit.
		LF_ArmoryKits.ApplyAll(null);
		IssueRifle(ctx, account, officer);
		account.SetPoliceRank(LF_PoliceRanks.PATROL);
		ctx.EqualStr("", LF_PoliceRankAdmin.SetRank(ADMIN, true, OFFICER_UID, LF_PoliceRanks.SENIOR), "an administrator promotes the officer");
		ctx.Equal(1, LF_ArmoryKits.Outstanding(OFFICER_UID), "a promotion takes no kit");

		LF_ArmoryKits.ApplyAll(booksBefore);
		LF_FenceSerials.ApplyAll(serialsBefore);
		account.SetPoliceRank(LF_PoliceRanks.PATROL);
		account.SetOnDutySeconds(0);
		LF_TestPoliceKit.Release(OFFICER_UID);
		LF_PoliceApplicationStaff.SetStaffPath("");
		LF_TestCharacterFixture.Delete(officer);
	}
}
