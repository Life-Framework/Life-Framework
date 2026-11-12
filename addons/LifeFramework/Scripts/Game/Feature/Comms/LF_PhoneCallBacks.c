//------------------------------------------------------------------------------------------------
//! Call-back requests on the server (#147): the book (LF_PhoneCallBackBook) fed from the world,
//! and the two client requests it answers. Leaving a request and ringing one back are owner RPCs
//! on the player controller; the server checks who asks, where they stand, the offer or the duty,
//! the message length, and the shared comms rate limit (LF_RateLimit, #834). The message is logged
//! with the caller's uid like chat. Nothing is saved.
class LF_PhoneCallBacks
{
	static const string FEATURE = "Phone";

	protected static ref LF_PhoneCallBackBook s_Book;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Book = null;
	}

	//------------------------------------------------------------------------------------------------
	static LF_PhoneCallBackBook Book()
	{
		if (!s_Book)
			s_Book = new LF_PhoneCallBackBook();

		return s_Book;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the toast for a refused request.
	static string RefusalKey(LF_EPhoneRequestRefusal refusal)
	{
		switch (refusal)
		{
			case LF_EPhoneRequestRefusal.NO_OFFER: return "#LF-Phone_NoOffer";
			case LF_EPhoneRequestRefusal.TOO_LONG: return "#LF-Phone_RequestTooLong";
			case LF_EPhoneRequestRefusal.NOT_FOUND: return "#LF-Phone_RequestGone";
			case LF_EPhoneRequestRefusal.CALLING: return "#LF-Phone_RequestCalling";
			case LF_EPhoneRequestRefusal.OK: return string.Empty;
		}
		return "#LF-Payphone_Broken";
	}

	//------------------------------------------------------------------------------------------------
	//! The bus record of a request's life: left, rung back, answered, expired.
	protected static void Publish(notnull LF_PhoneCallBackRequest request, string kind, string officerUid)
	{
		LF_EventBus.Publish(LF_OnPhoneCallBack.Create(kind, request.m_iId, request.m_sService, request.m_sCallerUid, officerUid, request.m_sPlace, request.m_sMessage));
	}

	//------------------------------------------------------------------------------------------------
	//! From LF_PhoneCalls after every call change: a missed call offers its caller a request; a
	//! call-back that was picked up closes its request, one that was not reopens it.
	static void OnCallChanged(notnull LF_PhoneCall call, float now)
	{
		if (call.m_bCallBack)
		{
			bool connected = call.m_eState == LF_EPhoneCallState.CONNECTED;
			LF_PhoneCallBackRequest request = Book().OnCallBack(call.m_iId, connected, !call.IsLive());
			if (request && connected)
			{
				LF_Debug.Info(FEATURE, string.Format("call-back request %1 answered at %2: off the pad", request.m_iId, request.m_sPhoneKey));
				Publish(request, "answered", LF_Utils.GetPlayerUID(call.m_iCallerId));
			}
			else if (request && !call.IsLive())
			{
				LF_Debug.Info(FEATURE, string.Format("call-back request %1 not picked up at %2: back on the pad", request.m_iId, request.m_sPhoneKey));
			}
			return;
		}

		if (call.m_eState != LF_EPhoneCallState.MISSED)
			return;

		int offerSeconds = LF_PhoneConfig.CallBackOfferSeconds();
		Book().Offer(now, call.m_sCallerUid, call.m_iCallerId, call.m_sFromPhone, call.m_sService, call.m_sPlace, call.RangSeconds(), offerSeconds);
		LF_Debug.Info(FEATURE, string.Format("call %1: player %2 may leave a call-back request at %3 for %4 s", call.m_iId, call.m_iCallerId, call.m_sFromPhone, offerSeconds));
	}

	//------------------------------------------------------------------------------------------------
	//! From the tick: expired requests leave the pad; run-out offers leave their line.
	//! \return True when a line may need to drop an offer.
	static bool Purge(float now)
	{
		if (!s_Book)
			return false;

		array<ref LF_PhoneCallBackRequest> expired = {};
		bool offers = s_Book.Purge(now, expired);
		foreach (LF_PhoneCallBackRequest request : expired)
		{
			LF_Debug.Info(FEATURE, string.Format("call-back request %1 from %2 expired unanswered", request.m_iId, request.m_sPlace));
			Publish(request, "expired", string.Empty);
		}
		return offers;
	}

	//------------------------------------------------------------------------------------------------
	//! Player id of whoever holds an offer at this phone; 0 for nobody. The tick purges run-out
	//! offers, and leaving a request checks the time again.
	static int OfferedAt(string phoneKey)
	{
		if (!s_Book)
			return 0;

		return s_Book.OfferedAt(phoneKey);
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from the action: open the message slip for a caller who has an offer here.
	static void ServerOpenSlip(IEntity user, IEntity phoneEntity)
	{
		if (!Replication.IsServer())
			return;

		SCR_PlayerController controller = ControllerOf(user);
		LF_PhoneComponent phone = LF_PhoneComponent.Of(phoneEntity);
		if (!controller || !phone)
		{
			LF_Debug.Error(FEATURE, "message slip refused: no player controller or no phone");
			return;
		}

		int playerId = controller.GetPlayerId();
		string uid = LF_Utils.GetPlayerUID(playerId);
		if (!Book().OfferFor(LF_PhoneCalls.Clock(), uid, phone.Key()))
		{
			LF_Debug.Info(FEATURE, string.Format("message slip refused for player %1 at %2: no offer", playerId, phone.Key()));
			Toast(playerId, "#LF-Phone_NoOffer");
			return;
		}

		controller.LF_PhoneShowSlip(phone.Key(), LF_PhoneConfig.MessageMaxChars());
	}

	//------------------------------------------------------------------------------------------------
	//! Server body of the leave RPC.
	static void ServerLeave(SCR_PlayerController controller, string phoneKey, string message)
	{
		if (!controller)
			return;

		int playerId = controller.GetPlayerId();
		LF_EPhoneRequestRefusal refusal = LeaveAt(LF_PhoneCalls.Clock(), playerId, LF_Utils.GetPlayerUID(playerId), controller.GetControlledEntity(), LF_PhoneComponent.ByKey(phoneKey), message);
		Toast(playerId, RefusalKey(refusal));
	}

	//------------------------------------------------------------------------------------------------
	//! The seam tests drive. players: who is on duty (null reads the connected players).
	static LF_EPhoneRequestRefusal LeaveAt(float now, int playerId, string uid, IEntity body, LF_PhoneComponent phone, string message, array<ref LF_ServiceCandidate> players = null)
	{
		if (!Replication.IsServer())
			return LF_EPhoneRequestRefusal.NO_OFFER;

		if (!phone || !body || uid.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("call-back request refused for player %1: no phone by that key or no body", playerId));
			return LF_EPhoneRequestRefusal.NO_OFFER;
		}

		if (!LF_PhoneCalls.IsNear(body, phone))
		{
			LF_Debug.Warn(FEATURE, string.Format("call-back request refused for player %1: not at phone %2", playerId, phone.Key()));
			return LF_EPhoneRequestRefusal.NO_OFFER;
		}

		// A raw payload far above the limit is refused before any work on it.
		if (message.Length() > LF_PhoneConfig.MessageMaxChars() * 4 + 16)
		{
			LF_Debug.Warn(FEATURE, string.Format("call-back request refused for player %1: %2 raw characters", playerId, message.Length()));
			return LF_EPhoneRequestRefusal.TOO_LONG;
		}

		string clean = LF_PhoneCallBackBook.Clean(message);
		LF_EPhoneRequestRefusal refusal;
		LF_PhoneCallBackRequest request = Book().Leave(now, uid, phone.Key(), clean, LF_Dispatch.ClockText(), LF_PhoneConfig.MessageMaxChars(), LF_ServiceCallsConfig.BoardExpirySeconds(), LF_PhoneConfig.MaxCallBacks(), refusal);
		if (!request)
		{
			LF_Debug.Info(FEATURE, string.Format("call-back request refused for player %1 at %2: %3 (%4 chars)", playerId, phone.Key(), typename.EnumToString(LF_EPhoneRequestRefusal, refusal), clean.Length()));
			return refusal;
		}

		// Logged like chat, with the uid, for moderation.
		LF_Debug.Info(FEATURE, string.Format("call-back request %1 left by %2 (player %3) at %4 for %5: '%6'", request.m_iId, uid, playerId, request.m_sPlace, request.m_sService, request.m_sMessage));
		Publish(request, "left", string.Empty);
		LF_PhoneCalls.SyncLines();
		Toast(playerId, "#LF-Phone_RequestLeft");
		LF_ServiceRoute route = LF_ServiceCalls.Check(request.m_sService, players);
		foreach (int receiver : route.m_aReceivers)
		{
			Toast(receiver, "#LF-Phone_RequestOnPad");
		}
		return LF_EPhoneRequestRefusal.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from the action: an officer reads the pad at the station telephone.
	static void ServerOpenPad(IEntity user, IEntity phoneEntity)
	{
		if (!Replication.IsServer())
			return;

		SCR_PlayerController controller = ControllerOf(user);
		LF_PhoneComponent phone = LF_PhoneComponent.Of(phoneEntity);
		if (!controller || !phone || !phone.IsDispatchPhone())
		{
			LF_Debug.Error(FEATURE, "station pad refused: no player controller or not a station telephone");
			return;
		}

		int playerId = controller.GetPlayerId();
		if (!LF_RateLimit.Allow(playerId, LF_ERequestKind.COMMS, "station pad read"))
			return;

		if (!LF_PhoneCalls.MayAnswerFor(playerId, phone.DispatchService(), null))
		{
			LF_Debug.Info(FEATURE, string.Format("station pad refused for player %1: not on duty for %2", playerId, phone.DispatchService()));
			Toast(playerId, "#LF-Phone_PadNotOnDuty");
			return;
		}

		array<string> entries = {};
		Book().Entries(LF_PhoneCalls.Clock(), phone.DispatchService(), entries);
		LF_Debug.Info(FEATURE, string.Format("station pad read by player %1 at %2: %3 request(s)", playerId, phone.Key(), entries.Count()));
		controller.LF_PhoneShowPad(phone.Key(), entries);
	}

	//------------------------------------------------------------------------------------------------
	//! Server body of the ring-back RPC.
	static void ServerCallBack(SCR_PlayerController controller, string phoneKey, int requestId)
	{
		if (!controller)
			return;

		int playerId = controller.GetPlayerId();
		LF_EPhoneRefusal refusal = LF_PhoneCalls.CallBackAt(LF_PhoneCalls.Clock(), playerId, LF_Utils.GetPlayerUID(playerId), controller.GetControlledEntity(), LF_PhoneComponent.ByKey(phoneKey), requestId);
		Toast(playerId, LF_PhoneCalls.RefusalKey(refusal));
	}

	//------------------------------------------------------------------------------------------------
	//! A request was claimed for this call-back call: it shows "calling" until the call is over.
	static void Claimed(notnull LF_PhoneCallBackRequest request, notnull LF_PhoneCall call, string officerUid)
	{
		request.m_iCallId = call.m_iId;
		LF_Debug.Info(FEATURE, string.Format("call-back request %1 rung back by %2: call %3 rings %4", request.m_iId, officerUid, call.m_iId, request.m_sPhoneKey));
		Publish(request, "rung", officerUid);

		// The caller, wherever they are, hears that the station rings their telephone.
		array<int> players = {};
		GetGame().GetPlayerManager().GetPlayers(players);
		foreach (int other : players)
		{
			if (LF_Utils.GetPlayerUID(other) == request.m_sCallerUid)
				Toast(other, "#LF-Phone_RingingBack");
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static SCR_PlayerController ControllerOf(IEntity user)
	{
		if (!user || !GetGame() || !GetGame().GetPlayerManager())
			return null;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(user);
		return SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
	}

	//------------------------------------------------------------------------------------------------
	protected static void Toast(int playerId, string key)
	{
		if (playerId <= 0 || key.IsEmpty() || !GetGame() || !GetGame().GetPlayerManager())
			return;

		if (GetGame().GetPlayerManager().IsPlayerConnected(playerId))
			LF_NotificationManagerComponent.NotifyPlayer(playerId, LF_PhoneCalls.TITLE, key, 5.0);
	}
}

//------------------------------------------------------------------------------------------------
//! Call-back bridge on the player controller. server -> owner: the slip and the pad;
//! owner -> server: leave a request, ring one back.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	void LF_PhoneShowSlip(string phoneKey, int maxChars)
	{
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_PhoneShowSlip(phoneKey, maxChars);
		else
			Rpc(RpcDo_LF_PhoneShowSlip, phoneKey, maxChars);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_PhoneShowSlip(string phoneKey, int maxChars)
	{
		if (!GetGame().GetWorkspace())
		{
			LF_Debug.Error(LF_PhoneCallBacks.FEATURE, "message slip not opened: no workspace on this machine");
			return;
		}

		LF_PhoneSlipSheet.Open(this, phoneKey, maxChars);
	}

	//------------------------------------------------------------------------------------------------
	void LF_PhoneAskLeave(string phoneKey, string message)
	{
		if (Replication.IsServer())
			RpcAsk_LF_PhoneLeave(phoneKey, message);
		else
			Rpc(RpcAsk_LF_PhoneLeave, phoneKey, message);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_PhoneLeave(string phoneKey, string message)
	{
		if (!LF_RateLimit.Allow(GetPlayerId(), LF_ERequestKind.COMMS, "call-back request"))
			return;

		LF_PhoneCallBacks.ServerLeave(this, phoneKey, message);
	}

	//------------------------------------------------------------------------------------------------
	void LF_PhoneShowPad(string phoneKey, array<string> entries)
	{
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_PhoneShowPad(phoneKey, entries);
		else
			Rpc(RpcDo_LF_PhoneShowPad, phoneKey, entries);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_PhoneShowPad(string phoneKey, array<string> entries)
	{
		if (!GetGame().GetWorkspace())
		{
			LF_Debug.Error(LF_PhoneCallBacks.FEATURE, "station pad not opened: no workspace on this machine");
			return;
		}

		LF_PhonePadSheet.Open(this, phoneKey, entries);
	}

	//------------------------------------------------------------------------------------------------
	void LF_PhoneAskCallBack(string phoneKey, int requestId)
	{
		if (Replication.IsServer())
			RpcAsk_LF_PhoneCallBack(phoneKey, requestId);
		else
			Rpc(RpcAsk_LF_PhoneCallBack, phoneKey, requestId);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_PhoneCallBack(string phoneKey, int requestId)
	{
		if (!LF_RateLimit.Allow(GetPlayerId(), LF_ERequestKind.COMMS, "call-back ring"))
			return;

		LF_PhoneCallBacks.ServerCallBack(this, phoneKey, requestId);
	}
}
