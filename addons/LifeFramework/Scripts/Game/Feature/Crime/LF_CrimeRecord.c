//------------------------------------------------------------------------------------------------
//! One row of the hidden crime record: how many times one account completed one kind of crime.
//! Persisted as is (plain values). The key is the player uid, the stable account id.
class LF_CrimeRecordRow
{
	string m_sPlayerUid;
	//! The offence key of LF_WantedReports (OFFENCE_ROBBERY, ...).
	string m_sOffence;
	int m_iCount;

	LF_CrimeRecordRow Copy()
	{
		LF_CrimeRecordRow copy = new LF_CrimeRecordRow();
		copy.m_sPlayerUid = m_sPlayerUid;
		copy.m_sOffence = m_sOffence;
		copy.m_iCount = m_iCount;
		return copy;
	}
}

//------------------------------------------------------------------------------------------------
//! The bus subscriber that writes the record. Crime paths never call it: each publishes
//! LF_OnCrimeCommitted through LF_WantedReports.Report and this counts it.
class LF_CrimeRecordSubscriber : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnCrimeCommitted crime = LF_OnCrimeCommitted.Cast(busEvent);
		if (!crime)
			return false;

		return LF_CrimeRecord.Record(crime);
	}
}

//------------------------------------------------------------------------------------------------
//! Hidden server record of the crimes each account has completed (#270). Never replicated and
//! never shown: the fence (#271) and serial checks (#272) read it through Count/Total.
//! An unpaid ticket is a debt, not a crime pulled off, so it is not counted.
//! Persisted as the "crimerecord" payload.
class LF_CrimeRecord
{
	static const string FEATURE = "CrimeRecord";

	protected static ref array<ref LF_CrimeRecordRow> s_aRows;
	protected static ref LF_CrimeRecordSubscriber s_Subscriber;

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_CrimeRecordRow> Rows()
	{
		if (!s_aRows)
			s_aRows = new array<ref LF_CrimeRecordRow>();

		return s_aRows;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aRows = null;
		s_Subscriber = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: subscribes to LF_OnCrimeCommitted once. Called when the game mode starts.
	static bool EnsureSubscribed()
	{
		if (!Replication.IsServer())
			return false;

		if (s_Subscriber && LF_EventBus.SubscriberCount(LF_OnCrimeCommitted) > 0)
			return true;

		s_Subscriber = new LF_CrimeRecordSubscriber();
		return LF_EventBus.Subscribe(LF_OnCrimeCommitted, s_Subscriber);
	}

	//------------------------------------------------------------------------------------------------
	//! Counts one crime. Only the subscriber calls it.
	//! \return False when the event has no account or offence.
	static bool Record(notnull LF_OnCrimeCommitted crime)
	{
		if (LF_Systems.Refuses("Crime", "crime record"))
			return false;

		if (crime.m_sPlayerUid.IsEmpty() || crime.m_sOffence.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "not counted: crime event has no account or offence");
			return false;
		}

		if (crime.m_sOffence == LF_FineTickets.OFFENCE_UNPAID)
			return true;

		LF_CrimeRecordRow row = Find(crime.m_sPlayerUid, crime.m_sOffence);
		if (!row)
		{
			row = new LF_CrimeRecordRow();
			row.m_sPlayerUid = crime.m_sPlayerUid;
			row.m_sOffence = crime.m_sOffence;
			Rows().Insert(row);
		}

		row.m_iCount++;
		LF_Debug.Info(FEATURE, string.Format("%1 now has %2 x %3", row.m_sPlayerUid, row.m_iCount, row.m_sOffence));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_CrimeRecordRow Find(string playerUid, string offence)
	{
		foreach (LF_CrimeRecordRow row : Rows())
		{
			if (row.m_sPlayerUid == playerUid && row.m_sOffence == offence)
				return row;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Completed crimes of one kind by the account.
	static int Count(string playerUid, string offence)
	{
		LF_CrimeRecordRow row = Find(playerUid, offence);
		if (!row)
			return 0;

		return row.m_iCount;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Completed crimes of every kind by the account.
	static int Total(string playerUid)
	{
		int total = 0;
		foreach (LF_CrimeRecordRow row : Rows())
		{
			if (row.m_sPlayerUid == playerUid)
				total += row.m_iCount;
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	static array<ref LF_CrimeRecordRow> ExportAll()
	{
		array<ref LF_CrimeRecordRow> records = new array<ref LF_CrimeRecordRow>();
		foreach (LF_CrimeRecordRow row : Rows())
		{
			records.Insert(row.Copy());
		}

		return records;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the record (idempotent). Rows with no account, offence or count are skipped;
	//! duplicate keys are summed.
	static void ApplyAll(array<ref LF_CrimeRecordRow> records)
	{
		s_aRows = new array<ref LF_CrimeRecordRow>();
		if (!records)
			return;

		foreach (LF_CrimeRecordRow record : records)
		{
			if (!record || record.m_sPlayerUid.IsEmpty() || record.m_sOffence.IsEmpty() || record.m_iCount <= 0)
			{
				LF_Debug.Warn(FEATURE, "load skipped a row with no account, offence or count");
				continue;
			}

			LF_CrimeRecordRow row = Find(record.m_sPlayerUid, record.m_sOffence);
			if (row)
				row.m_iCount += record.m_iCount;
			else
				s_aRows.Insert(record.Copy());
		}
	}
}
