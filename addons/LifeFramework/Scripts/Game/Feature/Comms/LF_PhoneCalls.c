//------------------------------------------------------------------------------------------------
// SPIKE (docs/architecture/phone-calls-spike.md). Proof of concept, not production code:
// raw English strings, no persistence, no tests, call state not replicated to clients.
//
// A call joins two phones. The server keeps the state (RINGING, CONNECTED, ENDED, MISSED). Each
// phone owns a "line": a replicated entity with a BaseRadioComponent whose one transceiver sits on a
// band no handheld can tune (100000..165000 kHz). On answer the server puts both lines on one
// frequency with one per-call encryption key and tells the two players' machines to transmit
// through their own line (VoNComponent.SetTransmitRadio). Everything logs under [LFDebug:Phone].
//------------------------------------------------------------------------------------------------
enum LF_EPhoneCallState
{
	RINGING,
	CONNECTED,
	ENDED,
	MISSED
}

//! Where the two transceivers of a call live.
enum LF_EPhoneVoiceMode
{
	//! On the line entity standing at each phone prop.
	PROP,
	//! On a handheld radio the server puts in each talker's inventory for the length of the call.
	CARRIED
}

//------------------------------------------------------------------------------------------------
class LF_PhoneCall
{
	int m_iId;
	LF_EPhoneCallState m_eState;
	string m_sService;
	//! The phone the call was placed from, and the one phone it rings when it is a call-back.
	EntityID m_FromPhone = EntityID.INVALID;
	EntityID m_RingPhone = EntityID.INVALID;
	//! The phone that answered.
	EntityID m_ToPhone = EntityID.INVALID;
	int m_iCallerId;
	int m_iAnswererId;
	float m_fStarted;
	float m_fLastPulse = -100;
	float m_fConnected;
	int m_iFrequency;
	string m_sKey;
	LF_EPhoneVoiceMode m_eMode;
	//! CARRIED mode: the radios the server handed out, deleted on hang-up.
	EntityID m_CallerRadio = EntityID.INVALID;
	EntityID m_AnswererRadio = EntityID.INVALID;
	int m_iVonUses;
}

//------------------------------------------------------------------------------------------------
//! What a caller leaves when nobody answers: a call-back request, not a recording.
class LF_PhoneMessage
{
	int m_iId;
	string m_sCallerUid;
	string m_sCallerName;
	string m_sPlace;
	string m_sSummary;
	EntityID m_Phone = EntityID.INVALID;
	float m_fLeft;
	float m_fRang;
}

//------------------------------------------------------------------------------------------------
//! A scripted radio message the server sends over a line to log who the radio layer delivers to.
class LF_PhoneProbeMessage : ScriptedRadioMessage
{
	int m_iCallId;
	string m_sLabel;

	override void OnDelivery(BaseTransceiver receiver, int freq, float quality)
	{
		LF_PhoneCalls.OnProbeDelivered(this, receiver, freq, quality);
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Comms", description: "Spike: a phone that can place and take voice calls.")]
class LF_PhoneComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_PhoneComponent : ScriptComponent
{
	static const ResourceName LINE_PREFAB = "{EEAF530F3437D3DA}Prefabs/Comms/LF_PhoneLine.et";

	[Attribute("", UIWidgets.EditBox, "Service this phone answers (police) when it is a dispatch phone; empty on a payphone")]
	protected string m_sDispatchService;

	protected static ref array<LF_PhoneComponent> s_aPhones;
	protected EntityID m_Line = EntityID.INVALID;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aPhones = null;
	}

	//------------------------------------------------------------------------------------------------
	static array<LF_PhoneComponent> Phones()
	{
		if (!s_aPhones)
			s_aPhones = {};

		return s_aPhones;
	}

	//------------------------------------------------------------------------------------------------
	static LF_PhoneComponent Of(IEntity phone)
	{
		if (!phone)
			return null;

		return LF_PhoneComponent.Cast(phone.FindComponent(LF_PhoneComponent));
	}

	//------------------------------------------------------------------------------------------------
	static LF_PhoneComponent ById(EntityID id)
	{
		if (id == EntityID.INVALID || !GetGame().GetWorld())
			return null;

		return Of(GetGame().GetWorld().FindEntityByID(id));
	}

	//------------------------------------------------------------------------------------------------
	string DispatchService()
	{
		return m_sDispatchService;
	}

	//------------------------------------------------------------------------------------------------
	string Place()
	{
		return LF_WantedReports.DescribePlace(GetOwner().GetOrigin());
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		Phones().Insert(this);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_aPhones)
			s_aPhones.RemoveItem(this);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the line entity of this phone, spawned on first use.
	IEntity ServerLine()
	{
		if (!Replication.IsServer())
			return null;

		IEntity line;
		if (m_Line != EntityID.INVALID)
			line = GetGame().GetWorld().FindEntityByID(m_Line);

		if (line)
			return line;

		line = LF_Utils.SpawnEntityPrefab(LINE_PREFAB, GetOwner().GetOrigin() + "0 1.2 0");
		if (!line)
		{
			LF_Debug.Error(LF_PhoneCalls.FEATURE, "line prefab did not spawn; this phone has no voice");
			return null;
		}

		m_Line = line.GetID();
		LF_Debug.Info(LF_PhoneCalls.FEATURE, string.Format("line spawned for phone at %1 (%2), rplId %3", LF_Payphone.PhoneKey(GetOwner().GetOrigin()), Place(), LF_NetworkUtils.GetRplId(line)));
		return line;
	}
}

//------------------------------------------------------------------------------------------------
class LF_PhoneCalls
{
	static const string FEATURE = "Phone";
	static const string TITLE = "Phone";
	static const int RING_TIMEOUT_S = 20;
	static const int RING_PULSE_S = 4;
	static const float RING_HEAR_M = 40;
	static const float CORD_M = 6;
	static const int BASE_FREQUENCY = 100000;
	static const int CARRIED_BASE_FREQUENCY = 60000;
	static const int MISSED_WINDOW_S = 180;
	static const int MAX_MESSAGES = 20;

	protected static ref array<ref LF_PhoneCall> s_aCalls;
	protected static ref array<ref LF_PhoneMessage> s_aMessages;
	//! uid -> world seconds of that player's last unanswered call, and the phone it was made from.
	protected static ref map<string, float> s_mMissedAt;
	protected static ref map<string, EntityID> s_mMissedPhone;
	protected static ref map<string, float> s_mMissedRang;
	protected static ref array<ref LF_PhoneProbeMessage> s_aProbes;
	protected static int s_iNextId = 1;
	protected static bool s_bTicking;
	protected static bool s_bVonHooked;
	protected static LF_EPhoneVoiceMode s_eMode = LF_EPhoneVoiceMode.PROP;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aCalls = null;
		s_aMessages = null;
		s_mMissedAt = null;
		s_mMissedPhone = null;
		s_mMissedRang = null;
		s_aProbes = null;
		s_iNextId = 1;
		s_bTicking = false;
		s_bVonHooked = false;
		s_eMode = LF_EPhoneVoiceMode.PROP;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Ensure()
	{
		if (!s_aCalls)
			s_aCalls = {};
		if (!s_aMessages)
			s_aMessages = {};
		if (!s_mMissedAt)
			s_mMissedAt = new map<string, float>();
		if (!s_mMissedPhone)
			s_mMissedPhone = new map<string, EntityID>();
		if (!s_mMissedRang)
			s_mMissedRang = new map<string, float>();
		if (!s_aProbes)
			s_aProbes = {};

		if (!s_bVonHooked)
		{
			SCR_VoNComponent.GetOnVoNUsed().Insert(OnVoNUsed);
			s_bVonHooked = true;
		}

		if (!s_bTicking)
		{
			GetGame().GetCallqueue().CallLater(Tick, 1000, true);
			s_bTicking = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static float Now()
	{
		return GetGame().GetWorld().GetWorldTime() / 1000;
	}

	//------------------------------------------------------------------------------------------------
	protected static int PlayerIdOf(IEntity user)
	{
		return GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(user);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Toast(int playerId, string text)
	{
		if (playerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(playerId, TITLE, text, 6.0);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsLive(LF_PhoneCall call)
	{
		return call.m_eState == LF_EPhoneCallState.RINGING || call.m_eState == LF_EPhoneCallState.CONNECTED;
	}

	//------------------------------------------------------------------------------------------------
	//! The live call this player is part of, or null.
	protected static LF_PhoneCall CallOfPlayer(int playerId)
	{
		foreach (LF_PhoneCall call : s_aCalls)
		{
			if (IsLive(call) && (call.m_iCallerId == playerId || call.m_iAnswererId == playerId))
				return call;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Does this live call ring at the phone?
	protected static bool RingsAt(LF_PhoneCall call, LF_PhoneComponent phone)
	{
		if (call.m_eState != LF_EPhoneCallState.RINGING)
			return false;

		if (call.m_RingPhone != EntityID.INVALID)
			return call.m_RingPhone == phone.GetOwner().GetID();

		return !phone.DispatchService().IsEmpty() && phone.DispatchService() == call.m_sService;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool PhoneBusy(LF_PhoneComponent phone)
	{
		EntityID id = phone.GetOwner().GetID();
		foreach (LF_PhoneCall call : s_aCalls)
		{
			if (!IsLive(call))
				continue;

			if (call.m_FromPhone == id || call.m_ToPhone == id)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: debug switch between the two voice wirings.
	static void ToggleMode(IEntity user)
	{
		if (s_eMode == LF_EPhoneVoiceMode.PROP)
			s_eMode = LF_EPhoneVoiceMode.CARRIED;
		else
			s_eMode = LF_EPhoneVoiceMode.PROP;

		LF_Debug.Info(FEATURE, string.Format("voice mode is now %1", typename.EnumToString(LF_EPhoneVoiceMode, s_eMode)));
		Toast(PlayerIdOf(user), "Voice mode: " + typename.EnumToString(LF_EPhoneVoiceMode, s_eMode));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the player at a payphone dials a service.
	static void Dial(IEntity user, IEntity phoneEntity, string service)
	{
		if (!Replication.IsServer())
			return;

		Ensure();
		LF_PhoneComponent phone = LF_PhoneComponent.Of(phoneEntity);
		int playerId = PlayerIdOf(user);
		if (!phone || playerId <= 0)
		{
			LF_Debug.Error(FEATURE, "dial refused: no phone component or no player");
			return;
		}

		if (CallOfPlayer(playerId) || PhoneBusy(phone))
		{
			LF_Debug.Info(FEATURE, string.Format("dial refused for player %1: already on a call or the phone is busy", playerId));
			Toast(playerId, "The line is busy.");
			return;
		}

		string uid = LF_Utils.GetPlayerUID(playerId);
		LF_ServiceRoute route = LF_ServiceCalls.Check(service);
		if (!route.m_bKnown)
		{
			Toast(playerId, "That number does not exist.");
			return;
		}

		if (!route.IsAnswered())
		{
			s_mMissedAt.Set(uid, Now());
			s_mMissedPhone.Set(uid, phoneEntity.GetID());
			s_mMissedRang.Set(uid, 0);
			LF_Debug.Info(FEATURE, string.Format("player %1 dialled %2 from %3: MISSED at once, nobody on duty; message offered", playerId, service, phone.Place()));
			Toast(playerId, "Nobody is on duty. You can leave a message at this phone.");
			return;
		}

		LF_PhoneCall call = new LF_PhoneCall();
		call.m_iId = s_iNextId++;
		call.m_eState = LF_EPhoneCallState.RINGING;
		call.m_sService = service;
		call.m_FromPhone = phoneEntity.GetID();
		call.m_iCallerId = playerId;
		call.m_fStarted = Now();
		s_aCalls.Insert(call);

		// Lines exist from the first ring, so they have streamed to the talkers by the time of the answer.
		phone.ServerLine();
		int ringing;
		foreach (LF_PhoneComponent other : LF_PhoneComponent.Phones())
		{
			if (other && RingsAt(call, other))
			{
				other.ServerLine();
				ringing++;
			}
		}

		LF_Debug.Info(FEATURE, string.Format("call %1 RINGING: player %2 dialled %3 from %4; %5 dispatch phone(s) ring, %6 on duty", call.m_iId, playerId, service, phone.Place(), ringing, route.m_aReceivers.Count()));
		Toast(playerId, "Ringing...");
		foreach (int receiver : route.m_aReceivers)
			Toast(receiver, "The dispatch phone is ringing: a call from " + phone.Place() + ".");

		Pulse(call);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one ring. Every player near a ringing phone hears it; sent again every RING_PULSE_S,
	//! so a player who joins or walks up mid-ring hears the next one.
	protected static void Pulse(LF_PhoneCall call)
	{
		call.m_fLastPulse = Now();
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		string heard;
		int phones;
		foreach (LF_PhoneComponent phone : LF_PhoneComponent.Phones())
		{
			if (!phone || !RingsAt(call, phone))
				continue;

			phones++;
			vector at = phone.GetOwner().GetOrigin();
			foreach (int playerId : playerIds)
			{
				IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
				if (!body || vector.Distance(body.GetOrigin(), at) > RING_HEAR_M)
					continue;

				SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
				if (!controller)
					continue;

				controller.LF_PhoneRing(at);
				heard += " " + playerId.ToString();
			}
		}

		LF_Debug.Info(FEATURE, string.Format("call %1 ring pulse at %2 phone(s); heard by player(s):%3", call.m_iId, phones, heard));
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		if (!s_aCalls || !GetGame() || !GetGame().GetWorld())
			return;

		float now = Now();
		foreach (LF_PhoneCall call : s_aCalls)
		{
			if (call.m_eState == LF_EPhoneCallState.RINGING)
			{
				if (now - call.m_fStarted >= RING_TIMEOUT_S)
					Miss(call);
				else if (now - call.m_fLastPulse >= RING_PULSE_S)
					Pulse(call);

				continue;
			}

			if (call.m_eState != LF_EPhoneCallState.CONNECTED)
				continue;

			// The cord: a talker who leaves the phone, dies or disconnects ends the call.
			if (!AtPhone(call.m_iCallerId, call.m_FromPhone) || !AtPhone(call.m_iAnswererId, call.m_ToPhone))
				End(call, "a talker left the phone");
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static bool AtPhone(int playerId, EntityID phoneId)
	{
		IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		IEntity phone = GetGame().GetWorld().FindEntityByID(phoneId);
		if (!body || !phone)
			return false;

		return vector.Distance(body.GetOrigin(), phone.GetOrigin()) <= CORD_M;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Miss(LF_PhoneCall call)
	{
		call.m_eState = LF_EPhoneCallState.MISSED;
		float rang = Now() - call.m_fStarted;
		LF_Debug.Info(FEATURE, string.Format("call %1 MISSED after %2 s: nobody picked up", call.m_iId, Math.Round(rang)));
		if (call.m_RingPhone != EntityID.INVALID)
		{
			Toast(call.m_iCallerId, "Nobody picked up.");
			return;
		}

		string uid = LF_Utils.GetPlayerUID(call.m_iCallerId);
		s_mMissedAt.Set(uid, Now());
		s_mMissedPhone.Set(uid, call.m_FromPhone);
		s_mMissedRang.Set(uid, rang);
		Toast(call.m_iCallerId, "Nobody picked up. You can leave a message at this phone.");
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a player picks up a ringing phone.
	static void Answer(IEntity user, IEntity phoneEntity)
	{
		if (!Replication.IsServer())
			return;

		Ensure();
		LF_PhoneComponent phone = LF_PhoneComponent.Of(phoneEntity);
		int playerId = PlayerIdOf(user);
		if (!phone || playerId <= 0)
			return;

		LF_PhoneCall ringing;
		foreach (LF_PhoneCall call : s_aCalls)
		{
			if (RingsAt(call, phone))
			{
				ringing = call;
				break;
			}
		}

		if (!ringing)
		{
			LF_Debug.Info(FEATURE, string.Format("answer refused for player %1: this phone is not ringing", playerId));
			Toast(playerId, "This phone is not ringing.");
			return;
		}

		if (ringing.m_iCallerId == playerId || CallOfPlayer(playerId))
		{
			Toast(playerId, "You are already on a call.");
			return;
		}

		// A dispatch phone is picked up by its service only; a ringing payphone by anyone.
		if (!phone.DispatchService().IsEmpty())
		{
			LF_ServiceRoute route = LF_ServiceCalls.Check(phone.DispatchService());
			if (!route.m_aReceivers.Contains(playerId))
			{
				LF_Debug.Info(FEATURE, string.Format("answer refused for player %1: not on duty for %2", playerId, phone.DispatchService()));
				Toast(playerId, "Only the service on duty picks up this phone.");
				return;
			}
		}

		ringing.m_eState = LF_EPhoneCallState.CONNECTED;
		ringing.m_ToPhone = phoneEntity.GetID();
		ringing.m_iAnswererId = playerId;
		ringing.m_fConnected = Now();
		ringing.m_eMode = s_eMode;
		ringing.m_sKey = string.Format("LF_call_%1_%2", ringing.m_iId, Math.RandomInt(100000, 999999));
		LF_Debug.Info(FEATURE, string.Format("call %1 CONNECTED after %2 s: player %3 (caller) <-> player %4 (answered at %5), mode %6", ringing.m_iId, Math.Round(Now() - ringing.m_fStarted), ringing.m_iCallerId, playerId, phone.Place(), typename.EnumToString(LF_EPhoneVoiceMode, s_eMode)));
		Toast(ringing.m_iCallerId, "Connected. Speak; hang up at the phone.");
		Toast(playerId, "Connected. Speak; hang up at the phone.");
		ConnectVoice(ringing);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: put a radio on the call's frequency and key.
	protected static BaseTransceiver TuneRadio(IEntity radioEntity, int frequency, string key, string label)
	{
		if (!radioEntity)
			return null;

		BaseRadioComponent radio = BaseRadioComponent.Cast(radioEntity.FindComponent(BaseRadioComponent));
		if (!radio || radio.TransceiversCount() < 1)
		{
			LF_Debug.Error(FEATURE, label + ": no radio component or no transceiver");
			return null;
		}

		BaseTransceiver transceiver = radio.GetTransceiver(0);
		radio.SetPower(true);
		radio.SetEncryptionKey(key);
		transceiver.SetFrequency(frequency);
		LF_Debug.Info(FEATURE, string.Format("%1: rplId %2 powered=%3 f=%4 kHz (asked %5, band %6..%7) key=%8 range=%9 m", label, LF_NetworkUtils.GetRplId(radioEntity), radio.IsPowered(), transceiver.GetFrequency(), frequency, transceiver.GetMinFrequency(), transceiver.GetMaxFrequency(), radio.GetEncryptionKey(), transceiver.GetRange()));
		return transceiver;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, CARRIED mode: a handheld for one talker; the new radio is the one not carried before.
	protected static IEntity HandRadio(int playerId)
	{
		IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		array<BaseRadioComponent> before = {};
		LF_ServiceRadio.FindCarried(body, before);
		SCR_InventoryStorageManagerComponent inventory = LF_Component<SCR_InventoryStorageManagerComponent>.Find(body);
		if (!inventory || !LF_InventoryUtils.AddItem(inventory, LF_ServiceRadio.RADIO_PREFAB))
		{
			LF_Debug.Error(FEATURE, string.Format("no radio could be put in player %1's inventory", playerId));
			return null;
		}

		array<BaseRadioComponent> after = {};
		LF_ServiceRadio.FindCarried(body, after);
		foreach (BaseRadioComponent radio : after)
		{
			if (before.Find(radio) < 0)
				return radio.GetOwner();
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static void ConnectVoice(LF_PhoneCall call)
	{
		LF_PhoneComponent from = LF_PhoneComponent.ById(call.m_FromPhone);
		LF_PhoneComponent to = LF_PhoneComponent.ById(call.m_ToPhone);
		if (!from || !to)
		{
			LF_Debug.Error(FEATURE, string.Format("call %1 has no voice: a phone is gone", call.m_iId));
			return;
		}

		IEntity radioA;
		IEntity radioB;
		if (call.m_eMode == LF_EPhoneVoiceMode.CARRIED)
		{
			// A handheld tunes 32000..68000 kHz in 200 kHz steps.
			call.m_iFrequency = CARRIED_BASE_FREQUENCY + (call.m_iId % 40) * 200;
			radioA = HandRadio(call.m_iCallerId);
			radioB = HandRadio(call.m_iAnswererId);
			if (radioA)
				call.m_CallerRadio = radioA.GetID();
			if (radioB)
				call.m_AnswererRadio = radioB.GetID();
		}
		else
		{
			call.m_iFrequency = BASE_FREQUENCY + call.m_iId;
			radioA = from.ServerLine();
			radioB = to.ServerLine();
		}

		BaseTransceiver a = TuneRadio(radioA, call.m_iFrequency, call.m_sKey, string.Format("call %1 caller line", call.m_iId));
		BaseTransceiver b = TuneRadio(radioB, call.m_iFrequency, call.m_sKey, string.Format("call %1 answerer line", call.m_iId));
		if (!a || !b)
		{
			LF_Debug.Error(FEATURE, string.Format("call %1 has no voice: a line is missing", call.m_iId));
			return;
		}

		float apart = vector.Distance(radioA.GetOrigin(), radioB.GetOrigin());
		LF_Debug.Info(FEATURE, string.Format("call %1 lines are %2 m apart", call.m_iId, Math.Round(apart)));
		SendVoice(call.m_iCallerId, true, call.m_iId, LF_NetworkUtils.GetRplId(radioA), call.m_iFrequency);
		SendVoice(call.m_iAnswererId, true, call.m_iId, LF_NetworkUtils.GetRplId(radioB), call.m_iFrequency);

		// Who does the radio layer deliver to? One probe with the call's key, one with a wrong key.
		Probe(call, a, call.m_sKey, "right key");
		Probe(call, a, "LF_wrong_key", "wrong key");
		// ListTuned is not called: RadioManagerEntity.GetTransceiversInRange crashed the dedicated
		// server (access violation, 2026-11-12 run 1).
	}

	//------------------------------------------------------------------------------------------------
	protected static void SendVoice(int playerId, bool on, int callId, RplId line, int frequency)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (!controller)
		{
			LF_Debug.Warn(FEATURE, string.Format("call %1: player %2 has no controller, voice order not sent", callId, playerId));
			return;
		}

		controller.LF_PhoneVoiceOrder(on, callId, line, frequency);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Probe(LF_PhoneCall call, BaseTransceiver from, string key, string label)
	{
		LF_PhoneProbeMessage probe = new LF_PhoneProbeMessage();
		probe.m_iCallId = call.m_iId;
		probe.m_sLabel = label;
		probe.SetEncryptionKey(key);
		s_aProbes.Insert(probe);
		if (s_aProbes.Count() > 16)
			s_aProbes.RemoveOrdered(0);

		LF_Debug.Info(FEATURE, string.Format("call %1 probe '%2' sent on %3 kHz", call.m_iId, label, from.GetFrequency()));
		from.BeginTransmission(probe);
	}

	//------------------------------------------------------------------------------------------------
	static void OnProbeDelivered(LF_PhoneProbeMessage probe, BaseTransceiver receiver, int freq, float quality)
	{
		string who = "unknown";
		if (receiver && receiver.GetRadio() && receiver.GetRadio().GetOwner())
		{
			IEntity owner = receiver.GetRadio().GetOwner();
			who = string.Format("rplId %1 at %2 key=%3", LF_NetworkUtils.GetRplId(owner), owner.GetOrigin(), receiver.GetRadio().GetEncryptionKey());
		}

		LF_Debug.Info(FEATURE, string.Format("call %1 probe '%2' DELIVERED to %3 on %4 kHz quality %5 (message key %6)", probe.m_iCallId, probe.m_sLabel, who, freq, quality, probe.GetEncryptionKey()));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: every powered transceiver in range that sits on the call's frequency.
	protected static void ListTuned(LF_PhoneCall call, vector at)
	{
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world || !world.GetRadioManager())
		{
			LF_Debug.Warn(FEATURE, "no radio manager in this world; tuned list skipped");
			return;
		}

		array<BaseTransceiver> all = {};
		int count = world.GetRadioManager().GetTransceiversInRange(at, 60000, all);
		int tuned;
		foreach (BaseTransceiver transceiver : all)
		{
			if (transceiver && transceiver.GetFrequency() == call.m_iFrequency)
				tuned++;
		}

		LF_Debug.Info(FEATURE, string.Format("call %1: %2 powered transceiver(s) in the world, %3 on %4 kHz", call.m_iId, count, tuned, call.m_iFrequency));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: every use of voice by a talker of a live call, counted.
	protected static void OnVoNUsed(int senderId)
	{
		if (!s_aCalls)
			return;

		foreach (LF_PhoneCall call : s_aCalls)
		{
			if (call.m_eState != LF_EPhoneCallState.CONNECTED)
				continue;

			if (call.m_iCallerId != senderId && call.m_iAnswererId != senderId)
				continue;

			call.m_iVonUses++;
			if (call.m_iVonUses == 1 || call.m_iVonUses % 200 == 0)
				LF_Debug.Info(FEATURE, string.Format("call %1: server saw voice from player %2 (%3 uses so far)", call.m_iId, senderId, call.m_iVonUses));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a talker hangs up, or a caller gives up on a ringing call.
	static void HangUp(IEntity user)
	{
		if (!Replication.IsServer())
			return;

		Ensure();
		int playerId = PlayerIdOf(user);
		LF_PhoneCall call = CallOfPlayer(playerId);
		if (!call)
		{
			Toast(playerId, "You are not on a call.");
			return;
		}

		End(call, string.Format("player %1 hung up", playerId));
	}

	//------------------------------------------------------------------------------------------------
	protected static void End(LF_PhoneCall call, string why)
	{
		bool wasConnected = call.m_eState == LF_EPhoneCallState.CONNECTED;
		call.m_eState = LF_EPhoneCallState.ENDED;
		float length;
		if (wasConnected)
			length = Now() - call.m_fConnected;

		LF_Debug.Info(FEATURE, string.Format("call %1 ENDED: %2; talked %3 s; server saw %4 voice use(s)", call.m_iId, why, Math.Round(length), call.m_iVonUses));
		Toast(call.m_iCallerId, "Call ended.");
		Toast(call.m_iAnswererId, "Call ended.");
		if (!wasConnected)
			return;

		SendVoice(call.m_iCallerId, false, call.m_iId, RplId.Invalid(), 0);
		SendVoice(call.m_iAnswererId, false, call.m_iId, RplId.Invalid(), 0);
		if (call.m_eMode == LF_EPhoneVoiceMode.CARRIED)
		{
			DeleteRadio(call.m_CallerRadio);
			DeleteRadio(call.m_AnswererRadio);
			return;
		}

		LF_PhoneComponent from = LF_PhoneComponent.ById(call.m_FromPhone);
		LF_PhoneComponent to = LF_PhoneComponent.ById(call.m_ToPhone);
		BaseTransceiver a;
		if (from)
			a = TuneRadio(from.ServerLine(), BASE_FREQUENCY, string.Format("LF_line_idle_%1_a", call.m_iId), string.Format("call %1 caller line idle", call.m_iId));
		if (to)
			TuneRadio(to.ServerLine(), BASE_FREQUENCY + 50000, string.Format("LF_line_idle_%1_b", call.m_iId), string.Format("call %1 answerer line idle", call.m_iId));

		// After the hang-up the old key reaches nobody.
		if (a)
		{
			a.SetFrequency(call.m_iFrequency);
			Probe(call, a, call.m_sKey, "after hang-up");
			a.SetFrequency(BASE_FREQUENCY);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void DeleteRadio(EntityID id)
	{
		if (id == EntityID.INVALID)
			return;

		IEntity radio = GetGame().GetWorld().FindEntityByID(id);
		if (radio)
			SCR_EntityHelper.DeleteEntityAndChildren(radio);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the caller of an unanswered call leaves a call-back request at the same phone.
	static void LeaveMessage(IEntity user, IEntity phoneEntity, string summary)
	{
		if (!Replication.IsServer())
			return;

		Ensure();
		int playerId = PlayerIdOf(user);
		LF_PhoneComponent phone = LF_PhoneComponent.Of(phoneEntity);
		if (!phone || playerId <= 0)
			return;

		string uid = LF_Utils.GetPlayerUID(playerId);
		float missedAt = -1;
		EntityID missedPhone = EntityID.INVALID;
		float rang;
		s_mMissedAt.Find(uid, missedAt);
		s_mMissedPhone.Find(uid, missedPhone);
		s_mMissedRang.Find(uid, rang);
		if (missedAt < 0 || Now() - missedAt > MISSED_WINDOW_S || missedPhone != phoneEntity.GetID())
		{
			LF_Debug.Info(FEATURE, string.Format("message refused for player %1: no unanswered call from this phone in the last %2 s", playerId, MISSED_WINDOW_S));
			Toast(playerId, "Call first. A message can be left when nobody picks up.");
			return;
		}

		LF_PhoneMessage message = new LF_PhoneMessage();
		message.m_iId = s_iNextId++;
		message.m_sCallerUid = uid;
		message.m_sCallerName = GetGame().GetPlayerManager().GetPlayerName(playerId);
		message.m_sPlace = phone.Place();
		message.m_sSummary = summary;
		message.m_Phone = phoneEntity.GetID();
		message.m_fLeft = Now();
		message.m_fRang = rang;
		s_aMessages.Insert(message);
		if (s_aMessages.Count() > MAX_MESSAGES)
			s_aMessages.RemoveOrdered(0);

		s_mMissedAt.Remove(uid);
		LF_Debug.Info(FEATURE, string.Format("message %1 left by player %2 (%3) from %4: '%5'; %6 waiting", message.m_iId, playerId, message.m_sCallerName, message.m_sPlace, summary, s_aMessages.Count()));
		Toast(playerId, "Message left. The police can call this phone back.");
	}

	//------------------------------------------------------------------------------------------------
	protected static bool OnDutyAt(int playerId, LF_PhoneComponent phone)
	{
		if (!phone || phone.DispatchService().IsEmpty())
			return false;

		return LF_ServiceCalls.Check(phone.DispatchService()).m_aReceivers.Contains(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an officer reads the waiting messages at the dispatch phone.
	static void ReadMessages(IEntity user, IEntity phoneEntity)
	{
		if (!Replication.IsServer())
			return;

		Ensure();
		int playerId = PlayerIdOf(user);
		if (!OnDutyAt(playerId, LF_PhoneComponent.Of(phoneEntity)))
		{
			Toast(playerId, "Only the service on duty reads these messages.");
			return;
		}

		LF_Debug.Info(FEATURE, string.Format("player %1 reads %2 message(s)", playerId, s_aMessages.Count()));
		if (s_aMessages.IsEmpty())
		{
			Toast(playerId, "No messages.");
			return;
		}

		foreach (LF_PhoneMessage message : s_aMessages)
		{
			string line = string.Format("#%1, %2 min ago, %3, rang %4 s: %5", message.m_iId, Math.Round((Now() - message.m_fLeft) / 60), message.m_sPlace, Math.Round(message.m_fRang), message.m_sSummary);
			LF_Debug.Info(FEATURE, "message listed: " + line);
			Toast(playerId, line);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an officer returns the oldest message; the phone it was left from rings.
	static void CallBack(IEntity user, IEntity phoneEntity)
	{
		if (!Replication.IsServer())
			return;

		Ensure();
		int playerId = PlayerIdOf(user);
		LF_PhoneComponent phone = LF_PhoneComponent.Of(phoneEntity);
		if (!OnDutyAt(playerId, phone))
		{
			Toast(playerId, "Only the service on duty calls back.");
			return;
		}

		if (s_aMessages.IsEmpty())
		{
			Toast(playerId, "No messages.");
			return;
		}

		if (CallOfPlayer(playerId) || PhoneBusy(phone))
		{
			Toast(playerId, "The line is busy.");
			return;
		}

		LF_PhoneMessage message = s_aMessages[0];
		LF_PhoneComponent target = LF_PhoneComponent.ById(message.m_Phone);
		if (!target)
		{
			LF_Debug.Error(FEATURE, string.Format("message %1 dropped: its phone is gone", message.m_iId));
			s_aMessages.RemoveOrdered(0);
			return;
		}

		LF_PhoneCall call = new LF_PhoneCall();
		call.m_iId = s_iNextId++;
		call.m_eState = LF_EPhoneCallState.RINGING;
		call.m_sService = phone.DispatchService();
		call.m_FromPhone = phoneEntity.GetID();
		call.m_RingPhone = message.m_Phone;
		call.m_iCallerId = playerId;
		call.m_fStarted = Now();
		s_aCalls.Insert(call);
		phone.ServerLine();
		target.ServerLine();
		LF_Debug.Info(FEATURE, string.Format("call %1 RINGING: player %2 calls back message %3; the phone at %4 rings", call.m_iId, playerId, message.m_iId, message.m_sPlace));
		Toast(playerId, "Ringing the phone at " + message.m_sPlace + "...");

		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int other : playerIds)
		{
			if (LF_Utils.GetPlayerUID(other) == message.m_sCallerUid)
				Toast(other, "The police are calling you back at the phone at " + message.m_sPlace + ".");
		}

		s_aMessages.RemoveOrdered(0);
		Pulse(call);
	}
}

//------------------------------------------------------------------------------------------------
//! The talking machine's side of a call: transmit through the line the server named, and count
//! what the engine captures and delivers. `harness phone` prints Describe().
class LF_PhoneVoice
{
	static const ResourceName RING_PROJECT = "{FD5E9674E51B9D13}Sounds/Props/Military/Radios/Props_Radios_Signals.acp";
	static const string RING_EVENT = "SOUND_RADIO_SIGNAL_2";
	static const int HOLD_MS = 250;
	static const int MAX_TRIES = 40;

	protected static bool s_bOn;
	protected static int s_iCallId;
	protected static RplId s_Line;
	protected static int s_iFrequency;
	protected static int s_iTries;
	protected static bool s_bUp;
	protected static bool s_bCaptureOk;
	protected static bool s_bHooked;
	protected static int s_iCaptures;
	protected static int s_iCapturesOnLine;
	protected static int s_iReceives;
	protected static int s_iReceivesOnLine;
	protected static int s_iLastRxFrom;
	protected static int s_iLastRxFreq;
	protected static int s_iRings;
	protected static bool s_bRingPlayed;
	protected static string s_sLast = "idle";

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_bOn = false;
		s_bUp = false;
		s_bHooked = false;
		s_iRings = 0;
		s_sLast = "idle";
	}

	//------------------------------------------------------------------------------------------------
	protected static SCR_VoNComponent LocalVon()
	{
		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character)
			return null;

		return SCR_VoNComponent.Cast(character.FindComponent(SCR_VoNComponent));
	}

	//------------------------------------------------------------------------------------------------
	protected static BaseTransceiver LineTransceiver()
	{
		IEntity line = LF_NetworkUtils.FindEntityByRplId(s_Line);
		if (!line)
			return null;

		BaseRadioComponent radio = BaseRadioComponent.Cast(line.FindComponent(BaseRadioComponent));
		if (!radio || radio.TransceiversCount() < 1)
			return null;

		return radio.GetTransceiver(0);
	}

	//------------------------------------------------------------------------------------------------
	static void Ring(vector at)
	{
		s_iRings++;
		// The ring plays from the phone prop nearest the ringing point on this client.
		IEntity prop;
		foreach (LF_PhoneComponent phone : LF_PhoneComponent.Phones())
		{
			if (phone && vector.Distance(phone.GetOwner().GetOrigin(), at) < 1)
				prop = phone.GetOwner();
		}

		s_bRingPlayed = prop != null;
		if (prop)
		{
			SCR_AudioSourceConfiguration config = new SCR_AudioSourceConfiguration();
			config.m_sSoundProject = RING_PROJECT;
			config.m_sSoundEventName = RING_EVENT;
			SCR_SoundManagerModule.CreateAndPlayAudioSource(prop, config);
		}

		if (s_iRings == 1 || s_iRings % 5 == 0)
			LF_Debug.Info(LF_PhoneCalls.FEATURE, string.Format("ring %1 heard on this client at %2, played at the phone prop=%3", s_iRings, at, s_bRingPlayed));
	}

	//------------------------------------------------------------------------------------------------
	static void Order(bool on, int callId, RplId line, int frequency)
	{
		if (!on)
		{
			Stop();
			return;
		}

		s_bOn = true;
		s_bUp = false;
		s_iCallId = callId;
		s_Line = line;
		s_iFrequency = frequency;
		s_iTries = 0;
		s_iCaptures = 0;
		s_iCapturesOnLine = 0;
		s_iReceives = 0;
		s_iReceivesOnLine = 0;
		s_iLastRxFrom = 0;
		s_iLastRxFreq = 0;
		s_sLast = "ordered";
		LF_Debug.Info(LF_PhoneCalls.FEATURE, string.Format("voice order: call %1, line rplId %2, %3 kHz", callId, line, frequency));
		GetGame().GetCallqueue().Remove(Hold);
		GetGame().GetCallqueue().CallLater(Hold, HOLD_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! While on a call: keep the microphone open on the line. The vanilla push-to-talk keys reset the
	//! comm method and stop the capture when released, so this is asserted again every HOLD_MS.
	protected static void Hold()
	{
		if (!s_bOn)
		{
			GetGame().GetCallqueue().Remove(Hold);
			return;
		}

		SCR_VoNComponent von = LocalVon();
		BaseTransceiver transceiver = LineTransceiver();
		if (!von || !transceiver)
		{
			s_iTries++;
			if (s_iTries == 1 || s_iTries % 8 == 0)
				LF_Debug.Warn(LF_PhoneCalls.FEATURE, string.Format("call %1: line not on this client yet (try %2, von=%3, line=%4)", s_iCallId, s_iTries, von != null, transceiver != null));

			if (s_iTries >= MAX_TRIES && !s_bUp)
			{
				LF_Debug.Error(LF_PhoneCalls.FEATURE, string.Format("call %1: no voice on this client, the line never streamed in", s_iCallId));
				s_sLast = "line never streamed";
				s_bOn = false;
			}
			return;
		}

		if (!s_bHooked)
		{
			von.m_OnCaptureVON.Insert(OnCapture);
			von.m_OnReceivedVON.Insert(OnReceive);
			s_bHooked = true;
		}

		if (von.GetCommMethod() != ECommMethod.SQUAD_RADIO)
			von.SetCommMethod(ECommMethod.SQUAD_RADIO);

		if (von.GetTransmitRadio() != transceiver)
			von.SetTransmitRadio(transceiver);

		if (!von.IsTransmiting())
			s_bCaptureOk = von.SetCapture(true);

		if (!s_bUp)
		{
			s_bUp = true;
			s_sLast = "up";
			BaseRadioComponent radio = transceiver.GetRadio();
			LF_Debug.Info(LF_PhoneCalls.FEATURE, string.Format("voice up: call %1, transmit radio is the line=%2, line f=%3 kHz (ordered %4) key=%5 powered=%6, comm=%7, capture accepted=%8", s_iCallId, von.GetTransmitRadio() == transceiver, transceiver.GetFrequency(), s_iFrequency, radio.GetEncryptionKey(), radio.IsPowered(), typename.EnumToString(ECommMethod, von.GetCommMethod()), s_bCaptureOk));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void Stop()
	{
		if (!s_bOn && !s_bUp)
			return;

		s_bOn = false;
		s_bUp = false;
		GetGame().GetCallqueue().Remove(Hold);
		SCR_VoNComponent von = LocalVon();
		if (von)
		{
			von.SetCapture(false);
			von.SetTransmitRadio(null);
			von.SetCommMethod(ECommMethod.DIRECT);
			if (s_bHooked)
			{
				von.m_OnCaptureVON.Remove(OnCapture);
				von.m_OnReceivedVON.Remove(OnReceive);
			}
		}

		s_bHooked = false;
		s_sLast = "stopped";
		LF_Debug.Info(LF_PhoneCalls.FEATURE, string.Format("voice down: call %1; captures=%2 (on line %3), receives=%4 (on line %5)", s_iCallId, s_iCaptures, s_iCapturesOnLine, s_iReceives, s_iReceivesOnLine));
	}

	//------------------------------------------------------------------------------------------------
	protected static void OnCapture(BaseTransceiver transmitter)
	{
		s_iCaptures++;
		bool onLine = transmitter && transmitter == LineTransceiver();
		if (onLine)
			s_iCapturesOnLine++;

		if (s_iCaptures == 1)
			LF_Debug.Info(LF_PhoneCalls.FEATURE, string.Format("call %1: first capture frame, transmitter is the line=%2 (null transmitter=%3)", s_iCallId, onLine, transmitter == null));
	}

	//------------------------------------------------------------------------------------------------
	protected static void OnReceive(int playerId, BaseTransceiver receiver, int frequency, float quality)
	{
		s_iReceives++;
		bool onLine = receiver && receiver == LineTransceiver();
		if (onLine)
			s_iReceivesOnLine++;

		s_iLastRxFrom = playerId;
		s_iLastRxFreq = frequency;
		if (s_iReceives == 1 || (onLine && s_iReceivesOnLine == 1))
			LF_Debug.Info(LF_PhoneCalls.FEATURE, string.Format("call %1: voice received from player %2, receiver is the line=%3 (null receiver=%4), %5 kHz, quality %6", s_iCallId, playerId, onLine, receiver == null, frequency, quality));
	}

	//------------------------------------------------------------------------------------------------
	static string Describe()
	{
		string line = "none";
		BaseTransceiver transceiver;
		if (s_bOn)
			transceiver = LineTransceiver();

		bool tx;
		string comm = "none";
		SCR_VoNComponent von = LocalVon();
		if (von)
		{
			comm = typename.EnumToString(ECommMethod, von.GetCommMethod());
			tx = transceiver && von.GetTransmitRadio() == transceiver;
		}

		if (transceiver)
			line = string.Format("f=%1,key=%2,powered=%3", transceiver.GetFrequency(), transceiver.GetRadio().GetEncryptionKey(), transceiver.GetRadio().IsPowered());

		return string.Format("voice=%1 call=%2 line=[%3] txOnLine=%4 comm=%5 captureOk=%6 captures=%7/%8 receives=%9", s_sLast, s_iCallId, line, tx, comm, s_bCaptureOk, s_iCapturesOnLine, s_iCaptures, s_iReceivesOnLine) + string.Format("/%1 lastRxFrom=%2 lastRxFreq=%3 rings=%4 ringSound=%5", s_iReceives, s_iLastRxFrom, s_iLastRxFreq, s_iRings, s_bRingPlayed);
	}
}

//------------------------------------------------------------------------------------------------
//! Phone bridge on the player controller: server -> owner only.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	void LF_PhoneRing(vector at)
	{
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_PhoneRing(at);
		else
			Rpc(RpcDo_LF_PhoneRing, at);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_PhoneRing(vector at)
	{
		LF_PhoneVoice.Ring(at);
	}

	//------------------------------------------------------------------------------------------------
	void LF_PhoneVoiceOrder(bool on, int callId, RplId line, int frequency)
	{
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_PhoneVoiceOrder(on, callId, line, frequency);
		else
			Rpc(RpcDo_LF_PhoneVoiceOrder, on, callId, line, frequency);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_PhoneVoiceOrder(bool on, int callId, RplId line, int frequency)
	{
		LF_PhoneVoice.Order(on, callId, line, frequency);
	}
}

//------------------------------------------------------------------------------------------------
//! The phone actions. Call state lives on the server only in this spike, so every action shows
//! always and the server refuses with a toast.
class LF_PhoneActionBase : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer() || !pOwnerEntity || !pUserEntity)
			return;

		ServerPerform(pOwnerEntity, pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	protected void ServerPerform(IEntity phone, IEntity user);
}

class LF_PhoneDialAction : LF_PhoneActionBase
{
	[Attribute("police", UIWidgets.EditBox, "Service key from Configs/Comms/LF_ServiceCalls.conf")]
	protected string m_sService;

	override protected void ServerPerform(IEntity phone, IEntity user)
	{
		LF_PhoneCalls.Dial(user, phone, m_sService);
	}
}

class LF_PhoneAnswerAction : LF_PhoneActionBase
{
	override protected void ServerPerform(IEntity phone, IEntity user)
	{
		LF_PhoneCalls.Answer(user, phone);
	}
}

class LF_PhoneHangUpAction : LF_PhoneActionBase
{
	override protected void ServerPerform(IEntity phone, IEntity user)
	{
		LF_PhoneCalls.HangUp(user);
	}
}

class LF_PhoneLeaveMessageAction : LF_PhoneActionBase
{
	[Attribute("Please call me back.", UIWidgets.EditBox, "The message left")]
	protected string m_sSummary;

	override protected void ServerPerform(IEntity phone, IEntity user)
	{
		LF_PhoneCalls.LeaveMessage(user, phone, m_sSummary);
	}
}

class LF_PhoneMessagesAction : LF_PhoneActionBase
{
	override protected void ServerPerform(IEntity phone, IEntity user)
	{
		LF_PhoneCalls.ReadMessages(user, phone);
	}
}

class LF_PhoneCallBackAction : LF_PhoneActionBase
{
	override protected void ServerPerform(IEntity phone, IEntity user)
	{
		LF_PhoneCalls.CallBack(user, phone);
	}
}

class LF_PhoneModeAction : LF_PhoneActionBase
{
	override protected void ServerPerform(IEntity phone, IEntity user)
	{
		LF_PhoneCalls.ToggleMode(user);
	}
}
