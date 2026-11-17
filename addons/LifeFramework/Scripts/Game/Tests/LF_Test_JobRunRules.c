// red-proof: 2026-11-17, `tools\cli test --no-build --red LF_Test_JobRunRules,LF_Test_JobRunCargoWorld`: RED-OK. RedProof() skips
//   recording the run's start (so every distance and time check fails, fail-closed) and skips the deposit and the start check in the
//   world test (so the stash loop is open again).
//   Unfixed code (#2408): a run paid at a drop 6 m from the start in zero seconds, a taxi fare paid with no passenger, and
//   start, abandon, start, abandon kept every parcel and jerrycan.

//------------------------------------------------------------------------------------------------
// tier: LOGIC
//! jobs/run-rules (#2408): a delivery run pays only when the drop is at least the configured distance from where
//! the run started and the run took at least the configured share of the expected driving time at road speed; a
//! taxi fare also needs a seated passenger. The cargo deposit is held for the run, returned once on delivery and
//! kept when the run is abandoned. The numbers are the jobs config's. Pure run state, no entity.
class LF_Test_JobRunRules : LF_Test
{
	protected static const string UID = "job-run-rules-courier";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "jobs/run-rules";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_JobsConfig.Reload();
		LF_CivilianJob.ForgetCareer(UID);
		RunNumbers(ctx);
		RunDropRules(ctx);
		RunTaxi(ctx);
		RunDeposit(ctx);
		RunConfigDriven(ctx);
		LF_CivilianJob.ForgetCareer(UID);
		LF_JobsConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunNumbers(LF_TestContext ctx)
	{
		ctx.EqualFloat(300, LF_CivilianJob.RunMinDistance(), 0.001, "the shipped config asks 300 m between start and drop");
		ctx.EqualFloat(18, LF_CivilianJob.MinRunSeconds(300, 10, 0.6), 0.01, "300 m at 10 m/s is 30 s, and 60% of it is 18 s");
		ctx.EqualFloat(0, LF_CivilianJob.MinRunSeconds(300, 0, 0.6), 0.001, "no road speed gives no time bar");

		ctx.Equal(40, LF_CivilianJob.DepositAmount(40, 100, 50), "the deposit is the cargo's sell price");
		ctx.Equal(20, LF_CivilianJob.DepositAmount(40, 50, 50), "the deposit follows the percent");
		ctx.Equal(50, LF_CivilianJob.DepositAmount(-1, 100, 50), "no sell price falls back to the configured base");
		ctx.Equal(0, LF_CivilianJob.DepositAmount(40, 0, 50), "0 percent turns the deposit off");
		ctx.Equal(0, LF_CivilianJob.DepositFor(LF_EJobType.TAXI), "a taxi fare has no cargo and no deposit");
		ctx.True(LF_CivilianJob.DepositFor(LF_EJobType.PARCEL) > 0, "a parcel is held against a deposit");
		ctx.True(LF_CivilianJob.DepositFor(LF_EJobType.FUEL_DELIVERY) > 0, "a fuel can is held against a deposit");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunDropRules(LF_TestContext ctx)
	{
		ctx.True(LF_CivilianJob.SignUp(UID, LF_EJobType.PARCEL), "the courier signs up");
		ctx.True(LF_CivilianJob.StartRun(UID, LF_EJobType.PARCEL), "a run starts");
		ctx.EqualStr(LF_CivilianJob.KEY_NO_RUN, LF_CivilianJob.DropoffRefusal(UID, Vector(400, 0, 0), 1000, 0), "a run with no start on record pays nothing");

		if (!RedProof())
			LF_CivilianJob.RecordRunStart(UID, Vector(0, 0, 0), 100);

		ctx.EqualStr(LF_CivilianJob.KEY_TOO_CLOSE, LF_CivilianJob.DropoffRefusal(UID, Vector(6, 0, 0), 5000, 0), "a drop 6 m from the start pays nothing however long the run took");
		ctx.EqualStr(LF_CivilianJob.KEY_TOO_CLOSE, LF_CivilianJob.DropoffRefusal(UID, Vector(299, 0, 0), 5000, 0), "299 m is still too close");
		ctx.EqualStr(LF_CivilianJob.KEY_TOO_FAST, LF_CivilianJob.DropoffRefusal(UID, Vector(300, 0, 0), 101, 0), "a far drop reached in a second is too fast");
		ctx.EqualStr(LF_CivilianJob.KEY_TOO_FAST, LF_CivilianJob.DropoffRefusal(UID, Vector(300, 0, 0), 117, 0), "17 s is under 60% of the 30 s expected");
		ctx.EqualStr("", LF_CivilianJob.DropoffRefusal(UID, Vector(300, 0, 0), 119, 0), "300 m in 19 s pays");
		ctx.EqualStr("", LF_CivilianJob.DropoffRefusal(UID, Vector(300, 0, 0), 119, 1), "a parcel run ignores passengers");
		ctx.EqualStr(LF_CivilianJob.KEY_TOO_FAST, LF_CivilianJob.DropoffRefusal(UID, Vector(1000, 0, 0), 150, 0), "a farther drop expects a longer run: 1000 m wants 60 s");
		ctx.EqualStr("", LF_CivilianJob.DropoffRefusal(UID, Vector(1000, 0, 0), 161, 0), "and 61 s pays");
		ctx.EqualStr(LF_CivilianJob.KEY_NO_RUN, LF_CivilianJob.DropoffRefusal("job-run-rules-nobody", Vector(400, 0, 0), 1000, 0), "a player with no run is refused");

		ctx.True(LF_CivilianJob.AbandonRun(UID), "the run ends");
		ctx.EqualStr(LF_CivilianJob.KEY_NO_RUN, LF_CivilianJob.DropoffRefusal(UID, Vector(400, 0, 0), 1000, 0), "an ended run is refused");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunTaxi(LF_TestContext ctx)
	{
		LF_TestJobRuns.ToLevel(ctx, UID, LF_EJobType.PARCEL, 3);
		ctx.True(LF_CivilianJob.SignUp(UID, LF_EJobType.TAXI), "level 3 opens the taxi");
		ctx.True(LF_CivilianJob.StartRun(UID, LF_EJobType.TAXI), "a fare starts");
		LF_CivilianJob.RecordRunStart(UID, Vector(0, 0, 0), 100);

		ctx.EqualStr(LF_CivilianJob.KEY_NO_PASSENGER, LF_CivilianJob.DropoffRefusal(UID, Vector(400, 0, 0), 200, 0), "a fare with no passenger pays nothing");
		ctx.EqualStr("", LF_CivilianJob.DropoffRefusal(UID, Vector(400, 0, 0), 200, 1), "a fare with a seated passenger pays");
		ctx.EqualStr(LF_CivilianJob.KEY_TOO_CLOSE, LF_CivilianJob.DropoffRefusal(UID, Vector(6, 0, 0), 200, 1), "a fare to the pad next door is too close even with a passenger");
		ctx.EqualStr(LF_CivilianJob.KEY_TOO_FAST, LF_CivilianJob.DropoffRefusal(UID, Vector(400, 0, 0), 110, 1), "and a fare that is over at once is too fast");
		ctx.Equal(0, LF_CivilianJob.CountPassengers(null), "no driver has no passengers");
		LF_CivilianJob.AbandonRun(UID);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunDeposit(LF_TestContext ctx)
	{
		ctx.True(LF_CivilianJob.StartRun(UID, LF_EJobType.TAXI), "a run starts");
		ctx.Equal(0, LF_CivilianJob.GetDeposit(UID), "a new run holds no deposit");
		LF_CivilianJob.RecordDeposit(UID, 40);
		ctx.Equal(40, LF_CivilianJob.GetDeposit(UID), "the deposit is held against the run");
		ctx.True(LF_CivilianJob.AbandonRun(UID), "the run is abandoned");
		ctx.Equal(0, LF_CivilianJob.GetDeposit(UID), "an abandoned run keeps the deposit: nothing is left to give back");
		ctx.Equal(0, LF_CivilianJob.ReleaseDeposit(UID), "so nothing is released");

		ctx.True(LF_CivilianJob.StartRun(UID, LF_EJobType.TAXI), "a second run starts");
		LF_CivilianJob.RecordDeposit(UID, 40);
		ctx.True(LF_CivilianJob.CompleteRun(UID) > 0, "the run is delivered");
		ctx.Equal(40, LF_CivilianJob.ReleaseDeposit(UID), "a delivered run gives the deposit back");
		ctx.Equal(0, LF_CivilianJob.ReleaseDeposit(UID), "once");

		ctx.True(LF_CivilianJob.StartRun(UID, LF_EJobType.TAXI), "a third run starts");
		ctx.Equal(0, LF_CivilianJob.GetDeposit(UID), "a new run does not inherit the last deposit");
		LF_CivilianJob.AbandonRun(UID);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunConfigDriven(LF_TestContext ctx)
	{
		LF_JobsConfig config = LF_JobsConfig.Get();
		ctx.NotNull(config, "the jobs config loads");
		if (!config)
			return;

		config.m_fRunMinDistance = 500;
		config.m_fRunMinTimeFraction = 1.0;
		config.m_fRunRoadSpeed = 20;
		ctx.True(LF_CivilianJob.StartRun(UID, LF_EJobType.TAXI), "a run starts");
		LF_CivilianJob.RecordRunStart(UID, Vector(0, 0, 0), 100);
		ctx.EqualStr(LF_CivilianJob.KEY_TOO_CLOSE, LF_CivilianJob.DropoffRefusal(UID, Vector(400, 0, 0), 1000, 1), "a raised distance in config refuses a 400 m drop");
		ctx.EqualStr(LF_CivilianJob.KEY_TOO_FAST, LF_CivilianJob.DropoffRefusal(UID, Vector(500, 0, 0), 120, 1), "a full share at 20 m/s wants 25 s for 500 m");
		ctx.EqualStr("", LF_CivilianJob.DropoffRefusal(UID, Vector(500, 0, 0), 126, 1), "and 26 s pays");
		LF_CivilianJob.AbandonRun(UID);
		LF_JobsConfig.Reload();
	}
}
