// red-proof: RedProof() swaps the flagship table for an empty one, so every trade/level pair reports "has no unlock row"; the batched `tools\cli test --no-build --red LF_Test_EveronTradeUnlocks` run records the RED-OK line below.
//! #4185: the flagship names one unlock or perk for every level 2 to 30 of every trade, so the
//! level-up card and F2 can say "Opens: ...". Each row carries a localization key whose pack text
//! resolves, and no trade names the same unlock twice.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronTradeUnlocks : LF_Test
{
	static const ResourceName CONFIG_PATH = "{2AD4170BBC219196}Configs/Jobs/LF_TradeUnlocks_Everon1989.conf";
	static const string KEY_PREFIX = "LF-Unlock_";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/trade-unlocks";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<string> trades = { "FARMER", "MINER", "SMELTING", "LUMBERJACK", "FISHER", "DELIVERY", "SALVAGE", "POLICE", "MEDIC" };

		LF_TradeUnlocksConfig config = LF_TradeUnlocksConfig.Load(CONFIG_PATH);
		ctx.NotNull(config, "the flagship trade unlock table loads");
		if (!config)
			return;

		if (RedProof())
			config.m_aRows = {};

		foreach (string trade : trades)
		{
			array<string> seen = {};
			for (int level = LF_TradeUnlocksConfig.FIRST_LEVEL; level <= LF_TradeUnlocksConfig.LAST_LEVEL; level++)
			{
				string where = string.Format("%1 level %2", trade, level);
				string key = config.KeyFor(trade, level);
				ctx.False(key.IsEmpty(), where + " has an unlock row");
				if (key.IsEmpty())
					continue;

				ctx.True(key.StartsWith("#" + KEY_PREFIX), where + " names an unlock key");
				string text = WidgetManager.Translate(key);
				ctx.True(!text.IsEmpty() && text != key, where + " has pack text for its key");
				ctx.True(seen.Find(text) < 0, where + " does not repeat an unlock of its trade");
				seen.Insert(text);
			}

			ctx.True(config.KeyFor(trade, 1).IsEmpty(), trade + " has no row for level 1");
			ctx.True(config.KeyFor(trade, LF_TradeUnlocksConfig.LAST_LEVEL + 1).IsEmpty(), trade + " has no row past level 30");
		}

		ctx.Equal(trades.Count() * (LF_TradeUnlocksConfig.LAST_LEVEL - LF_TradeUnlocksConfig.FIRST_LEVEL + 1), config.m_aRows.Count(), "the table holds exactly 29 rows per trade");
	}
}
