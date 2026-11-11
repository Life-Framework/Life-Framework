// red-proof: observed 2026-11-12 with `tools\cli test --no-build --only LF_Test_PoliceRankTable` and LF_Police.conf patrol m_iDutySecondsForNext 1800:
// [LFTEST] FAIL police/rank-table - patrol promotes after an hour on duty (expected 3600, got 1800). Restored to 3600; green.

// tier: LOGIC
class LF_Test_PoliceRankTable : LF_Test
{
	override string GetName()
	{
		return "police/rank-table";
	}

	override void Run(LF_TestContext ctx)
	{
		array<ref LF_PoliceRank> ranks = {};
		LF_PoliceConfig.Ranks(ranks);
		ctx.Equal(2, ranks.Count(), "the shipped ladder has two ranks");
		ctx.Equal(1, LF_PoliceRanks.TopRank(), "the top rank index is 1");

		// PATROL and SENIOR stay ladder ids 0 and 1, so saves and permissions keep their meaning.
		ctx.EqualStr("patrol", ranks.Get(LF_PoliceRanks.PATROL).m_sId, "ladder id 0 is patrol");
		ctx.EqualStr("senior", ranks.Get(LF_PoliceRanks.SENIOR).m_sId, "ladder id 1 is senior");
		ctx.EqualStr("EL-Rank_Patrol", LF_PoliceRanks.TitleKey(LF_PoliceRanks.PATROL), "patrol title comes from the ladder");
		ctx.EqualStr("EL-Rank_Senior", LF_PoliceRanks.TitleKey(LF_PoliceRanks.SENIOR), "senior title comes from the ladder");

		// Fine cap and promotion time per rank.
		ctx.Equal(1000, LF_PoliceRanks.FineCap(LF_PoliceRanks.PATROL), "patrol fine cap is 1000");
		ctx.Equal(LF_PoliceUtils.MAX_FINE, LF_PoliceRanks.FineCap(LF_PoliceRanks.SENIOR), "senior fine cap is the maximum");
		ctx.Equal(3600, LF_PoliceRanks.DutySecondsForNext(LF_PoliceRanks.PATROL), "patrol promotes after an hour on duty");
		ctx.Equal(0, LF_PoliceRanks.DutySecondsForNext(LF_PoliceRanks.SENIOR), "the top rank has no time promotion");

		// Unknown ranks: not usable, and the table clamps instead of failing.
		ctx.True(LF_PoliceRanks.IsRankUsable(0), "rank 0 is usable");
		ctx.True(LF_PoliceRanks.IsRankUsable(1), "rank 1 is usable");
		ctx.False(LF_PoliceRanks.IsRankUsable(2), "rank 2 is not on the ladder");
		ctx.False(LF_PoliceRanks.IsRankUsable(-1), "a negative rank is not on the ladder");
		ctx.Equal(LF_PoliceRanks.FineCap(LF_PoliceRanks.SENIOR), LF_PoliceRanks.FineCap(99), "an unknown high rank clamps to the top rung");
		ctx.Equal(LF_PoliceRanks.FineCap(LF_PoliceRanks.PATROL), LF_PoliceRanks.FineCap(-5), "an unknown low rank clamps to the bottom rung");
		ctx.Equal(0, LF_PoliceRanks.DutySecondsForNext(99), "an unknown rank never promotes by time");

		// Kit per rank comes from the row.
		array<ResourceName> kit = {};
		LF_PoliceRanks.CollectKit(LF_PoliceRanks.PATROL, kit);
		ctx.Equal(1, kit.Count(), "patrol kit is one line");
		kit.Clear();
		LF_PoliceRanks.CollectKit(LF_PoliceRanks.SENIOR, kit);
		ctx.Equal(1, kit.Count(), "senior kit is one line");
		if (kit.Count() == 1)
			ctx.True(kit.Get(0) == LF_PoliceRanks.LONG_GUN, "senior draws the rifle");

		// Duty pay: a negative rank rate defers to the jobs rate; a rank rate overrides it.
		LF_PlayerAccount officer = LF_PlayerAccountManager.GetOrCreate("rank-table-officer");
		officer.SetFaction(LF_Faction.POLICE);
		officer.SetPoliceRank(LF_PoliceRanks.PATROL);
		ctx.True(LF_PoliceRanks.PayPerSecond(LF_PoliceRanks.PATROL) < 0, "the shipped ladder defers pay to the jobs config");
		ctx.Equal(LF_DutyPay.PoliceRatePerSecond(), LF_DutyPay.RateForAccount(officer), "patrol is paid the jobs police rate");
		ctx.Equal(0, LF_DutyPay.RateForAccount(null), "no account is owed nothing");

		// A promotion past the top refuses.
		officer.SetPoliceRank(LF_PoliceRanks.SENIOR);
		ctx.False(LF_PoliceRanks.AccrueDuty("rank-table-officer", 100000), "the top rank is not promoted again");
		ctx.Equal(LF_PoliceRanks.SENIOR, officer.GetPoliceRank(), "the rank stays at the top");
	}
}
