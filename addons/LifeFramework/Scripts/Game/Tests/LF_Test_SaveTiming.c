// red-proof: observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_SaveTiming` (RedProof skips the slow part and the long frame):
//   red: the slowest parts, slowest first, under 2 ms left out (expected 'caseboard=12 carried=7 lastpos=3', got 'carried=7 lastpos=3')
//   red: the longest frame is the largest gap between ticks (expected 1500, got 34)
//   RED PROOF OK (all 1 listed test(s) fail with the hook on)
//! Save timing (#2945): the store line names the slowest parts, slowest first, and the engine line's
//! longest frame is the largest gap between frame ticks while a save runs.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_SaveTiming : LF_Test
{
	override string GetName()
	{
		return "persist/save-timing";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_SaveTiming.ResetStatics();

		LF_SaveTiming.BeginStore();
		LF_SaveTiming.Record("survival", 0);
		LF_SaveTiming.Record("carried", 7);
		LF_SaveTiming.Record("ledger", 1);
		if (!RedProof())
			LF_SaveTiming.Record("caseboard", 12);
		LF_SaveTiming.Record("lastpos", 3);
		ctx.EqualStr("caseboard=12 carried=7 lastpos=3", LF_SaveTiming.Slowest(), "the slowest parts, slowest first, under 2 ms left out");

		LF_SaveTiming.BeginStore();
		LF_SaveTiming.Record("survival", 1);
		ctx.EqualStr("none slow", LF_SaveTiming.Slowest(), "a store with no slow part says so");

		// Frame ticks at 0, 33, 67 and 1567 ms: the longest frame is the 1500 ms one.
		LF_SaveTiming.StartFrames(1000);
		LF_SaveTiming.NoteFrame(1033);
		LF_SaveTiming.NoteFrame(1067);
		if (!RedProof())
			LF_SaveTiming.NoteFrame(2567);
		ctx.Equal(1500, LF_SaveTiming.GetLongestFrameMs(), "the longest frame is the largest gap between ticks");
		ctx.Equal(3, LF_SaveTiming.GetFrames(), "every tick counts a frame");

		LF_SaveTiming.ResetStatics();
	}
}
