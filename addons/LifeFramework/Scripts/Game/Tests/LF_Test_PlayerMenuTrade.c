// red-proof: observed 2026-11-23 `tools/cli test --no-build --red LF_Test_PlayerMenuTrade`: RED PROOF OK (tier key, perk and wire assertions fail with the hook on). Green with `--only`.
//! The F2 trade detail card (#4414): each skill row carries the tier word, the next level's unlock key,
//! the picked perks and the weekly rank slot over the wire and reads them back the same; a level-12
//! miner's card names level 13's unlock; the rank slot says "not ranked yet" until the shift boards fill it.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_PlayerMenuTrade : LF_Test
{
	override string GetName()
	{
		return "playermenu/trade-card";
	}

	override void Run(LF_TestContext ctx)
	{
		string uid = "test-uid-menu-trade";
		LF_TradePerks.Pick(uid, "MINER", 10, "STEADY_HANDS");

		LF_JobTrackDef track = LF_JobTrackDef.Make("MINER", {100, 300, 600, 1000, 1500, 2100, 2800, 3600, 4300, 5000, 5800, 6700});
		LF_PlayerMenuSkill skill = LF_PlayerMenuServer.SkillOf(track, 6700);
		ctx.Equal(13, skill.m_iLevel, "6,700 XP on this table is level 13");

		// A level-12 miner: 5,800 XP.
		skill = LF_PlayerMenuServer.SkillOf(track, 5800);
		ctx.Equal(12, skill.m_iLevel, "5,800 XP is level 12");

		// RedProof(): the server fill is skipped, so the card carries nothing (`--red`).
		if (!RedProof())
			LF_PlayerMenuTrade.Fill(skill, uid);

		ctx.EqualStr(LF_PlayerMenuTrade.TierKeyFor(12), skill.m_sTierKey, "the skill carries its tier word key");
		ctx.EqualStr("#LF-Level_TradeTier_3", skill.m_sTierKey, "level 12 is the third tier word (Journeyman)");
		ctx.EqualStr(LF_TradeUnlocks.NextKey("MINER", 12), skill.m_sNextUnlockKey, "the next unlock key is the unlock table's row for level 13");
		ctx.Equal(1, skill.m_aPerks.Count(), "the perk picked at level 10 is on the card");
		if (skill.m_aPerks.Count() == 1)
			ctx.EqualStr("STEADY_HANDS", skill.m_aPerks[0], "the perk id");

		ctx.Equal(0, skill.m_iRank, "no shift board fills the rank slot yet");
		ctx.EqualStr("#LF-PlayerMenu_SkillUnranked", LF_PlayerMenuTrade.RankKey(skill), "an empty rank slot reads not ranked yet");

		// The tier bands: 1-5, 6-11, 12-19, 20-27, 28-30.
		ctx.EqualStr("#LF-Level_TradeTier_1", LF_PlayerMenuTrade.TierKeyFor(1), "level 1");
		ctx.EqualStr("#LF-Level_TradeTier_1", LF_PlayerMenuTrade.TierKeyFor(5), "level 5 ends the first band");
		ctx.EqualStr("#LF-Level_TradeTier_2", LF_PlayerMenuTrade.TierKeyFor(6), "level 6 starts the second");
		ctx.EqualStr("#LF-Level_TradeTier_3", LF_PlayerMenuTrade.TierKeyFor(19), "level 19 ends the third");
		ctx.EqualStr("#LF-Level_TradeTier_4", LF_PlayerMenuTrade.TierKeyFor(20), "level 20 starts the fourth");
		ctx.EqualStr("#LF-Level_TradeTier_5", LF_PlayerMenuTrade.TierKeyFor(28), "level 28 starts the fifth");
		ctx.EqualStr("#LF-Level_TradeTier_5", LF_PlayerMenuTrade.TierKeyFor(30), "level 30");

		ctx.EqualStr("1st", LF_PlayerMenuTrade.Ordinal(1), "1st");
		ctx.EqualStr("2nd", LF_PlayerMenuTrade.Ordinal(2), "2nd");
		ctx.EqualStr("3rd", LF_PlayerMenuTrade.Ordinal(3), "3rd");
		ctx.EqualStr("6th", LF_PlayerMenuTrade.Ordinal(6), "6th");
		ctx.EqualStr("11th", LF_PlayerMenuTrade.Ordinal(11), "11th, not 11st");
		ctx.EqualStr("12th", LF_PlayerMenuTrade.Ordinal(12), "12th");
		ctx.EqualStr("13th", LF_PlayerMenuTrade.Ordinal(13), "13th");
		ctx.EqualStr("21st", LF_PlayerMenuTrade.Ordinal(21), "21st");

		// A ranked skill, a stand-in unlock key (an existing row), and two picked perks.
		skill.m_iRank = 6;
		skill.m_sNextUnlockKey = "#LF-PlayerMenu_Skill_MINER";
		skill.m_aPerks.Insert("DEEP_VEIN");
		ctx.EqualStr("#LF-PlayerMenu_SkillRank", LF_PlayerMenuTrade.RankKey(skill), "a ranked skill reads its rank");

		string unlock = LF_PlayerMenuTrade.UnlockLine(skill);
		ctx.True(unlock.Contains("13"), "the unlock line names level 13");
		ctx.True(unlock.Contains(LF_Terms.Text("#LF-PlayerMenu_Skill_MINER")), "the unlock line carries the unlock text");
		skill.m_sNextUnlockKey = string.Empty;
		ctx.EqualStr(string.Empty, LF_PlayerMenuTrade.UnlockLine(skill), "no unlock row, no line");

		skill.m_sNextUnlockKey = "#LF-PlayerMenu_Skill_MINER";
		LF_PlayerMenuData data = new LF_PlayerMenuData();
		data.m_sName = "Ann Novak";
		data.m_aSkills.Insert(skill);
		LF_PlayerMenuData back = LF_PlayerMenuData.Decode(data.Encode());
		ctx.NotNull(back, "the wire decodes");
		if (!back || back.m_aSkills.Count() != 1)
		{
			ctx.Fail("one skill came back");
			LF_TradePerks.Forget(uid);
			return;
		}

		LF_PlayerMenuSkill read = back.m_aSkills[0];
		ctx.EqualStr(skill.m_sNextUnlockKey, read.m_sNextUnlockKey, "the next unlock key reads back");
		ctx.EqualStr(skill.m_sTierKey, read.m_sTierKey, "the tier word key reads back");
		ctx.Equal(6, read.m_iRank, "the rank reads back");
		ctx.Equal(2, read.m_aPerks.Count(), "both perks read back");
		ctx.Equal(12, read.m_iLevel, "the old fields still read back");

		// An older wire (no trade fields) still decodes.
		LF_PlayerMenuData old = LF_PlayerMenuData.Decode("V|1;P|Ann|0|0|0|0|0|0|0|0|0;S|MINER|5800|12|5000|6700");
		ctx.NotNull(old, "a wire without the trade fields decodes");
		if (old && old.m_aSkills.Count() == 1)
			ctx.True(old.m_aSkills[0].m_sNextUnlockKey.IsEmpty(), "and its trade fields are empty");

		LF_TradePerks.Forget(uid);
	}
}
