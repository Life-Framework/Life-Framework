// red-proof: IsStaff returns true for every caller: '[LFTEST] SUMMARY tier=only passed=0 failed=1 total=1', restored (2026-11-14)
// red-proof (level column, 2026-11-14): with ReadRoster taking the whole trimmed line as the uid: '[LFTEST] SUMMARY tier=only passed=0 failed=1 total=1', restored
// tier: LOGIC
class LF_Test_PoliceApplicationStaff : LF_Test
{
	override string GetName()
	{
		return "police/application-staff";
	}

	override void Run(LF_TestContext ctx)
	{
		string appsPath = "$profile:LifeFramework/test_police_staff_apps.txt";
		string rosterPath = "$profile:LifeFramework/test_police_staff_roster.txt";
		string whitelistPath = "$profile:LifeFramework/test_police_staff_whitelist.txt";
		LF_PoliceApplications.SetSavePath(appsPath);
		LF_PoliceApplicationStaff.SetStaffPath(rosterPath);
		LF_WhitelistManager.SetSavePath(whitelistPath);
		LF_WhitelistManager.Reset();
		FileIO.DeleteFile(appsPath);
		FileIO.DeleteFile(rosterPath);

		string staff = "test-staff-clerk";
		string moderator = "test-staff-mod";
		string civilian = "test-staff-civilian";
		string ann = "test-staff-ann";
		string bo = "test-staff-bo";
		int hour = 3600;
		int rejectedAt = 2000000;

		ctx.EqualStr("", LF_PoliceApplications.Submit(ann, "Ann Lee", "To help", "Evenings", false), "ann's application is filed");
		ctx.EqualStr("", LF_PoliceApplications.Submit(bo, "Bo Kay", "To serve", "Weekends", false), "bo's application is filed");

		// The roster file: a missing file is empty, comments and blanks are skipped, an edit is read at once.
		ctx.False(LF_PoliceApplicationStaff.IsStaff(staff, false), "with no roster file nobody is staff");
		FileHandle handle = FileIO.OpenFile(rosterPath, FileMode.WRITE);
		handle.WriteLine("# staff account uids");
		handle.WriteLine("");
		handle.WriteLine("  " + staff + "  ");
		handle.WriteLine("#pending administrator Jasper");
		handle.WriteLine(moderator + "	moderator   # Mo, a display name");
		handle.Close();
		ctx.True(LF_PoliceApplicationStaff.IsStaff(staff, false), "an account on the roster file is staff");
		ctx.True(LF_PoliceApplicationStaff.IsStaff(moderator, false), "a line with a level column and a trailing comment is read by its uid");
		ctx.False(LF_PoliceApplicationStaff.IsStaff("moderator", false), "the level column is not a uid");
		ctx.False(LF_PoliceApplicationStaff.IsStaff("Jasper", false), "a #pending line is a comment, not a uid");
		ctx.False(LF_PoliceApplicationStaff.IsStaff(civilian, false), "an account off the roster is not");
		ctx.False(LF_PoliceApplicationStaff.IsStaff("", false), "an empty uid is not staff");
		ctx.True(LF_PoliceApplicationStaff.IsStaff(civilian, true), "the engine administrator is staff");

		// Non-staff: refused, nothing changes.
		string refusal;
		array<ref LF_PoliceApplication> rows = LF_PoliceApplicationStaff.ListOpen(civilian, false, refusal);
		ctx.EqualStr("#LF-PoliceApp_NotAdmin", refusal, "a non-staff list is refused");
		ctx.Equal(0, rows.Count(), "and returns nothing");
		ctx.EqualStr("#LF-PoliceApp_NotAdmin", LF_PoliceApplicationStaff.ApproveByUid(civilian, false, ann), "a non-staff approve is refused");
		ctx.EqualStr("#LF-PoliceApp_NotAdmin", LF_PoliceApplicationStaff.RejectByUid(civilian, false, ann, rejectedAt), "a non-staff reject is refused");
		ctx.True(LF_PoliceApplications.HasOpen(ann), "the refused calls left the application open");
		ctx.False(LF_WhitelistManager.IsWhitelistedForFaction(ann, LF_EFactionType.POLICE), "and off the roster");
		ctx.Equal(0, LF_PoliceApplications.LastRejectedAt(ann), "and no cooldown");

		// Staff list.
		rows = LF_PoliceApplicationStaff.ListOpen(staff, false, refusal);
		ctx.EqualStr("", refusal, "a staff list is allowed");
		ctx.Equal(2, rows.Count(), "it holds both open applications");
		ctx.EqualStr(ann, rows[0].m_sUid, "oldest first");
		rows[0].m_sStatus = "APPROVED";
		ctx.True(LF_PoliceApplications.HasOpen(ann), "the list is a copy: editing it changes nothing");

		// Bad payloads.
		ctx.EqualStr("#LF-PoliceApp_Error_NoAccount", LF_PoliceApplicationStaff.ApproveByUid(staff, false, ""), "an empty target is refused");
		ctx.EqualStr("#LF-PoliceApp_Error_NoAccount", LF_PoliceApplicationStaff.ApproveByUid(staff, false, "a|b"), "a target with the file separator is refused");
		string longUid = "x";
		while (longUid.Length() <= LF_PoliceApplicationStaff.UID_MAX_LENGTH)
			longUid = longUid + longUid;
		ctx.EqualStr("#LF-PoliceApp_Error_NoAccount", LF_PoliceApplicationStaff.RejectByUid(staff, false, longUid, rejectedAt), "an over-long target is refused");
		ctx.EqualStr("#LF-PoliceApp_NoneOpen", LF_PoliceApplicationStaff.ApproveByUid(staff, false, "test-staff-nobody"), "a target with no open application is refused");

		// Staff approve and reject.
		ctx.EqualStr("", LF_PoliceApplicationStaff.ApproveByUid(staff, false, ann), "a staff approve is accepted");
		ctx.True(LF_WhitelistManager.IsWhitelistedForFaction(ann, LF_EFactionType.POLICE), "approval puts the uid on the police roster");
		ctx.False(LF_PoliceApplications.HasOpen(ann), "ann is no longer open");
		ctx.EqualStr("#LF-PoliceApp_NoneOpen", LF_PoliceApplicationStaff.ApproveByUid(staff, false, ann), "a second approve finds nothing open");

		ctx.EqualStr("", LF_PoliceApplicationStaff.RejectByUid(civilian, true, bo, rejectedAt), "an engine administrator rejects");
		ctx.Equal(rejectedAt, LF_PoliceApplications.LastRejectedAt(bo), "a reject records the time");
		ctx.False(LF_WhitelistManager.IsWhitelistedForFaction(bo, LF_EFactionType.POLICE), "bo is not on the roster");
		ctx.EqualStr("#LF-PoliceApp_Error_Cooldown", LF_PoliceApplications.SubmitGated(bo, "Bo Kay", "Again", "Weekends", false, 0, 0, 72, rejectedAt + hour), "a reject starts the cooldown");
		ctx.Equal(0, LF_PoliceApplicationStaff.ListOpen(staff, false, refusal).Count(), "nothing is open after both decisions");

		FileIO.DeleteFile(appsPath);
		FileIO.DeleteFile(rosterPath);
		FileIO.DeleteFile(whitelistPath);
		LF_PoliceApplications.SetSavePath("");
		LF_PoliceApplicationStaff.SetStaffPath("");
		LF_WhitelistManager.SetSavePath("");
	}
}
