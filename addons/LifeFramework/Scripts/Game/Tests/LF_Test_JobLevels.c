// red-proof: observed 2026-11-12 with `tools\cli test --no-build --only LF_Test_JobLevels`: LF_JobLevels.CapToday ignoring the
// cap failed `- the daily cap cuts the grant to what is left (expected 150, got 200)` and
// `- the worker holds the capped 250 xp (expected 250, got 305)`. Restored; green.
// red-proof (#749 review fixes, same day and command, three mutations at once): the cooldown stamped
// before the cap failed `- the capped chop left the cooldown open (expected 40, got 0)`; the single
// level clamped at 20 failed `- single level 25 with 10 xp is 30010, not clamped at 20 (expected
// 30010, got 19010)`; the career curve ignoring the config failed `- a configured 100-then-triple
// career level 3 is 407 (expected 407, got 1507)`. Restored; green.
// tier: LOGIC
//! Per-job XP (#749, D12): XP reaches levels through the job's thresholds, an action's cooldown and
//! a job's daily cap hold back farming, an unknown job or action earns nothing, and the pre-#749
//! curves convert both ways.
class LF_Test_JobLevels : LF_Test
{
	static const string UID = "test-joblevels-worker";
	static const string OTHER = "test-joblevels-other";

	override string GetName()
	{
		return "jobs/job-levels";
	}

	//------------------------------------------------------------------------------------------------
	//! A test config: WOOD levels at 100 / 300 / 600 XP with a 250-a-day cap, MINE at 50 with none;
	//! "chop" gives 40 WOOD XP every 30 s, "dig" 10 MINE XP a unit with no cooldown.
	static LF_JobsConfig MakeConfig()
	{
		LF_JobsConfig config = new LF_JobsConfig();
		array<int> wood = {100, 300, 600};
		array<int> mine = {50};
		config.m_aTracks = {};
		config.m_aTracks.Insert(LF_JobTrackDef.Make("WOOD", wood, 250));
		config.m_aTracks.Insert(LF_JobTrackDef.Make("MINE", mine));
		config.m_aXpAwards = {};
		config.m_aXpAwards.Insert(LF_JobXpAwardDef.Make("chop", "WOOD", 40, 30));
		config.m_aXpAwards.Insert(LF_JobXpAwardDef.Make("dig", "MINE", 10));
		config.m_aXpAwards.Insert(LF_JobXpAwardDef.Make("ghost", "NO_SUCH_JOB", 10));
		config.m_aLicenceGates = {};
		config.m_iJobXpFirstLevel = 500;
		config.m_fJobXpLevelGrowth = 2;
		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_JobsConfig.UseForTest(MakeConfig());
		LF_JobLevels.Forget(UID);
		LF_JobLevels.Forget(OTHER);

		RunLevels(ctx);
		RunAwards(ctx);
		RunLegacy(ctx);

		LF_JobLevels.Forget(UID);
		LF_JobLevels.Forget(OTHER);
		LF_JobsConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunLevels(LF_TestContext ctx)
	{
		LF_JobTrackDef wood = LF_JobLevels.Track("WOOD");
		ctx.NotNull(wood, "the test config has the WOOD track");
		if (!wood)
			return;

		ctx.Equal(1, wood.LevelFor(0), "no xp is level 1");
		ctx.Equal(1, wood.LevelFor(99), "one short of the first threshold is level 1");
		ctx.Equal(2, wood.LevelFor(100), "the first threshold is level 2");
		ctx.Equal(3, wood.LevelFor(300), "the second threshold is level 3");
		ctx.Equal(4, wood.LevelFor(100000), "past the table is the top level");
		ctx.Equal(300, wood.XpForLevel(3), "level 3 needs 300 total xp");
		ctx.Equal(0, wood.XpForLevel(1), "level 1 needs nothing");
		ctx.Equal(-1, wood.XpForLevel(5), "a level past the table has no threshold");

		array<int> shuffled = {600, 100, 300};
		LF_JobTrackDef unsorted = LF_JobTrackDef.Make("X", shuffled);
		ctx.Equal(3, unsorted.LevelFor(350), "a table out of order still counts the thresholds reached");

		ctx.Equal(1, LF_JobLevels.LevelOf(UID, "WOOD"), "a new worker is level 1");
		ctx.Equal(90, LF_JobLevels.AddXp(UID, "WOOD", 90, "test"), "90 wood xp is added");
		ctx.Equal(1, LF_JobLevels.LevelOf(UID, "WOOD"), "90 xp stays level 1");
		ctx.Equal(10, LF_JobLevels.AddXp(UID, "WOOD", 10, "test"), "10 more is added");
		ctx.Equal(2, LF_JobLevels.LevelOf(UID, "WOOD"), "100 xp is level 2");
		ctx.Equal(1, LF_JobLevels.LevelOf(UID, "MINE"), "wood xp does not level another job");
		ctx.Equal(1, LF_JobLevels.LevelOf(OTHER, "WOOD"), "another character keeps its own level");

		// Daily cap 250: 100 already earned today, 200 more is cut to 150, then nothing.
		ctx.Equal(150, LF_JobLevels.AddXp(UID, "WOOD", 200, "test"), "the daily cap cuts the grant to what is left");
		ctx.Equal(0, LF_JobLevels.AddXp(UID, "WOOD", 5, "test"), "a capped job earns nothing more today");
		ctx.Equal(250, LF_JobLevels.XpOf(UID, "WOOD"), "the worker holds the capped 250 xp");
		ctx.Equal(80, LF_JobLevels.AddXp(UID, "MINE", 80, "test"), "a job with no cap is not held back");

		// Unknown job, empty id, nothing: no xp.
		ctx.Equal(0, LF_JobLevels.AddXp(UID, "NO_SUCH_JOB", 50, "test"), "a job the config does not list earns nothing");
		ctx.Equal(0, LF_JobLevels.XpOf(UID, "NO_SUCH_JOB"), "no xp is stored for an unknown job");
		ctx.Equal(1, LF_JobLevels.LevelOf(UID, "NO_SUCH_JOB"), "an unknown job reads as level 1");
		ctx.Equal(0, LF_JobLevels.AddXp("", "WOOD", 50, "test"), "an empty player id earns nothing");
		ctx.Equal(0, LF_JobLevels.AddXp(OTHER, "WOOD", 0, "test"), "zero xp is refused");
		ctx.Equal(0, LF_JobLevels.AddXp(OTHER, "WOOD", -5, "test"), "negative xp is refused");
		ctx.Equal(0, LF_JobLevels.XpOf(OTHER, "WOOD"), "a refused grant leaves the other character at 0");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunAwards(LF_TestContext ctx)
	{
		LF_JobLevels.Forget(OTHER);
		ctx.Equal(40, LF_JobLevels.Award(OTHER, "chop"), "an action awards its configured xp to its job");
		ctx.Equal(40, LF_JobLevels.XpOf(OTHER, "WOOD"), "the chop xp lands on WOOD");
		ctx.Equal(0, LF_JobLevels.Award(OTHER, "chop"), "the same action inside its cooldown awards nothing");
		ctx.Equal(40, LF_JobLevels.XpOf(OTHER, "WOOD"), "the cooldown leaves the xp where it was");
		ctx.Equal(30, LF_JobLevels.Award(OTHER, "dig", 3), "an action without a cooldown pays per unit");
		ctx.Equal(10, LF_JobLevels.Award(OTHER, "dig"), "and pays again at once");
		ctx.Equal(0, LF_JobLevels.Award(OTHER, "no_such_action"), "an action the config does not list awards nothing");
		ctx.Equal(0, LF_JobLevels.Award(OTHER, "ghost"), "an action whose job has no track awards nothing");
		ctx.Equal(0, LF_JobLevels.Award(OTHER, "dig", 0), "zero units award nothing");

		// A capped award does not start the cooldown: once the cap allows again, the action pays.
		LF_JobLevels.Forget(OTHER);
		ctx.Equal(250, LF_JobLevels.AddXp(OTHER, "WOOD", 250, "test"), "the other character fills today's WOOD cap");
		ctx.Equal(0, LF_JobLevels.Award(OTHER, "chop"), "a chop over the cap earns nothing");
		LF_JobsConfig config = LF_JobsConfig.Get();
		LF_JobTrackDef wood = config.FindTrack("WOOD");
		wood.m_iDailyXpCap = 0;
		ctx.Equal(40, LF_JobLevels.Award(OTHER, "chop"), "the capped chop left the cooldown open");
		wood.m_iDailyXpCap = 250;
	}

	//------------------------------------------------------------------------------------------------
	//! The pre-#749 curves (save migration): the single level cost level x 100 to the next, the
	//! delivery career 500 and then doubling.
	protected void RunLegacy(LF_TestContext ctx)
	{
		ctx.Equal(0, LF_JobLevels.LegacyLevelToXp(1, 0), "single level 1 with no xp is 0");
		ctx.Equal(340, LF_JobLevels.LegacyLevelToXp(3, 40), "single level 3 with 40 xp is 100 + 200 + 40");
		ctx.Equal(1500, LF_JobLevels.LegacyCareerToXp(3, 0), "career level 3 is 500 + 1000");
		ctx.Equal(1720, LF_JobLevels.LegacyCareerToXp(3, 220), "career level 3 with 220 xp is 1720");

		int level;
		float xp;
		LF_JobLevels.XpToLegacyLevel(340, level, xp);
		ctx.Equal(3, level, "340 xp is single level 3");
		ctx.EqualFloat(40, xp, 0.01, "with 40 xp toward level 4");
		LF_JobLevels.XpToLegacyCareer(1720, level, xp);
		ctx.Equal(3, level, "1720 xp is career level 3");
		ctx.EqualFloat(220, xp, 0.01, "with 220 xp toward level 4");
		LF_JobLevels.XpToLegacyCareer(0, level, xp);
		ctx.Equal(1, level, "no xp is career level 1");
		// The single level had no top (#749 review): level 25 is 50 x 25 x 24 = 30000 and back.
		ctx.Equal(30010, LF_JobLevels.LegacyLevelToXp(25, 10), "single level 25 with 10 xp is 30010, not clamped at 20");
		LF_JobLevels.XpToLegacyLevel(30010, level, xp);
		ctx.Equal(25, level, "30010 xp is single level 25");
		ctx.EqualFloat(10, xp, 0.01, "with 10 xp toward level 26");
		ctx.Equal(LF_JobLevels.LegacyLevelToXp(LF_JobLevels.LEGACY_MAX_SINGLE_LEVEL, 0), LF_JobLevels.LegacyLevelToXp(1000000, 0), "an absurd level is clamped, not overflowed");
		ctx.True(LF_JobLevels.LegacyCareerToXp(1000, 0) > 0, "an absurd career level is clamped, not overflowed");

		// The career curve is the configured one, as the old career read it.
		LF_JobsConfig curve = LF_JobsConfig.Get();
		curve.m_iJobXpFirstLevel = 100;
		curve.m_fJobXpLevelGrowth = 3;
		ctx.Equal(100 + 300 + 7, LF_JobLevels.LegacyCareerToXp(3, 7), "a configured 100-then-triple career level 3 is 407");
		LF_JobLevels.XpToLegacyCareer(407, level, xp);
		ctx.Equal(3, level, "407 xp on the configured curve is career level 3");
		curve.m_iJobXpFirstLevel = 500;
		curve.m_fJobXpLevelGrowth = 2;
	}
}
