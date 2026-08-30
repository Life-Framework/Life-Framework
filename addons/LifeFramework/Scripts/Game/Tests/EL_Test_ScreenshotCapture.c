// red-proof: temporarily changed the null-shot expectation to "error:oops" in
// the first assertion; `cli test --tier fast` FAILed with the expected/got
// diff (expected 'error:oops', got 'error:null_shot'), then reverted.

// tier: LOGIC
class EL_Test_ScreenshotFailSafe : EL_Test
{
	override string GetName()
	{
		return "screenshot/fail-safe";
	}

	override void Run(EL_TestContext ctx)
	{
		// A null request must be rejected cleanly - no crash, no busy stuck.
		ctx.False(EL_ScreenshotCapture.StartCapture(null), "null shot is rejected");
		ctx.EqualStr("error:null_shot", EL_ScreenshotCapture.GetLastResult(), "null shot yields a machine-readable error");
		ctx.False(EL_ScreenshotCapture.IsBusy(), "rejected capture leaves the class idle");

		// A request to spawn a prefab that does not exist must degrade that one
		// capture instead of crashing (EL_Utils prefab-resolve guard is the
		// reference pattern).
		EL_ScreenshotShot badSpawn = new EL_ScreenshotShot();
		badSpawn.AimAtTarget = false;
		badSpawn.SpawnPrefab = "{DEADBEEF00000000}Prefabs/DoesNotExist.et";
		ctx.False(EL_ScreenshotCapture.StartCapture(badSpawn), "unresolvable spawn prefab is rejected");
		ctx.EqualStr("error:spawn_failed", EL_ScreenshotCapture.GetLastResult(), "spawn failure yields error:spawn_failed");
		ctx.False(EL_ScreenshotCapture.IsBusy(), "spawn failure leaves the class idle");

		// On a headless server (no camera manager, no registered camera) an
		// aim-at-target capture must fail safe rather than VME.
		EL_ScreenshotShot aim = new EL_ScreenshotShot();
		ctx.False(EL_ScreenshotCapture.StartCapture(aim), "aim capture rejected when no camera is available");
		ctx.False(EL_ScreenshotCapture.IsBusy(), "aim failure leaves the class idle");

		// The class must accept a new request after a failure (no stuck state).
		ctx.False(EL_ScreenshotCapture.StartCapture(null), "capture path is reusable after a failure");
	}
};