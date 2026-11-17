// red-proof: Observed 2026-11-17 `tools/cli test --no-build --only LF_Test_PoliceStaticsReset` with the application reset and the roster trim disabled failed `police/statics-reset` with `- the world-end reset drops the cached rows and the file override` and `- the sent list is capped (expected 64, got 74)`. Restored: green.
// tier: LOGIC
//! #2419: the police application list has a world-end reset hook, and the roster notice's sent list
//! is trimmed instead of growing for ever.
class LF_Test_PoliceStaticsReset : LF_Test
{
	protected static const string PATH = "$profile:LifeFramework/test_police_statics_reset.txt";

	override string GetName()
	{
		return "police/statics-reset";
	}

	override void Run(LF_TestContext ctx)
	{
		array<string> hooks = LF_StaticReset.Registered();
		ctx.True(hooks.Contains("LF_PoliceApplications"), "LF_PoliceApplications registers its reset hook");
		ctx.True(hooks.Contains("LF_Hideout"), "LF_Hideout registers its reset hook");

		string uid = "test-police-statics-reset";
		FileIO.DeleteFile(PATH);
		LF_PoliceApplications.SetSavePath(PATH);
		ctx.EqualStr("", LF_PoliceApplications.Submit(uid, "Ann Lee", "To help", "Evenings", false), "an application is filed");
		ctx.True(LF_PoliceApplications.HasOpen(uid), "it is open");
		LF_PoliceApplications.ResetStatics();
		ctx.False(LF_PoliceApplications.HasOpen(uid), "the world-end reset drops the cached rows and the file override");
		FileIO.DeleteFile(PATH);

		LF_PoliceRosterNotice.ResetStatics();
		int total = LF_PoliceRosterNotice.MAX_SENT + 10;
		for (int i = 0; i < total; i++)
		{
			LF_PoliceRosterNotice.RememberSent(string.Format("%1|key", i));
		}

		ctx.Equal(LF_PoliceRosterNotice.MAX_SENT, LF_PoliceRosterNotice.SentCount(), "the sent list is capped");
		ctx.EqualStr(string.Format("%1|key", total - 1), LF_PoliceRosterNotice.SentAt(LF_PoliceRosterNotice.MAX_SENT - 1), "the newest notice is kept");
		ctx.EqualStr("10|key", LF_PoliceRosterNotice.SentAt(0), "the oldest notices are the ones dropped");
		LF_PoliceRosterNotice.ResetStatics();
	}
}
