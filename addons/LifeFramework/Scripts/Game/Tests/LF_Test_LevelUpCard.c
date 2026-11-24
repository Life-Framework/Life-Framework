// red-proof: observed 2026-11-23 `tools/cli test --no-build --red LF_Test_LevelUpCard`: RED-OK level/level-up-card (fails with the hook on, 12 assertions). Green with `--only`.
//! The level-up card (#4404): a level change builds the card text with the trade name, the level, the
//! tier word (F2's band words) and the unlock of the new level from `LF_TradeUnlocks.KeyFor`; a change
//! of 0 levels builds nothing; a level with no unlock row shows no "Opens:" line.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_LevelUpCard : LF_Test
{
	override string GetName()
	{
		return "level/level-up-card";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_TradeUnlocksConfig.Reload();
		LF_TradeUnlocksConfig table = LF_TradeUnlocksConfig.Get();
		table.m_aRows.Insert(LF_TradeUnlockRow.Make("MINER", 7, "#LF-PlayerMenu_Skill_MINER", LF_ETradeUnlockKind.MACHINE, "smelter-iron"));

		// RedProof(): the build is skipped, so no card exists (`--red`).
		LF_LevelUpCardLines card;
		if (!RedProof())
			card = LF_LevelUpCard.Build("MINER", 6, 7);

		ctx.True(card != null, "miner 6 to 7 builds a card");
		if (card)
		{
			ctx.EqualStr("MINER", card.m_sJob, "the card names the trade id");
			ctx.Equal(7, card.m_iLevel, "the card carries the new level");
			ctx.EqualStr(LF_PlayerMenuSheet.SkillName("MINER"), card.m_sTradeName, "the trade is printed by its F2 name");
			ctx.EqualStr("#LF-Level_TradeTier_2", card.m_sTierKey, "level 7 is the Hand band");
			ctx.EqualStr(LF_TradeUnlocks.KeyFor("MINER", 7), card.m_sUnlockKey, "the unlock is the table's row for the new level");
			ctx.EqualStr("#LF-PlayerMenu_Skill_MINER", card.m_sUnlockKey, "the seeded row is the one named");
			ctx.EqualStr(LF_Terms.Text("#LF-LevelUpCard_Opens", LF_Terms.Text("#LF-PlayerMenu_Skill_MINER")), card.OpensLine(), "a level with a real unlock prints an Opens line");
			ctx.True(card.LevelLine().IndexOf("7") >= 0, "the level line carries the level");
		}

		// A level with no row shows no Opens line; police and medic get the same card.
		LF_LevelUpCardLines police = LF_LevelUpCard.Build("POLICE", 2, 3);
		ctx.True(police != null, "police levels get a card too");
		LF_LevelUpCardLines bare = LF_LevelUpCard.Build("NO_SUCH_TRADE", 2, 3);
		ctx.True(bare != null, "a trade with no rows still gets a card");
		if (bare)
		{
			ctx.EqualStr("", bare.m_sUnlockKey, "no unlock row, no unlock key");
			ctx.EqualStr("", bare.OpensLine(), "no unlock row, no Opens line");
		}

		LF_LevelUpCardLines medic = LF_LevelUpCard.Build("MEDIC", 11, 12);
		ctx.True(medic != null, "medic levels get a card too");
		if (medic)
			ctx.EqualStr("#LF-Level_TradeTier_3", medic.m_sTierKey, "level 12 is the Journeyman band");

		// No level gained builds nothing.
		ctx.True(LF_LevelUpCard.Build("MINER", 7, 7) == null, "a change of 0 levels builds nothing");
		ctx.True(LF_LevelUpCard.Build("MINER", 8, 7) == null, "a level that fell builds nothing");

		// A two-level jump shows the level reached and its unlock.
		table.m_aRows.Insert(LF_TradeUnlockRow.Make("FARMER", 3, "#LF-PlayerMenu_Skill_FARMER"));
		LF_LevelUpCardLines jump = LF_LevelUpCard.Build("FARMER", 1, 3);
		ctx.True(jump != null, "a two-level jump builds one card");
		if (jump)
		{
			ctx.EqualStr("#LF-PlayerMenu_Skill_FARMER", jump.m_sUnlockKey, "the card shows the unlock of the level reached");
			// The row is a milestone (#4869): it opens nothing, so its text shows as it is, with no "Opens:".
			ctx.EqualStr(LF_Terms.Text("#LF-PlayerMenu_Skill_FARMER"), jump.OpensLine(), "a milestone row prints its text with no Opens prefix");
		}

		LF_TradeUnlocksConfig.Reload();
	}
}
