// red-proof: observed 2026-11-18 with `tools\cli test --no-build --only LF_Test_IntTimeMath` on the unconverted
// sites: `- ShiftCredit keeps a huge delta exact with no cap (expected 1800000481, got 1800000512)`, the same for
// JailSeconds, WindowFor and StartingBank, and `- vehicle Price keeps 2^24 + 1 exact (expected 16777217, got 16777216)`
// and the chop Payout line failed (the helper checks and the engine-error check passed); after the sites moved to
// LF_Utils.MaxInt/MinInt/ClampInt the test is green.

//! #2787: Enforce `Math.Max`/`Math.Min`/`Math.Clamp` work in floats, so an int above 2^24 loses its low bits
//! (unix 1800000480 came back as 1800000512). The int helpers in LF_Utils are exact, and the pure rules that
//! take seconds, milliseconds or money stay exact at unix-time magnitude.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_IntTimeMath : LF_Test
{
	// A unix time (2027) that a float cannot hold: floats near 1.8e9 step by 128, so only multiples of 128 survive.
	static const int UNIX_LOW = 1800000480;
	static const int UNIX_HIGH = 1800000481;
	// The first int a float cannot hold (2^24 + 1).
	static const int FLOAT_EDGE = 16777217;

	override string GetName()
	{
		return "core/int-time-math";
	}

	override void Run(LF_TestContext ctx)
	{
		// The engine error itself, kept as the reason the helpers exist: if this stops failing the engine
		// grew an exact int path and the helpers can go.
		int viaFloat = Math.Max(UNIX_LOW, 0);
		ctx.True(viaFloat != UNIX_LOW, string.Format("Math.Max on a unix-time int rounds it (%1 came back as %2)", UNIX_LOW, viaFloat));

		// The helpers.
		ctx.Equal(UNIX_LOW, LF_Utils.MaxInt(UNIX_LOW, 0), "MaxInt keeps a unix time exact");
		ctx.Equal(UNIX_HIGH, LF_Utils.MaxInt(0, UNIX_HIGH), "MaxInt keeps an odd unix time exact");
		ctx.Equal(UNIX_HIGH, LF_Utils.MinInt(UNIX_HIGH, 2000000000), "MinInt keeps an odd unix time exact");
		ctx.Equal(UNIX_HIGH, LF_Utils.ClampInt(UNIX_HIGH, 0, 2000000000), "ClampInt inside the range keeps an odd unix time exact");
		ctx.Equal(UNIX_HIGH, LF_Utils.ClampInt(2100000000, 0, UNIX_HIGH), "ClampInt above the range returns the exact top");
		ctx.Equal(UNIX_HIGH, LF_Utils.ClampInt(0, UNIX_HIGH, 2000000000), "ClampInt below the range returns the exact bottom");
		ctx.Equal(-5, LF_Utils.ClampInt(-9, -5, 5), "ClampInt works below zero");
		ctx.Equal(FLOAT_EDGE, LF_Utils.MaxInt(FLOAT_EDGE, 0), "MaxInt keeps 2^24 + 1 exact");

		// The converted rules, each fed a value a float cannot hold.
		ctx.Equal(UNIX_HIGH, LF_PoliceRanks.ShiftCredit(UNIX_HIGH, 0, 0), "ShiftCredit keeps a huge delta exact with no cap");
		ctx.Equal(UNIX_HIGH, LF_PoliceRanks.ShiftCredit(UNIX_HIGH, 0, 2000000000), "ShiftCredit keeps a huge delta exact under a cap");
		ctx.Equal(UNIX_HIGH, LF_DisconnectRules.JailSeconds(UNIX_HIGH, 5), "JailSeconds keeps a huge sentence exact");
		ctx.Equal(UNIX_HIGH, LF_LogoutBody.WindowFor(true, 5, UNIX_HIGH), "WindowFor keeps a huge downed window exact");
		ctx.Equal(UNIX_HIGH, LF_DebugMoneyTopUp.StartingBank(true, UNIX_HIGH, 0), "StartingBank keeps a huge balance exact");
		ctx.Equal(FLOAT_EDGE, LF_VehiclePriceModel.Price(FLOAT_EDGE, 100, -1), "vehicle Price keeps 2^24 + 1 exact");
		ctx.Equal(FLOAT_EDGE, LF_ChopShopRules.Payout(FLOAT_EDGE, 0, 100), "chop Payout keeps 2^24 + 1 exact");
	}
};
