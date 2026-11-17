// red-proof: observed 2026-11-17 with `tools\cli test --no-build --only LF_Test_JobLevelWords` before the gates took their words from the helper: `test-e2e: FAILED (scripts do not compile)` (LF_ChainGate.LevelKey and LF_LicenseGate.RefusalKey did not exist); with them, green `passed=1 failed=0`.
// tier: LOGIC
//! A job-level gate refuses in words, never a number (#2700, outline decision 7): the helper picks a
//! `#` key by job family and by distance, the chain gate and the licence gate take their keys from it.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_JobLevelWords : LF_Test
{
	override string GetName()
	{
		return "level/job-level-words";
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasDigit(string text)
	{
		string digits = "0123456789";
		for (int i = 0; i < text.Length(); i++)
		{
			if (digits.Contains(text.Get(i)))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<string> jobs = {"FARMER", "MINER", "LUMBERJACK", "MEDIC", "POLICE", "DELIVERY", "CIVILIAN", "SOME_FUTURE_JOB", ""};
		foreach (string job : jobs)
		{
			string near = LF_JobLevelWords.Refusal(job, 2, 3);
			string far = LF_JobLevelWords.Refusal(job, 0, 5);
			ctx.True(near.StartsWith("#"), "one level short is a key for '" + job + "'");
			ctx.True(far.StartsWith("#"), "further short is a key for '" + job + "'");
			ctx.True(near != far, "one level short reads differently from further for '" + job + "'");
			ctx.False(HasDigit(near), "no digit in the near key for '" + job + "'");
			ctx.False(HasDigit(far), "no digit in the far key for '" + job + "'");
			ctx.EqualStr("", LF_JobLevelWords.Refusal(job, 3, 3), "a met gate has no refusal for '" + job + "'");
			ctx.EqualStr("", LF_JobLevelWords.Refusal(job, 9, 3), "a passed gate has no refusal for '" + job + "'");
			ctx.EqualStr(near, LF_JobLevelWords.Refusal(job, 0, 1), "the distance is what matters, not the levels");
		}

		ctx.True(LF_JobLevelWords.Refusal("FARMER", 0, 5) != LF_JobLevelWords.Refusal("MINER", 0, 5), "job families read differently");
		ctx.True(LF_JobLevelWords.Refusal("FARMER", 0, 5) != LF_JobLevelWords.Refusal("DELIVERY", 0, 5), "delivery has its own words");
		ctx.EqualStr(LF_JobLevelWords.Refusal("SOME_FUTURE_JOB", 0, 5), LF_JobLevelWords.Refusal("", 0, 5), "an unknown job falls back to the generic words");

		// The chain gate: a level refusal reads the step's job (CIVILIAN when it names none).
		LF_ChainStepDef step = new LF_ChainStepDef();
		step.m_iMinLevel = 5;
		step.m_sLevelJob = "MINER";
		ctx.EqualStr(LF_JobLevelWords.Refusal("MINER", 4, 5), LF_ChainGate.LevelKey(step, 4), "the chain gate says it with the helper's words");
		ctx.EqualStr(LF_JobLevelWords.Refusal("MINER", 1, 5), LF_ChainGate.LevelKey(step, 1), "the chain gate far key is the helper's");
		ctx.EqualStr(LF_JobLevelWords.Refusal("MINER", 4, 5), LF_ChainGate.ReasonKey(LF_EChainGate.LEVEL, step, 4), "ReasonKey carries it for a level refusal");
		ctx.EqualStr("#LF-Chain_NeedLicense", LF_ChainGate.ReasonKey(LF_EChainGate.LICENSE, step, 4), "other refusals keep their key");
		step.m_sLevelJob = "";
		ctx.EqualStr(LF_JobLevelWords.Refusal(LF_JobLevels.JOB_CIVILIAN, 0, 5), LF_ChainGate.LevelKey(step, 0), "a step with no job reads CIVILIAN");

		// The licence gate: the family's job and the catalog level (MINER_GOLD ships at MINER level 6).
		ctx.EqualStr(LF_JobLevelWords.Refusal("MINER", 5, 6), LF_LicenseGate.RefusalKey(LF_ELicenseType.MINER_GOLD, 6, 5), "a miner one level short of gold hears the near words");
		ctx.EqualStr(LF_JobLevelWords.Refusal("MINER", 1, 6), LF_LicenseGate.RefusalKey(LF_ELicenseType.MINER_GOLD, 6, 1), "a new miner hears the far words");
		ctx.EqualStr("", LF_LicenseGate.RefusalKey(LF_ELicenseType.MINER_GOLD, 6, 6), "a miner at the level hears nothing");
		ctx.True(LF_LicenseGate.RefusalKey(LF_ELicenseType.FARMER_PLUM, 4, 1) != LF_LicenseGate.RefusalKey(LF_ELicenseType.MINER_GOLD, 6, 1), "a farm licence and a mine licence read differently");
	}
}
