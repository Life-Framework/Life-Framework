// red-proof: #4341 2026-11-23, run on the unfixed SkillKeys: `--only LF_Test_PlayerMenuXp` FAILED (SMELTING and SALVAGE have F2 skill keys), green after. Earlier: observed 2026-11-22 `tools/cli test --no-build --red LF_Test_PlayerMenuXp,LF_Test_PlayerMenuData`: RED PROOF OK (both fail with the hook on). Green with `--only`.
//! The personal file's XP bar, no world: the level and its span from a job's thresholds (any order),
//! the share of the bar from the floor to the next level, full at the top of the ladder, empty at the
//! floor, and the skill row the server builds from a track.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_PlayerMenuXp : LF_Test
{
	override string GetName()
	{
		return "playermenu/xp-bar";
	}

	override void Run(LF_TestContext ctx)
	{
		array<int> thresholds = {300, 100, 600};
		int level;
		int floor;
		int next;

		LF_PlayerMenuXp.Span(thresholds, 0, level, floor, next);
		ctx.Equal(1, level, "no XP is level 1");
		ctx.Equal(0, floor, "level 1 starts at 0 XP");
		ctx.Equal(100, next, "level 2 is the lowest threshold, whatever the order");

		// RedProof(): the span is read at 0 XP, so every assertion about 140 XP fails (`--red`).
		int xp = 140;
		if (RedProof())
			xp = 0;

		LF_PlayerMenuXp.Span(thresholds, xp, level, floor, next);
		ctx.Equal(2, level, "140 XP passed one threshold");
		ctx.Equal(100, floor, "level 2 started at 100");
		ctx.Equal(300, next, "level 3 is at 300");
		ctx.EqualFloat(0.2, LF_PlayerMenuXp.Fraction(xp, floor, next), 0.001, "40 of the 200 XP to the next level fill a fifth of the bar");

		LF_JobTrackDef track = LF_JobTrackDef.Make("MINER", thresholds);
		LF_PlayerMenuSkill skill = LF_PlayerMenuServer.SkillOf(track, xp);
		ctx.Equal(track.LevelFor(xp), skill.m_iLevel, "the row's level is the track's own level");
		ctx.Equal(300, skill.m_iNext, "the row carries the next level's XP");
		ctx.EqualStr("MINER", skill.m_sJob, "the row names its job");

		LF_PlayerMenuXp.Span(thresholds, 600, level, floor, next);
		ctx.Equal(4, level, "600 XP is the top level");
		ctx.Equal(-1, next, "there is no next level at the top");
		ctx.EqualFloat(1, LF_PlayerMenuXp.Fraction(900, floor, next), 0.001, "the bar is full at the top");

		ctx.EqualFloat(0, LF_PlayerMenuXp.Fraction(100, 100, 300), 0.001, "the bar is empty on reaching a level");
		ctx.EqualFloat(1, LF_PlayerMenuXp.Fraction(50, 100, 100), 0.001, "a level with no span reads full, never divides by zero");

		// #4341: SMELTING and SALVAGE read their own F2 name and description keys (RedProof(): a job with no rows).
		array<string> trades = {"SMELTING", "SALVAGE"};
		foreach (string trade : trades)
		{
			string lookup = trade;
			if (RedProof())
				lookup = "NO_SUCH_TRADE";

			string nameKey;
			string descKey;
			ctx.True(LF_PlayerMenuSheet.SkillKeys(lookup, nameKey, descKey), trade + " has F2 skill keys");
			ctx.True(nameKey.Contains("Skill_" + trade), trade + " name key");
			ctx.True(descKey.Contains("SkillDesc_" + trade), trade + " description key");
		}

		LF_PlayerMenuXp.Span(null, 500, level, floor, next);
		ctx.Equal(1, level, "a job with no thresholds stays level 1");
		ctx.Equal(-1, next, "and has no next level");
	}
}
