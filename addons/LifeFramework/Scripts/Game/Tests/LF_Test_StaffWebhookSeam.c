// red-proof: RED-OK moderation/webhook-seam (fails with the hook on, 7 assertions): two Send calls record two lines (expected 2, got 0)
// tier: LOGIC
//! Staff webhook seam (#1346): with a capture installed Send records lines in order; with none it
//! only logs and returns. The real HTTP POST (S4) goes behind the same call.
class LF_Test_StaffWebhookSeam : LF_Test
{
	override string GetName()
	{
		return "moderation/webhook-seam";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_StaffWebhook.SetCaptureForTest(null);

		// No capture: Send logs one line and sends nothing, and never throws.
		LF_StaffWebhook.Send("lost line");
		ctx.False(LF_StaffWebhook.HasCapture(), "no capture is installed after a bare Send");

		// With a capture: two calls, two lines, in order.
		ref array<string> sink = new array<string>();
		LF_StaffWebhook.SetCaptureForTest(sink);
		ctx.True(LF_StaffWebhook.HasCapture(), "a capture is installed");

		if (!RedProof())
		{
			LF_StaffWebhook.Send("first line");
			LF_StaffWebhook.Send("second line");
		}

		ctx.Equal(2, sink.Count(), "two Send calls record two lines");
		if (sink.Count() == 2)
		{
			ctx.True(sink[0] == "first line", "the first line is recorded first");
			ctx.True(sink[1] == "second line", "the second line is recorded second");
		}

		// Clearing the capture stops recording.
		LF_StaffWebhook.SetCaptureForTest(null);
		LF_StaffWebhook.Send("third line");
		ctx.Equal(2, sink.Count(), "a cleared capture records nothing more");
		ctx.False(LF_StaffWebhook.HasCapture(), "the capture is cleared");
	}
}
