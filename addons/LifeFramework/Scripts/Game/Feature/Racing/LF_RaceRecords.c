// LF_RaceRecords.c - the best race time per character per course (#1925, chapter Evenings in the
// village #1011). A finished race (LF_OnRaceFinished, #1923) offers the winner's time; a faster time
// than that character's record on that course replaces it, a slower one changes nothing. The board
// at the track reads the top times through LF_RaceBoardProvider (LF_RaceBoard.c). Server only, never
// replicated. Saved as the "racerecords" payload, keyed by course id and player uid (the character
// record key, LF_Utils.GetPlayerUID), never an entity id.

//------------------------------------------------------------------------------------------------
//! One character's best time on one course. The name is the character's full name when the record
//! was set, so the board names a racer who is offline. Persisted as is.
class LF_RaceRecordRow
{
	string m_sCourseId;
	string m_sUid;
	string m_sName;
	//! The best time from the start, in milliseconds. Always above 0.
	int m_iBestMs;

	LF_RaceRecordRow Copy()
	{
		LF_RaceRecordRow copy = new LF_RaceRecordRow();
		copy.m_sCourseId = m_sCourseId;
		copy.m_sUid = m_sUid;
		copy.m_sName = m_sName;
		copy.m_iBestMs = m_iBestMs;
		return copy;
	}
}

//------------------------------------------------------------------------------------------------
class LF_RaceRecordsSubscriber : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnRaceFinished finish = LF_OnRaceFinished.Cast(busEvent);
		if (!finish)
			return false;

		LF_RaceRecords.Offer(finish.m_sCourseId, finish.m_sWinnerUid, LF_RaceRecords.NameOf(finish.m_sWinnerUid), finish.m_iWinnerMs);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
class LF_RaceRecords
{
	static const string FEATURE = "RaceRecords";
	//! Rows the board shows per course.
	static const int BOARD_ROWS = 5;	// lf-fixed: the slice's acceptance (#1925), a board of five
	//! The longest name a row keeps; a longer one is cut.
	static const int NAME_MAX = 48;	// lf-fixed: bounds a saved row

	protected static ref array<ref LF_RaceRecordRow> s_aRows;
	protected static ref LF_RaceRecordsSubscriber s_Subscriber;

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_RaceRecordRow> Rows()
	{
		if (!s_aRows)
			s_aRows = new array<ref LF_RaceRecordRow>();

		return s_aRows;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aRows = null;
		s_Subscriber = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: listen for finished races. Safe to call again; a bus reset re-subscribes.
	static bool EnsureSubscribed()
	{
		if (!Replication.IsServer())
			return false;

		if (s_Subscriber && LF_EventBus.IsSubscribed(LF_OnRaceFinished, s_Subscriber))
			return true;

		if (!s_Subscriber)
			s_Subscriber = new LF_RaceRecordsSubscriber();

		return LF_EventBus.Subscribe(LF_OnRaceFinished, s_Subscriber);
	}

	//------------------------------------------------------------------------------------------------
	protected static int IndexOf(string courseId, string uid)
	{
		array<ref LF_RaceRecordRow> rows = Rows();
		for (int i = 0; i < rows.Count(); i++)
		{
			if (rows[i].m_sCourseId == courseId && rows[i].m_sUid == uid)
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! The character's full name for a record, or empty when the account manager does not know the uid.
	static string NameOf(string uid)
	{
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!accounts)
			return string.Empty;

		LF_PlayerCharacter character = accounts.FindCharacterByKey(uid);
		if (!character)
			return string.Empty;

		return character.GetFullName();
	}

	//------------------------------------------------------------------------------------------------
	//! A finish time for a character on a course. A first time, or one faster than the record, becomes
	//! the record (and refreshes the name); an equal or slower one changes nothing.
	//! \return True when the record changed.
	static bool Offer(string courseId, string uid, string name, int ms)
	{
		if (courseId.IsEmpty() || uid.IsEmpty() || ms <= 0)
		{
			LF_Debug.Warn(FEATURE, string.Format("time not recorded: course '%1' uid '%2' ms %3", courseId, uid, ms));
			return false;
		}

		if (name.Length() > NAME_MAX)
			name = name.Substring(0, NAME_MAX);

		int idx = IndexOf(courseId, uid);
		if (idx != -1)
		{
			LF_RaceRecordRow row = Rows()[idx];
			if (ms >= row.m_iBestMs)
			{
				LF_Debug.Info(FEATURE, string.Format("'%1' %2 ran %3 ms, record stays %4 ms", courseId, uid, ms, row.m_iBestMs));
				return false;
			}

			LF_Debug.Info(FEATURE, string.Format("'%1' %2 new record %3 ms (was %4 ms)", courseId, uid, ms, row.m_iBestMs));
			row.m_iBestMs = ms;
			if (!name.IsEmpty())
				row.m_sName = name;

			return true;
		}

		LF_RaceRecordRow added = new LF_RaceRecordRow();
		added.m_sCourseId = courseId;
		added.m_sUid = uid;
		added.m_sName = name;
		added.m_iBestMs = ms;
		Rows().Insert(added);
		LF_Debug.Info(FEATURE, string.Format("'%1' %2 first record %3 ms", courseId, uid, ms));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The character's best time on the course in ms, or -1 for none.
	static int BestOf(string courseId, string uid)
	{
		int idx = IndexOf(courseId, uid);
		if (idx == -1)
			return -1;

		return Rows()[idx].m_iBestMs;
	}

	//------------------------------------------------------------------------------------------------
	//! The fastest rows of one course, fastest first (a tie keeps the earlier record first).
	//! \param[out] outRows Cleared, then filled with up to `limit` copies.
	static void Top(string courseId, int limit, notnull array<ref LF_RaceRecordRow> outRows)
	{
		outRows.Clear();
		foreach (LF_RaceRecordRow row : Rows())
		{
			if (row.m_sCourseId != courseId)
				continue;

			// Insertion sort: a course has one row per character, so the list stays short.
			int at = outRows.Count();
			while (at > 0 && outRows[at - 1].m_iBestMs > row.m_iBestMs)
			{
				at--;
			}

			outRows.InsertAt(row.Copy(), at);
		}

		while (outRows.Count() > limit)
		{
			outRows.Remove(outRows.Count() - 1);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Course ids that have at least one record, in first-record order.
	static void Courses(notnull array<string> outIds)
	{
		outIds.Clear();
		foreach (LF_RaceRecordRow row : Rows())
		{
			if (outIds.Find(row.m_sCourseId) == -1)
				outIds.Insert(row.m_sCourseId);
		}
	}

	//------------------------------------------------------------------------------------------------
	static int Count()
	{
		return Rows().Count();
	}

	//------------------------------------------------------------------------------------------------
	static array<ref LF_RaceRecordRow> ExportAll()
	{
		array<ref LF_RaceRecordRow> records = new array<ref LF_RaceRecordRow>();
		foreach (LF_RaceRecordRow row : Rows())
		{
			records.Insert(row.Copy());
		}

		return records;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the records (idempotent). A row with no course, no uid or no positive time is skipped;
	//! a repeated course and uid keeps the faster time.
	//! \return Rows applied.
	static int ApplyAll(array<ref LF_RaceRecordRow> records)
	{
		s_aRows = new array<ref LF_RaceRecordRow>();
		EnsureSubscribed();
		if (!records)
			return 0;

		int applied = 0;
		foreach (LF_RaceRecordRow record : records)
		{
			if (!record || record.m_sCourseId.IsEmpty() || record.m_sUid.IsEmpty() || record.m_iBestMs <= 0)
			{
				LF_Debug.Warn(FEATURE, "load skipped a race record with no course, no uid or no time");
				continue;
			}

			int idx = IndexOf(record.m_sCourseId, record.m_sUid);
			if (idx != -1)
			{
				LF_Debug.Warn(FEATURE, string.Format("load found two records for '%1' %2; the faster stays", record.m_sCourseId, record.m_sUid));
				if (record.m_iBestMs < s_aRows[idx].m_iBestMs)
					s_aRows[idx].m_iBestMs = record.m_iBestMs;

				continue;
			}

			s_aRows.Insert(record.Copy());
			applied++;
		}

		return applied;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_RaceRecords's statics (registered at script load; Core/LF_StaticReset.c).
class LF_RaceRecords_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_RaceRecords_StaticResetHook());

	override string Owner()
	{
		return "LF_RaceRecords";
	}

	override void Apply()
	{
		LF_RaceRecords.ResetStatics();
	}
}
