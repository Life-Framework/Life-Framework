enum LF_EPayphoneResult
{
	CONNECTED,
	NOT_SERVER,
	BAD_CALLER,
	PLAYER_COOLDOWN,
	PHONE_COOLDOWN,
	UNKNOWN_SERVICE,
	NO_ANSWER,
	NO_COIN
}

//------------------------------------------------------------------------------------------------
//! The payphone rules (#247). Server only. A call is refused before any coin moves when the player
//! or the phone is cooling down, the service is unknown, or nobody is on duty for it. Otherwise the
//! coin is taken once and the call goes out as LF_OnServiceCall on the bus (LF_ServiceCalls.Place);
//! the police dispatch subscriber (LF_ServiceCallDispatch) turns it into a radio call carrying the
//! payphone's place. Every call, refused or connected, logs under [LFDebug:Payphone].
class LF_Payphone
{
	static const string FEATURE = "Payphone";

	//! Seconds of the last connected call, by player uid and by phone key.
	protected static ref map<string, float> s_mPlayerLast;
	protected static ref map<string, float> s_mPhoneLast;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mPlayerLast = null;
		s_mPhoneLast = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: seconds left of a cooldown; 0 when free. last < 0 means never used.
	static float CooldownLeft(float now, float last, float cooldownSeconds)
	{
		if (last < 0)
			return 0;

		return Math.Max(0, last + cooldownSeconds - now);
	}

	//------------------------------------------------------------------------------------------------
	//! Stable key of a phone: where it stands, to the metre.
	static string PhoneKey(vector position)
	{
		return string.Format("%1,%2", Math.Round(position[0]), Math.Round(position[2]));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the player dials service from the phone at phonePos.
	static LF_EPayphoneResult Use(IEntity user, vector phonePos, string service, string summary)
	{
		float now = 0;
		if (GetGame() && GetGame().GetWorld())
			now = GetGame().GetWorld().GetWorldTime() / 1000;

		return UseAt(now, LF_Utils.GetPlayerUid(user), user, phonePos, service, summary);
	}

	//------------------------------------------------------------------------------------------------
	//! The seam tests drive with their own clock (seconds).
	//! param players Who is on duty, for tests; null reads the connected players.
	static LF_EPayphoneResult UseAt(float now, string uid, IEntity user, vector phonePos, string service, string summary, array<ref LF_ServiceCandidate> players = null)
	{
		if (!Replication.IsServer())
			return LF_EPayphoneResult.NOT_SERVER;

		if (!user || uid.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "call refused: no player behind the handset");
			return LF_EPayphoneResult.BAD_CALLER;
		}

		if (!s_mPlayerLast)
			s_mPlayerLast = new map<string, float>();
		if (!s_mPhoneLast)
			s_mPhoneLast = new map<string, float>();

		string phoneKey = PhoneKey(phonePos);
		float playerLast = -1;
		float phoneLast = -1;
		s_mPlayerLast.Find(uid, playerLast);
		s_mPhoneLast.Find(phoneKey, phoneLast);

		if (CooldownLeft(now, playerLast, LF_ServiceCallsConfig.PlayerCooldown()) > 0)
		{
			LF_Debug.Info(FEATURE, string.Format("%1 refused at %2: player cooldown", uid, phoneKey));
			return LF_EPayphoneResult.PLAYER_COOLDOWN;
		}

		if (CooldownLeft(now, phoneLast, LF_ServiceCallsConfig.PhoneCooldown()) > 0)
		{
			LF_Debug.Info(FEATURE, string.Format("%1 refused at %2: phone cooldown", uid, phoneKey));
			return LF_EPayphoneResult.PHONE_COOLDOWN;
		}

		LF_ServiceRoute route = LF_ServiceCalls.Check(service, players);
		if (!route.m_bKnown)
			return LF_EPayphoneResult.UNKNOWN_SERVICE;

		if (!route.IsAnswered())
		{
			LF_Debug.Info(FEATURE, string.Format("%1 called %2 from %3: no answer, no coin taken", uid, service, phoneKey));
			return LF_EPayphoneResult.NO_ANSWER;
		}

		int cost = LF_ServiceCallsConfig.PayphoneCost();
		if (cost > 0 && LF_MoneyUtils.RemoveCash(user, cost, "payphone") < cost)
		{
			LF_Debug.Info(FEATURE, string.Format("%1 refused at %2: no coin (needs %3)", uid, phoneKey, cost));
			return LF_EPayphoneResult.NO_COIN;
		}

		s_mPlayerLast.Set(uid, now);
		s_mPhoneLast.Set(phoneKey, now);
		string place = LF_WantedReports.DescribePlace(phonePos);
		LF_ServiceCalls.Place(uid, service, place, summary, phonePos, players);
		LF_Debug.Info(FEATURE, string.Format("%1 called %2 from %3 (%4): coin %5 taken", uid, service, phoneKey, summary, cost));
		return LF_EPayphoneResult.CONNECTED;
	}

	//------------------------------------------------------------------------------------------------
	//! Localization key the caller sees for a refusal; empty for CONNECTED.
	static string RefusalKey(LF_EPayphoneResult result)
	{
		switch (result)
		{
			case LF_EPayphoneResult.PLAYER_COOLDOWN:
			case LF_EPayphoneResult.PHONE_COOLDOWN:
				return "#LF-Payphone_Cooldown";
			case LF_EPayphoneResult.NO_ANSWER:
				return LF_ServiceCalls.NO_ANSWER_KEY;
			case LF_EPayphoneResult.NO_COIN:
				return "#LF-Payphone_NoCoin";
			case LF_EPayphoneResult.CONNECTED:
				return string.Empty;
		}
		return "#LF-Payphone_Broken";
	}
}

//------------------------------------------------------------------------------------------------
//! Bus subscriber: a call to the police service becomes a radio call to every on-duty officer
//! through LF_Dispatch, with the phone's place (no suspect, no crime report: the caller is a witness).
class LF_ServiceCallDispatch : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnServiceCall call = LF_OnServiceCall.Cast(busEvent);
		if (!call)
			return false;

		if (call.m_sService != "police")
			return true;

		return LF_Dispatch.Receive(call) != null;
	}

	//! Server: subscribe once per world (the bus table is cleared at world end).
	static ref LF_ServiceCallDispatch s_Subscriber;

	static void Install()
	{
		if (!Replication.IsServer())
			return;

		// Idempotent even after a bus reset: drop any earlier subscription, then subscribe.
		if (s_Subscriber)
			LF_EventBus.Unsubscribe(LF_OnServiceCall, s_Subscriber);

		s_Subscriber = new LF_ServiceCallDispatch();
		LF_EventBus.Subscribe(LF_OnServiceCall, s_Subscriber);
	}

	static void ResetStatics()
	{
		s_Subscriber = null;
	}
}

//------------------------------------------------------------------------------------------------
//! One dial-a-service entry on a payphone: pick the service and what happened by picking the action
//! (a phone carries one action per preset). The server does everything; the client only asks.
class LF_PayphoneAction : ScriptedUserAction
{
	[Attribute("police", UIWidgets.EditBox, "Service key from Configs/Comms/LF_ServiceCalls.conf")]
	protected string m_sService;

	[Attribute("", UIWidgets.EditBox, "What happened: a localization key sent to the service as the summary")]
	protected string m_sSummary;

	[Attribute("SOUND_PICK_UP", UIWidgets.EditBox, "Sound event on the phone's SCR_SoundDataComponent: the coin dropping")]
	protected string m_sCoinSound;

	[Attribute("SOUND_RADIO_SIGNAL_2", UIWidgets.EditBox, "Sound event on the phone's SCR_SoundDataComponent: the dial tone")]
	protected string m_sDialSound;

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!user)
			return false;

		if (LF_MoneyUtils.GetCash(user) < LF_ServiceCallsConfig.PayphoneCost())
		{
			SetCannotPerformReason("#LF-Payphone_NoCoin");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || !pUserEntity)
		{
			LF_Debug.Error(LF_Payphone.FEATURE, "action performed without a phone or a user");
			return;
		}

		// The acting client reaches for the handset; the server does the work.
		if (pUserEntity == LF_Utils.GetLocalCharacter())
		{
			CharacterControllerComponent controller = CharacterControllerComponent.Cast(pUserEntity.FindComponent(CharacterControllerComponent));
			if (controller && controller.CanPlayItemGesture())
				controller.TryPlayItemGesture(EItemGesture.EItemGesturePickUp);
		}

		if (!Replication.IsServer())
			return;

		LF_EPayphoneResult result = LF_Payphone.Use(pUserEntity, pOwnerEntity.GetOrigin(), m_sService, m_sSummary);
		if (result != LF_EPayphoneResult.CONNECTED)
		{
			LF_Utils.NotifyEntity(pUserEntity, LF_Payphone.RefusalKey(result), "#LF-Payphone_Title", 4.0);
			return;
		}

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(pUserEntity);
		if (character)
		{
			character.LF_BroadcastStationSound("LF_PayphoneAction", m_sCoinSound);
			character.LF_BroadcastStationSound("LF_PayphoneAction", m_sDialSound);
		}

		LF_Utils.NotifyEntity(pUserEntity, "#LF-Payphone_Connected", "#LF-Payphone_Title", 4.0);
	}
}
