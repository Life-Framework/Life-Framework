// red-proof: 2026-11-23, `--red LF_Test_TradeSources` points the smelt row at MINER: RED PROOF OK (a smelter batch awards its xp: expected 12, got 0). Red first: the script did not compile before AwardSmelting existed.
//! jobs/trade-sources (#4184, W2 of the 0.3.0 work scope, decision N3): SMELTING and SALVAGE are trades of their
//! own. A smelter batch awards SMELTING xp and never MINER xp; a salvage award goes to SALVAGE; both tracks read the
//! same 30-level table as the other trades. An award row that points at another trade (the old "batch gives MINER xp"
//! idea) pays nothing and logs an error. The test uses the shipped rows when the jobs config has them and adds rows
//! in memory when it has not (the config rows are W1's files), then reloads the config.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_TradeSources : LF_Test
{
	static const string SMELTER = "test-trade-smelter";
	static const string SCRAPPER = "test-trade-scrapper";

	override string GetName()
	{
		return "jobs/trade-sources";
	}

	//------------------------------------------------------------------------------------------------
	//! The track and the award row for one trade; adds an in-memory copy of the MINER table when none ships.
	protected LF_JobXpAwardDef Prepare(LF_JobsConfig config, string trade, string action, int xp, LF_TestContext ctx)
	{
		LF_JobTrackDef miner = config.FindTrack("MINER");
		ctx.NotNull(miner, "the shipped MINER table loads");
		if (!miner || !miner.m_aLevelThresholds)
			return null;

		if (!config.FindTrack(trade))
			config.m_aTracks.Insert(LF_JobTrackDef.Make(trade, miner.m_aLevelThresholds, miner.m_iDailyXpCap));

		LF_JobTrackDef track = config.FindTrack(trade);
		ctx.NotNull(track, trade + " has a track");
		if (track && track.m_aLevelThresholds)
			ctx.Equal(miner.m_aLevelThresholds.Count(), track.m_aLevelThresholds.Count(), trade + " reads the same level table as MINER");

		if (!config.FindAward(action))
			config.m_aXpAwards.Insert(LF_JobXpAwardDef.Make(action, trade, xp, 0));

		LF_JobXpAwardDef award = config.FindAward(action);
		ctx.NotNull(award, action + " has an award row");
		if (award)
		{
			ctx.EqualStr(trade, award.m_sJob, action + " feeds " + trade);
			ctx.True(award.m_iXp > 0, action + " is worth some xp");
		}

		return award;
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

		ctx.EqualStr("SMELTING", typename.EnumToString(LF_EJobType, LF_EJobType.SMELTING), "SMELTING is a job type");
		ctx.EqualStr("SALVAGE", typename.EnumToString(LF_EJobType, LF_EJobType.SALVAGE), "SALVAGE is a job type");

		LF_JobXpAwardDef smelt = Prepare(config, "SMELTING", LF_JobLevelSources.ACTION_SMELT_BATCH, 12, ctx);
		LF_JobXpAwardDef salvage = Prepare(config, "SALVAGE", LF_JobLevelSources.ACTION_SALVAGE_WORK, 8, ctx);
		if (!smelt || !salvage)
		{
			LF_JobsConfig.Reload();
			return;
		}

		smelt.m_fCooldownSeconds = 0;
		salvage.m_fCooldownSeconds = 0;
		LF_JobLevels.Forget(SMELTER);
		LF_JobLevels.Forget(SCRAPPER);

		if (RedProof())
			smelt.m_sJob = "MINER";

		// A smelter batch pays SMELTING and nothing else.
		int paid = LF_JobLevelSources.AwardSmelting(SMELTER);
		ctx.Equal(smelt.m_iXp, paid, "a smelter batch awards its xp");
		ctx.Equal(smelt.m_iXp, LF_JobLevels.XpOf(SMELTER, "SMELTING"), "a smelter batch awards SMELTING xp");
		ctx.Equal(0, LF_JobLevels.XpOf(SMELTER, "MINER"), "a smelter batch awards no MINER xp");
		ctx.Equal(0, LF_JobLevels.XpOf(SMELTER, "SALVAGE"), "a smelter batch awards no SALVAGE xp");

		LF_JobLevelSources.AwardSmelting(SMELTER, 2);
		ctx.Equal(smelt.m_iXp * 3, LF_JobLevels.XpOf(SMELTER, "SMELTING"), "three batches award three batches of xp");

		// Salvage work pays SALVAGE and nothing else.
		LF_JobLevelSources.AwardSalvage(SCRAPPER);
		ctx.Equal(salvage.m_iXp, LF_JobLevels.XpOf(SCRAPPER, "SALVAGE"), "salvage work awards SALVAGE xp");
		ctx.Equal(0, LF_JobLevels.XpOf(SCRAPPER, "MINER"), "salvage work awards no MINER xp");
		ctx.Equal(0, LF_JobLevels.XpOf(SCRAPPER, "SMELTING"), "salvage work awards no SMELTING xp");

		// A row that points a batch at MINER (the old #3239 idea) pays nothing: the trade is the contract.
		smelt.m_sJob = "MINER";
		LF_JobLevels.Forget(SMELTER);
		ctx.Equal(0, LF_JobLevelSources.AwardSmelting(SMELTER), "a smelt row that feeds MINER pays nothing");
		ctx.Equal(0, LF_JobLevels.XpOf(SMELTER, "MINER"), "the misrouted row gave no MINER xp");

		LF_JobLevels.Forget(SMELTER);
		LF_JobLevels.Forget(SCRAPPER);
		LF_JobsConfig.Reload();
	}
}
