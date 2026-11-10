// red-proof: Observed 2026-11-10 with `tools\cli test --no-build --only LF_Test_DebugMoneyTopUp`: changing the
// expected bank top-up from 1000000 to 1000001 failed it with `[LFTEST] FAIL money/debug-topup`. Restored; green.
// tier: LOGIC
class LF_Test_DebugMoneyTopUp : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "money/debug-topup";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.Equal(1000000, LF_DebugMoneyTopUp.Missing(true, 0, 100000, 1000000), "empty bank fills to the target");
		ctx.Equal(1, LF_DebugMoneyTopUp.Missing(true, 99999, 100000, 100000), "just under the low mark tops up");
		ctx.Equal(0, LF_DebugMoneyTopUp.Missing(true, 100000, 100000, 1000000), "at the low mark does nothing");
		ctx.Equal(15000, LF_DebugMoneyTopUp.Missing(true, 5000, 5000 + 1, 20000), "cash 5000 under low 5001 fills to 20000");
		ctx.Equal(0, LF_DebugMoneyTopUp.Missing(false, 0, 100000, 1000000), "off in the flagship mode does nothing");
		ctx.Equal(1000000, LF_DebugMoneyTopUp.StartingBank(true, 1000, 1000000), "on: new characters open with the target");
		ctx.Equal(1000, LF_DebugMoneyTopUp.StartingBank(false, 1000, 1000000), "off: the normal opening bank");
	}
}
