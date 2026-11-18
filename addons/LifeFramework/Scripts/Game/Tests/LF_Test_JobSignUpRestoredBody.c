// red-proof: RED-OK jobs/signup-restored-body, observed 2026-11-18 with `tools\cli test --no-build --only LF_Test_JobSignUpRestoredBody` on the unfixed AdoptLivingBody:
// `the restored body carries the saved fuel sign-up` and `the restored body carries the career level (expected 2, got 1)` failed. Green with the fix.
//! jobs/signup-restored-body (#3353): after a restart the save restores the player's body and
//! LF_SpawnLogic takes the "already controls a character" branch, which only called AdoptLivingBody.
//! That never put the saved delivery sign-up (or the career level) on the body, so a signed-up
//! courier, fuel driver or taxi driver came back UNEMPLOYED at level 1 and every job action was greyed.
//! AdoptLivingBody now also applies the sign-up and pulls the career level.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_JobSignUpRestoredBody : LF_Test
{
	protected static const string UID = "lf-test-signup-restored-body";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "jobs/signup-restored-body";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_Progression.Forget(UID);
		LF_CivilianJob.ForgetCareer(UID);

		// The fuel driver: level 2 from paid parcel runs, signed up for fuel. This is what the save restores.
		LF_TestProgressionKit.SeedCourier(ctx, UID);
		ctx.True(LF_CivilianJob.SignUp(UID, LF_EJobType.FUEL_DELIVERY), "the level 2 driver signs up for fuel");
		int careerLevel = LF_CivilianJob.GetLevel(UID);

		// The restored body: a fresh component, as a save brings it back (unemployed, level 1).
		IEntity body = LF_TestCharacterFixture.Spawn(ctx);
		if (!body)
		{
			LF_CivilianJob.ForgetCareer(UID);
			LF_Progression.Forget(UID);
			return;
		}

		LF_Utils.SetUidForTests(body, UID);
		LF_PlayerJobComponent jobs = LF_Component<LF_PlayerJobComponent>.Find(body);
		ctx.NotNull(jobs, "the body has a job component");
		if (jobs)
		{
			ctx.True(jobs.GetJob() == LF_EJobType.UNEMPLOYED, "the restored body starts unemployed");

			if (!RedProof())
				LF_SpawnLogic.AdoptLivingBody(UID, body);

			ctx.True(jobs.GetJob() == LF_EJobType.FUEL_DELIVERY, "the restored body carries the saved fuel sign-up");
			ctx.Equal(careerLevel, jobs.GetJobLevel(), "the restored body carries the career level");

			// Idempotent: a second hand-over changes nothing.
			if (!RedProof())
				LF_SpawnLogic.AdoptLivingBody(UID, body);
			ctx.True(jobs.GetJob() == LF_EJobType.FUEL_DELIVERY, "applying twice keeps the fuel sign-up");
			ctx.Equal(careerLevel, jobs.GetJobLevel(), "applying twice keeps the career level");
		}

		LF_Utils.SetUidForTests(null, "");
		LF_CivilianJob.ForgetCareer(UID);
		LF_Progression.Forget(UID);
		LF_TestCharacterFixture.Delete(body);
	}
}
