// red-proof: observed 2026-11-19 with `tools\cli test --no-build --red LF_Test_HiveRules` (RED PROOF OK): with the hook on the
// first advance is skipped, so `one comb in the hive (expected 1, got 0)` and the later comb and cap assertions fail.

//! The beehive rules (#3446): one comb per configured game hours up to the cap, nothing in rain or at night,
//! taking empties the hive, and an owner-only hive refuses a stranger.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_HiveRules : LF_Test
{
	override string GetName()
	{
		return "farm/hive-rules";
	}

	override void Run(LF_TestContext ctx)
	{
		// The shipped config loads and is sane.
		LF_HiveConfig cfg = LF_HiveConfig.Get();
		ctx.True(cfg != null, "the hive config loads");
		if (!cfg)
			return;

		ctx.True(cfg.m_fHoursPerComb > 0, "a comb takes some game hours");
		ctx.True(cfg.m_iMaxCombs >= 1, "a hive holds at least one comb");
		float perComb = 6;
		int cap = 3;

		// Combs over time: one per perComb hours, remainder carried.
		LF_HiveState hive = new LF_HiveState();
		int gained = 0;
		if (!RedProof())
			gained = LF_HiveRules.Advance(hive, perComb, cap, 6, false, false);
		ctx.Equal(1, gained, "6 h of 6 h gives one comb");
		ctx.Equal(1, hive.m_iCombs, "one comb in the hive");
		gained = LF_HiveRules.Advance(hive, perComb, cap, 5, false, false);
		ctx.Equal(0, gained, "5 more hours give no comb yet");
		gained = LF_HiveRules.Advance(hive, perComb, cap, 2, false, false);
		ctx.Equal(1, gained, "the next hours give the second comb");
		ctx.Equal(2, hive.m_iCombs, "two combs in the hive");
		ctx.EqualFloat(1, hive.m_fProgressHours, 0.001, "the spare hour is kept toward the next comb");

		// The cap: a long absence fills to the cap and no further.
		gained = LF_HiveRules.Advance(hive, perComb, cap, 100, false, false);
		ctx.Equal(1, gained, "the hive fills to the cap only");
		ctx.Equal(cap, hive.m_iCombs, "a full hive holds the cap");
		ctx.EqualFloat(0, hive.m_fProgressHours, 0.001, "a full hive keeps no progress");
		ctx.Equal(0, LF_HiveRules.Advance(hive, perComb, cap, 50, false, false), "a full hive gains nothing");
		ctx.Equal(cap, hive.m_iCombs, "still at the cap");

		// Rain and night: no growth, no progress.
		LF_HiveState wet = new LF_HiveState();
		ctx.Equal(0, LF_HiveRules.Advance(wet, perComb, cap, 24, true, false), "rain gives nothing");
		ctx.Equal(0, LF_HiveRules.Advance(wet, perComb, cap, 24, false, true), "night gives nothing");
		ctx.Equal(0, wet.m_iCombs, "no comb from rain or night");
		ctx.EqualFloat(0, wet.m_fProgressHours, 0.001, "rain and night add no progress");
		ctx.Equal(0, LF_HiveRules.Advance(null, perComb, cap, 24, false, false), "no hive, no comb");

		// The night window, wrapping midnight (20 to 6) and not (1 to 5).
		ctx.True(LF_HiveRules.IsNight(23, 20, 6), "23:00 is night");
		ctx.True(LF_HiveRules.IsNight(3, 20, 6), "03:00 is night");
		ctx.True(!LF_HiveRules.IsNight(12, 20, 6), "noon is day");
		ctx.True(!LF_HiveRules.IsNight(6, 20, 6), "the bees start at 06:00");
		ctx.True(LF_HiveRules.IsNight(20, 20, 6), "the bees stop at 20:00");
		ctx.True(LF_HiveRules.IsNight(2, 1, 5), "a window inside one day");
		ctx.True(!LF_HiveRules.IsNight(7, 1, 5), "outside a window inside one day");
		ctx.True(!LF_HiveRules.IsNight(3, 4, 4), "an empty window is never night");

		// Taking empties the hive; an empty hive refuses; progress is kept.
		LF_HiveState full = new LF_HiveState();
		full.m_iCombs = 3;
		full.m_fProgressHours = 2;
		int taken;
		ctx.True(LF_HiveRules.Take(full, false, "", "anyone", taken) == LF_EHiveTake.OK, "an open hive gives its combs");
		ctx.Equal(3, taken, "all three combs come out");
		ctx.Equal(0, full.m_iCombs, "taking empties the hive");
		ctx.EqualFloat(2, full.m_fProgressHours, 0.001, "work toward the next comb survives a taking");
		ctx.True(LF_HiveRules.Take(full, false, "", "anyone", taken) == LF_EHiveTake.EMPTY, "an empty hive refuses");
		ctx.Equal(0, taken, "nothing taken from an empty hive");
		ctx.Equal(1, LF_HiveRules.Advance(full, perComb, cap, 4, false, false), "the kept progress finishes the next comb");

		// Owner-only: a stranger is refused and the hive keeps its combs.
		LF_HiveState mine = new LF_HiveState();
		mine.m_iCombs = 2;
		ctx.True(LF_HiveRules.Take(mine, true, "owner-1", "stranger-2", taken) == LF_EHiveTake.NOT_OWNER, "a stranger is refused at an owner-only hive");
		ctx.Equal(0, taken, "a refused stranger takes nothing");
		ctx.Equal(2, mine.m_iCombs, "the refused hive keeps its combs");
		ctx.True(LF_HiveRules.Take(mine, true, "", "stranger-2", taken) == LF_EHiveTake.NOT_OWNER, "an owner-only hive with no owner key refuses everyone");
		ctx.True(LF_HiveRules.Take(mine, true, "owner-1", "owner-1", taken) == LF_EHiveTake.OK, "the owner takes the combs");
		ctx.Equal(2, taken, "the owner gets both combs");
	}
}
