// red-proof: 2026-11-18, `--red LF_Test_JobLevelSources` unsubscribes LF_JobLevelSources: RED PROOF OK, the delivery, self-revive and daily-cap checks fail (expected 50, got 0 and expected 13, got 0). Green with the subscriber on.
// tier: LOGIC
//! jobs/level-sources (#2707): LF_OnGathered, LF_OnPlayerRevived and LF_OnPatientDelivered award the
//! shipped `jobs.xpAwards` rows to MINER, LUMBERJACK and MEDIC; a repeat inside the cooldown, a
//! gather family that pays nothing, a medic with no player id and a self-revive award nothing; the
//! track's daily cap cuts the rest.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_JobLevelSources : LF_Test
{
	static const string MINER = "test-sources-miner";
	static const string CUTTER = "test-sources-cutter";
	static const string MEDIC = "test-sources-medic";
	static const string PATIENT = "test-sources-patient";

	override string GetName()
	{
		return "jobs/level-sources";
	}

	//------------------------------------------------------------------------------------------------
	protected void Gather(string uid, string family)
	{
		LF_EventBus.Publish(LF_OnGathered.Create(uid, "node-1", family, "Prefabs/Items/Resources/Sandbag.et", 1, 1.0));
	}

	//------------------------------------------------------------------------------------------------
	protected void ForgetAll()
	{
		LF_JobLevels.Forget(MINER);
		LF_JobLevels.Forget(CUTTER);
		LF_JobLevels.Forget(MEDIC);
		LF_JobLevels.Forget(PATIENT);
	}

	//------------------------------------------------------------------------------------------------
	protected int Rows(LF_JobsConfig config, string action, string job, LF_TestContext ctx)
	{
		LF_JobXpAwardDef award = config.FindAward(action);
		ctx.NotNull(award, "the shipped config awards " + action);
		if (!award)
			return 0;

		ctx.EqualStr(job, award.m_sJob, action + " feeds " + job);
		ctx.True(award.m_fCooldownSeconds > 0, action + " has a cooldown");
		return award.m_iXp;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_JobsConfig.Reload();
		LF_JobsConfig config = LF_JobsConfig.Get();
		ctx.NotNull(config, "the shipped jobs config loads");
		if (!config)
			return;

		int mining = Rows(config, LF_JobLevelSources.ACTION_GATHER_MINING, "MINER", ctx);
		int logging = Rows(config, LF_JobLevelSources.ACTION_GATHER_LOGGING, "LUMBERJACK", ctx);
		int revive = Rows(config, LF_JobLevelSources.ACTION_MEDIC_REVIVE, "MEDIC", ctx);
		int delivery = Rows(config, LF_JobLevelSources.ACTION_MEDIC_DELIVERY, "MEDIC", ctx);
		LF_JobTrackDef minerTrack = config.FindTrack("MINER");
		ctx.True(minerTrack && minerTrack.m_iDailyXpCap > 0, "the MINER track has a daily cap");

		ForgetAll();
		if (RedProof())
			LF_JobLevelSources.ResetStatics();
		else
			LF_JobLevelSources.EnsureSubscribed();

		// Gathering: mining pays MINER, logging pays LUMBERJACK, digging and foraging pay nothing.
		Gather(MINER, LF_JobLevelSources.FAMILY_MINING);
		ctx.Equal(mining, LF_JobLevels.XpOf(MINER, "MINER"), "a mining gather awards MINER xp");
		Gather(MINER, LF_JobLevelSources.FAMILY_MINING);
		ctx.Equal(mining, LF_JobLevels.XpOf(MINER, "MINER"), "a second mining gather inside the cooldown awards nothing");
		Gather(MINER, LF_GatherTools.FAMILY_DIGGING);
		Gather(MINER, LF_GatherTools.FAMILY_FORAGING);
		ctx.Equal(mining, LF_JobLevels.XpOf(MINER, "MINER"), "digging and foraging award no MINER xp");
		ctx.Equal(0, LF_JobLevels.XpOf(MINER, "LUMBERJACK"), "mining does not feed LUMBERJACK");

		Gather(CUTTER, LF_JobLevelSources.FAMILY_LOGGING);
		ctx.Equal(logging, LF_JobLevels.XpOf(CUTTER, "LUMBERJACK"), "a logging gather awards LUMBERJACK xp");
		ctx.Equal(0, LF_JobLevels.XpOf(CUTTER, "MINER"), "logging does not feed MINER");

		// Medic work: a revive and a delivery both pay the medic, never the patient.
		LF_EventBus.Publish(LF_OnPlayerRevived.Create(PATIENT, MEDIC, "0 0 0"));
		ctx.Equal(revive, LF_JobLevels.XpOf(MEDIC, "MEDIC"), "a revive awards MEDIC xp to the medic");
		LF_EventBus.Publish(LF_OnPlayerRevived.Create(PATIENT, MEDIC, "0 0 0"));
		ctx.Equal(revive, LF_JobLevels.XpOf(MEDIC, "MEDIC"), "a second revive inside the cooldown awards nothing");
		LF_EventBus.Publish(LF_OnPatientDelivered.Create(PATIENT, MEDIC, 1, "0 0 0"));
		ctx.Equal(revive + delivery, LF_JobLevels.XpOf(MEDIC, "MEDIC"), "a patient delivered to a bed awards MEDIC xp");
		ctx.Equal(0, LF_JobLevels.XpOf(PATIENT, "MEDIC"), "the patient earns no MEDIC xp");

		LF_EventBus.Publish(LF_OnPlayerRevived.Create(MEDIC, MEDIC, "0 0 0"));
		LF_EventBus.Publish(LF_OnPatientDelivered.Create(PATIENT, "", 0, "0 0 0"));
		ctx.Equal(revive + delivery, LF_JobLevels.XpOf(MEDIC, "MEDIC"), "a self-revive earns nothing");

		// The track's daily cap holds back a farm that has no cooldown left.
		LF_JobXpAwardDef mine = config.FindAward(LF_JobLevelSources.ACTION_GATHER_MINING);
		if (mine && minerTrack)
		{
			float cooldown = mine.m_fCooldownSeconds;
			int cap = minerTrack.m_iDailyXpCap;
			ForgetAll();
			mine.m_fCooldownSeconds = 0;
			minerTrack.m_iDailyXpCap = mining + 3;
			Gather(MINER, LF_JobLevelSources.FAMILY_MINING);
			Gather(MINER, LF_JobLevelSources.FAMILY_MINING);
			Gather(MINER, LF_JobLevelSources.FAMILY_MINING);
			ctx.Equal(mining + 3, LF_JobLevels.XpOf(MINER, "MINER"), "the daily cap cuts the gathers to what is left");
			mine.m_fCooldownSeconds = cooldown;
			minerTrack.m_iDailyXpCap = cap;
		}

		ForgetAll();
		LF_JobLevelSources.EnsureSubscribed();
		LF_JobsConfig.Reload();
	}
}
