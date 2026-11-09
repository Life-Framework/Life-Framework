//------------------------------------------------------------------------------------------------
//! One call pinned to a service's dispatch board. Session-local: calls are minutes old by nature
//! and a restart clears them.
class LF_ServiceBoardCall : Managed
{
	int m_iSequence;
	string m_sService;
	string m_sSummary;
	string m_sPlace;
	//! In-game clock at the call, "HH:MM".
	string m_sTime;
	//! System tick (ms) after which the call leaves the board.
	int m_iExpiresMs;
}

//------------------------------------------------------------------------------------------------
//! The dispatch board at each service's station (#250). A bus subscriber to LF_OnServiceCall pins
//! every call whose channels include "board" to that service's list; a call stays until it expires
//! (m_iBoardExpirySeconds). Reading a board asks the server, which answers per read: the calls
//! for a reader the service would reach right now (on-duty police, a current medic), a closed
//! board for everyone else. Nothing is replicated continuously, so a late joiner reads the
//! current calls. The wanted board is the police service's board (LF_WantedBoard); the
//! `LF_WantedBoardReadAction.m_sService` attribute turns the same world board into another service's.
class LF_ServiceBoard
{
	static const string FEATURE = "ServiceBoard";
	static const string FIELD_SEPARATOR = "|";
	static const string CHANNEL = "board";

	protected static ref array<ref LF_ServiceBoardCall> s_aCalls;
	protected static int s_iSequence;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aCalls = null;
		s_iSequence = 0;
		LF_ServiceBoardSubscriber.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: subscribe the board to service calls (idempotent, also after a bus reset).
	static void Install()
	{
		LF_ServiceBoardSubscriber.Install();
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: does this channel list include the board?
	static bool HasBoardChannel(string channels)
	{
		array<string> list = {};
		channels.Split(",", list, true);
		return list.Find(CHANNEL) >= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: pin a call to its service's board. Calls without the board channel are ignored.
	//! \return The pinned call, or null.
	static LF_ServiceBoardCall Add(notnull LF_OnServiceCall call, int nowMs)
	{
		if (!HasBoardChannel(call.m_sChannels))
			return null;

		if (!s_aCalls)
			s_aCalls = {};

		PurgeExpired(nowMs);
		LF_ServiceBoardCall pinned = new LF_ServiceBoardCall();
		s_iSequence++;
		pinned.m_iSequence = s_iSequence;
		pinned.m_sService = call.m_sService;
		pinned.m_sSummary = call.m_sSummary;
		pinned.m_sPlace = call.m_sPlace;
		pinned.m_sTime = LF_Dispatch.ClockText();
		pinned.m_iExpiresMs = nowMs + LF_ServiceCallsConfig.BoardExpirySeconds() * 1000;
		s_aCalls.Insert(pinned);
		LF_Debug.Info(FEATURE, string.Format("call #%1 pinned to the %2 board: '%3' at '%4'", pinned.m_iSequence, pinned.m_sService, pinned.m_sSummary, pinned.m_sPlace));
		return pinned;
	}

	//------------------------------------------------------------------------------------------------
	protected static void PurgeExpired(int nowMs)
	{
		if (!s_aCalls)
			return;

		for (int i = s_aCalls.Count() - 1; i >= 0; i--)
		{
			if (nowMs - s_aCalls[i].m_iExpiresMs >= 0)
			{
				LF_Debug.Info(FEATURE, string.Format("call #%1 expired from the %2 board", s_aCalls[i].m_iSequence, s_aCalls[i].m_sService));
				s_aCalls.Remove(i);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the live calls on a service's board, newest first, as "time|summary|place" entries.
	static int BuildEntries(string service, int nowMs, int maxLines, notnull array<string> outEntries)
	{
		outEntries.Clear();
		PurgeExpired(nowMs);
		if (!s_aCalls)
			return 0;

		for (int i = s_aCalls.Count() - 1; i >= 0; i--)
		{
			if (maxLines > 0 && outEntries.Count() >= maxLines)
				break;

			LF_ServiceBoardCall call = s_aCalls[i];
			if (call.m_sService == service)
				outEntries.Insert(string.Format("%1|%2|%3", call.m_sTime, call.m_sSummary, call.m_sPlace));
		}

		return outEntries.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: may this player read the service's board? Only the service's own on-duty people.
	static bool MayRead(string service, notnull array<ref LF_ServiceDef> services, LF_ServiceCandidate reader)
	{
		if (!reader)
			return false;

		LF_ServiceDef def = LF_ServiceCalls.Find(services, service);
		if (!def)
			return false;

		return LF_ServiceCalls.Receives(def, reader);
	}

	//------------------------------------------------------------------------------------------------
	//! Server body of the read RPC. The reader must stand at a board; the entries go only to a
	//! reader the service reaches, everyone else gets a closed board.
	static void ServerRead(SCR_PlayerController controller, string service)
	{
		if (!controller)
			return;

		int playerId = controller.GetPlayerId();
		IEntity body = controller.GetControlledEntity();
		if (!body || !LF_ActionSite.IsNear(body.GetOrigin(), LF_PoliceConfig.BoardReadDistance(), LF_WantedBoard.READ_ACTION))
		{
			LF_Debug.Warn(FEATURE, string.Format("board read refused for player %1: not at a board", playerId));
			return;
		}

		LF_ServiceCandidate reader;
		array<ref LF_ServiceCandidate> candidates = LF_ServiceCalls.Candidates();
		foreach (LF_ServiceCandidate candidate : candidates)
		{
			if (candidate.m_iPlayerId == playerId)
				reader = candidate;
		}

		array<string> entries = {};
		bool open = MayRead(service, LF_ServiceCallsConfig.Services(), reader);
		if (open)
			BuildEntries(service, System.GetTickCount(), LF_PoliceConfig.DispatchLogLines() * 2, entries);

		LF_Debug.Info(FEATURE, string.Format("%1 board read by player %2: open=%3 calls=%4", service, playerId, open, entries.Count()));
		controller.LF_ShowServiceBoard(service, open, entries);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: one entry as a line: "21:14  Emergency reported at Village payphone".
	static string EntryLine(string entry)
	{
		array<string> fields = {};
		entry.Split(FIELD_SEPARATOR, fields, false);
		if (fields.Count() < 3)
		{
			LF_Debug.Error(FEATURE, string.Format("board entry dropped: '%1'", entry));
			return string.Empty;
		}

		string place = fields[2];
		for (int i = 3; i < fields.Count(); i++)
		{
			place += FIELD_SEPARATOR + fields[i];
		}

		if (place.IsEmpty())
			place = WidgetManager.Translate("#LF-Dispatch_UnknownPlace");
		else
			place = LF_WantedBoard.ResolvePlace(place);

		return WidgetManager.Translate("#LF-Dispatch_ServiceLine", fields[0], LF_WantedBoard.ResolveKey(fields[1]), place);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the lines of the card (no UI), so a test reads what a player would see.
	static array<ref LF_UIPart> BuildParts(string service, bool open, notnull array<string> entries)
	{
		array<ref LF_UIPart> parts = {};
		string name = service;
		LF_ServiceDef def = LF_ServiceCalls.Find(LF_ServiceCallsConfig.Services(), service);
		if (def)
			name = WidgetManager.Translate(def.m_sName);

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "Title", WidgetManager.Translate("#LF-Dispatch_ServiceTitle", name)));
		if (!open)
		{
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Closed", WidgetManager.Translate("#LF-Dispatch_ServiceClosed")));
		}
		else if (entries.IsEmpty())
		{
			parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "None", WidgetManager.Translate("#LF-Dispatch_ServiceNone")));
		}
		else
		{
			foreach (int index, string entry : entries)
			{
				string line = EntryLine(entry);
				if (!line.IsEmpty())
					parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "Call" + index, line));
			}
		}

		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "Close", "#LF-Sign_Close"));
		return parts;
	}
}

//------------------------------------------------------------------------------------------------
//! Bus subscriber: pins board-channel calls to their service's board.
class LF_ServiceBoardSubscriber : LF_EventSubscriber
{
	static ref LF_ServiceBoardSubscriber s_Subscriber;

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnServiceCall call = LF_OnServiceCall.Cast(busEvent);
		if (!call)
			return false;

		LF_ServiceBoard.Add(call, System.GetTickCount());
		return true;
	}

	static void Install()
	{
		if (!Replication.IsServer())
			return;

		if (s_Subscriber)
			LF_EventBus.Unsubscribe(LF_OnServiceCall, s_Subscriber);

		s_Subscriber = new LF_ServiceBoardSubscriber();
		LF_EventBus.Subscribe(LF_OnServiceCall, s_Subscriber);
	}

	static void ResetStatics()
	{
		s_Subscriber = null;
	}
}

//------------------------------------------------------------------------------------------------
//! Dispatch board bridge on the player controller.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Client: ask the server for the calls on the service board this player stands at.
	void LF_AskServiceBoard(string service)
	{
		if (Replication.IsServer())
			RpcAsk_LF_ServiceBoard(service);
		else
			Rpc(RpcAsk_LF_ServiceBoard, service);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: send this player the board.
	void LF_ShowServiceBoard(string service, bool open, array<string> entries)
	{
		if (LF_IsLocalController())
			RpcDo_LF_ShowServiceBoard(service, open, entries);
		else
			Rpc(RpcDo_LF_ShowServiceBoard, service, open, entries);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_ServiceBoard(string service)
	{
		LF_ServiceBoard.ServerRead(this, service);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_ShowServiceBoard(string service, bool open, array<string> entries)
	{
		if (!GetGame().GetWorkspace())
		{
			LF_Debug.Error(LF_ServiceBoard.FEATURE, "board card not opened: no workspace on this machine");
			return;
		}

		LF_Debug.Info(LF_ServiceBoard.FEATURE, string.Format("%1 board card: open=%2 calls=%3", service, open, entries.Count()));
		foreach (int index, string entry : entries)
		{
			LF_Debug.Info(LF_ServiceBoard.FEATURE, string.Format("call line %1: %2", index, entry));
		}

		LF_WantedBoard.OpenParts(LF_ServiceBoard.BuildParts(service, open, entries));
	}
}
