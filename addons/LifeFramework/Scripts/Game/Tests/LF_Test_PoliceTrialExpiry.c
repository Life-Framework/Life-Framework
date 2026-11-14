// red-proof: TrialExpired returns false: '[LFTEST] FAIL police/trial-expiry', restored (2026-11-14)
// tier: LOGIC
class LF_Test_PoliceTrialExpiry : LF_Test
{
	override string GetName()
	{
		return "police/trial-expiry";
	}

	override void Run(LF_TestContext ctx)
	{
		int hour = 3600;
		int start = 2000000;
		ctx.Equal(start + 168 * hour, LF_PoliceApplicationRules.TrialEndsAt(start, 168), "a trial runs the configured hours");
		ctx.Equal(0, LF_PoliceApplicationRules.TrialEndsAt(start, 0), "0 hours means no trial");
		ctx.False(LF_PoliceApplicationRules.TrialExpired(start + 100, start + 99), "a running trial has not expired");
		ctx.True(LF_PoliceApplicationRules.TrialExpired(start + 100, start + 100), "it expires when the clock reaches its end");
		ctx.False(LF_PoliceApplicationRules.TrialExpired(0, start), "no trial never expires");

		string path = "$profile:LifeFramework/test_police_gate_trial.txt";
		LF_PoliceApplications.SetSavePath(path);
		LF_WhitelistManager.SetSavePath("$profile:LifeFramework/test_police_gate_trial_whitelist.txt");
		LF_WhitelistManager.Reset();
		FileIO.DeleteFile(path);

		string expires = "test-trial-expires";
		string promoted = "test-trial-promoted";
		string untried = "test-trial-none";
		ctx.EqualStr("", LF_PoliceApplications.Submit(expires, "Officer One", "To help", "Evenings", false), "first filed");
		ctx.EqualStr("", LF_PoliceApplications.Submit(promoted, "Officer Two", "To help", "Evenings", false), "second filed");
		ctx.EqualStr("", LF_PoliceApplications.Submit(untried, "Officer Three", "To help", "Evenings", false), "third filed");
		ctx.True(LF_PoliceApplications.ApproveAt(expires, start, 168), "approved on trial");
		ctx.True(LF_PoliceApplications.ApproveAt(promoted, start, 168), "approved on trial and later promoted");
		ctx.True(LF_PoliceApplications.ApproveAt(untried, start, 0), "approved with no trial");
		ctx.True(LF_WhitelistManager.IsWhitelistedForFaction(expires, LF_EFactionType.POLICE), "on the roster");

		ctx.True(LF_PoliceApplications.EndTrial(promoted), "a promotion ends the trial");
		ctx.False(LF_PoliceApplications.EndTrial(promoted), "once");

		ctx.Equal(0, LF_PoliceApplications.ExpireTrials(start + 167 * hour), "nobody leaves before the trial ends");
		ctx.True(LF_WhitelistManager.IsWhitelistedForFaction(expires, LF_EFactionType.POLICE), "still on the roster before the end");

		// A restart in the middle of the trial: the end time is read back.
		LF_PoliceApplications.SetSavePath(path);
		ctx.Equal(1, LF_PoliceApplications.ExpireTrials(start + 168 * hour), "one trial ends unpromoted");
		ctx.False(LF_WhitelistManager.IsWhitelistedForFaction(expires, LF_EFactionType.POLICE), "the unpromoted officer leaves the roster");
		ctx.True(LF_WhitelistManager.IsWhitelistedForFaction(promoted, LF_EFactionType.POLICE), "the promoted officer stays");
		ctx.True(LF_WhitelistManager.IsWhitelistedForFaction(untried, LF_EFactionType.POLICE), "an officer with no trial stays");
		ctx.Equal(0, LF_PoliceApplications.ExpireTrials(start + 1000 * hour), "nothing left to expire");

		// The legacy six-field row still loads.
		FileIO.DeleteFile(path);
		FileHandle handle = FileIO.OpenFile(path, FileMode.WRITE);
		handle.WriteLine("OPEN|test-legacy|Old Row|why|evenings|1234");
		handle.Close();
		LF_PoliceApplications.SetSavePath(path);
		ctx.True(LF_PoliceApplications.HasOpen("test-legacy"), "a six-field row from before the gates loads");

		FileIO.DeleteFile(path);
		FileIO.DeleteFile("$profile:LifeFramework/test_police_gate_trial_whitelist.txt");
		LF_PoliceApplications.SetSavePath("");
		LF_WhitelistManager.SetSavePath("");
		LF_WhitelistManager.Reset();
	}
}
