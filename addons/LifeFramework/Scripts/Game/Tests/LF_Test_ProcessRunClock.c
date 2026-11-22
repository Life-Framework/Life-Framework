// red-proof: 2026-11-22 `node tools/cli.mjs test --no-build --red LF_Test_OreStackCount,LF_Test_TillSellKey,LF_Test_ProcessRunClock`: RED PROOF OK (all 3 fail with the hook on). RedProof() ends the run as it starts.
//! processing/run-clock (first-2h review B10): a timed machine's row reads the time left of its run as
//! m:ss, counted from the run's end in unix seconds, so a late joiner reads the same time.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ProcessRunClock : LF_Test
{
	override string GetName()
	{
		return "processing/run-clock";
	}

	override void Run(LF_TestContext ctx)
	{
		int now = 1800000000;
		int ends = now + 240;
		if (RedProof())
			ends = now;

		ctx.Equal(240, LF_ProcessRunFxComponent.SecondsLeft(ends, now), "a run of 240 s that just started has 240 s left");
		ctx.Equal(48, LF_ProcessRunFxComponent.SecondsLeft(ends, now + 192), "192 s in, 48 s are left");
		ctx.Equal(0, LF_ProcessRunFxComponent.SecondsLeft(ends, now + 300), "a run past its end has none left");
		ctx.Equal(0, LF_ProcessRunFxComponent.SecondsLeft(0, now), "no run: nothing left");

		ctx.EqualStr("4:00", LF_ProcessRunFxComponent.Clock(LF_ProcessRunFxComponent.SecondsLeft(ends, now)), "240 s reads 4:00");
		ctx.EqualStr("3:12", LF_ProcessRunFxComponent.Clock(192), "192 s reads 3:12");
		ctx.EqualStr("0:05", LF_ProcessRunFxComponent.Clock(5), "seconds under ten keep their leading zero");
		ctx.EqualStr("0:00", LF_ProcessRunFxComponent.Clock(-3), "a negative count reads 0:00");
	}
}
