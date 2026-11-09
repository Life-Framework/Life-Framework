//------------------------------------------------------------------------------------------------
//! Game Master incident feed (#240, epic #145 Moderation). The server half: an administrator's
//! request is checked here, and only then are incidents read from LF_IncidentLog and sent as flat
//! rows to that one client. A non-admin gets nothing and a logged refusal; the log itself is never
//! replicated. The client half is LF_IncidentGmPanel (Game Master EDIT mode).
//!
//! The client sends only where its camera is. Radius, time window and row cap are fixed here, so a
//! client cannot widen what it is allowed to read.
//!
//! Row (pipe separated): number|kind|ageSeconds|x|z|subject|other|amount|how|zone|murder|flagged
//! Flagged rows (#241 player reports) are sent even when they are outside the radius or window.
//------------------------------------------------------------------------------------------------
class LF_IncidentGm
{
	static const string FEATURE = "IncidentGM";

	static const float RADIUS = 400;
	static const int WINDOW_SECONDS = 3600;
	static const int MAX_ROWS = 40;
	static const int MAX_FLAGGED_ROWS = 20;
	static const string SEP = "|";

	//------------------------------------------------------------------------------------------------
	//! Pure: the rows for an administrator's view around center, newest last, at most MAX_ROWS
	//! (the newest kept). Names are resolved by uid.
	static array<string> BuildRows(notnull array<ref LF_IncidentRecord> records, int now)
	{
		array<string> rows = new array<string>();
		int first = 0;
		if (records.Count() > MAX_ROWS)
			first = records.Count() - MAX_ROWS;

		for (int i = first; i < records.Count(); i++)
		{
			LF_IncidentRecord r = records[i];
			string head = string.Format("%1|%2|%3|%4|%5|", r.m_iNumber, r.m_sKind, now - r.m_iTime, Math.Round(r.m_fX), Math.Round(r.m_fZ));
			string tail = string.Format("%1|%2|%3|%4|%5|%6|%7", NameOfUid(r.m_sSubjectUid), NameOfUid(r.m_sOtherUid), r.m_iAmount, Clean(r.m_sHow), r.m_sZone, Bit(r.m_bMurder), Bit(LF_IncidentFlags.IsFlagged(r.m_iNumber)));
			rows.Insert(head + tail);
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! The server's answer to a request. isAdmin is decided by the caller from the server's own
	//! role table. A refusal returns an empty list and logs; it never throws.
	static array<string> Answer(bool isAdmin, int playerId, vector center, int now)
	{
		if (!isAdmin)
		{
			LF_Debug.Warn(FEATURE, string.Format("refused incident request from player %1: not an administrator", playerId));
			return new array<string>();
		}

		array<ref LF_IncidentRecord> found = LF_IncidentLog.Query(center, RADIUS, now - WINDOW_SECONDS, -1, "");
		LF_IncidentFlags.Prune();
		array<ref LF_IncidentRecord> flaggedFar = new array<ref LF_IncidentRecord>();
		array<ref LF_IncidentRecord> everything = LF_IncidentLog.Query(vector.Zero, -1, 0, -1, "");
		for (int i = everything.Count() - 1; i >= 0 && flaggedFar.Count() < MAX_FLAGGED_ROWS; i--)
		{
			if (LF_IncidentFlags.IsFlagged(everything[i].m_iNumber) && !IsInside(found, everything[i].m_iNumber))
				flaggedFar.InsertAt(everything[i], 0);
		}

		array<string> rows = BuildRows(flaggedFar, now);
		rows.InsertAll(BuildRows(found, now));
		LF_Debug.Info(FEATURE, string.Format("sent %1 incident row(s) (%4 flagged outside the view) to admin %2 around %3", rows.Count(), playerId, center, flaggedFar.Count()));
		return rows;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsInside(notnull array<ref LF_IncidentRecord> records, int number)
	{
		foreach (LF_IncidentRecord record : records)
		{
			if (record.m_iNumber == number)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: answer a request from this controller; called by the Ask RPC.
	static void ServerRead(SCR_PlayerController controller, vector center)
	{
		if (!Replication.IsServer() || !controller)
			return;

		int playerId = controller.GetPlayerId();
		array<string> rows = Answer(AdminChatChannel.IsAdminPlayer(playerId), playerId, center, System.GetUnixTime());
		controller.LF_DeliverIncidents(rows);
	}

	//------------------------------------------------------------------------------------------------
	//! Display name of a connected player by uid; the uid itself when they are offline; empty for none.
	static string NameOfUid(string uid)
	{
		if (uid.IsEmpty())
			return "";

		PlayerManager players = GetGame().GetPlayerManager();
		if (players)
		{
			array<int> ids = {};
			players.GetPlayers(ids);
			foreach (int id : ids)
			{
				if (LF_Utils.GetPlayerUID(id) == uid)
					return Clean(players.GetPlayerName(id));
			}
		}

		return Clean(uid);
	}

	//------------------------------------------------------------------------------------------------
	protected static string Bit(bool value)
	{
		if (value)
			return "1";

		return "0";
	}

	//------------------------------------------------------------------------------------------------
	protected static string Clean(string text)
	{
		text.Replace(SEP, " ");
		return text;
	}
}

//------------------------------------------------------------------------------------------------
//! Game Master bridge on the player controller (the one entity every client owns).
modded class SCR_PlayerController
{
	//! Rows from the last answer; the Game Master panel reads them. Client only.
	protected static ref array<string> s_aLF_IncidentRows = new array<string>();
	protected static int s_iLF_IncidentAnswers;

	//------------------------------------------------------------------------------------------------
	static array<string> LF_GetIncidentRows()
	{
		return s_aLF_IncidentRows;
	}

	//------------------------------------------------------------------------------------------------
	static int LF_GetIncidentAnswers()
	{
		return s_iLF_IncidentAnswers;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: ask the server for incidents around center. The server decides if we may know.
	void LF_AskIncidents(vector center)
	{
		if (Replication.IsServer())
			RpcAsk_LF_Incidents(center);
		else
			Rpc(RpcAsk_LF_Incidents, center);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: send this player the rows. Empty rows still answer, so the panel can say "none".
	void LF_DeliverIncidents(array<string> rows)
	{
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_Incidents(rows);
		else
			Rpc(RpcDo_LF_Incidents, rows);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the /report chat command (#241). The server decides what it flags and if the
	//! cooldown allows it.
	void LF_AskReport()
	{
		if (Replication.IsServer())
			RpcAsk_LF_Report();
		else
			Rpc(RpcAsk_LF_Report);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_Report()
	{
		LF_IncidentFlags.ServerReport(GetPlayerId());
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_Incidents(vector center)
	{
		LF_IncidentGm.ServerRead(this, center);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_Incidents(array<string> rows)
	{
		s_aLF_IncidentRows = new array<string>();
		s_aLF_IncidentRows.Copy(rows);
		s_iLF_IncidentAnswers++;
	}
}
