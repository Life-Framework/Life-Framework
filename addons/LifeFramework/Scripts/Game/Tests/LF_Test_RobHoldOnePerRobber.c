// red-proof: Observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_RobHoldOnePerRobber`: RED-OK, the second demand was not refused (got '') and the unseated hold settled as NONE (got 0); green without the hook.
// tier: LOGIC
//! A robber holds one till at a time, and a downed or dead attendant ends the hold unpaid (#2540).
//! Goes through LF_RobHold's seams: a seated hold, the refusal a second demand gets, and the hold
//! settled the way the tick settles it once the attendant is down.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_RobHoldOnePerRobber : LF_Test
{
	static const int ROBBER = 4242;
	static const int OTHER = 4243;

	override string GetName()
	{
		return "crime/rob-hold-one-per-robber";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_RobHold.ResetStatics();
		if (!RedProof())
			LF_RobHold.SeatForTest(ROBBER);

		ctx.EqualStr(LF_RobHold.REFUSED_HOLDING, LF_RobHold.HolderRefusal(ROBBER), "a robber already holding a till is refused a second");
		ctx.EqualStr("", LF_RobHold.HolderRefusal(OTHER), "another robber is not refused for it");
		ctx.EqualStr("#LF-RobHold_Refused_Holding", LF_RobHoldRules.RefusalKey(LF_RobHold.REFUSED_HOLDING), "the attendant's caption says why");

		bool clerkDown = !RedProof();
		ctx.Equal(LF_ERobHoldEnd.CLERK, LF_RobHold.SettleForTest(ROBBER, LF_ERobHoldEnd.PAID, clerkDown), "a dead attendant ends even a kept hold");
		ctx.Equal(0, LF_RobHold.LastPaid(), "the hold ends without paying");
		ctx.EqualStr("", LF_RobHold.HolderRefusal(ROBBER), "the ended hold frees the robber");
		ctx.Equal(0, LF_RobHold.ActiveCount(), "no hold is left running");

		ctx.Equal(LF_ERobHoldEnd.CLERK, LF_RobHoldRules.JudgeClerk(LF_ERobHoldEnd.NONE, true), "a downed attendant ends a running hold");
		ctx.Equal(LF_ERobHoldEnd.LEFT, LF_RobHoldRules.JudgeClerk(LF_ERobHoldEnd.LEFT, true), "a robber who ran is told he ran");
		ctx.Equal(LF_ERobHoldEnd.PAID, LF_RobHoldRules.JudgeClerk(LF_ERobHoldEnd.PAID, false), "a standing attendant lets a kept hold pay");
		ctx.False(LF_RobHoldRules.Pays(LF_ERobHoldEnd.CLERK), "a hold ended by the attendant pays nothing");
		ctx.True(!LF_RobHoldRules.EndKey(LF_ERobHoldEnd.CLERK).IsEmpty(), "the robber is told the attendant went down");
		ctx.EqualStr("", LF_RobHoldRules.ClerkKey(LF_ERobHoldEnd.CLERK), "a downed attendant cries nothing");
		LF_RobHold.ResetStatics();
	}
}
