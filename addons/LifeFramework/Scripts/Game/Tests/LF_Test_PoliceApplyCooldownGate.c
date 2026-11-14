// red-proof: pending
// tier: LOGIC
class LF_Test_PoliceApplyCooldownGate : LF_Test
{
	override string GetName()
	{
		return "police/application-cooldown-gate";
	}

	override void Run(LF_TestContext ctx)
	{
		int left;
		int hour = 3600;
		int rejectedAt = 1000000;
		ctx.EqualStr("#LF-PoliceApp_Error_Cooldown", LF_PoliceApplicationRules.RefuseSubmitGated(false, false, 0, 0, rejectedAt, 72, rejectedAt + hour, left), "an hour after a rejection is refused");
		ctx.Equal(71 * hour, left, "the refusal carries the time left");
		ctx.EqualStr("", LF_PoliceApplicationRules.RefuseSubmitGated(false, false, 0, 0, rejectedAt, 72, rejectedAt + 72 * hour, left), "the cooldown over, the applicant may apply");
		ctx.EqualStr("", LF_PoliceApplicationRules.RefuseSubmitGated(false, false, 0, 0, rejectedAt, 0, rejectedAt + hour, left), "a cooldown of 0 hours turns the gate off");
		ctx.EqualStr("", LF_PoliceApplicationRules.RefuseSubmitGated(false, false, 0, 0, 0, 72, rejectedAt, left), "no rejection, no wait");

		string path = "$profile:LifeFramework/test_police_gate_cooldown.txt";
		LF_PoliceApplications.SetSavePath(path);
		FileIO.DeleteFile(path);
		string uid = "test-gate-cooldown";
		ctx.EqualStr("", LF_PoliceApplications.Submit(uid, "Bo Kay", "To help", "Evenings", false), "the application is filed");
		ctx.False(LF_PoliceApplications.Reject(uid, false, rejectedAt), "a non-admin cannot reject");
		ctx.True(LF_PoliceApplications.Reject(uid, true, rejectedAt), "an admin rejects");
		ctx.False(LF_PoliceApplications.HasOpen(uid), "a rejected application is no longer open");
		ctx.Equal(rejectedAt, LF_PoliceApplications.LastRejectedAt(uid), "the rejection time is kept");
		ctx.EqualStr("#LF-PoliceApp_Error_Cooldown", LF_PoliceApplications.SubmitGated(uid, "Bo Kay", "Again", "Evenings", false, 0, 0, 72, rejectedAt + 2 * hour), "re-applying inside the cooldown is refused");
		ctx.Equal(70 * hour, LF_PoliceApplications.GetLastSecondsLeft(), "naming 70 hours left");

		// The cooldown survives a restart: the file is read back.
		LF_PoliceApplications.SetSavePath(path);
		ctx.Equal(rejectedAt, LF_PoliceApplications.LastRejectedAt(uid), "the rejection reloads from the file");
		ctx.EqualStr("#LF-PoliceApp_Error_Cooldown", LF_PoliceApplications.SubmitGated(uid, "Bo Kay", "Again", "Evenings", false, 0, 0, 72, rejectedAt + 2 * hour), "still refused after the reload");
		ctx.EqualStr("", LF_PoliceApplications.SubmitGated(uid, "Bo Kay", "Again", "Evenings", false, 0, 0, 72, rejectedAt + 73 * hour), "and accepted once the cooldown is over");

		FileIO.DeleteFile(path);
		LF_PoliceApplications.SetSavePath("");
	}
}
