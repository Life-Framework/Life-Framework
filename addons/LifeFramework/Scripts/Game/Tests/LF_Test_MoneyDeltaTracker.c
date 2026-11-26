// red-proof: with `RedProof()` on no money observation is fed to the tracker, so the ticker never shows a delta and the
// "+500", "combined", "clears" and "re-seeds" checks fail.

//! money/delta-tracker: the HUD's "+$500" ticker state (LF_MoneyDeltaTracker). The first reading only seeds the
//! baseline (opening the HUD is not a transaction), every change adds to the running delta and restarts the
//! countdown, a quiet spell clears it, a net-zero run hides it, a non-positive timeout is ignored and Reset seeds again.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MoneyDeltaTracker : LF_Test
{
	override string GetName()
	{
		return "money/delta-tracker";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_MoneyDeltaTracker tracker = new LF_MoneyDeltaTracker();
		ctx.EqualFloat(LF_MoneyDeltaTracker.DEFAULT_RESET_SECONDS, tracker.GetResetSeconds(), 0.001, "a new ticker clears after the default seconds");
		ctx.False(tracker.IsVisible(), "a new ticker shows nothing");
		ctx.EqualStr("", tracker.GetText(), "a new ticker has no text");

		int afterSeed = -1;
		int afterGain = 0;
		string gainText = "";
		int afterSecondChange = 0;
		float timerAfterChange = 0;
		int afterQuiet = 0;
		int afterClear = -1;
		bool visibleAfterClear = true;
		int afterNetZero = -1;
		bool visibleNetZero = true;
		int afterLoss = 0;
		string lossText = "";
		int afterReseed = -1;
		if (!RedProof())
		{
			tracker.Update(1000, 0.016);
			afterSeed = tracker.GetDelta();

			tracker.Update(1500, 0.016);
			afterGain = tracker.GetDelta();
			gainText = tracker.GetText();

			tracker.Update(1300, 0.016);
			afterSecondChange = tracker.GetDelta();
			timerAfterChange = tracker.GetTimeRemaining();

			tracker.Update(1300, 3.0);
			afterQuiet = tracker.GetDelta();

			tracker.Update(1300, 1.5);
			afterClear = tracker.GetDelta();
			visibleAfterClear = tracker.IsVisible();

			tracker.Update(1400, 0.016);
			tracker.Update(1300, 0.016);
			afterNetZero = tracker.GetDelta();
			visibleNetZero = tracker.IsVisible();

			tracker.Update(1100, 0.016);
			afterLoss = tracker.GetDelta();
			lossText = tracker.GetText();

			tracker.Reset();
			tracker.Update(9000, 0.016);
			afterReseed = tracker.GetDelta();
		}

		ctx.Equal(0, afterSeed, "the first reading only seeds the baseline");
		ctx.Equal(500, afterGain, "a gain of 500 shows +500");
		ctx.EqualStr("+" + LF_MoneyFormat.FormatMoney(500), gainText, "the gain reads +<amount>");
		ctx.Equal(300, afterSecondChange, "a second change adds to the running delta: +500 then -200 reads +300");
		ctx.EqualFloat(LF_MoneyDeltaTracker.DEFAULT_RESET_SECONDS, timerAfterChange, 0.001, "every change restarts the countdown");
		ctx.Equal(300, afterQuiet, "3 s of quiet is under the 4 s timeout and keeps the delta");
		ctx.Equal(0, afterClear, "the delta clears once the timeout runs out");
		ctx.False(visibleAfterClear, "a cleared ticker hides");
		ctx.Equal(0, afterNetZero, "a gain undone by a loss nets to zero");
		ctx.False(visibleNetZero, "a net-zero run is not shown");
		ctx.Equal(-200, afterLoss, "a drop of 200 from 1300 reads -200");
		ctx.EqualStr(LF_MoneyFormat.FormatMoney(-200), lossText, "a loss reads with the minus outside the symbol");
		ctx.Equal(0, afterReseed, "Reset forgets the baseline, so the next reading seeds instead of flashing the balance");

		tracker.SetResetSeconds(0);
		ctx.EqualFloat(LF_MoneyDeltaTracker.DEFAULT_RESET_SECONDS, tracker.GetResetSeconds(), 0.001, "a zero timeout is ignored");
		tracker.SetResetSeconds(-3);
		ctx.EqualFloat(LF_MoneyDeltaTracker.DEFAULT_RESET_SECONDS, tracker.GetResetSeconds(), 0.001, "a negative timeout is ignored");
		tracker.SetResetSeconds(2);
		ctx.EqualFloat(2, tracker.GetResetSeconds(), 0.001, "a positive timeout is taken");
	}
}
