// red-proof: observed 2026-11-16 with --red (curve act skipped): RED-OK, FAIL "half the full level follows the curve (power 2: a quarter of the max)".

//------------------------------------------------------------------------------------------------
//! The drunk view (#1920): the sway amplitude for a level follows the config curve, is zero at level
//! 0, is capped at the maximum, the offset is zero when sober and bounded when drunk, and the world-end
//! reset drops the harness pin.
// tier: LOGIC
class LF_Test_ImpairmentView : LF_Test
{
	override string GetName()
	{
		return "survival/impairment-view";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ImpairmentViewConfig config = LF_ImpairmentViewConfig.CreateDefault();
		config.m_fFullLevel = 40;
		config.m_fCurvePower = 2;
		config.m_fMaxDegrees = 2;

		ctx.True(LF_ImpairmentView.SwayAmplitude(0, config) == 0, "level 0 does not sway");
		ctx.True(LF_ImpairmentView.SwayAmplitude(-5, config) == 0, "a negative level does not sway");

		// Half of the full level on a power-2 curve is a quarter of the maximum: 0.5.
		float half = 0;
		if (!RedProof())
			half = LF_ImpairmentView.SwayAmplitude(20, config);
		ctx.True(Math.AbsFloat(half - 0.5) < 0.001, "half the full level follows the curve (power 2: a quarter of the max)");

		ctx.True(Math.AbsFloat(LF_ImpairmentView.SwayAmplitude(40, config) - 2) < 0.001, "the full level reaches the maximum");
		ctx.True(LF_ImpairmentView.SwayAmplitude(100, config) <= 2.0001, "a level past full is capped at the maximum");
		ctx.True(LF_ImpairmentView.SwayAmplitude(30, config) > LF_ImpairmentView.SwayAmplitude(10, config), "more drunk sways more");

		config.m_fCurvePower = 1;
		ctx.True(Math.AbsFloat(LF_ImpairmentView.SwayAmplitude(20, config) - 1) < 0.001, "power 1 is a straight line");

		ctx.True(LF_ImpairmentView.SwayOffset(0, 3.7, config) == vector.Zero, "sober: no offset at any time");

		float peak = 0;
		for (int i = 0; i < 400; i++)
		{
			vector offset = LF_ImpairmentView.SwayOffset(100, i * 0.1, config);
			peak = Math.Max(peak, Math.AbsFloat(offset[0]));
			if (Math.AbsFloat(offset[0]) > 2.0001 || Math.AbsFloat(offset[1]) > 1.2001)
			{
				ctx.Fail("the offset stays inside the cap (yaw 2, pitch 60%)");
				return;
			}
		}
		ctx.True(peak > 1.5, "at the cap the yaw swings close to the maximum over time");

		LF_ImpairmentView.SetDebugLevel(55);
		ctx.True(LF_ImpairmentView.ViewLevel(10) == 55, "a pinned level wins over the replicated one");
		// Listing only: running every feature's world-end reset here wipes the placed-entity registries the later
		// WORLD tests read (the 35 failures that got #2011 reverted). The view's own reset runs below.
		ctx.True(LF_StaticReset.Registered().Contains("LF_ImpairmentView"), "the world-end reset covers the view");
		LF_ImpairmentView.ResetStatics();
		ctx.True(LF_ImpairmentView.ViewLevel(10) == 10, "the reset drops the pin");
	}
}
