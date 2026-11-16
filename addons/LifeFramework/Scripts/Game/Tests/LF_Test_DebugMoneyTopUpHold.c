// red-proof: Observed 2026-11-16 with `tools\cli test --no-build --red LF_Test_CarriedStolenPersist,LF_Test_DebugMoneyTopUpHold`: RED-OK money/debug-topup-hold, `- held: $750 after a buy stays $750 (expected 0, got 19250)` with HoldCash skipped (the #1855 jump). Green with `--only`.
// tier: LOGIC
//! #1855: a cash amount `resetplayer <cash>` set holds. The 60 s playtest top-up tick refilled $750 to
//! 20,000 between a fence buy and sell, so the sale read as $20,100.
class LF_Test_DebugMoneyTopUpHold : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "money/debug-topup-hold";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		string held = "lf-test-topup-held";
		string other = "lf-test-topup-other";
		LF_DebugMoneyTopUp.ResetStatics();

		ctx.Equal(19250, LF_DebugMoneyTopUp.CashToAdd(held, 750, 5000, 20000), "before a hold, $750 tops up to 20,000");

		if (!RedProof())
			LF_DebugMoneyTopUp.HoldCash(held);

		ctx.True(LF_DebugMoneyTopUp.IsCashHeld(held), "resetplayer with an amount holds the cash");
		ctx.Equal(0, LF_DebugMoneyTopUp.CashToAdd(held, 750, 5000, 20000), "held: $750 after a buy stays $750");
		ctx.Equal(19250, LF_DebugMoneyTopUp.CashToAdd(other, 750, 5000, 20000), "another player still tops up");

		LF_DebugMoneyTopUp.ReleaseCash(held);
		ctx.Equal(19250, LF_DebugMoneyTopUp.CashToAdd(held, 750, 5000, 20000), "released: the top-up fills again");

		LF_DebugMoneyTopUp.ResetStatics();
	}
}
