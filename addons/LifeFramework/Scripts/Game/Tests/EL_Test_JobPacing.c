// red-proof: observed 2026-11-08 with `tools\cli test --no-build --only EL_Test_JobPacing,EL_Test_Roadmap_Jobs`
// and LF_Jobs.conf m_iJobXpFirstLevel 100 (one run is one level, the old pacing):
// [ELTEST] FAIL jobs/pacing
// [ELTEST]   - fuel delivery takes five runs (expected 5, got 1)
// [ELTEST]   - four parcel runs keep the courier at the first level (expected 1, got 3)
// [ELTEST]   - four runs do not open fuel delivery
// Restored to 500; green.

//------------------------------------------------------------------------------------------------
//! Completes runs of one job until the career reaches a level (bounded, so a broken curve fails
//! instead of looping).
class EL_TestJobRuns
{
	static void ToLevel(EL_TestContext ctx, string uid, EL_EJobType job, int level)
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

// tier: LOGIC
class EL_Test_JobPacing : EL_Test
{
	override string GetName()
	{
		return "jobs/pacing";
	}

	override void Run(EL_TestContext ctx)
	{
		string uid = "jobs-pacing-courier";
		LF_CivilianJob.ForgetCareer(uid);

		ctx.NotNull(LF_JobsConfig.Get(), "the jobs config loads");
		ctx.Equal(100, LF_CivilianJob.XpPerRun(), "a run is worth 100 experience");
		ctx.Equal(5, LF_CivilianJob.RunsToReach(2), "fuel delivery takes five runs");
		ctx.Equal(15, LF_CivilianJob.RunsToReach(3), "the taxi takes fifteen runs");

		ctx.True(LF_CivilianJob.SignUp(uid, EL_EJobType.PARCEL), "a new courier signs up");
		for (int i = 0; i < 4; i++)
		{
			LF_CivilianJob.StartRun(uid, EL_EJobType.PARCEL);
			LF_CivilianJob.CompleteRun(uid);
		}

		ctx.Equal(1, LF_CivilianJob.GetLevel(uid), "four parcel runs keep the courier at the first level");
		ctx.False(LF_CivilianJob.CanSignUp(uid, EL_EJobType.FUEL_DELIVERY), "four runs do not open fuel delivery");

		LF_CivilianJob.StartRun(uid, EL_EJobType.PARCEL);
		LF_CivilianJob.CompleteRun(uid);
		ctx.Equal(2, LF_CivilianJob.GetLevel(uid), "the fifth run opens the next level");
		ctx.True(LF_CivilianJob.CanSignUp(uid, EL_EJobType.FUEL_DELIVERY), "the fifth run opens fuel delivery");
		ctx.False(LF_CivilianJob.CanSignUp(uid, EL_EJobType.TAXI), "the taxi is still closed");

		ctx.Equal(EL_EJobType.FUEL_DELIVERY, LF_CivilianJob.NewlyOpened(1, 2), "the clerk mentions fuel delivery when it opens");
		ctx.Equal(EL_EJobType.TAXI, LF_CivilianJob.NewlyOpened(2, 3), "the clerk mentions the taxi when it opens");
		ctx.Equal(EL_EJobType.UNEMPLOYED, LF_CivilianJob.NewlyOpened(2, 2), "no level change, no message");
		ctx.Equal(EL_EJobType.UNEMPLOYED, LF_CivilianJob.NewlyOpened(3, 4), "a level past the taxi opens nothing new");

		LF_CivilianJob.ForgetCareer(uid);
	}
}
