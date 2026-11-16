// red-proof: observed 2026-11-16 with --red (first dose skipped): RED-OK, FAIL "a dose of 30 raises the level to 30",
// "linear: half the duration takes half the level", "a second dose stacks".

//------------------------------------------------------------------------------------------------
//! A drink impairs for a while (#1919): a dose raises the level, it decays on the configured curve,
//! a second dose stacks to the cap, death clears it, a restart (world-end static reset) clears it,
//! and replication bumps only on a significant change.
// tier: LOGIC
class LF_Test_Impairment : LF_Test
{
	override string GetName()
	{
		return "survival/impairment";
	}

	override void Run(LF_TestContext ctx)
	{
		string id = "lf-test-impairment";
		LF_Impairment.ResetStatics();

		if (!RedProof())
			LF_Impairment.Apply(id, 30, 100, LF_EImpairmentCurve.LINEAR, 50);
		ctx.True(Math.AbsFloat(LF_Impairment.GetLevel(id) - 30) < 0.01, "a dose of 30 raises the level to 30");

		LF_Impairment.Advance(id, 50);
		ctx.True(Math.AbsFloat(LF_Impairment.GetLevel(id) - 15) < 0.01, "linear: half the duration takes half the level");

		LF_Impairment.Apply(id, 30, 100, LF_EImpairmentCurve.LINEAR, 50);
		ctx.True(Math.AbsFloat(LF_Impairment.GetLevel(id) - 45) < 0.01, "a second dose stacks");
		LF_Impairment.Apply(id, 30, 100, LF_EImpairmentCurve.LINEAR, 50);
		ctx.True(Math.AbsFloat(LF_Impairment.GetLevel(id) - 50) < 0.01, "a third dose stops at the cap of 50");

		LF_Impairment.Advance(id, 101);
		ctx.True(LF_Impairment.GetLevel(id) == 0, "linear: the level is gone after the duration");

		LF_Impairment.Apply(id, 40, 100, LF_EImpairmentCurve.EXPONENTIAL, 100);
		LF_Impairment.Advance(id, 20);
		ctx.True(Math.AbsFloat(LF_Impairment.GetLevel(id) - 20) < 0.01, "exponential: a fifth of the duration halves the level");
		LF_Impairment.Advance(id, 80);
		ctx.True(LF_Impairment.GetLevel(id) > 0 && LF_Impairment.GetLevel(id) < 2, "exponential: a small tail is left at the duration");

		LF_Impairment.Apply(id, 40, 100, LF_EImpairmentCurve.LINEAR, 100);
		LF_Impairment.Clear(id, "death");
		ctx.True(LF_Impairment.GetLevel(id) == 0, "death clears the level");

		LF_Impairment.Apply(id, 40, 100, LF_EImpairmentCurve.LINEAR, 100);
		LF_StaticResetHooks hooks = new LF_StaticResetHooks();
		array<string> done = {};
		hooks.Run(done, false);
		ctx.True(done.Contains("LF_Impairment"), "the world-end reset covers impairment");
		LF_Impairment.ResetStatics();
		ctx.True(LF_Impairment.GetLevel(id) == 0, "a restart clears the level (persisting is off)");

		ctx.False(LF_Impairment.NeedsSync(40, 37.6), "a small decay does not bump replication");
		ctx.True(LF_Impairment.NeedsSync(40, 35), "a 5-point change bumps");
		ctx.True(LF_Impairment.NeedsSync(0, 3), "the first impairment bumps");
		ctx.True(LF_Impairment.NeedsSync(3, 0.2), "sobering up bumps");
	}
}
