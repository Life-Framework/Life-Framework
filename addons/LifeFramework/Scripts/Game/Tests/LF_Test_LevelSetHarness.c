// red-proof: observed 2026-11-24 `tools\cli test --no-build --red LF_Test_LevelSetHarness`: RED-OK level/harness-level-set (the set skipped, the capped character stays at level 2). Green with `--only`.
//! The harness level set (#4773): `levelset` puts a character at an exact trade level whatever the daily
//! XP cap says, without counting toward the cap, fires the level event, and refuses outside a harness
//! build. A MINER stopped at 2,000 XP (level 6) by the cap could not reach the level-10 perk pick.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_LevelSetHarness : LF_Test
{
	static const string UID = "test-levelset-capped";
	static const string FRESH = "test-levelset-fresh";
	static const string JOB = "WOOD";

	override string GetName()
	{
		return "level/harness-level-set";
	}

	override void Run(LF_TestContext ctx)
	{
		// WOOD levels at 100 / 300 / 600 XP (levels 2-4) with a 250-a-day cap.
		LF_JobsConfig.UseForTest(LF_Test_JobLevels.MakeConfig());
		LF_JobLevels.Forget(UID);
		LF_JobLevels.Forget(FRESH);

		// The daily cap holds a normal grant: 1000 XP offered, 250 kept, level 2.
		ctx.Equal(250, LF_JobLevels.AddXp(UID, JOB, 1000, "test"), "a normal grant stops at the daily cap");
		ctx.Equal(0, LF_JobLevels.AddXp(UID, JOB, 100, "test"), "the cap is spent for the day");
		ctx.Equal(2, LF_JobLevels.LevelOf(UID, JOB), "250 xp is level 2");

		// RedProof(): the set is skipped, so the capped character stays at level 2 (`--red`).
		int held = -1;
		if (!RedProof())
			held = LF_JobLevels.SetLevelUnchecked(UID, JOB, 4);

		ctx.Equal(600, held, "level 4 holds the 600 xp that reaches it");
		ctx.Equal(4, LF_JobLevels.LevelOf(UID, JOB), "the capped character is at level 4, past what the cap allows");
		ctx.Equal(600, LF_JobLevels.XpOf(UID, JOB), "the xp is the exact threshold");

		// The set did not book against the cap: a character who never earned today still gets the whole cap after a set.
		if (!RedProof())
			LF_JobLevels.SetLevelUnchecked(FRESH, JOB, 4);

		ctx.Equal(250, LF_JobLevels.AddXp(FRESH, JOB, 1000, "test"), "a set is not counted toward the daily cap");
		ctx.Equal(850, LF_JobLevels.XpOf(FRESH, JOB), "the set xp plus the whole day's cap");

		// A level can be set down, and a set by xp lands where it says.
		ctx.Equal(100, LF_JobLevels.SetLevelUnchecked(UID, JOB, 2), "a level can be set down");
		ctx.Equal(2, LF_JobLevels.LevelOf(UID, JOB), "level 2 after setting down");
		ctx.Equal(299, LF_JobLevels.SetXpUnchecked(UID, JOB, 299), "xp is set exactly");
		ctx.Equal(2, LF_JobLevels.LevelOf(UID, JOB), "299 xp is still level 2");

		// A level the table does not reach, an unknown job and a negative xp change nothing.
		ctx.Equal(-1, LF_JobLevels.SetLevelUnchecked(UID, JOB, 9), "a level past the table is refused");
		ctx.Equal(-1, LF_JobLevels.SetLevelUnchecked(UID, "NO_SUCH_JOB", 2), "an unknown job is refused");
		ctx.Equal(-1, LF_JobLevels.SetXpUnchecked(UID, JOB, -5), "negative xp is refused");
		ctx.Equal(299, LF_JobLevels.XpOf(UID, JOB), "a refused set leaves the xp alone");

		// Outside a harness build the gated entries refuse (the suite is built without LF_HARNESS).
		if (!LF_DevGate.HarnessBuild())
		{
			ctx.Equal(-1, LF_JobLevels.HarnessSetLevel(UID, JOB, 4), "a build without LF_HARNESS refuses the level set");
			ctx.Equal(-1, LF_JobLevels.HarnessSetXp(UID, JOB, 600), "a build without LF_HARNESS refuses the xp set");
			ctx.Equal(299, LF_JobLevels.XpOf(UID, JOB), "the refusals changed nothing");
		}

		LF_JobLevels.Forget(UID);
		LF_JobLevels.Forget(FRESH);
		LF_JobsConfig.Reload();
	}
}
