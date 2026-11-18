// red-proof: observed 2026-11-18 (#2740) with `tools\cli test --no-build --red LF_Test_DialogueJobSignUp`: RedProof() skips the hook run, so `the reply signs the player up for parcels (expected 5, got 0)` and `the sign-up is recorded for the player's account` fail (RED-OK dialogue/job-sign-up).
//! #2740: a dialogue reply can sign the player up for a job. `LF_DialogueHook_JobSignUp` reads the
//! job from the reply's param (PARCEL), signs the player up and sets the job on the body, server-side.
//! A param that is no delivery job, or a job above the player's career level, changes nothing.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_DialogueJobSignUp : LF_Test
{
	protected static const string UID = "dialogue-jobsignup-uid";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "dialogue/job-sign-up";
	}

	//------------------------------------------------------------------------------------------------
	//! Runs the reply's action as the server does when the player picks it.
	protected void Pick(IEntity player, string param)
	{
		LF_DialogueHook hook = LF_DialogueHook.Find("JobSignUp");
		if (!hook)
			return;

		array<string> spoken = {};
		hook.Run(player, null, param, spoken);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.NotNull(LF_DialogueHook.Find("JobSignUp"), "the JobSignUp dialogue hook has its class");
		ctx.Equal(LF_EJobType.PARCEL, LF_DialogueHook_JobSignUp.JobOf("PARCEL"), "PARCEL names the parcel job");
		ctx.Equal(LF_EJobType.UNEMPLOYED, LF_DialogueHook_JobSignUp.JobOf("NO_SUCH_JOB"), "an unknown job name names nothing");
		ctx.Equal(LF_EJobType.UNEMPLOYED, LF_DialogueHook_JobSignUp.JobOf("MINER"), "a job that is not delivery work names nothing");
		ctx.Equal(LF_EJobType.UNEMPLOYED, LF_DialogueHook_JobSignUp.JobOf(""), "an empty param names nothing");

		IEntity player = LF_ExploitGuardsWorldFixture.SpawnCharacter(ctx, "100 0 100", false);
		if (!player)
			return;

		LF_Utils.SetUidForTests(player, UID);
		LF_PlayerJobComponent jobs = LF_Component<LF_PlayerJobComponent>.Find(player);
		ctx.NotNull(jobs, "the character has a job component");
		if (!jobs)
		{
			LF_ExploitGuardsWorldFixture.Despawn(player);
			return;
		}

		ctx.Equal(LF_EJobType.UNEMPLOYED, jobs.GetJob(), "the character starts unemployed");

		// Refused picks change nothing: an unknown job, a non-delivery job, a job above career level 1.
		Pick(player, "NO_SUCH_JOB");
		Pick(player, "MINER");
		Pick(player, "TAXI");
		ctx.Equal(LF_EJobType.UNEMPLOYED, jobs.GetJob(), "an unknown, non-delivery or over-level job leaves the player unemployed");
		ctx.Equal(LF_EJobType.UNEMPLOYED, LF_CivilianJob.GetSignedUpJob(UID), "no sign-up was recorded by a refused pick");

		// The act: the take-the-job reply. Red proof: skip the hook run.
		if (!RedProof())
			Pick(player, "PARCEL");

		ctx.Equal(LF_EJobType.PARCEL, jobs.GetJob(), "the reply signs the player up for parcels");
		ctx.Equal(LF_EJobType.PARCEL, LF_CivilianJob.GetSignedUpJob(UID), "the sign-up is recorded for the player's account");

		// Picking it again is harmless.
		Pick(player, "PARCEL");
		ctx.Equal(LF_EJobType.PARCEL, jobs.GetJob(), "picking the reply again keeps the job");

		LF_Utils.SetUidForTests(null, "");
		LF_CivilianJob.ForgetCareer(UID);
		LF_ExploitGuardsWorldFixture.Despawn(player);
	}
}
