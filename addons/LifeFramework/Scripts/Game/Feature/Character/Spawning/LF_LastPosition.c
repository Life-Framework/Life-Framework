//------------------------------------------------------------------------------------------------
//! Where each character last stood (#1777, the first half of #640): position, heading and the time
//! it was taken, keyed by the character's persistence id (LF_PlayerCharacter.GetId).
//!
//! Taken on the server when a player disconnects (before the game mode's disconnect cleanup, while
//! the body still stands) and for every connected player just before each save writes the "lastpos"
//! payload. A dead body is not taken: the row keeps the last place the character stood alive.
//! This half only keeps the rows; spawning at them is the second half of #640. Server only, never
//! replicated. The store is a plain replacement on load, so a second load changes nothing.
class LF_LastPosition
{
	static const string FEATURE = "LastPosition";

	//! Key: character persistence id.
	protected static ref map<string, ref LF_LastPositionRow> s_mRows;

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref LF_LastPositionRow> Rows()
	{
		if (!s_mRows)
			s_mRows = new map<string, ref LF_LastPositionRow>();

		return s_mRows;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mRows = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Records where a character stands now. A newer record replaces the older one.
	//! \param[in] characterId The character's persistence id.
	//! \return False when the id is empty and nothing was recorded.
	static bool Record(string characterId, vector position, float yaw, int takenUnix)
	{
		if (characterId.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "not recorded: the character has no id");
			return false;
		}

		Rows().Set(characterId, LF_LastPositionRow.Create(characterId, position, yaw, takenUnix));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return A copy of the character's last position, or null when none is kept.
	static LF_LastPositionRow Get(string characterId)
	{
		if (characterId.IsEmpty())
			return null;

		LF_LastPositionRow row = Rows().Get(characterId);
		if (!row)
			return null;

		return row.Copy();
	}

	//------------------------------------------------------------------------------------------------
	//! Drops a character's row (a test, or a character that is gone). \return True when one was kept.
	static bool Forget(string characterId)
	{
		if (!Rows().Contains(characterId))
			return false;

		Rows().Remove(characterId);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static int Count()
	{
		return Rows().Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: takes the position of the character the player controls now.
	//! \return True when a row was recorded; false for no body, a dead body, or no character id.
	static bool CapturePlayer(int playerId, int takenUnix)
	{
		if (!Replication.IsServer() || !GetGame())
			return false;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return false;

		IEntity body = playerManager.GetPlayerControlledEntity(playerId);
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(body);
		if (!character)
			return false;

		CharacterControllerComponent controller = character.GetCharacterController();
		if (controller && controller.GetLifeState() == ECharacterLifeState.DEAD)
			return false;

		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!accounts)
			return false;

		LF_PlayerAccount account = accounts.GetFromCache(playerId);
		if (!account || !account.GetActiveCharacter())
		{
			LF_Debug.Warn(FEATURE, string.Format("player %1 has a body but no loaded character: last position not taken", playerId));
			return false;
		}

		return Record(account.GetActiveCharacter().GetId(), character.GetOrigin(), character.GetYawPitchRoll()[0], takenUnix);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: takes every connected player's position (just before a save).
	//! \return Rows recorded.
	static int CaptureOnline(int takenUnix)
	{
		if (!Replication.IsServer() || !GetGame() || !GetGame().GetPlayerManager())
			return 0;

		array<int> playerIds = new array<int>();
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		int taken = 0;
		foreach (int playerId : playerIds)
		{
			if (CapturePlayer(playerId, takenUnix))
				taken++;
		}

		return taken;
	}

	//------------------------------------------------------------------------------------------------
	static array<ref LF_LastPositionRow> ExportAll()
	{
		array<ref LF_LastPositionRow> records = new array<ref LF_LastPositionRow>();
		foreach (string characterId, LF_LastPositionRow row : Rows())
		{
			if (row)
				records.Insert(row.Copy());
		}

		return records;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the rows (idempotent). Rows with no id are skipped; a repeated id keeps the newest.
	//! \return Rows applied.
	static int ApplyAll(array<ref LF_LastPositionRow> records)
	{
		s_mRows = new map<string, ref LF_LastPositionRow>();
		if (!records)
			return 0;

		foreach (LF_LastPositionRow record : records)
		{
			if (!record || record.m_sCharacterId.IsEmpty())
			{
				LF_Debug.Warn(FEATURE, "load skipped a last-position row with no character id");
				continue;
			}

			LF_LastPositionRow kept = s_mRows.Get(record.m_sCharacterId);
			if (kept && kept.m_iTakenUnix > record.m_iTakenUnix)
				continue;

			s_mRows.Set(record.m_sCharacterId, record.Copy());
		}

		return s_mRows.Count();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_LastPosition's statics (registered at script load; Core/LF_StaticReset.c).
class LF_LastPosition_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_LastPosition_StaticResetHook());

	override string Owner()
	{
		return "LF_LastPosition";
	}

	override void Apply()
	{
		LF_LastPosition.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! A leaver's position is taken before the game mode's own disconnect cleanup, while the body still
//! stands where they left it.
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		if (Replication.IsServer())
			LF_LastPosition.CapturePlayer(playerId, System.GetUnixTime());

		super.OnPlayerDisconnected(playerId, cause, timeout);
	}
}
