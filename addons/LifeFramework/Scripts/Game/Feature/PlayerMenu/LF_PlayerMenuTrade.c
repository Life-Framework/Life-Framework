// LF_PlayerMenuTrade.c: the text of the F2 trade detail card (#4414). A skill's row carries the tier
// word, what the next level opens, the perks picked and this week's rank; the sheet only prints what
// these helpers return. Pure where it can be, so a LOGIC test drives it (LF_Test_PlayerMenuTrade).
// Levels show for legal trades (decision N1); crime and standing never show a number.

class LF_PlayerMenuTrade
{
	//! The levels a perk is picked at (WK-W8b); the card lists what the store holds for each.
	static const int PERK_LEVEL_FIRST = 10;
	static const int PERK_LEVEL_SECOND = 20;

	//------------------------------------------------------------------------------------------------
	//! \return 1 to 5: bands 1-5, 6-11, 12-19, 20-27 and 28-30 (Apprentice, Hand, Journeyman, Foreman, Master).
	static int TierBand(int level)
	{
		if (level <= 5)
			return 1;

		if (level <= 11)
			return 2;

		if (level <= 19)
			return 3;

		if (level <= 27)
			return 4;

		return 5;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The `#` key of the tier word for a trade level.
	static string TierKeyFor(int level)
	{
		int band = TierBand(level);
		if (band == 1)
			return "#LF-Level_TradeTier_1";

		if (band == 2)
			return "#LF-Level_TradeTier_2";

		if (band == 3)
			return "#LF-Level_TradeTier_3";

		if (band == 4)
			return "#LF-Level_TradeTier_4";

		return "#LF-Level_TradeTier_5";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: fills the trade card fields of a skill row for one player. Read only.
	static void Fill(notnull LF_PlayerMenuSkill skill, string uid)
	{
		skill.m_sTierKey = TierKeyFor(skill.m_iLevel);
		skill.m_sNextUnlockKey = LF_TradeUnlocks.NextKey(skill.m_sJob, skill.m_iLevel);
		skill.m_iRank = RankOf(uid, skill.m_sJob);
		skill.m_aPerks.Clear();
		AddPerk(skill, uid, PERK_LEVEL_FIRST);
		AddPerk(skill, uid, PERK_LEVEL_SECOND);
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddPerk(notnull LF_PlayerMenuSkill skill, string uid, int atLevel)
	{
		if (uid.IsEmpty())
			return;

		string perk = LF_TradePerks.PerkAt(uid, skill.m_sJob, atLevel);
		if (!perk.IsEmpty())
			skill.m_aPerks.Insert(perk);
	}

	//------------------------------------------------------------------------------------------------
	//! This week's rank of a player in a trade: 1 and up, 0 for not ranked. The slot is empty until the
	//! shift boards (W22) keep a weekly top list; that card changes this one function.
	static int RankOf(string uid, string job)
	{
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! \return "1st", "2nd", "3rd", "4th", "11th", "21st"; empty for a rank below 1.
	static string Ordinal(int rank)
	{
		if (rank < 1)
			return string.Empty;

		int lastTwo = rank % 100;
		int last = rank % 10;
		string suffix = "th";
		if (lastTwo < 11 || lastTwo > 13)
		{
			if (last == 1)
				suffix = "st";
			else if (last == 2)
				suffix = "nd";
			else if (last == 3)
				suffix = "rd";
		}

		return rank.ToString() + suffix;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The key of the rank line: the ranked row, or the "not ranked yet" row.
	static string RankKey(notnull LF_PlayerMenuSkill skill)
	{
		if (skill.m_iRank > 0)
			return "#LF-PlayerMenu_SkillRank";

		return "#LF-PlayerMenu_SkillUnranked";
	}

	//------------------------------------------------------------------------------------------------
	//! \param tradeName The trade's printed name.
	//! \return "Ranked 6th in Mining this week", or "Not ranked yet this week".
	static string RankLine(notnull LF_PlayerMenuSkill skill, string tradeName)
	{
		if (skill.m_iRank > 0)
			return LF_Terms.Text(RankKey(skill), Ordinal(skill.m_iRank), tradeName);

		return LF_Terms.Text(RankKey(skill));
	}

	//------------------------------------------------------------------------------------------------
	//! \return "Mining, Journeyman": the trade and its tier word, the card's title.
	static string TitleLine(notnull LF_PlayerMenuSkill skill, string tradeName)
	{
		string tierKey = skill.m_sTierKey;
		if (tierKey.IsEmpty())
			tierKey = TierKeyFor(skill.m_iLevel);

		return LF_Terms.Text("#LF-PlayerMenu_SkillTitle", tradeName, LF_Terms.Text(tierKey));
	}

	//------------------------------------------------------------------------------------------------
	//! \return "Level 13 opens: ...", naming the next level's unlock; empty at the top or with no row.
	static string UnlockLine(notnull LF_PlayerMenuSkill skill)
	{
		if (skill.m_sNextUnlockKey.IsEmpty())
			return string.Empty;

		return LF_Terms.Text("#LF-PlayerMenu_SkillOpens", (skill.m_iLevel + 1).ToString(), LF_Terms.Text(skill.m_sNextUnlockKey));
	}

	//------------------------------------------------------------------------------------------------
	//! \return "Perks: Steady hands, Deep vein", or empty when none was picked.
	static string PerksLine(notnull LF_PlayerMenuSkill skill)
	{
		if (skill.m_aPerks.IsEmpty())
			return string.Empty;

		string list = string.Empty;
		foreach (string perk : skill.m_aPerks)
		{
			if (!list.IsEmpty())
				list += ", ";

			// The perk's own name (the counter and board rows use the same words); an id with no name
			// row reads in plain case.
			string nameKey = LF_PerkPick.PerkNameKey(perk);
			if (nameKey.StartsWith("#"))
				list += LF_Terms.Text(nameKey);
			else
				list += LF_PlayerMenuSheet.PlainCase(perk);
		}

		return LF_Terms.Text("#LF-PlayerMenu_SkillPerks", list);
	}
}
