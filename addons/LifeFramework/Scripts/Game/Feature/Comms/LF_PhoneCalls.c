//------------------------------------------------------------------------------------------------
//! Telephone calls on the server (#147, docs/architecture/phone-calls-spike.md). The exchange
//! (LF_PhoneExchange) holds the calls; this class feeds it from the world and acts on what it
//! decides: it projects the call table onto the phone lines, publishes LF_OnPhoneCall on the bus,
//! tells the people involved, and logs every change and refusal under [LFDebug:Phone].
//!
//! Dialling the station from a payphone goes through LF_Payphone.UseAt first, so the coin, the
//! cooldowns and the duty check are the payphone's, and the call is published as LF_OnServiceCall
//! like any payphone call: the radio dispatch, the dispatch board and the pagers still get it.
//! A call ends when a talker hangs up, walks further than the cord from the phone, goes down,
//! dies or disconnects, when the phone is destroyed, or at the configured maximum length.
class LF_PhoneCalls
{
	static const string FEATURE = "Phone";
	static const string TITLE = "#LF-Phone_Title";
	static const string CALL_SUMMARY = "#LF-Payphone_Summary_Call";
	//! The call table's clock; once a second is enough for a cord and a ring timeout.
	static const int TICK_MS = 1000; // lf-fixed: polling interval of the server call table, not a gameplay knob

	protected static ref LF_PhoneExchange s_Exchange;
	protected static ref LF_PhonePresence s_Presence;
	protected static bool s_bTicking;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_bTicking && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_Exchange = null;
		s_Presence = null;
		s_bTicking = false;
	}

	//------------------------------------------------------------------------------------------------
	static LF_PhoneExchange Exchange()
	{
		if (!s_Exchange)
			s_Exchange = new LF_PhoneExchange();

		return s_Exchange;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests swap in a presence that does not need real players. null restores the world's.
	static void SetPresence(LF_PhonePresence presence)
	{
		s_Presence = presence;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_PhonePresence Presence()
	{
		if (!s_Presence)
			s_Presence = new LF_PhonePresence();

		return s_Presence;
	}

	//------------------------------------------------------------------------------------------------
	protected static float Now()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return 0;

		return GetGame().GetWorld().GetWorldTime() / 1000;
	}

	//------------------------------------------------------------------------------------------------
	//! The call table's clock in seconds, for the call-back book.
	static float Clock()
	{
		return Now();
	}

	//------------------------------------------------------------------------------------------------
	//! Is this body at the telephone, by the server's own reckoning?
	static bool IsNear(IEntity body, LF_PhoneComponent phone)
	{
		if (!phone)
			return false;

		return Presence().IsAt(body, phone.GetOwner(), LF_PhoneConfig.CordMeters());
	}

	//------------------------------------------------------------------------------------------------
	//! Is this player one of the service's people on duty right now (null players: the connected ones)?
	static bool MayAnswerFor(int playerId, string service, array<ref LF_ServiceCandidate> players)
	{
		if (playerId <= 0 || service.IsEmpty())
			return false;

		return LF_ServiceCalls.Check(service, players).m_aReceivers.Contains(playerId);
	}

	//------------------------------------------------------------------------------------------------
	protected static void EnsureTicking()
	{
		if (s_bTicking || !GetGame() || !GetGame().GetCallqueue())
			return;

		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
		s_bTicking = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the toast key for a refusal; empty when there is nothing to say.
	static string RefusalKey(LF_EPhoneRefusal refusal)
	{
		switch (refusal)
		{
			case LF_EPhoneRefusal.ON_A_CALL: return "#LF-Phone_OnACall";
			case LF_EPhoneRefusal.PHONE_BUSY: return "#LF-Phone_PhoneBusy";
			case LF_EPhoneRefusal.LINE_BUSY: return "#LF-Phone_LineBusy";
			case LF_EPhoneRefusal.NO_LINE: return "#LF-Phone_NoLine";
			case LF_EPhoneRefusal.NOT_RINGING: return "#LF-Phone_NotRinging";
			case LF_EPhoneRefusal.NOT_ON_DUTY: return "#LF-Phone_NotOnDuty";
			case LF_EPhoneRefusal.TOO_FAR: return "#LF-Phone_TooFar";
			case LF_EPhoneRefusal.NOT_YOUR_CALL: return "#LF-Phone_NotYourCall";
			case LF_EPhoneRefusal.NOBODY_ON_DUTY: return "#LF-Phone_NobodyOffer";
			case LF_EPhoneRefusal.NO_REQUEST: return "#LF-Phone_RequestGone";
			case LF_EPhoneRefusal.REQUEST_CALLING: return "#LF-Phone_RequestCalling";
			case LF_EPhoneRefusal.TARGET_BUSY: return "#LF-Phone_TargetBusy";
			case LF_EPhoneRefusal.OK: return string.Empty;
			case LF_EPhoneRefusal.PAYPHONE: return string.Empty;
			case LF_EPhoneRefusal.TOO_FAST: return string.Empty;
		}
		return "#LF-Payphone_Broken";
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: what a talker is told when the call ends; self is the talker whose doing it was.
	static string EndKey(LF_EPhoneEnd why, bool self)
	{
		if (!self)
			return "#LF-Phone_LineDead";

		switch (why)
		{
			case LF_EPhoneEnd.HUNG_UP: return "#LF-Phone_HungUp";
			case LF_EPhoneEnd.WALKED_AWAY: return "#LF-Phone_WalkedAway";
			case LF_EPhoneEnd.TOO_LONG: return "#LF-Phone_TooLong";
			case LF_EPhoneEnd.NO_ANSWER: return "#LF-Phone_NoAnswerOffer";
		}
		return "#LF-Phone_LineDead";
	}

	//------------------------------------------------------------------------------------------------
	//! A talker who just left gets nothing: the call table outlives their connection by a tick.
	protected static void Toast(int playerId, string key)
	{
		if (playerId <= 0 || key.IsEmpty() || !GetGame() || !GetGame().GetPlayerManager())
			return;

		if (GetGame().GetPlayerManager().IsPlayerConnected(playerId))
			LF_NotificationManagerComponent.NotifyPlayer(playerId, TITLE, key, 5.0);
	}

	//------------------------------------------------------------------------------------------------
	protected static int PlayerIdOf(IEntity user)
	{
		if (!user || !GetGame() || !GetGame().GetPlayerManager())
			return 0;

		return GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(user);
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from the dial action: the player at a payphone rings the station of service.
	static LF_EPhoneRefusal ServerDial(IEntity user, IEntity phoneEntity, string service)
	{
		if (LF_Systems.Refuses("Comms", "phone dial"))
			return LF_EPhoneRefusal.NO_LINE;

		if (!Replication.IsServer())
			return LF_EPhoneRefusal.NOT_SERVER;

		// A refused dial costs nothing, so the shared comms limit (#834) bounds a dial-spamming client.
		if (!LF_RateLimit.AllowCharacter(user, LF_ERequestKind.COMMS, "phone dial"))
			return LF_EPhoneRefusal.TOO_FAST;

		int playerId = PlayerIdOf(user);
		LF_EPhoneRefusal refusal = DialAt(Now(), playerId, LF_Utils.GetPlayerUID(playerId), user, LF_PhoneComponent.Of(phoneEntity), service);
		Toast(playerId, RefusalKey(refusal));
		return refusal;
	}

	//------------------------------------------------------------------------------------------------
	//! The seam tests drive: their own clock, player ids and duty list (null reads the connected players).
	static LF_EPhoneRefusal DialAt(float now, int callerId, string callerUid, IEntity user, LF_PhoneComponent phone, string service, array<ref LF_ServiceCandidate> players = null)
	{
		if (!Replication.IsServer())
			return LF_EPhoneRefusal.NOT_SERVER;

		if (!phone || !user || callerId <= 0 || callerUid.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "dial refused: no phone component or no player behind the receiver");
			return LF_EPhoneRefusal.BAD_CALLER;
		}

		string fromKey = phone.Key();
		if (!Presence().IsAt(user, phone.GetOwner(), LF_PhoneConfig.CordMeters()))
		{
			LF_Debug.Warn(FEATURE, string.Format("dial refused for player %1: not at phone %2", callerId, fromKey));
			return LF_EPhoneRefusal.TOO_FAR;
		}

		// The exchange decides before the payphone takes a coin.
		array<string> ringable = {};
		array<string> idle = {};
		LF_PhoneComponent.DispatchPhones(service, ringable);
		LF_EPhoneRefusal refusal = Exchange().CheckDial(callerId, fromKey, ringable, idle);
		if (refusal != LF_EPhoneRefusal.OK)
		{
			if (refusal == LF_EPhoneRefusal.NO_LINE)
				LF_Debug.Warn(FEATURE, string.Format("dial refused for player %1 at %2: no telephone in this world answers '%3'", callerId, fromKey, service));
			else
				LF_Debug.Info(FEATURE, string.Format("dial refused for player %1 at %2 to %3: %4", callerId, fromKey, service, typename.EnumToString(LF_EPhoneRefusal, refusal)));
			return refusal;
		}

		LF_EPayphoneResult paid = LF_Payphone.UseAt(now, callerUid, user, phone.GetOwner().GetOrigin(), service, CALL_SUMMARY, players);
		if (paid == LF_EPayphoneResult.NO_ANSWER)
		{
			LF_PhoneCall missed = Exchange().Missed(now, callerId, callerUid, fromKey, service);
			missed.m_sPlace = phone.Place();
			// The caller's line carries the call-back offer, so the phone needs one now.
			phone.ServerLine();
			Changed(missed, "nobody on duty");
			return LF_EPhoneRefusal.NOBODY_ON_DUTY;
		}

		if (paid != LF_EPayphoneResult.CONNECTED)
		{
			LF_Debug.Info(FEATURE, string.Format("dial refused for player %1 at %2: payphone said %3", callerId, fromKey, typename.EnumToString(LF_EPayphoneResult, paid)));
			Toast(callerId, LF_Payphone.RefusalKey(paid));
			return LF_EPhoneRefusal.PAYPHONE;
		}

		LF_PhoneCall call = Exchange().Dial(now, callerId, callerUid, fromKey, service, ringable, false, refusal);
		if (!call)
		{
			LF_Debug.Error(FEATURE, string.Format("dial for player %1 at %2 refused after the coin: %3", callerId, fromKey, typename.EnumToString(LF_EPhoneRefusal, refusal)));
			return refusal;
		}

		call.m_sPlace = phone.Place();
		Changed(call, string.Format("player %1 dialled %2 from %3", callerId, service, fromKey));
		NotifyReceivers(call, players);
		return LF_EPhoneRefusal.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! The service's people on duty hear that the station telephone rings, wherever they are.
	protected static void NotifyReceivers(notnull LF_PhoneCall call, array<ref LF_ServiceCandidate> players)
	{
		if (call.m_bCallBack)
			return;

		LF_ServiceRoute route = LF_ServiceCalls.Check(call.m_sService, players);
		foreach (int receiver : route.m_aReceivers)
		{
			Toast(receiver, "#LF-Phone_StationRinging");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from the pick-up action.
	static LF_EPhoneRefusal ServerAnswer(IEntity user, IEntity phoneEntity)
	{
		if (!Replication.IsServer())
			return LF_EPhoneRefusal.NOT_SERVER;

		if (!LF_RateLimit.AllowCharacter(user, LF_ERequestKind.COMMS, "phone pick-up"))
			return LF_EPhoneRefusal.TOO_FAST;

		int playerId = PlayerIdOf(user);
		LF_EPhoneRefusal refusal = AnswerAt(Now(), playerId, LF_Utils.GetPlayerUID(playerId), user, LF_PhoneComponent.Of(phoneEntity));
		Toast(playerId, RefusalKey(refusal));
		return refusal;
	}

	//------------------------------------------------------------------------------------------------
	//! A station telephone is picked up only by the service's people on duty; a ringing payphone
	//! by anyone standing at it.
	static LF_EPhoneRefusal AnswerAt(float now, int playerId, string playerUid, IEntity user, LF_PhoneComponent phone, array<ref LF_ServiceCandidate> players = null)
	{
		if (!Replication.IsServer())
			return LF_EPhoneRefusal.NOT_SERVER;

		if (!phone || !user || playerId <= 0)
		{
			LF_Debug.Error(FEATURE, "pick-up refused: no phone component or no player");
			return LF_EPhoneRefusal.BAD_CALLER;
		}

		string key = phone.Key();
		if (!Exchange().RingingAt(key))
		{
			LF_Debug.Info(FEATURE, string.Format("pick-up refused for player %1 at %2: not ringing", playerId, key));
			return LF_EPhoneRefusal.NOT_RINGING;
		}

		if (!Presence().IsAt(user, phone.GetOwner(), LF_PhoneConfig.CordMeters()))
		{
			LF_Debug.Warn(FEATURE, string.Format("pick-up refused for player %1: not at phone %2", playerId, key));
			return LF_EPhoneRefusal.TOO_FAR;
		}

		bool mayAnswer = true;
		if (phone.IsDispatchPhone())
			mayAnswer = MayAnswerFor(playerId, phone.DispatchService(), players);

		LF_EPhoneRefusal refusal;
		LF_PhoneCall call = Exchange().Answer(now, playerId, playerUid, key, mayAnswer, refusal);
		if (!call)
		{
			LF_Debug.Info(FEATURE, string.Format("pick-up refused for player %1 at %2: %3", playerId, key, typename.EnumToString(LF_EPhoneRefusal, refusal)));
			return refusal;
		}

		Changed(call, string.Format("player %1 picked up at %2 after %3 s", playerId, key, call.RangSeconds()));
		return LF_EPhoneRefusal.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! An officer at the station telephone rings back the payphone a call-back request came from.
	//! Free of charge; the payphone rings for anyone standing at it.
	static LF_EPhoneRefusal CallBackAt(float now, int playerId, string playerUid, IEntity user, LF_PhoneComponent station, int requestId, array<ref LF_ServiceCandidate> players = null)
	{
		if (!Replication.IsServer())
			return LF_EPhoneRefusal.NOT_SERVER;

		if (!station || !user || playerId <= 0 || !station.IsDispatchPhone())
		{
			LF_Debug.Warn(FEATURE, string.Format("ring-back refused for player %1: no station telephone by that key or no body", playerId));
			return LF_EPhoneRefusal.BAD_CALLER;
		}

		if (!IsNear(user, station))
		{
			LF_Debug.Warn(FEATURE, string.Format("ring-back refused for player %1: not at station phone %2", playerId, station.Key()));
			return LF_EPhoneRefusal.TOO_FAR;
		}

		if (!MayAnswerFor(playerId, station.DispatchService(), players))
		{
			LF_Debug.Info(FEATURE, string.Format("ring-back refused for player %1: not on duty for %2", playerId, station.DispatchService()));
			return LF_EPhoneRefusal.NOT_ON_DUTY;
		}

		LF_EPhoneRequestRefusal claim;
		LF_PhoneCallBackRequest request = LF_PhoneCallBacks.Book().Claimable(now, requestId, station.DispatchService(), claim);
		if (!request)
		{
			LF_Debug.Info(FEATURE, string.Format("ring-back refused for player %1: request %2 %3", playerId, requestId, typename.EnumToString(LF_EPhoneRequestRefusal, claim)));
			if (claim == LF_EPhoneRequestRefusal.CALLING)
				return LF_EPhoneRefusal.REQUEST_CALLING;
			return LF_EPhoneRefusal.NO_REQUEST;
		}

		if (!LF_PhoneComponent.ByKey(request.m_sPhoneKey))
		{
			LF_Debug.Error(FEATURE, string.Format("ring-back of request %1: the telephone at %2 is gone", requestId, request.m_sPhoneKey));
			return LF_EPhoneRefusal.NO_LINE;
		}

		array<string> target = {request.m_sPhoneKey};
		LF_EPhoneRefusal refusal;
		LF_PhoneCall call = Exchange().Dial(now, playerId, playerUid, station.Key(), station.DispatchService(), target, true, refusal);
		if (!call)
		{
			LF_Debug.Info(FEATURE, string.Format("ring-back refused for player %1 at %2: %3", playerId, station.Key(), typename.EnumToString(LF_EPhoneRefusal, refusal)));
			if (refusal == LF_EPhoneRefusal.LINE_BUSY)
				return LF_EPhoneRefusal.TARGET_BUSY;
			return refusal;
		}

		call.m_sPlace = station.Place();
		LF_PhoneCallBacks.Claimed(request, call, playerUid);
		Changed(call, string.Format("player %1 rings back request %2 at %3", playerId, requestId, request.m_sPhoneKey));
		return LF_EPhoneRefusal.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from the hang-up action: the talker at this phone puts the receiver down.
	static LF_EPhoneRefusal ServerHangUp(IEntity user, IEntity phoneEntity)
	{
		if (!Replication.IsServer())
			return LF_EPhoneRefusal.NOT_SERVER;

		int playerId = PlayerIdOf(user);
		LF_EPhoneRefusal refusal = HangUpAt(Now(), playerId, LF_PhoneComponent.Of(phoneEntity));
		Toast(playerId, RefusalKey(refusal));
		return refusal;
	}

	//------------------------------------------------------------------------------------------------
	static LF_EPhoneRefusal HangUpAt(float now, int playerId, LF_PhoneComponent phone)
	{
		if (!Replication.IsServer())
			return LF_EPhoneRefusal.NOT_SERVER;

		if (!phone || playerId <= 0)
			return LF_EPhoneRefusal.BAD_CALLER;

		LF_EPhoneRefusal refusal;
		LF_PhoneCall call = Exchange().HangUp(now, playerId, phone.Key(), refusal);
		if (!call)
		{
			LF_Debug.Info(FEATURE, string.Format("hang-up refused for player %1 at %2: not their call", playerId, phone.Key()));
			return refusal;
		}

		Changed(call, string.Format("player %1 hung up", playerId), playerId);
		return LF_EPhoneRefusal.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, from the game mode: a talker who leaves the server ends their call at once.
	static void OnPlayerDisconnected(int playerId)
	{
		if (!Replication.IsServer() || !s_Exchange)
			return;

		LF_PhoneCall call = s_Exchange.CallOfPlayer(playerId);
		if (call && s_Exchange.Finish(call, Now(), LF_EPhoneEnd.DISCONNECTED))
			Changed(call, string.Format("player %1 disconnected", playerId), playerId);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		TickAt(Now());
	}

	//------------------------------------------------------------------------------------------------
	//! The clock and the cord: rang out, too long, and every talker still at their phone.
	static void TickAt(float now)
	{
		if (LF_PhoneCallBacks.Purge(now))
			SyncLines();

		if (!s_Exchange)
			return;

		array<LF_PhoneCall> changed = {};
		s_Exchange.Expire(now, LF_PhoneConfig.RingSeconds(), LF_PhoneConfig.MaxCallSeconds(), changed);
		foreach (LF_PhoneCall call : changed)
		{
			if (call.m_eEnd == LF_EPhoneEnd.NO_ANSWER)
				Changed(call, string.Format("nobody picked up in %1 s", call.RangSeconds()));
			else
				Changed(call, string.Format("cut after %1 s, the most a call may last", call.TalkSeconds()));
		}

		array<LF_PhoneCall> live = {};
		foreach (LF_PhoneCall call : s_Exchange.Calls())
		{
			if (call.IsLive())
				live.Insert(call);
		}

		float cord = LF_PhoneConfig.CordMeters();
		foreach (LF_PhoneCall call : live)
		{
			int who = call.m_iCallerId;
			LF_EPhoneEnd why = Presence().Check(call.m_iCallerId, call.m_sFromPhone, cord);
			if (why == LF_EPhoneEnd.NONE && call.m_eState == LF_EPhoneCallState.CONNECTED)
			{
				who = call.m_iAnswererId;
				why = Presence().Check(call.m_iAnswererId, call.m_sToPhone, cord);
			}

			if (why == LF_EPhoneEnd.NONE || !s_Exchange.Finish(call, now, why))
				continue;

			Changed(call, string.Format("player %1: %2", who, typename.EnumToString(LF_EPhoneEnd, why)), who);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! After every change: lines, bus, people, log. byPlayer is the talker whose doing it was.
	protected static void Changed(notnull LF_PhoneCall call, string why, int byPlayer = 0)
	{
		EnsureTicking();
		// The call's own time stamps, so a test's clock and the world's agree.
		float at = call.m_fStarted;
		if (call.m_fEnded >= 0)
			at = call.m_fEnded;
		LF_PhoneCallBacks.OnCallChanged(call, at);
		SyncLines();
		LF_PhoneVoice.OnCallChanged(call);
		LF_EventBus.Publish(LF_OnPhoneCall.Create(call));

		string state = typename.EnumToString(LF_EPhoneCallState, call.m_eState);
		switch (call.m_eState)
		{
			case LF_EPhoneCallState.RINGING:
			{
				LF_Debug.Info(FEATURE, string.Format("call %1 RINGING: %2; %3 phone(s) ring: %4", call.m_iId, why, call.m_aRingPhones.Count(), call.m_sPlace));
				Toast(call.m_iCallerId, "#LF-Phone_Ringing");
				break;
			}
			case LF_EPhoneCallState.CONNECTED:
			{
				LF_Debug.Info(FEATURE, string.Format("call %1 CONNECTED: %2; player %3 (caller) <-> player %4", call.m_iId, why, call.m_iCallerId, call.m_iAnswererId));
				Toast(call.m_iCallerId, "#LF-Phone_ConnectedTalk");
				Toast(call.m_iAnswererId, "#LF-Phone_ConnectedTalk");
				break;
			}
			default:
			{
				LF_Debug.Info(FEATURE, string.Format("call %1 %2 (%3): %4; rang %5 s, talked %6 s", call.m_iId, state, typename.EnumToString(LF_EPhoneEnd, call.m_eEnd), why, call.RangSeconds(), call.TalkSeconds()));
				if (call.m_eEnd == LF_EPhoneEnd.NOBODY_ON_DUTY)
					break;

				string callerKey = EndKey(call.m_eEnd, byPlayer == call.m_iCallerId || call.m_eEnd == LF_EPhoneEnd.NO_ANSWER || call.m_eEnd == LF_EPhoneEnd.TOO_LONG);
				// A ring-back nobody picked up offers no message: the officer simply tries again.
				if (call.m_bCallBack && call.m_eEnd == LF_EPhoneEnd.NO_ANSWER)
					callerKey = "#LF-Phone_NoAnswer";
				Toast(call.m_iCallerId, callerKey);
				Toast(call.m_iAnswererId, EndKey(call.m_eEnd, byPlayer == call.m_iAnswererId || call.m_eEnd == LF_EPhoneEnd.TOO_LONG));
				break;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Projects the call table onto every phone line: a line spawns when a call first needs it, and
	//! every existing line shows its phone's state.
	static void SyncLines()
	{
		if (!Replication.IsServer())
			return;

		array<string> live = {};
		Exchange().LivePhones(live);
		foreach (string key : live)
		{
			LF_PhoneComponent phone = LF_PhoneComponent.ByKey(key);
			if (phone)
				phone.ServerLine();
		}

		foreach (LF_PhoneLineComponent line : LF_PhoneLineComponent.Lines())
		{
			if (!line)
				continue;

			int talker;
			LF_EPhoneLineState state = Exchange().LineState(line.GetPhoneKey(), talker);
			int offered;
			if (state == LF_EPhoneLineState.IDLE)
				offered = LF_PhoneCallBacks.OfferedAt(line.GetPhoneKey());
			line.ServerSet(state, talker, offered);
		}
	}
}

//------------------------------------------------------------------------------------------------
//! Is a talker still holding the receiver? The world's answer; tests replace it.
class LF_PhonePresence : Managed
{
	//------------------------------------------------------------------------------------------------
	//! Standing within reach of the phone (the server's own check; the action range is the client's).
	bool IsAt(IEntity user, IEntity phone, float cordMeters)
	{
		if (!user || !phone)
			return false;

		return vector.Distance(user.GetOrigin(), phone.GetOrigin()) <= cordMeters;
	}

	//------------------------------------------------------------------------------------------------
	//! NONE while the player is connected, up, and within the cord of their phone.
	LF_EPhoneEnd Check(int playerId, string phoneKey, float cordMeters)
	{
		LF_PhoneComponent phone = LF_PhoneComponent.ByKey(phoneKey);
		if (!phone)
			return LF_EPhoneEnd.PHONE_GONE;

		if (!GetGame() || !GetGame().GetPlayerManager())
			return LF_EPhoneEnd.DISCONNECTED;

		PlayerManager players = GetGame().GetPlayerManager();
		if (!players.IsPlayerConnected(playerId))
			return LF_EPhoneEnd.DISCONNECTED;

		IEntity body = players.GetPlayerControlledEntity(playerId);
		if (!body)
			return LF_EPhoneEnd.DOWN;

		ChimeraCharacter character = ChimeraCharacter.Cast(body);
		if (!character || !character.GetCharacterController() || character.GetCharacterController().GetLifeState() != ECharacterLifeState.ALIVE)
			return LF_EPhoneEnd.DOWN;

		if (!IsAt(body, phone.GetOwner(), cordMeters))
			return LF_EPhoneEnd.WALKED_AWAY;

		return LF_EPhoneEnd.NONE;
	}
}
