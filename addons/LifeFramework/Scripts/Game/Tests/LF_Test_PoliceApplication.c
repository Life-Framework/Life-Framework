// red-proof: make CanApprove return true for a non-admin; the "non-admin cannot approve" assertion fails.
// tier: LOGIC
class LF_Test_PoliceApplication : LF_Test
{
	override string GetName()
	{
		return "police/application";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.EqualStr("", LF_PoliceApplicationRules.RefuseForm("Ann Lee", "To help", "Evenings"), "a full form is accepted");
		ctx.EqualStr("#LF-PoliceApp_Error_Fields", LF_PoliceApplicationRules.RefuseForm("Ann Lee", "", "Evenings"), "a blank reason is refused");
		ctx.EqualStr("#LF-PoliceApp_Error_Fields", LF_PoliceApplicationRules.RefuseForm("", "To help", "Evenings"), "a blank name is refused");

		string longText = "x";
		while (longText.Length() <= LF_PoliceApplicationRules.WHY_MAX_LENGTH)
			longText = longText + longText;
		ctx.EqualStr("#LF-PoliceApp_Error_Length", LF_PoliceApplicationRules.RefuseForm("Ann Lee", longText, "Evenings"), "an over-long reason is refused");

		ctx.EqualStr("#LF-PoliceApp_Error_OnRoster", LF_PoliceApplicationRules.RefuseSubmit(true, false), "an officer cannot apply");
		ctx.EqualStr("#LF-PoliceApp_Error_AlreadyOpen", LF_PoliceApplicationRules.RefuseSubmit(false, true), "one open application per player");
		ctx.EqualStr("", LF_PoliceApplicationRules.RefuseSubmit(false, false), "a civilian with nothing open may apply");
		ctx.True(LF_PoliceApplicationRules.CanApprove(true), "an admin can approve");
		ctx.False(LF_PoliceApplicationRules.CanApprove(false), "a non-admin cannot approve");
		ctx.EqualStr("a/b c", LF_PoliceApplicationRules.Clean("  a|b\nc "), "the file separators are cleaned");

		string uid = "test-police-applicant";
		LF_PoliceApplications.SetSavePath("$profile:LifeFramework/test_police_applications.txt");
		LF_WhitelistManager.SetSavePath("$profile:LifeFramework/test_police_app_whitelist.txt");
		LF_WhitelistManager.Reset();
		FileIO.DeleteFile("$profile:LifeFramework/test_police_applications.txt");

		ctx.EqualStr("", LF_PoliceApplications.Submit(uid, "Ann Lee", "To help", "Evenings", false), "the application is filed");
		ctx.True(LF_PoliceApplications.HasOpen(uid), "it is open");
		ctx.EqualStr("#LF-PoliceApp_Error_AlreadyOpen", LF_PoliceApplications.Submit(uid, "Ann Lee", "Again", "Evenings", false), "a second application is refused");
		ctx.False(LF_PoliceApplications.Approve(uid, false), "a non-admin approval does nothing");
		ctx.False(LF_WhitelistManager.IsWhitelistedForFaction(uid, LF_EFactionType.POLICE), "still not on the roster");
		ctx.True(LF_PoliceApplications.Approve(uid, true), "an admin approves");
		ctx.True(LF_WhitelistManager.IsWhitelistedForFaction(uid, LF_EFactionType.POLICE), "approval puts the player on the roster");
		ctx.False(LF_PoliceApplications.HasOpen(uid), "no longer open");

		LF_PoliceApplications.SetSavePath("$profile:LifeFramework/test_police_applications.txt");
		ctx.False(LF_PoliceApplications.HasOpen(uid), "the approved record reloads as approved");
		ctx.Equal(0, LF_PoliceApplications.CountOpen(), "nothing is open after reload");

		FileIO.DeleteFile("$profile:LifeFramework/test_police_applications.txt");
		FileIO.DeleteFile("$profile:LifeFramework/test_police_app_whitelist.txt");
		LF_PoliceApplications.SetSavePath("");
		LF_WhitelistManager.SetSavePath("");
		LF_WhitelistManager.Reset();
	}
}
