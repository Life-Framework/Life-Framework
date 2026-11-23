//------------------------------------------------------------------------------------------------
//! One picked perk (WK-W8, #4186): the character (record key, LF_Utils.GetPlayerUID), the trade (the
//! job id of an LF_JobTrackDef: MINER, FARMER, ...), the level the pick belongs to (10, 20) and the
//! perk id. Saved in the "tradeperks" section (LF_TradePerksSavePayload).
//!
//! Positional in a binary save: never add, remove or reorder a field. A new field raises the
//! section's Version() and ReadSection reads both shapes.
class LF_TradePerkRow
{
	string m_sPlayerUid;
	string m_sTrade;
	int m_iAtLevel;
	string m_sPerk;

	//------------------------------------------------------------------------------------------------
	static LF_TradePerkRow Create(string playerUid, string trade, int atLevel, string perk)
	{
		LF_TradePerkRow row = new LF_TradePerkRow();
		row.m_sPlayerUid = playerUid;
		row.m_sTrade = trade;
		row.m_iAtLevel = atLevel;
		row.m_sPerk = perk;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! The perk store (WK-W8, #4186): which perk each character picked in each trade at each pick level.
//! Server state only; the pick itself (levels 10 and 20, the labour office counter, the $500 change)
//! is WK-W8b and WK-W13, which call Pick. The store does not check the level: the caller gates.
class LF_TradePerks
{
	static const string FEATURE = "TradePerks";

	//! "<uid>|<trade>|<atLevel>" -> perk id.
	protected static ref map<string, string> s_mPicks;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mPicks = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, string> Store()
	{
		if (!s_mPicks)
			s_mPicks = new map<string, string>();

		return s_mPicks;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Key(string playerUid, string trade, int atLevel)
	{
		return playerUid + "|" + trade + "|" + atLevel.ToString();
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Valid(string playerUid, string trade, int atLevel, string perk)
	{
		if (playerUid.IsEmpty() || trade.IsEmpty() || perk.IsEmpty() || atLevel <= 0)
			return false;

		// The key separator inside a field would make two picks share a key.
		return !playerUid.Contains("|") && !trade.Contains("|");
	}

	//------------------------------------------------------------------------------------------------
	//! Sets (or changes) the perk picked in a trade at a pick level. False on a bad input.
	static bool Pick(string playerUid, string trade, int atLevel, string perk)
	{
		if (!Valid(playerUid, trade, atLevel, perk))
		{
			LF_Debug.Warn(FEATURE, string.Format("pick refused: uid '%1' trade '%2' level %3 perk '%4'", playerUid, trade, atLevel, perk));
			return false;
		}

		Store().Set(Key(playerUid, trade, atLevel), perk);
		LF_Debug.Info(FEATURE, string.Format("%1 picked %2 in %3 at level %4", playerUid, perk, trade, atLevel));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The perk picked in a trade at a pick level, or "" when none.
	static string PerkAt(string playerUid, string trade, int atLevel)
	{
		if (!s_mPicks)
			return "";

		string perk;
		s_mPicks.Find(Key(playerUid, trade, atLevel), perk);
		return perk;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the character picked this perk in this trade at any level.
	static bool Has(string playerUid, string trade, string perk)
	{
		if (!s_mPicks || perk.IsEmpty())
			return false;

		string prefix = playerUid + "|" + trade + "|";
		foreach (string key, string picked : s_mPicks)
		{
			if (picked == perk && key.StartsWith(prefix))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops every perk of one character (a deleted character).
	static void Forget(string playerUid)
	{
		if (!s_mPicks || playerUid.IsEmpty())
			return;

		string prefix = playerUid + "|";
		array<string> drop = {};
		foreach (string key, string perk : s_mPicks)
		{
			if (key.StartsWith(prefix))
				drop.Insert(key);
		}

		foreach (string key : drop)
		{
			s_mPicks.Remove(key);
		}
	}

	//------------------------------------------------------------------------------------------------
	static int Count()
	{
		if (!s_mPicks)
			return 0;

		return s_mPicks.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Every pick as rows, for the save.
	static array<ref LF_TradePerkRow> ExportAll()
	{
		array<ref LF_TradePerkRow> rows = new array<ref LF_TradePerkRow>();
		if (!s_mPicks)
			return rows;

		foreach (string key, string perk : s_mPicks)
		{
			array<string> parts = {};
			key.Split("|", parts, false);
			if (parts.Count() != 3)
				continue;

			rows.Insert(LF_TradePerkRow.Create(parts[0], parts[1], parts[2].ToInt(), perk));
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the store with the rows (null or empty clears it), so a second load ends in the same
	//! state. Returns how many rows were kept; a row with a missing field is skipped with a warning.
	static int ApplyAll(array<ref LF_TradePerkRow> rows)
	{
		s_mPicks = new map<string, string>();
		if (!rows)
			return 0;

		int kept;
		foreach (LF_TradePerkRow row : rows)
		{
			if (!row || !Valid(row.m_sPlayerUid, row.m_sTrade, row.m_iAtLevel, row.m_sPerk))
			{
				LF_Debug.Warn(FEATURE, "a saved perk row has a missing field and is skipped");
				continue;
			}

			s_mPicks.Set(Key(row.m_sPlayerUid, row.m_sTrade, row.m_iAtLevel), row.m_sPerk);
			kept++;
		}

		return kept;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_TradePerks's statics (LF_StaticReset finds it by reflection).
class LF_TradePerks_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_TradePerks";
	}

	override void Apply()
	{
		LF_TradePerks.ResetStatics();
	}
}
