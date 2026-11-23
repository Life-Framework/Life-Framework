// red-proof: observed 2026-11-08 with `tools\cli test --no-build --only LF_Test_JobPacing,LF_Test_Roadmap_Jobs`
// and LF_Jobs.conf m_iJobXpFirstLevel 100 (one run is one level, the old pacing):
// [LFTEST] FAIL jobs/pacing
// [LFTEST]   - fuel delivery takes five runs (expected 5, got 1)
// [LFTEST]   - four parcel runs keep the courier at the first level (expected 1, got 3)
// [LFTEST]   - four runs do not open fuel delivery
// Restored to 500; green.

//------------------------------------------------------------------------------------------------
//! Completes runs of one job until the career reaches a level (bounded, so a broken curve fails
//! instead of looping).
class LF_TestJobRuns
{
	static void ToLevel(LF_TestContext ctx, string uid, LF_EJobType job, int level)
	{
		int guard = 0;
		while (LF_CivilianJob.GetLevel(uid) < level && guard < 100)
		{
			guard++;
			LF_CivilianJob.SignUp(uid, job);
			LF_CivilianJob.StartRun(uid, job);
			LF_CivilianJob.CompleteRun(uid);
		}

		ctx.Equal(level, LF_CivilianJob.GetLevel(uid), string.Format("runs reach job level %1", level));
	}
}

[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_JobPacing : LF_Test
{
	override string GetName()
	{
		return "jobs/pacing";
	}

	override void Run(LF_TestContext ctx)
	{
		string uid = "jobs-pacing-courier";
		LF_CivilianJob.ForgetCareer(uid);

		ctx.NotNull(LF_JobsConfig.Get(), "the jobs config loads");
		ctx.Equal(100, LF_CivilianJob.XpPerRun(), "a run is worth 100 experience");
		// The gates are level numbers on the 30-level DELIVERY table (#4596): level 4 is 600 XP, level 6 is 1500 XP.
		int fuelLevel = LF_CivilianJob.RequiredLevel(LF_EJobType.FUEL_DELIVERY);
		int taxiLevel = LF_CivilianJob.RequiredLevel(LF_EJobType.TAXI);
		ctx.Equal(6, LF_CivilianJob.RunsToReach(fuelLevel), "fuel delivery takes six runs");
		ctx.Equal(15, LF_CivilianJob.RunsToReach(taxiLevel), "the taxi takes fifteen runs");

		ctx.True(LF_CivilianJob.SignUp(uid, LF_EJobType.PARCEL), "a new courier signs up");
		for (int i = 0; i < 5; i++)
		{
			LF_CivilianJob.StartRun(uid, LF_EJobType.PARCEL);
			LF_CivilianJob.CompleteRun(uid);
		}

		ctx.True(LF_CivilianJob.GetLevel(uid) < fuelLevel, "five parcel runs keep the courier below the fuel delivery level");
		ctx.False(LF_CivilianJob.CanSignUp(uid, LF_EJobType.FUEL_DELIVERY), "five runs do not open fuel delivery");

		LF_CivilianJob.StartRun(uid, LF_EJobType.PARCEL);
		LF_CivilianJob.CompleteRun(uid);
		ctx.Equal(fuelLevel, LF_CivilianJob.GetLevel(uid), "the sixth run reaches the fuel delivery level");
		ctx.True(LF_CivilianJob.CanSignUp(uid, LF_EJobType.FUEL_DELIVERY), "the sixth run opens fuel delivery");
		ctx.False(LF_CivilianJob.CanSignUp(uid, LF_EJobType.TAXI), "the taxi is still closed");

		ctx.Equal(LF_EJobType.FUEL_DELIVERY, LF_CivilianJob.NewlyOpened(fuelLevel - 1, fuelLevel), "the clerk mentions fuel delivery when it opens");
		ctx.Equal(LF_EJobType.TAXI, LF_CivilianJob.NewlyOpened(taxiLevel - 1, taxiLevel), "the clerk mentions the taxi when it opens");
		ctx.Equal(LF_EJobType.UNEMPLOYED, LF_CivilianJob.NewlyOpened(fuelLevel, fuelLevel), "no level change, no message");
		ctx.Equal(LF_EJobType.UNEMPLOYED, LF_CivilianJob.NewlyOpened(taxiLevel, taxiLevel + 1), "a level past the taxi opens nothing new");

		LF_CivilianJob.ForgetCareer(uid);
	}
}
