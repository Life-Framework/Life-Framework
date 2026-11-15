// red-proof: RedProof() skips the playtest-shaped File call, so no line reaches the capture (expected 1, got 0).
// tier: LOGIC
//! The two calls the desk's application RPC makes (#1575): the playtest shape (gates 0/0/0) files and
//! sends one staff line; the gated shape under its playtime minimum is refused and sends nothing.
//! Uses the StaffWebhook capture seam (#1346). The RPC itself stays manual (needs a controller).
class LF_Test_PoliceApplicationDeskRelay : LF_Test
{
	override string GetName()
	{
		return "police/application-desk-relay";
	}

	override void Run(LF_TestContext ctx)
	{
		string path = "$profile:LifeFramework/test_police_application_desk_relay.txt";
		int now = 1800000000;

		LF_PoliceApplications.SetSavePath(path);
		FileIO.DeleteFile(path);

		ref array<string> sink = new array<string>();
		LF_StaffWebhook.SetCaptureForTest(sink);

		// Playtest mission: gates 0/0/0, a fresh account with no playtime still files and tells staff.
		string playtestRefusal = "x";
		if (!RedProof())
			playtestRefusal = LF_PoliceApplicationRelay.File("test-desk-playtest-a1b2", "Ann Lee", "To help", "Evenings", false, 0, 0, 0, now);
		ctx.EqualStr("", playtestRefusal, "the playtest application is filed");
		ctx.Equal(1, sink.Count(), "a filed application sends one staff line");

		// Real server: 10 h minimum, 1 h played. Refused, nothing sent.
		string gated = LF_PoliceApplicationRelay.File("test-desk-gated-c3d4", "Bo Ray", "To help", "Evenings", false, 3600, 10, 24, now);
		ctx.True(!gated.IsEmpty(), "an applicant under the playtime minimum is refused");
		ctx.Equal(1, sink.Count(), "a refused application sends no staff line");

		LF_StaffWebhook.SetCaptureForTest(null);
		FileIO.DeleteFile(path);
		LF_PoliceApplications.SetSavePath("");
	}
}
