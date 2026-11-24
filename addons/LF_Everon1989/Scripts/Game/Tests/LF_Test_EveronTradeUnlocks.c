// red-proof: #4861 2026-11-24: the SMELTING rows are checked against the smelter steps; RedProof() adds 1000 to each expected number, so every step row fails. Earlier: RedProof() swaps the flagship table for an empty one, so every trade/level pair reports "has no unlock row"; the batched `tools\cli test --no-build --red LF_Test_EveronTradeUnlocks` run records the RED-OK line below.
// red-proof: observed 2026-11-23: with the header line removed from LF_Everon1989.conf, `tools\cli test --no-build --only LF_Test_EveronTradeUnlocks` failed `- the header lists the trade unlocks table` and `- LF_TradeUnlocks.Get returns every row under the pack header (expected 261, got 0)`; with the line it passes.
//! #4185: the flagship names one unlock or perk for every level 2 to 30 of every trade, so the
//! level-up card and F2 can say "Opens: ...", and the pack header lists the table so Get() returns it. Each row carries a localization key whose pack text
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
	//! #4861: every SMELTING row that names a number or a place says what the machine rows really do at that level:
	//! the smelter's steps (bars a run, seconds a run), the cement works at 6 and the copper refinery at 15.
	protected void CheckSmeltingRows(LF_TestContext ctx, LF_TradeUnlocksConfig config)
	{
		LF_ResourceMachineDef smelter = LF_ResourceChainCatalog.FindMachine("smelter-iron");
		ctx.NotNull(smelter, "the iron smelter row loads");
		if (!smelter || !smelter.m_aSteps)
			return;

		ctx.True(smelter.m_aSteps.Count() > 0, "the iron smelter has level steps");
		foreach (LF_MachineStep step : smelter.m_aSteps)
		{
			int number = step.m_iMaxBatches;
			if (number <= 0)
				number = step.m_iProcessSeconds;

			// Red proof: a row that promises a number the machine does not give.
			if (RedProof())
				number = number + 1000;

			string text = WidgetManager.Translate(config.KeyFor("SMELTING", step.m_iLevel));
			ctx.True(text.Contains(number.ToString()), string.Format("SMELTING %1 row names the smelter's %2: %3", step.m_iLevel, number, text));
		}

		LF_ResourceMachineDef cement = LF_ResourceChainCatalog.FindMachine("cement-works");
		if (cement)
		{
			ctx.EqualStr("SMELTING", cement.m_sLevelJob, "the cement works reads SMELTING");
			ctx.True(WidgetManager.Translate(config.KeyFor("SMELTING", cement.m_iMinLevel)).Contains("cement"), "the row at the cement works' level names it");
		}

		LF_ResourceMachineDef refinery = LF_ResourceChainCatalog.FindMachine("refinery-copper");
		if (refinery)
			ctx.True(WidgetManager.Translate(config.KeyFor("SMELTING", refinery.m_iMinLevel)).Contains("refinery"), "the row at the refinery's level names it");
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

		CheckSmeltingRows(ctx, config);

		ctx.Equal(trades.Count() * (LF_TradeUnlocksConfig.LAST_LEVEL - LF_TradeUnlocksConfig.FIRST_LEVEL + 1), config.m_aRows.Count(), "the table holds exactly 29 rows per trade");

		// The header: the layered table the game reads (LF_TradeUnlocks.Get) holds those rows only when
		// the pack header lists the file (#4185 follow-up: the flagship header left it out and the level-up card was empty).
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");
		if (header && header.m_aLF_ConfigOverlays)
			ctx.True(header.m_aLF_ConfigOverlays.Find(CONFIG_PATH) != -1, "the header lists the trade unlocks table");
		else
			ctx.Fail("the header lists no config overlays");

		LF_TradeUnlocksConfig.Reload();
		LF_TradeUnlocksConfig layered = LF_TradeUnlocksConfig.Get();
		ctx.NotNull(layered, "the layered trade unlock table loads");
		if (layered)
			ctx.Equal(config.m_aRows.Count(), layered.m_aRows.Count(), "LF_TradeUnlocks.Get returns every row under the pack header");
		LF_TradeUnlocksConfig.Reload();
	}
}
