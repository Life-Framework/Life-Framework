// red-proof: 2026-11-15, cli test --no-build --only LF_Test_ServerPerfLog failed before LF_ServerPerfLog.c existed (scripts do not compile), then passed=1 failed=0; `--red` printed RED PROOF OK (the format and gate acts are skipped, the line and fire checks fail).
// tier: LOGIC
//! ops/server-perf-log (#1452): the once-a-minute `[LFDebug:Perf] players=N fps=F frameMs=M entities=E`
//! line. The formatter turns a fixed sample into the exact line, and the interval gate fires once
//! per 60 s of fed time, never per frame and never as a burst after a long stall.
class LF_Test_ServerPerfLog : LF_Test
{
	override string GetName()
	{
		return "ops/server-perf-log";
	}

	override void Run(LF_TestContext ctx)
	{
		// Formatter: a fixed sample, the exact line. The red-proof hook skips the act.
		string line = "";
		string rounded = "";
		string negative = "";
		if (!RedProof())
		{
			line = LF_ServerPerfLog.Format(12, 38, 26.3, 4821);
			rounded = LF_ServerPerfLog.Format(0, 60, 16.66, 0);
			negative = LF_ServerPerfLog.Format(-1, -5, -2.0, -9);
		}

		ctx.EqualStr("players=12 fps=38 frameMs=26.3 entities=4821", line, "the sample formats to the exact line");
		ctx.EqualStr("players=0 fps=60 frameMs=16.7 entities=0", rounded, "frameMs rounds to one decimal");
		ctx.EqualStr("players=0 fps=0 frameMs=0.0 entities=0", negative, "a bad sample clamps to zero, never a minus sign");

		// Gate: once per 60 s of fed time.
		LF_ServerPerfLogGate gate = new LF_ServerPerfLogGate();
		int fired = 0;
		int frames = 0;
		if (!RedProof())
		{
			// 119 half-second feeds = 59.5 s: not yet; per-frame feeding must not fire either.
			for (frames = 0; frames < 119; frames++)
			{
				if (gate.Feed(0.5))
					fired++;
			}
		}
		ctx.Equal(0, fired, "no line in the first 59.5 s of fed time");

		if (!RedProof() && gate.Feed(0.5))
			fired++;
		ctx.Equal(1, fired, "the line fires at 60 s");

		// The next 60 s fire once more, again not per frame.
		for (frames = 0; frames < 119; frames++)
		{
			if (gate.Feed(0.5))
				fired++;
		}
		ctx.Equal(1, fired, "nothing between two intervals");
		if (gate.Feed(0.5))
			fired++;
		ctx.Equal(2, fired, "the second line fires at 120 s");

		// A long stall logs one line, not one per missed minute, and keeps the remainder.
		LF_ServerPerfLogGate stalled = new LF_ServerPerfLogGate();
		int stallFired = 0;
		if (stalled.Feed(185))
			stallFired++;
		ctx.Equal(1, stallFired, "a 185 s stall fires once");
		if (stalled.Feed(54.5))
			stallFired++;
		ctx.Equal(1, stallFired, "the 5 s remainder is kept: 59.5 s is not yet due");
		if (stalled.Feed(0.5))
			stallFired++;
		ctx.Equal(2, stallFired, "and the next line is due 60 s after the stall's last whole minute");

		// A negative or zero feed never fires.
		LF_ServerPerfLogGate idle = new LF_ServerPerfLogGate();
		ctx.False(idle.Feed(0), "a zero feed does not fire");
		ctx.False(idle.Feed(-30), "a negative feed does not fire");
	}
}
