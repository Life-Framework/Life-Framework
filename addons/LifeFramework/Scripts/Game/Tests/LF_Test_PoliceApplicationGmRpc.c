// red-proof: pending
// tier: LOGIC
//! #1288: the server handlers behind the Game Master applications panel. A non-staff sender is
//! refused and changes nothing; staff approve and reject; a bad verb or target is refused; free text
//! cannot forge a row or a string key.
class LF_Test_PoliceApplicationGmRpc : LF_Test
{
	override string GetName()
	{
		return "police/application-gm-rpc";
	}

	override void Run(LF_TestContext ctx)
	{
		string appsPath = "$profile:LifeFramework/test_police_gm_apps.txt";
		string rosterPath = "$profile:LifeFramework/test_police_gm_roster.txt";
		string whitelistPath = "$profile:LifeFramework/test_police_gm_whitelist.txt";
		LF_PoliceApplications.SetSavePath(appsPath);
		LF_PoliceApplicationStaff.SetStaffPath(rosterPath);
		LF_WhitelistManager.SetSavePath(whitelistPath);
		LF_WhitelistManager.Reset();
		FileIO.DeleteFile(appsPath);
		FileIO.DeleteFile(rosterPath);

		string staff = "test-gm-staff";
		string civilian = "test-gm-civilian";
		string ann = "test-gm-ann";
		string bo = "test-gm-bo";
		int now = 3000000;

		FileHandle handle = FileIO.OpenFile(rosterPath, FileMode.WRITE);
		handle.WriteLine(staff);
		handle.Close();

		ctx.EqualStr("", LF_PoliceApplications.Submit(ann, "Ann Lee", "To help", "Evenings", false), "ann's application is filed");
		ctx.EqualStr("", LF_PoliceApplications.Submit(bo, "Bo Kay", "To serve", "Weekends", false), "bo's application is filed");

		// A non-staff sender: the list is empty with the refusal, a decision is refused and changes nothing.
		string refusal;
		array<string> rows = LF_PoliceApplicationGm.AnswerList(civilian, false, now, refusal);
		ctx.EqualStr("#LF-PoliceApp_NotAdmin", refusal, "a non-staff list is refused");
		ctx.Equal(0, rows.Count(), "and carries no rows");

		string name;
		ctx.EqualStr("#LF-PoliceApp_NotAdmin", LF_PoliceApplicationGm.AnswerDecision(civilian, false, LF_PoliceApplicationGm.VERB_APPROVE, ann, now, name), "a non-staff approve is refused");
		ctx.EqualStr("", name, "a refusal names nobody");
		ctx.True(LF_PoliceApplications.HasOpen(ann), "the refused approve left ann open");
		ctx.False(LF_WhitelistManager.IsWhitelistedForFaction(ann, LF_EFactionType.POLICE), "and off the police roster");

		// Staff list: rows are flat, free text cannot carry the separator or a string key.
		rows = LF_PoliceApplicationGm.AnswerList(staff, false, now, refusal);
		ctx.EqualStr("", refusal, "a staff list is allowed");
		ctx.Equal(2, rows.Count(), "it lists both open applications");
		array<string> parts = {};
		rows[0].Split(LF_PoliceApplicationGm.SEP, parts, false);
		ctx.Equal(5, parts.Count(), "a row has five fields");
		ctx.EqualStr(ann, parts[0], "the first row is ann's uid");

		LF_PoliceApplication forged = new LF_PoliceApplication();
		forged.m_sUid = "test-gm-forged";
		forged.m_sName = "Eve|x|y";
		forged.m_sWhy = "#LF-PoliceApp_Approved";
		forged.m_sAvailability = "Any";
		forged.m_iTime = now - 7200;
		array<ref LF_PoliceApplication> forgedList = {};
		forgedList.Insert(forged);
		parts.Clear();
		LF_PoliceApplicationGm.BuildRows(forgedList, now)[0].Split(LF_PoliceApplicationGm.SEP, parts, false);
		ctx.Equal(5, parts.Count(), "a name with the separator does not add a field");
		ctx.EqualStr("LF-PoliceApp_Approved", parts[2], "a leading # is taken off free text");
		ctx.EqualStr("7200", parts[4], "the row carries the seconds waited");

		// Bad payloads from staff.
		ctx.EqualStr("#LF-PoliceAppGm_BadRequest", LF_PoliceApplicationGm.AnswerDecision(staff, false, 7, ann, now, name), "an unknown verb is refused");
		ctx.EqualStr("#LF-PoliceApp_Error_NoAccount", LF_PoliceApplicationGm.AnswerDecision(staff, false, LF_PoliceApplicationGm.VERB_APPROVE, "a|b", now, name), "a malformed target is refused");
		ctx.True(LF_PoliceApplications.HasOpen(ann), "bad payloads changed nothing");

		// Staff approve (the roster file) and reject (the engine administrator flag).
		string approved = "not run";
		if (!RedProof())
			approved = LF_PoliceApplicationGm.AnswerDecision(staff, false, LF_PoliceApplicationGm.VERB_APPROVE, ann, now, name);
		ctx.EqualStr("", approved, "a staff approve goes through");
		ctx.EqualStr("Ann Lee", name, "the answer names the applicant");
		ctx.True(LF_WhitelistManager.IsWhitelistedForFaction(ann, LF_EFactionType.POLICE), "approval puts ann on the police roster");

		ctx.EqualStr("", LF_PoliceApplicationGm.AnswerDecision(civilian, true, LF_PoliceApplicationGm.VERB_REJECT, bo, now, name), "an engine administrator rejects");
		ctx.Equal(now, LF_PoliceApplications.LastRejectedAt(bo), "the reject starts bo's cooldown");
		ctx.Equal(0, LF_PoliceApplicationGm.AnswerList(staff, false, now, refusal).Count(), "nothing is open after both decisions");

		// The ask throttle: a second ask inside the interval is dropped, a later one is taken.
		ctx.True(LF_PoliceApplicationGm.TakeAskSlot(-4242, 1000), "a first ask is taken");
		ctx.False(LF_PoliceApplicationGm.TakeAskSlot(-4242, 1100), "an ask 100 ms later is dropped");
		ctx.True(LF_PoliceApplicationGm.TakeAskSlot(-4242, 1000 + LF_PoliceApplicationGm.MIN_ASK_INTERVAL_MS), "an ask after the interval is taken");

		FileIO.DeleteFile(appsPath);
		FileIO.DeleteFile(rosterPath);
		FileIO.DeleteFile(whitelistPath);
		LF_PoliceApplications.SetSavePath("");
		LF_PoliceApplicationStaff.SetStaffPath("");
		LF_WhitelistManager.SetSavePath("");
	}
}
