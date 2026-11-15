// red-proof: RED-OK moderation/staff-relay (fails with the hook on, 9 assertions): one admin action and one flag give two messages (expected 2, got 0)
// tier: LOGIC
//! moderation/staff-relay (#1284): one admin action and one report flag, each published on the bus,
//! reach the staff webhook as two lines once their windows end; ten flags inside one window give
//! one batched line; nothing is sent before the window ends. A captured sender stands in for the POST.
class LF_Test_StaffRelay : LF_Test
{
	override string GetName()
	{
		return "moderation/staff-relay";
	}

	override void Run(LF_TestContext ctx)
	{
		// The game mode installs the relay at start: drop that one (subscriber and tick) so the test has the only one.
		LF_StaffRelay.Uninstall();
		ref array<string> sink = new array<string>();
		LF_StaffWebhook.SetCaptureForTest(sink);
		ctx.True(LF_StaffRelay.EnsureSubscribed(), "the relay subscribes to both events");

		int start = System.GetUnixTime();
		int after = start + LF_StaffRelay.WINDOW_SECONDS + 1;

		// One admin action and one flag, through the bus: two lines.
		LF_EventBus.Publish(LF_OnAdminAction.Create("relay-admin", "relay-target", "#kick", vector.Zero));
		LF_EventBus.Publish(LF_OnIncidentFlagged.Create("relay-rep", 2));
		ctx.Equal(0, sink.Count(), "nothing goes out before the window ends");
		if (!RedProof())
			ctx.Equal(2, LF_StaffRelay.Flush(after), "both windows end and flush");

		ctx.Equal(2, sink.Count(), "one admin action and one flag give two messages");
		if (sink.Count() == 2)
		{
			ctx.True(sink[0].Contains("relay-admin") && sink[0].Contains("#kick") && sink[0].Contains("relay-target"), "the admin line names who, what and on whom");
			ctx.True(sink[1].Contains("relay-rep") && sink[1].Contains("2"), "the flag line names the reporter and the count");
		}

		ctx.Equal(0, LF_StaffRelay.Flush(after + 100), "nothing is left to send");

		// Ten flags in one window: one batched line.
		sink.Clear();
		int i;
		for (i = 0; i < 10; i++)
			LF_EventBus.Publish(LF_OnIncidentFlagged.Create("relay-rep-" + i.ToString(), 1));

		ctx.Equal(0, sink.Count(), "ten flags inside the window send nothing yet");
		LF_StaffRelay.Flush(System.GetUnixTime() + LF_StaffRelay.WINDOW_SECONDS + 1);
		ctx.Equal(1, sink.Count(), "ten flags give one batched message");
		if (sink.Count() == 1)
			ctx.True(sink[0].Contains("10 reports") && sink[0].Contains("10 incidents"), "the batch counts reports and incidents");

		LF_StaffWebhook.SetCaptureForTest(null);
		LF_StaffRelay.Uninstall();
	}
}
