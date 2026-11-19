// red-proof: Observed 2026-11-19 with `tools\cli test --no-build --red LF_Test_CrewRobRules`: with the RedProof hook skipping the on-spot tick the take stayed 0 (`6 s off the spot adds nothing (expected 20, got 0)`); RED PROOF OK. Hook off; green.
//! The rules of a crew robbery (#3401, LF_CrewRobRules): the crew minimum, a take that grows only
//! while the holder is on the till spot and stops at the cap, the response window that starts with
//! the alarm, and the even int split of the pay.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_CrewRobRules : LF_Test
{
	override string GetName()
	{
		return "crime/crew-rob-rules";
	}

	override void Run(LF_TestContext ctx)
	{
		const int PER_SECOND = 5;
		const int CAP = 100;

		// The crew minimum refuses a crew that is too small.
		ctx.False(LF_CrewRobRules.CrewMeets(1, 2), "one robber is under a minimum of two");
		ctx.True(LF_CrewRobRules.CrewMeets(2, 2), "the minimum itself starts the robbery");
		ctx.True(LF_CrewRobRules.CrewMeets(3, 2), "a lookout beyond the minimum is fine");

		// The take grows only with the holder on the spot, and pauses off it.
		float seconds = 0;
		if (!RedProof())
			seconds = LF_CrewRobRules.AdvanceSpotSeconds(seconds, 4, true);
		ctx.Equal(20, LF_CrewRobRules.Take(seconds, PER_SECOND, CAP), "4 s on the spot at 5/s is 20");
		seconds = LF_CrewRobRules.AdvanceSpotSeconds(seconds, 6, false);
		ctx.Equal(20, LF_CrewRobRules.Take(seconds, PER_SECOND, CAP), "6 s off the spot adds nothing");
		seconds = LF_CrewRobRules.AdvanceSpotSeconds(seconds, 2, true);
		ctx.Equal(30, LF_CrewRobRules.Take(seconds, PER_SECOND, CAP), "back on the spot it grows again");
		seconds = LF_CrewRobRules.AdvanceSpotSeconds(seconds, -5, true);
		ctx.Equal(30, LF_CrewRobRules.Take(seconds, PER_SECOND, CAP), "a negative step counts as none");

		// It caps.
		seconds = LF_CrewRobRules.AdvanceSpotSeconds(seconds, 1000, true);
		ctx.Equal(CAP, LF_CrewRobRules.Take(seconds, PER_SECOND, CAP), "the take holds at the cap");
		ctx.Equal(0, LF_CrewRobRules.Take(0, PER_SECOND, CAP), "no time on the spot is no take");

		// The response window starts with the alarm, not before it.
		ctx.False(LF_CrewRobRules.ResponseWindowOpen(5000, -1, 90), "no alarm, no window");
		ctx.True(LF_CrewRobRules.ResponseWindowOpen(10000, 10000, 90), "the window opens with the alarm");
		ctx.True(LF_CrewRobRules.ResponseWindowOpen(99999, 10000, 90), "just inside the window");
		ctx.False(LF_CrewRobRules.ResponseWindowOpen(100000, 10000, 90), "the window has closed after its seconds");

		// The split is even in int math: the remainder goes to the first members.
		array<int> shares = {};
		LF_CrewRobRules.Split(301, 3, shares);
		ctx.Equal(3, shares.Count(), "three members get a share");
		if (shares.Count() == 3)
		{
			ctx.Equal(101, shares[0], "301 among 3: first share");
			ctx.Equal(100, shares[1], "301 among 3: second share");
			ctx.Equal(100, shares[2], "301 among 3: third share");
		}
		LF_CrewRobRules.Split(300, 3, shares);
		if (shares.Count() == 3)
			ctx.Equal(100, shares[0], "an even split has no remainder");
		LF_CrewRobRules.Split(50, 0, shares);
		ctx.Equal(0, shares.Count(), "no crew present, no shares");
	}
}
