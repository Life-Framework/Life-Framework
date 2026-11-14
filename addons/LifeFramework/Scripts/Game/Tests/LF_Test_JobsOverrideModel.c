// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_JobsOverrideModel`: with RemoveAt a no-op in the three job lists and LF_ConfigOverlay.Apply turning a patch of a missing id into an add, it failed `- tracks: remove takes the job out`, `- tracks: patch of a missing id is refused, not added (even when it carries a whole record)`, `- xpAwards: remove takes the action out`, `- licenceGates: patch of a licence with no gate is refused, not added` and `- licenceGates: GOLD, COAL and COPPER are left (expected 3, got 5)`. Restored; green.
// tier: LOGIC
//! #1106: the jobs lists on the override model (#962). Each list (tracks by job id, xpAwards by
//! action id, licenceGates by licence name) takes add, replace, patch and remove from the owner's
//! section; `remove` takes the entry out, `patch` of a missing id is refused, and a refusal skips
//! only its own record. The no-verb upsert of #749 is tested in LF_Test_JobLevelsConfig.
class LF_Test_JobsOverrideModel : LF_Test
{
	override string GetName()
	{
		return "config/jobs-override-model";
	}

	//------------------------------------------------------------------------------------------------
	//! The test fixture plus three licence gates, with `json` as the owner's "jobs" section.
	protected LF_JobsConfig Merge(string sectionJson, out LF_ConfigReader reader)
	{
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"jobs\":" + sectionJson + "}", "test");
		LF_JobsConfig config = LF_Test_JobLevels.MakeConfig();
		config.m_aLicenceGates.Insert(LF_JobLicenceGateDef.Make("MINER_GOLD", "MINE", 2));
		config.m_aLicenceGates.Insert(LF_JobLicenceGateDef.Make("MINER_IRON", "MINE", 3));
		config.m_aLicenceGates.Insert(LF_JobLicenceGateDef.Make("MINER_COAL", "MINE", 1));
		reader = LF_ConfigService.OpenOwnerSection(LF_JobsConfig.SECTION);
		if (reader)
			config.ApplyOwner(reader, "test-owner.json");
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected int IndexOfTrack(LF_JobsConfig config, string job)
	{
		foreach (int i, LF_JobTrackDef track : config.m_aTracks)
		{
			if (track && track.m_sJob == job)
				return i;
		}
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		RunTracks(ctx);
		RunAwards(ctx);
		RunGates(ctx);
		LF_ConfigService.Reset();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunTracks(LF_TestContext ctx)
	{
		LF_ConfigReader reader;
		LF_JobsConfig config = Merge("{\"tracks\":{\"WOOD\":{\"op\":\"replace\",\"levelThresholds\":[10]},\"MINE\":{\"op\":\"remove\"},\"FISH\":{\"op\":\"add\",\"levelThresholds\":[5]},\"GHOST\":{\"op\":\"patch\",\"levelThresholds\":[5],\"dailyXpCap\":5},\"NOPE\":{\"op\":\"remove\"}}}", reader);
		ctx.NotNull(reader, "tracks: the jobs section opens");
		if (!reader)
			return;

		LF_JobTrackDef wood = config.FindTrack("WOOD");
		ctx.NotNull(wood, "tracks: replace keeps the job");
		if (wood)
		{
			ctx.Equal(2, wood.LevelFor(10), "tracks: replace takes the layer's thresholds");
			ctx.Equal(0, wood.m_iDailyXpCap, "tracks: replace drops the shipped cap (class default)");
		}
		ctx.Equal(0, IndexOfTrack(config, "WOOD"), "tracks: replace keeps the place in the list");
		ctx.True(config.FindTrack("MINE") == null, "tracks: remove takes the job out");
		ctx.NotNull(config.FindTrack("FISH"), "tracks: add puts a new job in");
		ctx.True(config.FindTrack("GHOST") == null, "tracks: patch of a missing id is refused, not added (even when it carries a whole record)");
		ctx.Equal(2, config.m_aTracks.Count(), "tracks: WOOD and FISH are left");
		ctx.Equal(2, reader.GetRejectedCount(), "tracks: the patch of a missing id and the remove of a missing id are refused");

		// Patch changes only the fields given; add of an id the list has, add with no thresholds and a
		// bad verb are refused.
		config = Merge("{\"tracks\":{\"MINE\":{\"op\":\"patch\",\"dailyXpCap\":9},\"WOOD\":{\"op\":\"add\",\"levelThresholds\":[1]},\"BAKE\":{\"op\":\"add\"},\"ODD\":{\"op\":\"delete\"}}}", reader);
		LF_JobTrackDef mine = config.FindTrack("MINE");
		ctx.NotNull(mine, "tracks: patch keeps the job");
		if (mine)
		{
			ctx.Equal(9, mine.m_iDailyXpCap, "tracks: patch applies the field given");
			ctx.Equal(2, mine.LevelFor(50), "tracks: patch keeps the thresholds it does not name");
		}
		wood = config.FindTrack("WOOD");
		if (wood)
			ctx.Equal(2, wood.LevelFor(100), "tracks: add of an existing job is refused and leaves its table");
		ctx.True(config.FindTrack("BAKE") == null, "tracks: a new job with no thresholds is refused");
		ctx.True(config.FindTrack("ODD") == null, "tracks: an unknown verb is refused");
		ctx.Equal(3, reader.GetRejectedCount(), "tracks: three records refused");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunAwards(LF_TestContext ctx)
	{
		LF_ConfigReader reader;
		LF_JobsConfig config = Merge("{\"xpAwards\":{\"chop\":{\"op\":\"patch\",\"xp\":55},\"dig\":{\"op\":\"remove\"},\"cast\":{\"op\":\"add\",\"job\":\"MINE\",\"xp\":3},\"nope\":{\"op\":\"patch\",\"job\":\"WOOD\",\"xp\":1},\"bare\":{\"op\":\"add\",\"xp\":3},\"ghost\":{\"op\":\"replace\",\"job\":\"WOOD\"}}}", reader);
		ctx.NotNull(reader, "xpAwards: the jobs section opens");
		if (!reader)
			return;

		LF_JobXpAwardDef chop = config.FindAward("chop");
		ctx.NotNull(chop, "xpAwards: patch keeps the action");
		if (chop)
		{
			ctx.Equal(55, chop.m_iXp, "xpAwards: patch applies the field given");
			ctx.EqualStr("WOOD", chop.m_sJob, "xpAwards: patch keeps the job it does not name");
			ctx.EqualFloat(30, chop.m_fCooldownSeconds, 0.01, "xpAwards: patch keeps the cooldown it does not name");
		}
		ctx.True(config.FindAward("dig") == null, "xpAwards: remove takes the action out");
		LF_JobXpAwardDef cast = config.FindAward("cast");
		ctx.NotNull(cast, "xpAwards: add puts a new action in");
		if (cast)
			ctx.Equal(3, cast.m_iXp, "xpAwards: the new action carries its xp");
		ctx.True(config.FindAward("nope") == null, "xpAwards: patch of a missing id is refused, not added");
		ctx.True(config.FindAward("bare") == null, "xpAwards: a new action with no job is refused");

		// ghost is in the fixture: replace rebuilds it from the layer alone.
		LF_JobXpAwardDef ghost = config.FindAward("ghost");
		ctx.NotNull(ghost, "xpAwards: replace keeps the action");
		if (ghost)
		{
			ctx.EqualStr("WOOD", ghost.m_sJob, "xpAwards: replace takes the layer's job");
			ctx.Equal(0, ghost.m_iXp, "xpAwards: replace drops the shipped xp (class default)");
		}
		ctx.Equal(2, reader.GetRejectedCount(), "xpAwards: the missing patch and the add with no job are refused");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunGates(LF_TestContext ctx)
	{
		LF_ConfigReader reader;
		LF_JobsConfig config = Merge("{\"licenceGates\":{\"MINER_GOLD\":{\"op\":\"replace\",\"job\":\"WOOD\",\"minLevel\":4},\"MINER_IRON\":{\"op\":\"remove\"},\"MINER_COAL\":{\"op\":\"patch\",\"minLevel\":2},\"MINER_COPPER\":{\"op\":\"add\",\"job\":\"MINE\",\"minLevel\":2},\"MEDIC_BASIC\":{\"op\":\"patch\",\"job\":\"MEDIC\",\"minLevel\":2},\"NOT_A_LICENCE\":{\"op\":\"add\",\"job\":\"MINE\",\"minLevel\":2},\"MINER_SPEED\":{\"op\":\"add\",\"job\":\"MINE\"},\"MINER_COAL_X\":{\"op\":\"remove\"}}}", reader);
		ctx.NotNull(reader, "licenceGates: the jobs section opens");
		if (!reader)
			return;

		LF_JobLicenceGateDef gold = config.FindLicenceGate("MINER_GOLD");
		ctx.NotNull(gold, "licenceGates: replace keeps the licence");
		if (gold)
		{
			ctx.EqualStr("WOOD", gold.m_sJob, "licenceGates: replace takes the layer's job");
			ctx.Equal(4, gold.m_iMinLevel, "licenceGates: replace takes the layer's level");
		}
		ctx.True(config.FindLicenceGate("MINER_IRON") == null, "licenceGates: remove takes the gate out (the licence keeps its shipped gate)");
		LF_JobLicenceGateDef coal = config.FindLicenceGate("MINER_COAL");
		ctx.NotNull(coal, "licenceGates: patch keeps the gate");
		if (coal)
		{
			ctx.Equal(2, coal.m_iMinLevel, "licenceGates: patch applies the level given");
			ctx.EqualStr("MINE", coal.m_sJob, "licenceGates: patch keeps the job it does not name");
		}
		ctx.NotNull(config.FindLicenceGate("MINER_COPPER"), "licenceGates: add puts a new gate in");
		ctx.True(config.FindLicenceGate("MEDIC_BASIC") == null, "licenceGates: patch of a licence with no gate is refused, not added");
		ctx.True(config.FindLicenceGate("NOT_A_LICENCE") == null, "licenceGates: a key that is no licence name is refused");
		ctx.True(config.FindLicenceGate("MINER_SPEED") == null, "licenceGates: a new gate with no level is refused");
		ctx.Equal(3, config.m_aLicenceGates.Count(), "licenceGates: GOLD, COAL and COPPER are left");
		ctx.Equal(4, reader.GetRejectedCount(), "licenceGates: the patch with no gate, the bad key, the add with no level and the unknown key are refused");
	}
}
