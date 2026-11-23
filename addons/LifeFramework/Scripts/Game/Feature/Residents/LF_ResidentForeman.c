//------------------------------------------------------------------------------------------------
//! What a work-site foreman says about the next step (#4237): the player's level in the foreman's
//! trade picks the voiced line that announces the next thing that level opens ("The coal seam at
//! East Rise will take you on at Mining 4"). The text is the pack's unlock table (LF_TradeUnlocks);
//! the foreman's dialogue holds one recorded line per level, voiced for the key
//! `LF-Foreman_<TRADE>_Next_<level>` with a leading hash (and `..._Next_Top` for a player who has seen the last row).
//! The core knows no trade, site or unlock: the trade is the hook param. Server only, no state.
class LF_ResidentForeman
{
	static const string FEATURE = "Residents";
	static const string KEY_PREFIX = "LF-Foreman_";
	static const string KEY_MID = "_Next_";
	static const string KEY_TOP = "Top";

	//------------------------------------------------------------------------------------------------
	//! \return The lowest level above `level` that `job` has an unlock row for; 0 when there is none
	//! (the player is past the last row, the trade has no rows, or there is no table).
	static int NextLevel(array<ref LF_TradeUnlockRow> rows, string job, int level)
	{
		if (!rows)
			return 0;

		int best = 0;
		foreach (LF_TradeUnlockRow row : rows)
		{
			if (!row || row.m_sJob != job || row.m_iLevel <= level)
				continue;

			if (best == 0 || row.m_iLevel < best)
				best = row.m_iLevel;
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The caption key the foreman speaks for a player at `level` in `job`:
	//! `#` + `LF-Foreman_<job>_Next_<level it announces>` (two digits), or `..._Next_Top` when nothing is left.
	static string KeyFor(array<ref LF_TradeUnlockRow> rows, string job, int level)
	{
		int next = NextLevel(rows, job, level);
		if (next <= 0)
			return "#" + KEY_PREFIX + job + KEY_MID + KEY_TOP;

		string digits = next.ToString();
		if (next < 10)
			digits = "0" + digits;

		return "#" + KEY_PREFIX + job + KEY_MID + digits;
	}

	//------------------------------------------------------------------------------------------------
	//! The same, for a character: reads their level in `job` from LF_JobLevels.
	static string KeyForPlayer(array<ref LF_TradeUnlockRow> rows, string playerUid, string job)
	{
		return KeyFor(rows, job, LF_JobLevels.LevelOf(playerUid, job));
	}
}

//------------------------------------------------------------------------------------------------
//! The foreman's "What's next for me?" reply. Param is the trade id (MINER, LUMBERJACK). The server
//! reads the level itself, so a client cannot ask for a line it has not earned; the key it picks is
//! put in `spoken` and the dialogue plays the line recorded for it (`for_key`).
class LF_DialogueHook_ForemanNext : LF_DialogueHook
{
	//------------------------------------------------------------------------------------------------
	override void Run(IEntity player, IEntity npc, string param, notnull array<string> spoken)
	{
		if (param.IsEmpty())
		{
			LF_Debug.Error(LF_ResidentForeman.FEATURE, "foreman reply has no trade in its param");
			return;
		}

		string uid = LF_Utils.GetPlayerUID(player);
		if (uid.IsEmpty())
		{
			LF_Debug.Warn(LF_ResidentForeman.FEATURE, string.Format("foreman %1 refused: no player behind the body", param));
			return;
		}

		LF_TradeUnlocksConfig table = LF_TradeUnlocksConfig.Get();
		string key = LF_ResidentForeman.KeyForPlayer(table.m_aRows, uid, param);
		spoken.Insert(key);
		LF_Debug.Info(LF_ResidentForeman.FEATURE, string.Format("foreman %1 tells %2 '%3'", param, uid, key));
	}
}
