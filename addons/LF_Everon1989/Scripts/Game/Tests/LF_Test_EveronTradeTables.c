// red-proof: 2026-11-22 `tools\cli test --no-build --red LF_Test_EveronTradeTables`: RED PROOF OK (RedProof() looks up a trade no table names and the tier words under a key no row holds: the tier checks read the raw key back). Green with the 30-level rows via `--only`.
//! everon/trade-tables (#4181, W1 of the 0.3.0 work scope): every legal trade levels to 30 on one curve. Under the
//! Everon 1989 header (the pack leg of `tools\cli test`) FARMER, MINER, LUMBERJACK, FISHER and MEDIC each have 29
//! thresholds (levels 2 to 30) that rise strictly, level 5 needs about 1.5 h and level 10 about 7 h at 700 XP an
//! hour, and each of the five tiers has a tier word the UI shows in place of a number (decision N1).
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronTradeTables : LF_Test
{
	protected static const int TOP_LEVEL = 30;
	protected static const int TIER_COUNT = 5;
	protected static const float XP_PER_HOUR = 700;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/trade-tables";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The leg boots under the pack header; layer the configs again so the pack overlays apply.
		LF_JobsConfig.Reload();

		LF_JobsConfig config = LF_JobsConfig.Get();
		ctx.NotNull(config, "the jobs config loads under the pack header");
		if (config)
		{
			array<string> trades = {"FARMER", "MINER", "LUMBERJACK", "FISHER", "MEDIC"};
			foreach (string trade : trades)
			{
				CheckTrade(ctx, config, trade);
			}
		}

		CheckTierWords(ctx);

		LF_JobsConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckTrade(LF_TestContext ctx, notnull LF_JobsConfig config, string trade)
	{
		string job = trade;
		if (RedProof())
			job = "NO_SUCH_TRADE";

		LF_JobTrackDef track = config.FindTrack(job);
		ctx.NotNull(track, string.Format("%1 has a level table", trade));
		if (!track || !track.m_aLevelThresholds)
			return;

		ctx.Equal(TOP_LEVEL - 1, track.m_aLevelThresholds.Count(), string.Format("%1 has 29 thresholds (levels 2 to 30)", trade));

		bool rising = track.m_aLevelThresholds.Count() > 0 && track.m_aLevelThresholds[0] > 0;
		for (int i = 1; i < track.m_aLevelThresholds.Count(); i++)
		{
			if (track.m_aLevelThresholds[i] <= track.m_aLevelThresholds[i - 1])
				rising = false;
		}

		ctx.True(rising, string.Format("%1 XP rises with every level", trade));

		int xp5 = track.XpForLevel(5);
		int xp10 = track.XpForLevel(10);
		float hours5 = xp5 / XP_PER_HOUR;
		float hours10 = xp10 / XP_PER_HOUR;
		ctx.True(xp5 > 0 && hours5 >= 1.2 && hours5 <= 1.8, string.Format("%1 level 5 needs about 1.5 h at 700 XP an hour (%2 XP)", trade, xp5));
		ctx.True(xp10 > 0 && hours10 >= 6.5 && hours10 <= 7.5, string.Format("%1 level 10 needs about 7 h at 700 XP an hour (%2 XP)", trade, xp10));

		int xpTop = track.XpForLevel(TOP_LEVEL);
		ctx.True(xpTop > 0, string.Format("%1 has an XP total for level 30", trade));
		ctx.Equal(TOP_LEVEL, track.LevelFor(xpTop), string.Format("%1 reaches level 30 at its last threshold", trade));
		ctx.Equal(TOP_LEVEL - 1, track.LevelFor(xpTop - 1), string.Format("%1 is level 29 one XP short of the last threshold", trade));
		ctx.Equal(-1, track.XpForLevel(TOP_LEVEL + 1), string.Format("%1 stops at level 30", trade));
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckTierWords(LF_TestContext ctx)
	{
		array<string> expected = {"Apprentice", "Hand", "Journeyman", "Foreman", "Master"};
		array<string> keys = {"#LF-Everon_TradeTier_1", "#LF-Everon_TradeTier_2", "#LF-Everon_TradeTier_3", "#LF-Everon_TradeTier_4", "#LF-Everon_TradeTier_5"};
		for (int tier = 1; tier <= TIER_COUNT; tier++)
		{
			string key = keys[tier - 1];
			if (RedProof())
				key = key + "_missing";

			ctx.EqualStr(expected[tier - 1], WidgetManager.Translate(key), string.Format("tier %1 has the word %2 (%3)", tier, expected[tier - 1], key));
		}
	}
}
