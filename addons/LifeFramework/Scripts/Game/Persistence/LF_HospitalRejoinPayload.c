//------------------------------------------------------------------------------------------------
//! One downed logout whose hospital wake is still owed (#2101). The key is the player uid
//! (LF_Utils.GetPlayerUID), stable across a restart; never an entity id. Persisted as the
//! "hospitalrejoin" payload; the shape is frozen once it ships (validate-record-shapes).
class LF_HospitalRejoinRow
{
	//! The uid of the player who logged out downed.
	string m_sPlayerUid;
	//! System.GetUnixTime() at the logout.
	int m_iLoggedOutUnix;

	//------------------------------------------------------------------------------------------------
	static LF_HospitalRejoinRow Create(string uid, int loggedOutUnix)
	{
		LF_HospitalRejoinRow row = new LF_HospitalRejoinRow();
		row.m_sPlayerUid = uid;
		row.m_iLoggedOutUnix = loggedOutUnix;
		return row;
	}

	//------------------------------------------------------------------------------------------------
	LF_HospitalRejoinRow Copy()
	{
		return Create(m_sPlayerUid, m_iLoggedOutUnix);
	}
}

//------------------------------------------------------------------------------------------------
//! The hospital rejoin of a downed logout survives a server restart (#2101, from #2063).
//!
//! A player who logs out downed dies at the logout (LF_DisconnectRules, downedDies) and wakes at the
//! hospital with the bill. The pending wake itself is session state (LF_HospitalRespawn), so a
//! restart between the logout and the rejoin used to drop it: the player spawned at the plaza with no
//! bill, and a restart was an escape hatch (maintainer decision 2026-11-15). LF_DisconnectRules.Apply
//! now Records the logout here; this list is the "hospitalrejoin" payload, and a load puts each
//! pending wake back (LF_HospitalRespawn.MarkDied), so the rejoin after the restart wakes at a bed,
//! writes the bill once and consumes it.
//!
//! A row lives until the wake is taken: the next save drops every row whose wake was consumed
//! (LF_HospitalRespawn.HasPendingWake false), so a woken player is never woken twice. Only a
//! downed logout is recorded here; a death in session that is not followed by a respawn before a
//! restart is not covered (out of scope of #2101). Server only, never replicated.
class LF_HospitalRejoin
{
	static const string FEATURE = "HospitalRejoin";

	protected static ref array<ref LF_HospitalRejoinRow> s_aRows;

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_HospitalRejoinRow> Rows()
	{
		if (!s_aRows)
			s_aRows = new array<ref LF_HospitalRejoinRow>();

		return s_aRows;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aRows = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static int IndexOf(string uid)
	{
		array<ref LF_HospitalRejoinRow> rows = Rows();
		for (int i = 0; i < rows.Count(); i++)
		{
			if (rows[i].m_sPlayerUid == uid)
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from LF_DisconnectRules.Apply: uid logged out downed and dies. The death marks the
	//! session wake (LF_HospitalRespawn.MarkDied); this row is what carries it over a restart.
	//! Recording twice is one row.
	//! \param[in] uid The leaving player's uid.
	//! \param[in] loggedOutUnix System.GetUnixTime() at the logout.
	//! \return False when the uid is empty and nothing was recorded.
	static bool Record(string uid, int loggedOutUnix)
	{
		if (uid.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "not recorded: the downed logout has no uid");
			return false;
		}

		// The session wake is the death's own (LF_SpawnLogic marks it when the body is killed): a logout
		// whose kill did not happen owes nothing, and the row is dropped at the next save.
		int index = IndexOf(uid);
		if (index != -1)
		{
			LF_HospitalRejoinRow existing = Rows().Get(index);
			existing.m_iLoggedOutUnix = loggedOutUnix;
			return true;
		}

		Rows().Insert(LF_HospitalRejoinRow.Create(uid, loggedOutUnix));
		LF_Debug.Info(FEATURE, uid + " logged out downed: the hospital wake is kept across a restart");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True while uid has a recorded downed logout whose wake is still owed.
	static bool IsPending(string uid)
	{
		return !uid.IsEmpty() && IndexOf(uid) != -1 && LF_HospitalRespawn.HasPendingWake(uid);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Recorded downed logouts, consumed ones included until the next save.
	static int Count()
	{
		return Rows().Count();
	}

	//------------------------------------------------------------------------------------------------
	//! The rows to save: those whose wake is still pending. A row whose wake was taken is dropped
	//! here, so a woken player is not woken again by the next load.
	static array<ref LF_HospitalRejoinRow> ExportAll()
	{
		array<ref LF_HospitalRejoinRow> records = new array<ref LF_HospitalRejoinRow>();
		array<ref LF_HospitalRejoinRow> rows = Rows();
		for (int i = rows.Count() - 1; i >= 0; i--)
		{
			if (!LF_HospitalRespawn.HasPendingWake(rows[i].m_sPlayerUid))
			{
				LF_Debug.Info(FEATURE, rows[i].m_sPlayerUid + " woke in hospital: the row is dropped");
				rows.Remove(i);
			}
		}

		foreach (LF_HospitalRejoinRow row : rows)
		{
			records.Insert(row.Copy());
		}

		return records;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the rows and puts each pending wake back (idempotent: a second load leaves one row
	//! and one pending wake per uid). Rows with no uid and duplicates are skipped.
	//! \return Rows applied.
	static int ApplyAll(array<ref LF_HospitalRejoinRow> records)
	{
		s_aRows = new array<ref LF_HospitalRejoinRow>();
		if (!records)
			return 0;

		int applied = 0;
		foreach (LF_HospitalRejoinRow record : records)
		{
			if (!record || record.m_sPlayerUid.IsEmpty())
			{
				LF_Debug.Warn(FEATURE, "load skipped a hospital-rejoin row with no uid");
				continue;
			}

			if (IndexOf(record.m_sPlayerUid) != -1)
				continue;

			s_aRows.Insert(record.Copy());
			LF_HospitalRespawn.MarkDied(record.m_sPlayerUid);
			applied++;
		}

		return applied;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_HospitalRejoin's statics (registered at script load; Core/LF_StaticReset.c).
class LF_HospitalRejoin_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_HospitalRejoin_StaticResetHook());

	override string Owner()
	{
		return "LF_HospitalRejoin";
	}

	override void Apply()
	{
		LF_HospitalRejoin.ResetStatics();
	}
}
