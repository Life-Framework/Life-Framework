//------------------------------------------------------------------------------------------------
enum LF_EServiceTake
{
	TAKEN,
	NO_CALL,
	ALREADY_TAKEN,
	ALREADY_YOURS,
	NOT_ON_DUTY
}

//------------------------------------------------------------------------------------------------
//! Taking a call (#251). A responder on duty for the service takes the oldest call on its board
//! that has room (LF_ServiceDef.m_iMaxTakers per call). The server validates and publishes
//! LF_OnServiceCallTaken; the board marks the call taken and LF_ServiceCallTakenNotifier tells the
//! caller. Entry points: LF_ServiceTakeAction on the board.
class LF_ServiceTake
{
	//------------------------------------------------------------------------------------------------
	//! Server: validate and perform a take for a player, then publish LF_OnServiceCallTaken.
	//! Only a player the service reaches right now (on duty) may take.
	static LF_EServiceTake ServerTake(int playerId, string takerUid, string service, array<ref LF_ServiceCandidate> players = null)
	{
		if (!Replication.IsServer())
			return LF_EServiceTake.NO_CALL;

		// A strong local: assigning to the parameter would free a fresh array on return.
		ref array<ref LF_ServiceCandidate> candidates = players;
		if (!candidates)
			candidates = LF_ServiceCalls.Candidates();

		LF_ServiceCandidate taker;
		foreach (LF_ServiceCandidate candidate : candidates)
		{
			if (candidate.m_iPlayerId == playerId)
				taker = candidate;
		}

		if (!LF_ServiceBoard.MayRead(service, LF_ServiceCallsConfig.Services(), taker))
		{
			LF_Debug.Warn(LF_ServiceBoard.FEATURE, string.Format("take refused for player %1 on the %2 board: not on duty for it", playerId, service));
			return LF_EServiceTake.NOT_ON_DUTY;
		}

		LF_ServiceBoardCall taken;
		LF_EServiceTake result = LF_ServiceBoard.Take(service, takerUid, LF_ServiceCallsConfig.MaxTakers(service), System.GetTickCount(), taken);
		if (result == LF_EServiceTake.TAKEN)
			LF_EventBus.Publish(LF_OnServiceCallTaken.Create(taken.m_sCallerUid, service, takerUid, taken.m_sPlace, taken.m_sSummary, taken.m_aTakers.Count()));
		else
			LF_Debug.Info(LF_ServiceBoard.FEATURE, string.Format("take by player %1 on the %2 board: %3", playerId, service, typename.EnumToString(LF_EServiceTake, result)));

		return result;
	}

	//------------------------------------------------------------------------------------------------
	static string ResultKey(LF_EServiceTake result)
	{
		switch (result)
		{
			case LF_EServiceTake.TAKEN:
				return "#LF-Dispatch_TakeDone";
			case LF_EServiceTake.ALREADY_TAKEN:
				return "#LF-Dispatch_TakeGone";
			case LF_EServiceTake.ALREADY_YOURS:
				return "#LF-Dispatch_TakeYours";
			case LF_EServiceTake.NOT_ON_DUTY:
				return "#LF-Dispatch_ServiceClosed";
		}
		return "#LF-Dispatch_TakeNone";
	}

	//------------------------------------------------------------------------------------------------
	static int FindPlayerId(string uid)
	{
		if (uid.IsEmpty() || !GetGame() || !GetGame().GetPlayerManager())
			return -1;

		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			if (LF_Utils.GetPlayerUID(playerId) == uid)
				return playerId;
		}
		return -1;
	}
}

//------------------------------------------------------------------------------------------------
//! Bus subscriber: tells the caller, if still connected, that a unit is on the way. The caller is
//! found by player id and told by an owner RPC on their own controller (LF_DeliverNotification);
//! no entity id is sent.
class LF_ServiceCallTakenNotifier : LF_EventSubscriber
{
	static ref LF_ServiceCallTakenNotifier s_Subscriber;

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnServiceCallTaken taken = LF_OnServiceCallTaken.Cast(busEvent);
		if (!taken)
			return false;

		int playerId = LF_ServiceTake.FindPlayerId(taken.m_sCallerUid);
		if (playerId <= 0)
		{
			LF_Debug.Info(LF_ServiceBoard.FEATURE, string.Format("caller %1 is gone: the %2 unit is on the way, nobody to tell", taken.m_sCallerUid, taken.m_sService));
			return true;
		}

		LF_Debug.Info(LF_ServiceBoard.FEATURE, string.Format("telling caller (player %1): the %2 unit is on the way", playerId, taken.m_sService));
		LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Service_UnitTitle", "#LF-Service_UnitOnTheWay", 6.0);
		return true;
	}

	static void Install()
	{
		if (!Replication.IsServer())
			return;

		if (s_Subscriber)
			LF_EventBus.Unsubscribe(LF_OnServiceCallTaken, s_Subscriber);

		s_Subscriber = new LF_ServiceCallTakenNotifier();
		LF_EventBus.Subscribe(LF_OnServiceCallTaken, s_Subscriber);
	}

	static void ResetStatics()
	{
		s_Subscriber = null;
	}
}

//------------------------------------------------------------------------------------------------
//! "Take call" on a service's dispatch board. The server validates everything (on duty for the
//! service, standing at the board, a call with room); the client only reaches.
class LF_ServiceTakeAction : ScriptedUserAction
{
	[Attribute("", UIWidgets.EditBox, "Service key whose board this is (police, medic, ...)")]
	protected string m_sService;

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-Dispatch_Take";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer() || !pOwnerEntity || !pUserEntity)
			return;

		if (vector.Distance(pOwnerEntity.GetOrigin(), pUserEntity.GetOrigin()) > LF_PoliceConfig.BoardReadDistance())
		{
			LF_Debug.Warn(LF_ServiceBoard.FEATURE, "take refused: user is not at the board");
			return;
		}

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);
		if (playerId <= 0)
		{
			LF_Debug.Error(LF_ServiceBoard.FEATURE, "take refused: no player behind the user");
			return;
		}

		LF_EServiceTake result = LF_ServiceTake.ServerTake(playerId, LF_Utils.GetPlayerUID(playerId), m_sService);
		LF_Utils.NotifyEntity(pUserEntity, LF_ServiceTake.ResultKey(result), "#LF-Service_UnitTitle", 4.0);
	}
}
