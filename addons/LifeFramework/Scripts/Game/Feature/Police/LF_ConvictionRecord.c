//------------------------------------------------------------------------------------------------
//! One account's conviction stamp: the in-game day of its latest arrest and how many it has had.
//! Persisted as is (plain values). The key is the player uid, the stable account id.
class LF_ConvictionRow
{
	string m_sPlayerUid;
	//! In-game day of the latest conviction (LF_ConvictionRecord.DayOrdinal of the game clock's date).
	int m_iDay;
	int m_iCount;

	LF_ConvictionRow Copy()
	{
		LF_ConvictionRow copy = new LF_ConvictionRow();
		copy.m_sPlayerUid = m_sPlayerUid;
		copy.m_iDay = m_iDay;
		copy.m_iCount = m_iCount;
		return copy;
	}
}

//------------------------------------------------------------------------------------------------
//! The bus subscriber that stamps a conviction. LF_Jail publishes LF_OnArrested when a sentence
//! starts; it never calls the record itself.
class LF_ConvictionRecordSubscriber : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnArrested arrest = LF_OnArrested.Cast(busEvent);
		if (!arrest)
			return false;

		return LF_ConvictionRecord.Record(arrest.m_sPlayerUid);
	}
}

//------------------------------------------------------------------------------------------------
//! Dated convictions per account (#1914, chapter Armed and dressed #1009). Server only, never
//! replicated. Every arrest stamps the in-game day, so a desk can ask "convicted in the last N
//! in-game days?" through IsRecent; the firearm permit's clean-record rule (#1766) reads it.
//! The day comes from the game clock's date, which vanilla persistence restores on a restart
//! (LF_Test_DayCycleSave), so a stamp keeps its age across restarts.
//! Persisted as the "convictions" payload.
class LF_ConvictionRecord
{
	static const string FEATURE = "Convictions";

	protected static ref array<ref LF_ConvictionRow> s_aRows;
	protected static ref LF_ConvictionRecordSubscriber s_Subscriber;
	//! Tests only: the in-game day Today() returns when 0 or more.
	protected static int s_iTodayOverride = -1;

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_ConvictionRow> Rows()
	{
		if (!s_aRows)
			s_aRows = new array<ref LF_ConvictionRow>();

		return s_aRows;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aRows = null;
		s_Subscriber = null;
		s_iTodayOverride = -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: subscribes to LF_OnArrested once. Called when the game mode starts.
	static bool EnsureSubscribed()
	{
		if (!Replication.IsServer())
			return false;

		if (s_Subscriber)
			return true;

		s_Subscriber = new LF_ConvictionRecordSubscriber();
		return LF_EventBus.Subscribe(LF_OnArrested, s_Subscriber);
	}

	//------------------------------------------------------------------------------------------------
	//! Days since a fixed epoch for a calendar date (proleptic Gregorian), so month and year ends
	//! count as one day like any other.
	static int DayOrdinal(int year, int month, int day)
	{
		if (month <= 2)
			year = year - 1;

		int era = year / 400;
		int yearOfEra = year - era * 400;
		int monthFromMarch = (month + 9) % 12;
		int dayOfYear = (153 * monthFromMarch + 2) / 5 + day - 1;
		int dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
		return era * 146097 + dayOfEra;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The current in-game day: the game clock's date, the system date with no clock.
	static int Today()
	{
		if (s_iTodayOverride >= 0)
			return s_iTodayOverride;

		int year, month, day;
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		TimeAndWeatherManagerEntity clock;
		if (world)
			clock = world.GetTimeAndWeatherManager();

		if (clock)
		{
			year = clock.GetYear();
			month = clock.GetMonth();
			day = clock.GetDay();
		}
		else
		{
			System.GetYearMonthDay(year, month, day);
		}

		return DayOrdinal(year, month, day);
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: pins Today() to a day; a negative day hands it back to the clock.
	static void SetTodayForTest(int day)
	{
		s_iTodayOverride = day;
	}

	//------------------------------------------------------------------------------------------------
	//! Stamps one conviction on today's day. Only the subscriber calls it.
	//! \return False when the arrest has no account.
	static bool Record(string playerUid)
	{
		if (LF_Systems.Refuses("Police", "conviction record"))
			return false;

		if (playerUid.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "not stamped: arrest event has no account");
			return false;
		}

		int today = Today();
		LF_ConvictionRow row = Find(playerUid);
		if (!row)
		{
			row = new LF_ConvictionRow();
			row.m_sPlayerUid = playerUid;
			Rows().Insert(row);
		}

		row.m_iDay = today;
		row.m_iCount++;
		LF_Debug.Info(FEATURE, string.Format("%1 convicted on day %2 (%3 in all)", playerUid, today, row.m_iCount));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_ConvictionRow Find(string playerUid)
	{
		foreach (LF_ConvictionRow row : Rows())
		{
			if (row.m_sPlayerUid == playerUid)
				return row;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The clean-record query (#1766 permit desk). A conviction stamped today is 0 days old, so a
	//! window of 3 covers the stamp day and the two after it.
	//! A stamp dated after today (an admin set the clock back) still counts as recent.
	//! \param[in] days The window in in-game days; 0 or less never matches.
	//! \return True when the account was convicted within the last `days` in-game days.
	static bool IsRecent(string playerUid, int days)
	{
		if (days <= 0 || playerUid.IsEmpty())
			return false;

		LF_ConvictionRow row = Find(playerUid);
		if (!row)
			return false;

		return Today() - row.m_iDay < days;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Convictions on the account in all, 0 with none.
	static int Count(string playerUid)
	{
		LF_ConvictionRow row = Find(playerUid);
		if (!row)
			return 0;

		return row.m_iCount;
	}

	//------------------------------------------------------------------------------------------------
	static array<ref LF_ConvictionRow> ExportAll()
	{
		array<ref LF_ConvictionRow> records = new array<ref LF_ConvictionRow>();
		foreach (LF_ConvictionRow row : Rows())
		{
			records.Insert(row.Copy());
		}

		return records;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the record (idempotent). Rows with no account or count are skipped; a duplicate
	//! account keeps the later day and sums the counts.
	//! \return Rows applied.
	static int ApplyAll(array<ref LF_ConvictionRow> records)
	{
		s_aRows = new array<ref LF_ConvictionRow>();
		if (!records)
			return 0;

		int applied = 0;
		foreach (LF_ConvictionRow record : records)
		{
			if (!record || record.m_sPlayerUid.IsEmpty() || record.m_iCount <= 0)
			{
				LF_Debug.Warn(FEATURE, "load skipped a conviction row with no account or count");
				continue;
			}

			applied++;
			LF_ConvictionRow row = Find(record.m_sPlayerUid);
			if (!row)
			{
				s_aRows.Insert(record.Copy());
				continue;
			}

			row.m_iCount += record.m_iCount;
			if (record.m_iDay > row.m_iDay)
				row.m_iDay = record.m_iDay;
		}

		return applied;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of this file's statics (Core/LF_StaticReset.c runs every feature's hook).
modded class LF_StaticResetHooks
{
	override void Run(notnull array<string> done, bool apply)
	{
		super.Run(done, apply);
		done.Insert("LF_ConvictionRecord");
		if (apply)
			LF_ConvictionRecord.ResetStatics();
	}
}
