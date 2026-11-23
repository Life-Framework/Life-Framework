// red-proof: 2026-11-23 (#4237) `tools\cli test --no-build --only LF_Test_ResidentForeman` on the tree without LF_ResidentForeman: the scripts do not compile (LF_ResidentForeman is undeclared), so the test fails; with the class in, green.
//! #4237 in game: a foreman at a work site tells a player what their job level opens next. The line
//! follows the level: the player at Miner 3 hears the Miner 4 unlock, one at the top hears the "nothing
//! left" line, and the trades never mix. The rows are built here (a server's table is its pack).
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ResidentForeman : LF_Test
{
	static const string UID = "lf-test-foreman-miner";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "residents/foreman";
	}

	//------------------------------------------------------------------------------------------------
	protected array<ref LF_TradeUnlockRow> Rows()
	{
		array<ref LF_TradeUnlockRow> rows = {};
		rows.Insert(LF_TradeUnlockRow.Make("MINER", 2, "#T-Miner_02"));
		rows.Insert(LF_TradeUnlockRow.Make("MINER", 3, "#T-Miner_03"));
		rows.Insert(LF_TradeUnlockRow.Make("MINER", 4, "#T-Miner_04"));
		rows.Insert(LF_TradeUnlockRow.Make("MINER", 7, "#T-Miner_07"));
		rows.Insert(LF_TradeUnlockRow.Make("LUMBERJACK", 2, "#T-Lumber_02"));
		rows.Insert(LF_TradeUnlockRow.Make("LUMBERJACK", 5, "#T-Lumber_05"));
		return rows;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_TradeUnlockRow> rows = Rows();

		// The next unlock follows the level, and skips a level that opens nothing.
		ctx.Equal(2, LF_ResidentForeman.NextLevel(rows, "MINER", 1), "a new miner hears about level 2");
		ctx.Equal(4, LF_ResidentForeman.NextLevel(rows, "MINER", 3), "a level 3 miner hears about level 4");
		ctx.Equal(7, LF_ResidentForeman.NextLevel(rows, "MINER", 4), "a gap in the table is skipped: level 7 is next after 4");
		ctx.Equal(0, LF_ResidentForeman.NextLevel(rows, "MINER", 7), "nothing is left after the last row");
		ctx.Equal(5, LF_ResidentForeman.NextLevel(rows, "LUMBERJACK", 2), "the lumberjack table is its own");
		ctx.Equal(0, LF_ResidentForeman.NextLevel(rows, "FARMER", 1), "a trade with no rows has no next");
		ctx.Equal(0, LF_ResidentForeman.NextLevel(null, "MINER", 1), "no table, no next");

		// The caption key the dialogue speaks (a line is voiced for each).
		ctx.EqualStr("#LF-Foreman_MINER_Next_04", LF_ResidentForeman.KeyFor(rows, "MINER", 3), "the line is named after the level it announces");
		ctx.EqualStr("#LF-Foreman_MINER_Next_02", LF_ResidentForeman.KeyFor(rows, "MINER", 1), "single-digit levels pad to two digits");
		ctx.EqualStr("#LF-Foreman_LUMBERJACK_Next_05", LF_ResidentForeman.KeyFor(rows, "LUMBERJACK", 3), "the key follows the trade");
		ctx.EqualStr("#LF-Foreman_MINER_Next_Top", LF_ResidentForeman.KeyFor(rows, "MINER", 12), "past the last row the foreman has nothing more");
		ctx.True(LF_ResidentForeman.KeyFor(rows, "MINER", 3) != LF_ResidentForeman.KeyFor(rows, "MINER", 1), "the line changes with the level");

		// The hook reads the player's real level from the job levels.
		int xpForThree = 0;
		LF_JobTrackDef track = LF_JobLevels.Track("MINER");
		ctx.NotNull(track, "the jobs config has a MINER track");
		if (!track)
			return;

		xpForThree = track.XpForLevel(3);
		ctx.True(xpForThree > 0, "the MINER curve reaches level 3");
		array<ref LF_JobXpRow> xpRows = {};
		if (!RedProof())
			xpRows.Insert(LF_JobXpRow.Create(UID, "MINER", xpForThree));

		LF_JobLevels.ApplyAll(xpRows);
		ctx.Equal(3, LF_JobLevels.LevelOf(UID, "MINER"), "the fixture miner is level 3");
		ctx.EqualStr("#LF-Foreman_MINER_Next_04", LF_ResidentForeman.KeyForPlayer(rows, UID, "MINER"), "the foreman reads the miner's level and announces level 4");

		LF_JobLevels.ResetStatics();
		ctx.EqualStr("#LF-Foreman_MINER_Next_02", LF_ResidentForeman.KeyForPlayer(rows, UID, "MINER"), "a character with no xp is level 1 and hears level 2");
	}
}
