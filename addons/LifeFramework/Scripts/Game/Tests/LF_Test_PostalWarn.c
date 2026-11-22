// red-proof: observed 2026-11-22 with `tools\cli test --no-build --red LF_Test_PostalWarn`: RED-OK jobs/postal-warn (the hook skips WarnDue and MinMinutes).
//! Postal run, first-2h review B14/B15: the driver is warned before the run is called off, and the
//! least run time is said in whole minutes (rounded up) for the stand and the too-fast refusal. No world.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_PostalWarn : LF_Test
{
	override string GetName()
	{
		return "jobs/postal-warn";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		bool early = true;
		bool late = false;
		if (!RedProof())
		{
			early = LF_PostalRun.WarnDue(30, 180);
			late = LF_PostalRun.WarnDue(120, 180);
		}

		ctx.False(early, "30 s away of 180 is too early to warn");
		ctx.True(late, "two thirds of the time gone: warn");
		ctx.False(LF_PostalRun.WarnDue(0, 180), "just left the van: no warning yet");

		int minutes = 0;
		if (!RedProof())
			minutes = LF_PostalRun.MinMinutes();

		LF_PostalRunConfig config = LF_PostalRunConfig.Get();
		ctx.True(minutes * 60 >= config.m_iMinRunSeconds, "the stated minutes cover the least run time");
		ctx.True(minutes > 0, "the least run time is stated");
		ctx.True((minutes - 1) * 60 < config.m_iMinRunSeconds, "the stated minutes are rounded up, not padded");
	}
}
