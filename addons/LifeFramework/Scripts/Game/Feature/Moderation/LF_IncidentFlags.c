//------------------------------------------------------------------------------------------------
//! One flag: a player reported, and this incident was one of theirs from the last few minutes.
class LF_IncidentFlagRow : Managed
{
	//! LF_IncidentRecord.m_iNumber.
	int m_iNumber;
	string m_sReporterUid;
	//! Real clock when the report was made (System.GetUnixTime).
	int m_iTime;

	//------------------------------------------------------------------------------------------------
	LF_IncidentFlagRow Copy()
	{
		LF_IncidentFlagRow copy = new LF_IncidentFlagRow();
		copy.m_iNumber = m_iNumber;
		copy.m_sReporterUid = m_sReporterUid;
		copy.m_iTime = m_iTime;
		return copy;
	}
}

//------------------------------------------------------------------------------------------------
//! Player reports on incidents (#241, epic #145 Moderation). A player's /report flags their own
//! incidents from the last N minutes (config); administrators see flagged incidents first in the
//! Game Master panel, and a flag stays until an administrator marks it handled (!handled <number>).
//! Server only, never replicated. A per-player cooldown (config) refuses report spam.
//! PERSISTENCE: the "incidentflags" payload (save version 17). ExportAll / ApplyAll; ApplyAll
//! replaces the list so a second load converges. The cooldown clock is not saved.
//------------------------------------------------------------------------------------------------
class LF_IncidentFlags
{
	static const string FEATURE = "IncidentReport";

	static const int REPORT_OK = 0;
	static const int REPORT_COOLDOWN = 1;
	static const int REPORT_NOTHING = 2;

	protected static ref array<ref LF_IncidentFlagRow> s_aRows;
	protected static ref map<string, int> s_mLastReport;

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_IncidentFlagRow> Rows()
	{
		if (!s_aRows)
			s_aRows = new array<ref LF_IncidentFlagRow>();

		return s_aRows;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aRows = null;
		s_mLastReport = null;
	}

	//------------------------------------------------------------------------------------------------
	static int Count()
	{
		return Rows().Count();
	}

	//------------------------------------------------------------------------------------------------
	static bool IsFlagged(int number)
	{
		foreach (LF_IncidentFlagRow row : Rows())
		{
			if (row.m_iNumber == number)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: is this incident one of the reporter's, from inside the window?
	static bool IsReportable(notnull LF_IncidentRecord record, string reporterUid, int now, int windowSeconds)
	{
		if (reporterUid.IsEmpty() || record.m_iTime < now - windowSeconds || record.m_iTime > now)
			return false;

		return record.m_sSubjectUid == reporterUid || record.m_sOtherUid == reporterUid;
	}

	//------------------------------------------------------------------------------------------------
	//! A player reports. Flags every incident of theirs from the window that is not flagged yet.
	//! \param[out] flagged How many incidents this report flagged.
	//! \return REPORT_OK, REPORT_COOLDOWN (too soon after their last report), or REPORT_NOTHING
	//! (nothing of theirs in the window; the cooldown does not start).
	static int Report(string reporterUid, int now, out int flagged)
	{
		flagged = 0;
		if (reporterUid.IsEmpty())
			return REPORT_NOTHING;

		if (!s_mLastReport)
			s_mLastReport = new map<string, int>();

		int last;
		if (s_mLastReport.Find(reporterUid, last) && now - last < LF_IncidentConfig.ReportCooldownSeconds())
		{
			LF_Debug.Warn(FEATURE, string.Format("report from %1 refused: cooldown, %2 s since the last", reporterUid, now - last));
			return REPORT_COOLDOWN;
		}

		int window = LF_IncidentConfig.ReportWindowSeconds();
		array<ref LF_IncidentRecord> recent = LF_IncidentLog.Query(vector.Zero, -1, now - window, now, "");
		foreach (LF_IncidentRecord record : recent)
		{
			if (!IsReportable(record, reporterUid, now, window) || IsFlagged(record.m_iNumber))
				continue;

			LF_IncidentFlagRow row = new LF_IncidentFlagRow();
			row.m_iNumber = record.m_iNumber;
			row.m_sReporterUid = reporterUid;
			row.m_iTime = now;
			Rows().Insert(row);
			flagged++;
		}

		if (flagged == 0)
		{
			LF_Debug.Info(FEATURE, string.Format("report from %1: nothing of theirs in the last %2 s", reporterUid, window));
			return REPORT_NOTHING;
		}

		s_mLastReport.Set(reporterUid, now);
		LF_Debug.Info(FEATURE, string.Format("report from %1 flagged %2 incident(s)", reporterUid, flagged));
		return REPORT_OK;
	}

	//------------------------------------------------------------------------------------------------
	//! An administrator marks an incident handled. Removes its flags and logs who did it.
	//! \return true when it was flagged.
	static bool Handle(int number, string adminUid)
	{
		bool found;
		for (int i = Rows().Count() - 1; i >= 0; i--)
		{
			if (Rows()[i].m_iNumber != number)
				continue;

			LF_Debug.Info(FEATURE, string.Format("incident #%1 flagged by %2 marked handled by %3", number, Rows()[i].m_sReporterUid, adminUid));
			Rows().Remove(i);
			found = true;
		}

		if (!found)
			LF_Debug.Warn(FEATURE, string.Format("handled #%1 by %2 ignored: not flagged", number, adminUid));

		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops flags whose incident is gone from the log (trimmed) or is newer than the flag (the
	//! number was reused after the log emptied).
	//! \return How many flags were dropped.
	static int Prune()
	{
		int dropped;
		for (int i = Rows().Count() - 1; i >= 0; i--)
		{
			LF_IncidentRecord record = LF_IncidentLog.Find(Rows()[i].m_iNumber);
			if (record && record.m_iTime <= Rows()[i].m_iTime)
				continue;

			LF_Debug.Info(FEATURE, string.Format("flag on incident #%1 dropped: the incident left the log", Rows()[i].m_iNumber));
			Rows().Remove(i);
			dropped++;
		}

		return dropped;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: copies of every flag.
	static array<ref LF_IncidentFlagRow> ExportAll()
	{
		array<ref LF_IncidentFlagRow> result = new array<ref LF_IncidentFlagRow>();
		foreach (LF_IncidentFlagRow row : Rows())
			result.Insert(row.Copy());

		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: replaces the flags (idempotent). Rows with no number are skipped.
	static void ApplyAll(array<ref LF_IncidentFlagRow> records)
	{
		s_aRows = new array<ref LF_IncidentFlagRow>();
		if (!records)
			return;

		foreach (LF_IncidentFlagRow record : records)
		{
			if (!record || record.m_iNumber <= 0)
			{
				LF_Debug.Warn(FEATURE, "load skipped a flag row with no incident number");
				continue;
			}

			s_aRows.Insert(record.Copy());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a player asked to report.
	static void ServerReport(int playerId)
	{
		if (!Replication.IsServer())
			return;

		int flagged;
		int result = Report(LF_Utils.GetPlayerUID(playerId), System.GetUnixTime(), flagged);
		if (result == REPORT_COOLDOWN)
		{
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-IncidentReport_Title", "#LF-IncidentReport_Cooldown", 5.0, LF_ENotificationType.WARNING);
			return;
		}

		if (result == REPORT_NOTHING)
		{
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-IncidentReport_Title", "#LF-IncidentReport_Nothing", 5.0, LF_ENotificationType.INFO);
			return;
		}

		LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-IncidentReport_Title", "#LF-IncidentReport_Sent", 5.0, LF_ENotificationType.SUCCESS);
		array<int> ids = {};
		GetGame().GetPlayerManager().GetPlayers(ids);
		foreach (int id : ids)
		{
			if (AdminChatChannel.IsAdminPlayer(id))
				LF_NotificationManagerComponent.NotifyPlayer(id, "#LF-IncidentReport_Title", "#LF-IncidentReport_Staff", 8.0, LF_ENotificationType.WARNING, flagged.ToString());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: "!handled 12" gives 12; false for anything else.
	static bool TryParseHandled(string message, out int number)
	{
		array<string> parts = new array<string>();
		message.Split(" ", parts, true);
		if (parts.Count() != 2 || parts[0] != "!handled")
			return false;

		int parsed = parts[1].ToInt();
		if (parsed <= 0)
			return false;

		number = parsed;
		return true;
	}
}
