//------------------------------------------------------------------------------------------------
//! Who knows whom, on the server. One pair per introduction: the knower heard the known
//! player's name. Keys are player uids (account ids), never entities, so a pair outlives a
//! respawn, a reconnect, and a restart.
//!
//! PERSISTENCE. ExportAll / ApplyAll are the "introductions" payload of
//! LF_PersistenceComponentSerializer. One flat record per pair keeps the positional binary save
//! simple. ApplyAll replaces the live set, so a second pass ends on the same state.
//------------------------------------------------------------------------------------------------
class LF_IntroductionRecord : Managed
{
	static const int VERSION = 1;

	string m_sKnowerUid;
	string m_sKnownUid;

	//------------------------------------------------------------------------------------------------
	static LF_IntroductionRecord Create(string knowerUid, string knownUid)
	{
		LF_IntroductionRecord record();
		record.m_sKnowerUid = knowerUid;
		record.m_sKnownUid = knownUid;
		return record;
	}
}

//------------------------------------------------------------------------------------------------
class LF_Introductions
{
	protected static ref map<string, ref array<string>> s_mKnown;

	//------------------------------------------------------------------------------------------------
	protected static map<string, ref array<string>> Known()
	{
		if (!s_mKnown)
			s_mKnown = new map<string, ref array<string>>();

		return s_mKnown;
	}

	//------------------------------------------------------------------------------------------------
	//! Records that knowerUid now knows knownUid's name.
	//! \return False when either uid is empty, both are the same player, or the pair already exists.
	static bool Introduce(string knowerUid, string knownUid)
	{
		if (knowerUid.IsEmpty() || knownUid.IsEmpty())
		{
			LF_Debug.Error("Interaction", "introduction refused: empty player uid");
			return false;
		}

		if (knowerUid == knownUid)
		{
			LF_Debug.Warn("Interaction", "introduction refused: a player cannot introduce themselves to themselves");
			return false;
		}

		array<string> known = Known().Get(knowerUid);
		if (!known)
		{
			known = new array<string>();
			Known().Set(knowerUid, known);
		}

		if (known.Contains(knownUid))
			return false;

		known.Insert(knownUid);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool Knows(string knowerUid, string knownUid)
	{
		array<string> known = Known().Get(knowerUid);
		if (!known)
			return false;

		return known.Contains(knownUid);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[out] result Every uid the knower has been introduced to.
	static void GetKnownBy(string knowerUid, notnull array<string> result)
	{
		result.Clear();
		array<string> known = Known().Get(knowerUid);
		if (!known)
			return;

		result.Copy(known);
	}

	//------------------------------------------------------------------------------------------------
	//! \param[out] result Every uid that knows knownUid.
	static void GetKnowersOf(string knownUid, notnull array<string> result)
	{
		result.Clear();
		foreach (string knowerUid, array<string> known : Known())
		{
			if (known && known.Contains(knownUid))
				result.Insert(knowerUid);
		}
	}

	//------------------------------------------------------------------------------------------------
	static int PairCount()
	{
		int count = 0;
		foreach (string knowerUid, array<string> known : Known())
		{
			if (known)
				count += known.Count();
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: one record per pair (never null).
	static array<ref LF_IntroductionRecord> ExportAll()
	{
		array<ref LF_IntroductionRecord> records = new array<ref LF_IntroductionRecord>();
		foreach (string knowerUid, array<string> known : Known())
		{
			if (!known || knowerUid.IsEmpty())
				continue;

			foreach (string knownUid : known)
			{
				if (!knownUid.IsEmpty())
					records.Insert(LF_IntroductionRecord.Create(knowerUid, knownUid));
			}
		}

		return records;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: the save is the complete set. Live pairs absent from it are dropped, so a
	//! second pass ends on the same state. A record with an empty uid is skipped and logged.
	static void ApplyAll(notnull array<ref LF_IntroductionRecord> records)
	{
		Known().Clear();
		int applied = 0;
		foreach (LF_IntroductionRecord record : records)
		{
			if (!record || record.m_sKnowerUid.IsEmpty() || record.m_sKnownUid.IsEmpty())
			{
				LF_Debug.Error("Interaction", "introduction record skipped: empty player uid");
				continue;
			}

			if (Introduce(record.m_sKnowerUid, record.m_sKnownUid))
				applied++;
		}

		LF_Debug.Info("Interaction", string.Format("apply-save restored %1 introduction(s)", applied));
	}

	//------------------------------------------------------------------------------------------------
	//! Drops every pair (LF_StaticReset, tests).
	static void ResetStatics()
	{
		s_mKnown = null;
	}
}
