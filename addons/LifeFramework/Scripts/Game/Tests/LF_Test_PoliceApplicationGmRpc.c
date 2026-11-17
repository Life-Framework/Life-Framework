// red-proof: RedProof() skips the staff approve: "RED PROOF OK (all 1 listed test(s) fail with the hook on)", red lines "the answer names the applicant (expected 'Ann Lee', got '')" (2026-11-15)
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
		string moderator = "test-gm-moderator";
		string civilian = "test-gm-civilian";
		string ann = "test-gm-ann";
		string bo = "test-gm-bo";
		int now = 3000000;

		FileHandle handle = FileIO.OpenFile(rosterPath, FileMode.WRITE);
		handle.WriteLine(staff + " Administrator # Test Admin");
		handle.WriteLine(moderator + " moderator # Test Mod");
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

		// A moderator (#1320 roster level): may list, may not decide; the refusal is logged.
		ctx.EqualStr("moderator", LF_PoliceApplicationGm.LevelOf(moderator), "the roster level is read");
		ctx.EqualStr("administrator", LF_PoliceApplicationGm.LevelOf(staff), "the level is compared in lower case");
		ctx.EqualStr("", LF_PoliceApplicationGm.LevelOf(civilian), "an unlisted uid has no level");
		LF_PoliceApplicationGm.AnswerList(moderator, false, now, refusal);
		ctx.EqualStr("", refusal, "a moderator may list");
		int refusalsBefore = LF_PoliceApplicationGm.LevelRefusals();
		ctx.EqualStr("#LF-PoliceAppGm_NeedAdministrator", LF_PoliceApplicationGm.AnswerDecision(moderator, false, LF_PoliceApplicationGm.VERB_APPROVE, ann, now, name), "a moderator approve is refused");
		ctx.EqualStr("#LF-PoliceAppGm_NeedAdministrator", LF_PoliceApplicationGm.AnswerDecision(moderator, false, LF_PoliceApplicationGm.VERB_REJECT, bo, now, name), "a moderator reject is refused");
		ctx.Equal(refusalsBefore + 2, LF_PoliceApplicationGm.LevelRefusals(), "both moderator refusals were logged");
		ctx.True(LF_PoliceApplications.HasOpen(ann), "the moderator approve left ann open");
		ctx.False(LF_WhitelistManager.IsWhitelistedForFaction(ann, LF_EFactionType.POLICE), "and off the police roster");
		ctx.Equal(0, LF_PoliceApplications.LastRejectedAt(bo), "and the moderator reject started no cooldown");

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
		forged.m_sName = "Eve|x|y %2";
		forged.m_sWhy = "#LF-PoliceApp_Approved";
		forged.m_sAvailability = "Any";
		forged.m_iTime = now - 7200;
		array<ref LF_PoliceApplication> forgedList = {};
		forgedList.Insert(forged);
		parts.Clear();
		LF_PoliceApplicationGm.BuildRows(forgedList, now)[0].Split(LF_PoliceApplicationGm.SEP, parts, false);
		ctx.Equal(5, parts.Count(), "a name with the separator does not add a field");
		ctx.EqualStr("LF-PoliceApp_Approved", parts[2], "a leading # is taken off free text");
		ctx.Equal(-1, parts[1].IndexOf("%"), "a format argument is taken out of free text");
		ctx.EqualStr("7200", parts[4], "the row carries the seconds waited");

		// Bad payloads from staff.
		ctx.EqualStr("#LF-PoliceAppGm_BadRequest", LF_PoliceApplicationGm.AnswerDecision(staff, false, 7, ann, now, name), "an unknown verb is refused");
		ctx.EqualStr("#LF-PoliceAppGm_BadRequest", LF_PoliceApplicationGm.AnswerDecision(staff, false, LF_PoliceApplicationGm.VERB_APPROVE, "a|b", now, name), "a malformed target is refused");
		ctx.EqualStr("#LF-PoliceApp_NoneOpen", LF_PoliceApplicationGm.AnswerDecision(staff, false, LF_PoliceApplicationGm.VERB_APPROVE, "test-gm-nobody", now, name), "a target with nothing open is refused");
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

		// The answer line of a decision.
		ctx.EqualStr("#LF-PoliceAppGm_Approved", LF_PoliceApplicationGm.ResultFor(LF_PoliceApplicationGm.VERB_APPROVE, ""), "an approve answers Approved");
		ctx.EqualStr("#LF-PoliceAppGm_Rejected", LF_PoliceApplicationGm.ResultFor(LF_PoliceApplicationGm.VERB_REJECT, ""), "a reject answers Rejected");
		ctx.EqualStr("#LF-PoliceAppGm_NeedAdministrator", LF_PoliceApplicationGm.ResultFor(LF_PoliceApplicationGm.VERB_APPROVE, "#LF-PoliceAppGm_NeedAdministrator"), "a refusal answers the refusal");

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
		LF_WhitelistManager.Reset();
	}
}
