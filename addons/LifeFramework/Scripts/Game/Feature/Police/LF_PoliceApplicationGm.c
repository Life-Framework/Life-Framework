//------------------------------------------------------------------------------------------------
//! Staff review of police applications from Game Master (#1288, M4 #1006). The server half: a
//! request from a client is checked here and only then answered through LF_PoliceApplicationStaff.
//! The client half is LF_PoliceApplicationGmPanel (UI/GM), opened with the /applications chat
//! command, in or out of Game Master.
//!
//! What the client may send: nothing to list, and a verb plus a target account uid to decide. Who
//! asks is read from the player controller the RPC arrived on, never from the payload; staff is
//! re-checked on every call (the roster file or the engine administrator flag). Any staff level may
//! list and read (moderators read the incident panel too); approve and reject need the roster level
//! `administrator` or the engine administrator flag, since only administrators hold in-game powers
//! (#313, #1320). The verb must be
//! one of the two below and the target uid is validated by LF_PoliceApplicationStaff.
//!
//! Row (pipe separated): uid|name|why|availability|waitedSeconds. Free text has the separator and a
//! leading # (a localization key) taken out, so a form field cannot forge a row or a string.
//------------------------------------------------------------------------------------------------
class LF_PoliceApplicationGm : LF_PoliceApplicationStaff
{
	static const string FEATURE = "PoliceAppGM";

	static const int VERB_APPROVE = 1;
	static const int VERB_REJECT = 2;

	static const int MAX_ROWS = 30;
	static const string LEVEL_ADMINISTRATOR = "administrator";
	static const int MAX_TEXT = 160;	// lf-fixed: a row must fit the sheet's detail line, not a server tunable
	static const string SEP = "|";
	//! A client that asks faster than this is dropped (each ask reads the roster file).
	static const int MIN_ASK_INTERVAL_MS = 300;	// lf-fixed: flood guard on the RPC, not a gameplay tunable

	protected static ref map<int, int> s_mLastAskMs = new map<int, int>();
	protected static int s_iLevelRefusals;

	//------------------------------------------------------------------------------------------------
	//! How many decisions were refused (and logged) because the caller's level is below administrator.
	static int LevelRefusals()
	{
		return s_iLevelRefusals;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: wire rows for the open applications, oldest first, at most MAX_ROWS.
	static array<string> BuildRows(notnull array<ref LF_PoliceApplication> apps, int now)
	{
		array<string> rows = new array<string>();
		foreach (LF_PoliceApplication app : apps)
		{
			if (rows.Count() >= MAX_ROWS)
				break;

			int waited = now - app.m_iTime;
			if (waited < 0)
				waited = 0;

			rows.Insert(string.Format("%1|%2|%3|%4|%5", Clean(app.m_sUid), Clean(app.m_sName), Clean(app.m_sWhy), Clean(app.m_sAvailability), waited));
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the list answer. A non-staff caller gets no rows and the refusal key.
	static array<string> AnswerList(string callerUid, bool engineAdmin, int now, out string refusal)
	{
		array<ref LF_PoliceApplication> apps = LF_PoliceApplicationStaff.ListOpen(callerUid, engineAdmin, refusal);
		if (!refusal.IsEmpty())
			return new array<string>();

		return BuildRows(apps, now);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the roster level of uid (lower case), "" when the uid is not on the roster.
	static string LevelOf(string uid)
	{
		if (uid.IsEmpty())
			return "";

		string path = GetStaffPath();
		if (!FileIO.FileExists(path))
			return "";

		FileHandle handle = FileIO.OpenFile(path, FileMode.READ);
		if (!handle)
		{
			LF_Debug.Error(FEATURE, string.Format("cannot open the staff roster: %1", path));
			return "";
		}

		string found;
		string line;
		while (handle.ReadLine(line) >= 0)
		{
			string lineUid;
			string level;
			if (ParseRosterLine(line, lineUid, level) && lineUid == uid)
			{
				level.ToLower();
				found = level;
				break;
			}
		}
		handle.Close();
		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: whether the caller may approve or reject (in-game powers): the engine administrator
	//! flag, or the roster level administrator.
	static bool MayDecide(string callerUid, bool engineAdmin)
	{
		if (engineAdmin)
			return true;

		return LevelOf(callerUid) == LEVEL_ADMINISTRATOR;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one approve or reject. Returns "" when it went through, else the refusal key.
	//! \param[out] targetName The applicant's name for the answer line, "" when refused.
	static string AnswerDecision(string callerUid, bool engineAdmin, int verb, string targetUid, int now, out string targetName)
	{
		targetName = "";
		if (verb != VERB_APPROVE && verb != VERB_REJECT)
		{
			LF_Debug.Warn(FEATURE, string.Format("decision by %1 refused: unknown verb %2", callerUid, verb));
			return "#LF-PoliceAppGm_BadRequest";
		}

		if (!IsStaff(callerUid, engineAdmin))
		{
			LF_Debug.Warn(FEATURE, string.Format("decision by %1 refused: not staff", callerUid));
			return "#LF-PoliceApp_NotAdmin";
		}

		if (!MayDecide(callerUid, engineAdmin))
		{
			s_iLevelRefusals++;
			LF_Debug.Warn(FEATURE, string.Format("decision by %1 refused: roster level '%2' is not %3", callerUid, LevelOf(callerUid), LEVEL_ADMINISTRATOR));
			return "#LF-PoliceAppGm_NeedAdministrator";
		}

		string name;
		if (IsValidUid(targetUid))
		{
			LF_PoliceApplication open = LF_PoliceApplications.FindOpen(targetUid);
			if (open)
				name = Clean(open.m_sName);
		}

		string refusal;
		if (verb == VERB_APPROVE)
			refusal = LF_PoliceApplicationStaff.ApproveByUid(callerUid, engineAdmin, targetUid);
		else
			refusal = LF_PoliceApplicationStaff.RejectByUid(callerUid, engineAdmin, targetUid, now);

		if (refusal.IsEmpty())
			targetName = name;

		return refusal;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: false when this player asked less than MIN_ASK_INTERVAL_MS ago.
	static bool TakeAskSlot(int playerId, int nowMs)
	{
		int last;
		if (s_mLastAskMs.Find(playerId, last) && nowMs - last >= 0 && nowMs - last < MIN_ASK_INTERVAL_MS)
		{
			LF_Debug.Warn(FEATURE, string.Format("dropped a request from player %1: asked again within %2 ms", playerId, MIN_ASK_INTERVAL_MS));
			return false;
		}

		s_mLastAskMs.Set(playerId, nowMs);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: called by the list RPC. The caller is the controller the RPC arrived on.
	static void ServerList(SCR_PlayerController controller)
	{
		if (!Replication.IsServer() || !controller)
			return;

		int playerId = controller.GetPlayerId();
		if (!TakeAskSlot(playerId, System.GetTickCount()))
			return;

		Reply(controller, "", "");
	}

	//------------------------------------------------------------------------------------------------
	//! Server: called by the decide RPC. Answers with the result and the fresh list.
	static void ServerDecide(SCR_PlayerController controller, int verb, string targetUid)
	{
		if (!Replication.IsServer() || !controller)
			return;

		int playerId = controller.GetPlayerId();
		if (!TakeAskSlot(playerId, System.GetTickCount()))
			return;

		string name;
		string refusal = AnswerDecision(LF_Utils.GetPlayerUID(playerId), AdminChatChannel.IsAdminPlayer(playerId), verb, targetUid, System.GetUnixTime(), name);
		string result = refusal;
		if (refusal.IsEmpty() && verb == VERB_APPROVE)
			result = "#LF-PoliceAppGm_Approved";
		else if (refusal.IsEmpty())
			result = "#LF-PoliceAppGm_Rejected";

		Reply(controller, result, name);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Reply(SCR_PlayerController controller, string result, string resultArg)
	{
		int playerId = controller.GetPlayerId();
		string refusal;
		array<string> rows = AnswerList(LF_Utils.GetPlayerUID(playerId), AdminChatChannel.IsAdminPlayer(playerId), System.GetUnixTime(), refusal);
		if (!refusal.IsEmpty())
		{
			result = refusal;
			resultArg = "";
		}
		else
		{
			LF_Debug.Info(FEATURE, string.Format("sent %1 open application(s) to staff player %2", rows.Count(), playerId));
		}

		controller.LF_DeliverPoliceAppGm(rows, result, resultArg);
	}

	//------------------------------------------------------------------------------------------------
	//! Free text for a row: no separator, no leading # (a string key), at most MAX_TEXT characters.
	static string Clean(string text)
	{
		text.Replace(SEP, " ");
		text.Replace("\n", " ");
		text = text.Trim();
		while (!text.IsEmpty() && text.Get(0) == "#")
			text = text.Substring(1, text.Length() - 1);

		if (text.Length() > MAX_TEXT)
			text = text.Substring(0, MAX_TEXT);

		return text;
	}
}

//------------------------------------------------------------------------------------------------
//! The /applications chat command opens the panel; the server decides what it shows.
[BaseContainerProps(configRoot: true)]
modded class SCR_ChatPanelManager
{
	//------------------------------------------------------------------------------------------------
	override protected void InitDefaultChatCommands()
	{
		super.InitDefaultChatCommands();
		ChatCommandInvoker invoker = GetCommandInvoker("applications");
		if (invoker)
			invoker.Insert(LF_OnPoliceApplicationsCommand);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_OnPoliceApplicationsCommand(SCR_ChatPanel panel, string data)
	{
		LF_PoliceApplicationGmPanel.Open();
	}
}

//------------------------------------------------------------------------------------------------
//! The RPC bridge on the player controller (the one entity every client owns).
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Client: ask the server for the open applications.
	void LF_AskPoliceAppGmList()
	{
		if (Replication.IsServer())
			RpcAsk_LF_PoliceAppGmList();
		else
			Rpc(RpcAsk_LF_PoliceAppGmList);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: approve or reject one application (LF_PoliceApplicationGm.VERB_*).
	void LF_AskPoliceAppGmDecide(int verb, string targetUid)
	{
		if (Replication.IsServer())
			RpcAsk_LF_PoliceAppGmDecide(verb, targetUid);
		else
			Rpc(RpcAsk_LF_PoliceAppGmDecide, verb, targetUid);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: send this player the rows and the result of their last request.
	void LF_DeliverPoliceAppGm(array<string> rows, string result, string resultArg)
	{
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_PoliceAppGm(rows, result, resultArg);
		else
			Rpc(RpcDo_LF_PoliceAppGm, rows, result, resultArg);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_PoliceAppGmList()
	{
		LF_PoliceApplicationGm.ServerList(this);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_PoliceAppGmDecide(int verb, string targetUid)
	{
		LF_PoliceApplicationGm.ServerDecide(this, verb, targetUid);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_PoliceAppGm(array<string> rows, string result, string resultArg)
	{
		LF_PoliceApplicationGmPanel.OnAnswer(rows, result, resultArg);
	}
}
