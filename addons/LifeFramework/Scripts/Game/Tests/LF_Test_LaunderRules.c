// red-proof: 2026-11-18 `tools/cli test --no-build --red LF_Test_LaunderRules`: RED-OK, the hook zeroes the cut (`expected 700, got 1000`). Review fix (#2885): on the unfixed rules `--only` failed `3 marked at 30 % yields 2 (expected 2, got 3)`, `1 marked ... (expected 0, got 1)`, the 333-washes-of-3 split, the 2e9 overflow and the refused-cut switch; green after.
//! Laundering rules (#2852): the 30 % cut in integer maths, the batch max, the per-character daily cap
//! with its 24 h window, and a config cut outside 0..90 refused at load.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_LaunderRules : LF_Test
{
	protected static const string KEY = "test-launder-rules";
	protected static const int T0 = 1800000000;

	override string GetName()
	{
		return "laundering/rules";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_LaunderRules.ResetStatics();

		int cut = 30;
		if (RedProof())
			cut = 0;

		ctx.Equal(700, LF_LaunderRules.CleanFor(1000, cut), "1000 marked at 30 % yields 700");
		ctx.Equal(2, LF_LaunderRules.CleanFor(3, 30), "3 marked at 30 % yields 2: the remainder stays with the house");
		ctx.Equal(0, LF_LaunderRules.CleanFor(1, 30), "1 marked at 30 % yields 0, never a zero-cut wash");
		int splitClean = 0;
		int split;
		for (split = 0; split < 333; split++)
			splitClean = splitClean + LF_LaunderRules.CleanFor(3, 30);
		ctx.True(splitClean <= LF_LaunderRules.CleanFor(999, 30), "333 washes of 3 pay no more than one of 999");
		ctx.Equal(1400000000, LF_LaunderRules.CleanFor(2000000000, 30), "a huge batch does not overflow");

		LF_LaunderQuote big = LF_LaunderRules.Quote(KEY, 5000, T0, 30, 2000, 10000);
		ctx.Equal(2000, big.m_iAccepted, "a batch above the max is cut to it");
		ctx.Equal(1400, big.m_iClean, "the cut batch yields 1400");
		ctx.Equal(3000, big.m_iLeftMarked, "the rest stays marked");

		ctx.Equal(60, LF_LaunderRules.SecondsFor(2000, 2000, 60), "one batch is 60 s");
		ctx.Equal(120, LF_LaunderRules.SecondsFor(2001, 2000, 60), "two started batches take 120 s");

		int i;
		for (i = 0; i < 5; i++)
			LF_LaunderRules.Record(KEY, 2000, T0 + i);

		LF_LaunderQuote capped = LF_LaunderRules.Quote(KEY, 1000, T0 + 100, 30, 2000, 10000);
		ctx.Equal(0, capped.m_iAccepted, "at the cap nothing is accepted");
		ctx.Equal(0, capped.m_iClean, "at the cap nothing is paid");
		ctx.Equal(1000, capped.m_iLeftMarked, "at the cap everything stays marked");
		ctx.True(capped.m_sReason == "daily_cap", "the reason is daily_cap");

		LF_LaunderQuote before = LF_LaunderRules.Quote(KEY, 1000, T0 + LF_LaunderRules.DAY_SECONDS - 1, 30, 2000, 10000);
		ctx.True(before.m_sReason == "daily_cap", "still capped one second before the window ends");
		LF_LaunderQuote rolled = LF_LaunderRules.Quote(KEY, 1000, T0 + LF_LaunderRules.DAY_SECONDS, 30, 2000, 10000);
		ctx.True(rolled.m_sReason == "", "the window rolls over 24 h after the first wash");
		ctx.Equal(700, rolled.m_iClean, "and washes again");

		LF_LaunderRules.ResetStatics();
		LF_LaunderRules.Record(KEY, 9500, T0);
		LF_LaunderQuote trim = LF_LaunderRules.Quote(KEY, 2000, T0 + 5, 30, 2000, 10000);
		ctx.Equal(500, trim.m_iAccepted, "the cap's remainder trims the batch");
		ctx.Equal(1500, trim.m_iLeftMarked, "the rest stays marked");

		LF_LaunderConfig.ResetStatics();
		LF_LaunderConfig good = LF_LaunderConfig.Get();
		ctx.NotNull(good, "the core config loads and passes validation");
		if (good)
		{
			ctx.Equal(30, good.m_iCutPct, "default cut 30");
			ctx.Equal(2000, good.m_iBatchMax, "default batch max 2000");
			ctx.Equal(60, good.m_iBatchSeconds, "default batch time 60 s");
			ctx.Equal(10000, good.m_iDailyCap, "default daily cap 10000");
		}

		LF_LaunderConfig bad = new LF_LaunderConfig();
		bad.m_iBatchMax = 2000;
		bad.m_iBatchSeconds = 60;
		bad.m_iDailyCap = 10000;
		bad.m_iCutPct = 30;
		ctx.True(LF_LaunderConfig.Validate(bad), "a sane config is accepted");
		bad.m_iCutPct = 91;
		ctx.False(LF_LaunderConfig.Validate(bad), "a cut of 91 % is refused");
		bad.m_iCutPct = -1;
		ctx.False(LF_LaunderConfig.Validate(bad), "a negative cut is refused");
		bad.m_iCutPct = 30;
		bad.m_iBatchMax = 0;
		ctx.False(LF_LaunderConfig.Validate(bad), "a zero batch max is refused");

		LF_LaunderConfig refused = new LF_LaunderConfig();
		refused.m_bCrimePaysMarked = true;
		refused.m_iCutPct = 95;
		refused.m_iBatchMax = 2000;
		refused.m_iBatchSeconds = 60;
		refused.m_iDailyCap = 10000;
		LF_LaunderConfig.Install(refused);
		ctx.True(LF_LaunderRules.QuoteWithConfig(KEY, 1000, T0) == null, "a refused cut refuses the wash");
		ctx.True(LF_LaunderConfig.CrimePaysMarked(), "a refused cut keeps the dirty-money switch, so robberies stay marked");

		LF_LaunderConfig.ResetStatics();
		LF_LaunderRules.ResetStatics();
	}
}
