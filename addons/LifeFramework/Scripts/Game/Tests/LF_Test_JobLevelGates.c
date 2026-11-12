// red-proof: observed 2026-11-12 with `tools\cli test --no-build --only LF_Test_JobLevelGates`: LF_CivilianJob.GetLevel
// reading CIVILIAN instead of DELIVERY failed `- CIVILIAN xp does not open delivery work`,
// `- 499 delivery xp is still level 1` and `- the taxi still needs level 3`. Restored; green.
// tier: LOGIC
//! Gates read job levels (#749, D12): delivery sign-up reads DELIVERY, a licence reads its family's
//! job (or the jobs config's licenceGates row), a chain step and a quest read the job they name.
class LF_Test_JobLevelGates : LF_Test
{
	static const string COURIER = "test-jobgates-courier";
	static const string MINER = "test-jobgates-miner";

	override string GetName()
	{
		return "jobs/job-level-gates";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_JobsConfig.Reload();
		LF_CivilianJob.ForgetCareer(COURIER);
		LF_JobLevels.Forget(COURIER);
		LF_JobLevels.Forget(MINER);

		// Delivery sign-up: fuel needs DELIVERY level 2 (500 xp), the taxi level 3 (1500 xp).
		ctx.False(LF_CivilianJob.CanSignUp(COURIER, LF_EJobType.FUEL_DELIVERY), "a new courier may not take fuel delivery");
		ctx.False(LF_CivilianJob.SignUp(COURIER, LF_EJobType.FUEL_DELIVERY), "fuel sign-up is refused below DELIVERY level 2");
		LF_JobLevels.AddXp(COURIER, LF_JobLevels.JOB_CIVILIAN, 5000, "test");
		ctx.False(LF_CivilianJob.CanSignUp(COURIER, LF_EJobType.FUEL_DELIVERY), "CIVILIAN xp does not open delivery work");
		LF_JobLevels.AddXp(COURIER, LF_JobLevels.JOB_DELIVERY, 499, "test");
		ctx.False(LF_CivilianJob.CanSignUp(COURIER, LF_EJobType.FUEL_DELIVERY), "499 delivery xp is still level 1");
		LF_JobLevels.AddXp(COURIER, LF_JobLevels.JOB_DELIVERY, 1, "test");
		ctx.True(LF_CivilianJob.SignUp(COURIER, LF_EJobType.FUEL_DELIVERY), "fuel sign-up is accepted at DELIVERY level 2");
		ctx.False(LF_CivilianJob.CanSignUp(COURIER, LF_EJobType.TAXI), "the taxi still needs level 3");
		LF_JobLevels.AddXp(COURIER, LF_JobLevels.JOB_DELIVERY, 1000, "test");
		ctx.True(LF_CivilianJob.CanSignUp(COURIER, LF_EJobType.TAXI), "the taxi opens at DELIVERY level 3");

		// Licences: the family's job and the catalog level. MINER_GOLD ships at level 6 in MINER.
		ctx.EqualStr("MINER", LF_LicenseGate.JobOf(LF_ELicenseType.MINER_GOLD), "a mining licence levels in MINER");
		ctx.EqualStr("FARMER", LF_LicenseGate.JobOf(LF_ELicenseType.FARMER_PLUM), "a farming licence levels in FARMER");
		ctx.EqualStr("POLICE", LF_LicenseGate.JobOf(LF_ELicenseType.POLICE_SEARCH), "a police licence levels in POLICE");
		ctx.EqualStr(LF_JobLevels.JOB_CIVILIAN, LF_LicenseGate.JobOf(LF_ELicenseType.CRIMINAL_WEED), "a criminal licence levels in CIVILIAN");
		ctx.False(LF_LicenseGate.Allows(MINER, LF_ELicenseType.MINER_GOLD, 6), "a new miner may not hold the gold licence");
		LF_JobLevels.AddXp(MINER, LF_JobLevels.JOB_CIVILIAN, 19000, "test");
		ctx.False(LF_LicenseGate.Allows(MINER, LF_ELicenseType.MINER_GOLD, 6), "CIVILIAN level 20 does not open a MINER licence");
		LF_JobLevels.AddXp(MINER, "MINER", 1499, "test");
		ctx.False(LF_LicenseGate.Allows(MINER, LF_ELicenseType.MINER_GOLD, 6), "MINER level 5 is below the gold licence");
		LF_JobLevels.AddXp(MINER, "MINER", 1, "test");
		ctx.True(LF_LicenseGate.Allows(MINER, LF_ELicenseType.MINER_GOLD, 6), "MINER level 6 meets the gold licence");
		ctx.True(LF_LicenseGate.Allows(MINER, LF_ELicenseType.CRIMINAL_WEED, 5), "CIVILIAN level 20 meets a criminal licence");

		// A licenceGates row moves the gate: MINER_GOLD to DELIVERY level 3.
		LF_JobsConfig config = LF_Test_JobLevels.MakeConfig();
		LF_JobsConfig shipped = LF_JobsConfig.Get();
		if (shipped)
			config.m_aTracks = shipped.m_aTracks;
		config.m_aLicenceGates.Insert(LF_JobLicenceGateDef.Make("MINER_GOLD", LF_JobLevels.JOB_DELIVERY, 3));
		LF_JobsConfig.UseForTest(config);
		string job;
		int minLevel;
		LF_LicenseGate.Resolve(LF_ELicenseType.MINER_GOLD, 6, job, minLevel);
		ctx.EqualStr(LF_JobLevels.JOB_DELIVERY, job, "the config row names the gate's job");
		ctx.Equal(3, minLevel, "the config row names the gate's level");
		ctx.False(LF_LicenseGate.Allows(MINER, LF_ELicenseType.MINER_GOLD, 6), "the miner has no DELIVERY level 3");
		ctx.True(LF_LicenseGate.Allows(COURIER, LF_ELicenseType.MINER_GOLD, 6), "the courier's DELIVERY level 3 meets the moved gate");
		LF_LicenseGate.Resolve(LF_ELicenseType.MINER_IRON, 4, job, minLevel);
		ctx.EqualStr("MINER", job, "a licence with no row keeps its family job");
		ctx.Equal(4, minLevel, "a licence with no row keeps its catalog level");
		LF_JobsConfig.Reload();

		// Chain steps and quests read the job they name; empty is CIVILIAN.
		LF_ChainStepDef step = new LF_ChainStepDef();
		ctx.EqualStr(LF_JobLevels.JOB_CIVILIAN, LF_ChainGate.LevelJob(step), "a chain step with no job reads CIVILIAN");
		step.m_sLevelJob = "MINER";
		ctx.EqualStr("MINER", LF_ChainGate.LevelJob(step), "a chain step reads the job it names");
		LF_QuestDefinition quest = new LF_QuestDefinition();
		ctx.EqualStr(LF_JobLevels.JOB_CIVILIAN, LF_QuestTrackerComponent.JobOf(quest), "a quest with no job reads CIVILIAN");

		LF_CivilianJob.ForgetCareer(COURIER);
		LF_JobLevels.Forget(COURIER);
		LF_JobLevels.Forget(MINER);
	}
}
