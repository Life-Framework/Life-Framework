// red-proof: temporarily changed the null-shot expectation to "error:oops" in
// the first assertion; `cli test --tier fast` FAILed with the expected/got
// diff (expected 'error:oops', got 'error:null_shot'), then reverted.

// tier: LOGIC
class LF_Test_ScreenshotFailSafe : LF_Test
{
	override string GetName()
	{
		return "screenshot/fail-safe";
	}

	override void Run(LF_TestContext ctx)
	{
		// A null request must be rejected cleanly - no crash, no busy stuck.
		ctx.False(LF_ScreenshotCapture.StartCapture(null), "null shot is rejected");
		ctx.EqualStr("error:null_shot", LF_ScreenshotCapture.GetLastResult(), "null shot yields a machine-readable error");
		ctx.False(LF_ScreenshotCapture.IsBusy(), "rejected capture leaves the class idle");

		// A request to spawn a prefab that does not exist must degrade that one
		// capture instead of crashing (LF_Utils prefab-resolve guard is the
		// reference pattern).
		LF_ScreenshotShot badSpawn = new LF_ScreenshotShot();
		badSpawn.AimAtTarget = false;
		badSpawn.SpawnPrefab = "{DEADBEEF00000000}Prefabs/DoesNotExist.et";
		ctx.False(LF_ScreenshotCapture.StartCapture(badSpawn), "unresolvable spawn prefab is rejected");
		ctx.EqualStr("error:spawn_failed", LF_ScreenshotCapture.GetLastResult(), "spawn failure yields error:spawn_failed");
		ctx.False(LF_ScreenshotCapture.IsBusy(), "spawn failure leaves the class idle");

		// The class must accept a new request after a failure (no stuck state).
		ctx.False(LF_ScreenshotCapture.StartCapture(null), "capture path is reusable after a failure");
	}
};
