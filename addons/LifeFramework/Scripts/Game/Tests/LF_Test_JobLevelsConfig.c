// red-proof: observed 2026-11-12 with `tools\cli test --no-build --only LF_Test_JobLevelsConfig`: LF_ConfigReader.ReadInts
// without its minimum check failed `- a threshold below 1 refuses the whole list (expected 2, got 3)`. Restored; green.
// tier: LOGIC
//! The owner's "jobs" section (#749, D8): tracks, XP awards and licence gates merge per key over the
//! shipped records, numbers outside the schema's range are refused, a threshold list with a bad
//! element is refused whole, and whole numbers read as fractions where a float is expected.
class LF_Test_JobLevelsConfig : LF_Test
{
	override string GetName()
	{
		return "config/jobs-owner";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_JobsConfig Merge(string sectionJson)
	{
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"jobs\":" + sectionJson + "}", "test");
		LF_JobsConfig config = LF_Test_JobLevels.MakeConfig();
		config.m_fPoliceDutyPayPerSecond = 0.5;
		config.m_iBenefitAmount = 100;
		config.ApplyLayers();
		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// A track's thresholds and cap change per key; a job the owner leaves out keeps its table.
		LF_JobsConfig config = Merge("{\"tracks\":{\"WOOD\":{\"levelThresholds\":[10,20],\"dailyXpCap\":0}}}");
		LF_JobTrackDef wood = config.FindTrack("WOOD");
		ctx.NotNull(wood, "WOOD is still a track");
		if (wood)
		{
			ctx.Equal(3, wood.LevelFor(20), "the owner's thresholds replace the shipped ones");
			ctx.Equal(0, wood.m_iDailyXpCap, "the owner removes the cap");
		}
		LF_JobTrackDef mine = config.FindTrack("MINE");
		if (mine)
			ctx.Equal(2, mine.LevelFor(50), "a track the owner left out keeps its table");

		// A new job needs thresholds; a new action needs a job; a new gate needs a licence name, a job and a level.
		config = Merge("{\"tracks\":{\"FISH\":{\"levelThresholds\":[5]},\"BAKE\":{\"dailyXpCap\":9}},\"xpAwards\":{\"cast\":{\"job\":\"FISH\",\"xp\":3},\"bare\":{\"xp\":3}},\"licenceGates\":{\"MINER_GOLD\":{\"job\":\"MINE\",\"minLevel\":2},\"NOT_A_LICENCE\":{\"job\":\"MINE\",\"minLevel\":2},\"MINER_IRON\":{\"job\":\"MINE\"}}}");
		ctx.NotNull(config.FindTrack("FISH"), "a new job with thresholds is added");
		ctx.True(config.FindTrack("BAKE") == null, "a new job without thresholds is refused");
		LF_JobXpAwardDef cast = config.FindAward("cast");
		ctx.NotNull(cast, "a new action with a job is added");
		if (cast)
			ctx.Equal(3, cast.m_iXp, "the new action carries its xp");
		ctx.True(config.FindAward("bare") == null, "a new action without a job is refused");
		LF_JobLicenceGateDef gold = config.FindLicenceGate("MINER_GOLD");
		ctx.NotNull(gold, "a licence gate is added by licence name");
		if (gold)
		{
			ctx.EqualStr("MINE", gold.m_sJob, "the gate names its job");
			ctx.Equal(2, gold.m_iMinLevel, "the gate names its level");
		}
		ctx.True(config.FindLicenceGate("NOT_A_LICENCE") == null, "a key that is no licence name is refused");
		ctx.True(config.FindLicenceGate("MINER_IRON") == null, "a new gate without a level is refused");

		// An existing award changes only the fields given.
		config = Merge("{\"xpAwards\":{\"chop\":{\"xp\":55}}}");
		LF_JobXpAwardDef chop = config.FindAward("chop");
		if (chop)
		{
			ctx.Equal(55, chop.m_iXp, "the owner's xp replaces the shipped one");
			ctx.EqualStr("WOOD", chop.m_sJob, "the job the owner left out stays");
			ctx.EqualFloat(30, chop.m_fCooldownSeconds, 0.01, "the cooldown the owner left out stays");
		}

		// Out of range or the wrong kind: refused, the shipped value stays.
		config = Merge("{\"benefitAmount\":-1,\"policeDutyPayPerSecond\":1000,\"tracks\":{\"WOOD\":{\"levelThresholds\":[0,5],\"dailyXpCap\":-3}},\"xpAwards\":{\"chop\":{\"cooldownSeconds\":100000}},\"licenceGates\":{\"MINER_GOLD\":{\"job\":\"MINE\",\"minLevel\":0}}}");
		ctx.Equal(100, config.m_iBenefitAmount, "a negative benefit is refused");
		ctx.EqualFloat(0.5, config.m_fPoliceDutyPayPerSecond, 0.001, "a pay rate above the range is refused");
		wood = config.FindTrack("WOOD");
		if (wood)
		{
			ctx.Equal(2, wood.LevelFor(100), "a threshold below 1 refuses the whole list");
			ctx.Equal(250, wood.m_iDailyXpCap, "a negative cap is refused");
		}
		chop = config.FindAward("chop");
		if (chop)
			ctx.EqualFloat(30, chop.m_fCooldownSeconds, 0.01, "a cooldown above a day is refused");
		ctx.True(config.FindLicenceGate("MINER_GOLD") == null, "a new gate with a level below 1 is refused");

		config = Merge("{\"tracks\":{\"WOOD\":{\"levelThresholds\":[10,\"x\",30]},\"MINE\":{\"levelThresholds\":7}}}");
		wood = config.FindTrack("WOOD");
		if (wood)
			ctx.Equal(1, wood.LevelFor(99), "a threshold list holding a string is refused whole");
		mine = config.FindTrack("MINE");
		if (mine)
			ctx.Equal(2, mine.LevelFor(50), "a number where the list belongs is refused");

		// A float field takes a whole number as well as a fraction.
		config = Merge("{\"policeDutyPayPerSecond\":2,\"medicDutyPayPerSecond\":0.25,\"benefitCooldownSeconds\":60}");
		ctx.EqualFloat(2, config.m_fPoliceDutyPayPerSecond, 0.001, "a whole number reads into a float field");
		ctx.EqualFloat(0.25, config.m_fMedicDutyPayPerSecond, 0.001, "a fraction reads into a float field");
		ctx.EqualFloat(60, config.m_fBenefitCooldownSeconds, 0.001, "the benefit cooldown is owner config");

		// The shipped file: every job the game awards has a track, and the delivery pacing holds.
		LF_ConfigService.Reset();
		LF_JobsConfig.Reload();
		LF_JobsConfig shipped = LF_JobsConfig.Get();
		ctx.NotNull(shipped, "the shipped jobs config loads");
		if (shipped)
		{
			array<string> actions = {LF_JobLevels.ACTION_DELIVERY_RUN, LF_JobLevels.ACTION_FRUIT_PICK, LF_JobLevels.ACTION_GATHER};
			foreach (string action : actions)
			{
				LF_JobXpAwardDef award = shipped.FindAward(action);
				ctx.NotNull(award, "the shipped config awards " + action);
				if (award)
					ctx.NotNull(shipped.FindTrack(award.m_sJob), action + " feeds a job that has a track");
			}

			array<string> jobs = {LF_JobLevels.JOB_CIVILIAN, LF_JobLevels.JOB_DELIVERY, "FARMER", "MINER", "LUMBERJACK", "MEDIC", "POLICE"};
			foreach (string job : jobs)
			{
				ctx.NotNull(shipped.FindTrack(job), "the shipped config has the " + job + " track");
			}

			// The CIVILIAN ladder is the pre-#749 single level, so a migrated save keeps its level.
			LF_JobTrackDef civilian = shipped.FindTrack(LF_JobLevels.JOB_CIVILIAN);
			if (civilian)
			{
				for (int level = 1; level <= 20; level++)
				{
					ctx.Equal(level, civilian.LevelFor(LF_JobLevels.LegacyLevelToXp(level, 0)), string.Format("a pre-#749 single level %1 is CIVILIAN level %1", level));
				}
			}
			LF_JobTrackDef delivery = shipped.FindTrack(LF_JobLevels.JOB_DELIVERY);
			if (delivery)
			{
				for (int careerLevel = 1; careerLevel <= 7; careerLevel++)
				{
					ctx.Equal(careerLevel, delivery.LevelFor(LF_JobLevels.LegacyCareerToXp(careerLevel, 0)), string.Format("a pre-#749 career level %1 is DELIVERY level %1", careerLevel));
				}
			}
		}
		LF_JobsConfig.Reload();
	}
}
