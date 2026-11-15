// red-proof: Observed 2026-11-15: with LF_RestartClose a do-nothing skeleton, `tools/cli test --no-build --only LF_Test_RestartClose` failed (no close request, no marker); with the subscriber, green.

// tier: LOGIC
//! O8 (#1267): when LF_OnRestartDue is published, LF_RestartClose writes the restart marker for the
//! host watchdog and only then asks the engine for a normal close (the SHUTDOWN save; O9 proves it).
//! The close seam records the request instead of closing; the marker goes to a test path so the
//! watchdog never sees one. A second due event while closing does not ask twice.
class LF_Test_RestartClose : LF_Test
{
	static const string TEST_MARKER = "$profile:lf-test-host-restart";

	override string GetName()
	{
		return "ops/restart-close";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_EventBus.ResetStatics();
		LF_RestartClose.ResetStatics();
		if (FileIO.FileExists(TEST_MARKER))
			FileIO.DeleteFile(TEST_MARKER);

		LF_RestartClose.SetTestSeam(TEST_MARKER);
		ctx.True(LF_RestartClose.Register(), "the close subscribed to LF_OnRestartDue");
		ctx.False(LF_RestartClose.Register(), "a second Register does not subscribe twice");
		ctx.Equal(1, LF_EventBus.SubscriberCount(LF_OnRestartDue), "one close subscriber on the bus");

		// Before the restart time nothing is written and nothing closes.
		ctx.False(FileIO.FileExists(TEST_MARKER), "no marker before the restart is due");
		ctx.Equal(0, LF_RestartClose.GetCloseRequests(), "no close before the restart is due");

		// The red-proof hook skips the publish: no marker, no close, and the checks below fail.
		if (!RedProof())
			LF_EventBus.Publish(LF_OnRestartDue.Create());

		ctx.Equal(1, LF_RestartClose.GetCloseRequests(), "the due event asked for one normal close");
		ctx.True(LF_RestartClose.WasMarkerPresentAtClose(), "the marker was on disk when the close was requested");
		ctx.True(FileIO.FileExists(TEST_MARKER), "the restart marker was written to the profile");

		LF_EventBus.Publish(LF_OnRestartDue.Create());
		ctx.Equal(1, LF_RestartClose.GetCloseRequests(), "a second due event while closing does not ask again");

		if (FileIO.FileExists(TEST_MARKER))
			FileIO.DeleteFile(TEST_MARKER);
		LF_RestartClose.ResetStatics();
		LF_EventBus.ResetStatics();
	}
}
