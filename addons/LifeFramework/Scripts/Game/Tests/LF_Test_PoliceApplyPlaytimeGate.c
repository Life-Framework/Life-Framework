// red-proof: pending
// tier: LOGIC
class LF_Test_PoliceApplyPlaytimeGate : LF_Test
{
	override string GetName()
	{
		return "police/application-playtime-gate";
	}

	override void Run(LF_TestContext ctx)
	{
		int left;
		int fiftyHours = 50 * 3600;
		ctx.EqualStr("#LF-PoliceApp_Error_Playtime", LF_PoliceApplicationRules.RefuseSubmitGated(false, false, fiftyHours - 600, 50, 0, 72, 1000000, left), "ten minutes short of the minimum is refused");
		ctx.Equal(600, left, "the refusal carries the time left");
		ctx.EqualStr("", LF_PoliceApplicationRules.RefuseSubmitGated(false, false, fiftyHours, 50, 0, 72, 1000000, left), "exactly the minimum may apply");
		ctx.Equal(0, left, "no wait left");
		ctx.EqualStr("", LF_PoliceApplicationRules.RefuseSubmitGated(false, false, 0, 0, 0, 0, 1000000, left), "a minimum of 0 hours turns the gate off");
		ctx.EqualStr("#LF-PoliceApp_Error_OnRoster", LF_PoliceApplicationRules.RefuseSubmitGated(true, false, 0, 50, 0, 72, 1000000, left), "the roster refusal still comes first");
		ctx.EqualStr("2h 15m", LF_PoliceApplicationRules.FormatDuration(2 * 3600 + 15 * 60), "the wait reads as hours and minutes");
		ctx.EqualStr("40m", LF_PoliceApplicationRules.FormatDuration(40 * 60), "under an hour reads as minutes");
		ctx.EqualStr("3h", LF_PoliceApplicationRules.FormatDuration(3 * 3600), "whole hours drop the minutes");
		ctx.EqualStr("1m", LF_PoliceApplicationRules.FormatDuration(1), "a part minute rounds up");

		string path = "$profile:LifeFramework/test_police_gate_playtime.txt";
		LF_PoliceApplications.SetSavePath(path);
		FileIO.DeleteFile(path);
		string uid = "test-gate-playtime";
		ctx.EqualStr("#LF-PoliceApp_Error_Playtime", LF_PoliceApplications.SubmitGated(uid, "Ann Lee", "To help", "Evenings", false, 3600, 2, 0, 1000000), "the store refuses a short account");
		ctx.Equal(3600, LF_PoliceApplications.GetLastSecondsLeft(), "and names an hour left");
		ctx.False(LF_PoliceApplications.HasOpen(uid), "a refused application is not filed");
		ctx.EqualStr("", LF_PoliceApplications.SubmitGated(uid, "Ann Lee", "To help", "Evenings", false, 7200, 2, 0, 1000000), "an account at the minimum files");
		ctx.True(LF_PoliceApplications.HasOpen(uid), "it is open");

		FileIO.DeleteFile(path);
		LF_PoliceApplications.SetSavePath("");
	}
}
