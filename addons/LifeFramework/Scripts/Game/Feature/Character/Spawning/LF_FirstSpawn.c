//------------------------------------------------------------------------------------------------
//! One character still waiting for its first spawn. The key is the character's persistence id
//! (LF_PlayerCharacter.GetId), stable for the life of the character. Persisted as is.
class LF_FirstSpawnRow
{
	string m_sCharacterId;

	LF_FirstSpawnRow Copy()
	{
		LF_FirstSpawnRow copy = new LF_FirstSpawnRow();
		copy.m_sCharacterId = m_sCharacterId;
		return copy;
	}
}

//------------------------------------------------------------------------------------------------
//! A character's first spawn (#1964, chapter Story: The relocated #1015). The character-creation
//! flow Marks a brand-new character; the spawn logic Consumes the mark when that character first
//! spawns, so the first spawn reads true once and every later spawn (after a death, a rejoin, a
//! restart) reads false. A character that never went through creation (an existing save) has no
//! mark and is never first. The first-night rule applies on first spawn only (#1758).
//! The mark survives a restart as the "firstspawns" payload: a character created and not yet
//! spawned when the server stops is still first afterwards. Server only, never replicated.
class LF_FirstSpawn
{
	static const string FEATURE = "FirstSpawn";

	protected static ref array<ref LF_FirstSpawnRow> s_aRows;

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_FirstSpawnRow> Rows()
	{
		if (!s_aRows)
			s_aRows = new array<ref LF_FirstSpawnRow>();

		return s_aRows;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aRows = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static int IndexOf(string characterId)
	{
		array<ref LF_FirstSpawnRow> rows = Rows();
		for (int i = 0; i < rows.Count(); i++)
		{
			if (rows[i].m_sCharacterId == characterId)
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Marks a brand-new character: its next spawn is its first. Marking twice is one mark.
	//! \param[in] characterId The character's persistence id.
	//! \return False when the id is empty and nothing was marked.
	static bool Mark(string characterId)
	{
		if (characterId.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "not marked: the new character has no id");
			return false;
		}

		if (IndexOf(characterId) != -1)
			return true;

		LF_FirstSpawnRow row = new LF_FirstSpawnRow();
		row.m_sCharacterId = characterId;
		Rows().Insert(row);
		LF_Debug.Info(FEATURE, "marked " + characterId + " for its first spawn");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True while the character is marked and has not spawned yet. Does not clear the mark.
	static bool IsPending(string characterId)
	{
		return !characterId.IsEmpty() && IndexOf(characterId) != -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads and clears the mark: true the once, false for every later call and for a character that
	//! was never marked. The spawn logic calls it when a character's body is created.
	//! \param[in] characterId The character's persistence id.
	static bool Consume(string characterId)
	{
		if (characterId.IsEmpty())
			return false;

		int idx = IndexOf(characterId);
		if (idx == -1)
			return false;

		Rows().Remove(idx);
		LF_Debug.Info(FEATURE, characterId + " spawns for the first time");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Characters still waiting for their first spawn.
	static int Count()
	{
		return Rows().Count();
	}

	//------------------------------------------------------------------------------------------------
	static array<ref LF_FirstSpawnRow> ExportAll()
	{
		array<ref LF_FirstSpawnRow> records = new array<ref LF_FirstSpawnRow>();
		foreach (LF_FirstSpawnRow row : Rows())
		{
			records.Insert(row.Copy());
		}

		return records;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the marks (idempotent). Rows with no id and duplicates are skipped.
	//! \return Marks applied.
	static int ApplyAll(array<ref LF_FirstSpawnRow> records)
	{
		s_aRows = new array<ref LF_FirstSpawnRow>();
		if (!records)
			return 0;

		int applied = 0;
		foreach (LF_FirstSpawnRow record : records)
		{
			if (!record || record.m_sCharacterId.IsEmpty())
			{
				LF_Debug.Warn(FEATURE, "load skipped a first-spawn row with no character id");
				continue;
			}

			if (IndexOf(record.m_sCharacterId) != -1)
				continue;

			s_aRows.Insert(record.Copy());
			applied++;
		}

		return applied;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_FirstSpawn's statics (registered at script load; Core/LF_StaticReset.c).
class LF_FirstSpawn_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_FirstSpawn_StaticResetHook());

	override string Owner()
	{
		return "LF_FirstSpawn";
	}

	override void Apply()
	{
		LF_FirstSpawn.ResetStatics();
	}
}
