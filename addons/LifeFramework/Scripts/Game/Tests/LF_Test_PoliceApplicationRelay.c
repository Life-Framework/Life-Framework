// red-proof: RedProof() skips the File call, so no line reaches the capture (expected 1, got 0).
// tier: LOGIC
//! A filed police application sends one staff line (#1289): the uid tail, playtime and time, never
//! the full uid; a refused application sends nothing. Uses the StaffWebhook capture seam (#1346).
class LF_Test_PoliceApplicationRelay : LF_Test
{
	override string GetName()
	{
		return "police/application-relay";
	}

	override void Run(LF_TestContext ctx)
	{
		string path = "$profile:LifeFramework/test_police_application_relay.txt";
		string uid = "test-relay-applicant-9f3c";
		int now = 1800000000;

		LF_PoliceApplications.SetSavePath(path);
		FileIO.DeleteFile(path);

		ref array<string> sink = new array<string>();
		LF_StaffWebhook.SetCaptureForTest(sink);

		string refusal = "x";
		if (!RedProof())
			refusal = LF_PoliceApplicationRelay.File(uid, "Ann Lee", "To help", "Evenings", false, 45000, 0, 0, now);

		ctx.EqualStr("", refusal, "the application is filed");
		ctx.Equal(1, sink.Count(), "one staff line for one filed application");
		if (sink.Count() == 1)
		{
			string line = sink[0];
			ctx.True(line.IndexOf(uid) < 0, "the line does not carry the full uid");
			ctx.True(line.IndexOf("...9f3c") >= 0, "the line carries the uid tail");
			ctx.True(line.IndexOf("12h 30m") >= 0, "the line carries the playtime");
			ctx.True(line.IndexOf(LF_ClipStamp.FormatTime(now)) >= 0, "the line carries the time");
			ctx.True(line.IndexOf("Ann Lee") < 0, "the line does not carry the applicant's name");
		}

		// A refused application (already open) sends nothing more.
		string again = LF_PoliceApplicationRelay.File(uid, "Ann Lee", "Again", "Evenings", false, 45000, 0, 0, now);
		ctx.EqualStr("#LF-PoliceApp_Error_AlreadyOpen", again, "a second application is refused");
		ctx.Equal(1, sink.Count(), "a refused application sends no line");

		// A short uid still never goes out whole.
		ctx.EqualStr("c", LF_PoliceApplicationRelay.UidTail("abc"), "a 3-character uid keeps one character");
		ctx.EqualStr("", LF_PoliceApplicationRelay.UidTail("a"), "a 1-character uid keeps none");

		LF_StaffWebhook.SetCaptureForTest(null);
		FileIO.DeleteFile(path);
		LF_PoliceApplications.SetSavePath("");
	}
}
