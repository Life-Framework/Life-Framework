//------------------------------------------------------------------------------------------------
//! The voice of a telephone call (#147, docs/architecture/phone-calls-spike.md "Voice"). Script
//! cannot route audio. It can only pick the transceiver a player's voice goes out on, so a call is
//! two radios on one frequency no handheld can tune (BAND_MIN..BAND_MAX) with one key per call.
//! Between calls a radio sits on BAND_MIN with a fresh random key, so it matches nothing. The
//! radios stay powered, as in the spike: frequency and key are the switch, not power. Tune sets
//! power, key and frequency in the spike's order, the order its clients saw arrive.
//!
//! Where those two radios live is a transport, behind LF_PhoneVoiceTransport:
//! - LF_PhoneLineTransport (the default) uses the radio on each phone's line entity, so the voice
//!   leaves from the telephone the talker stands at. Whether a listener hears a prop transceiver
//!   they do not carry is decided in native code and is unproven: the human voice test in the PR
//!   decides it.
//! - LF_PhoneHandsetTransport puts a handset item (Prefabs/Comms/LF_PhoneHandset.et, a radio on the
//!   same private band) in each talker's inventory for the call and takes it back after. That is
//!   the plain carried-radio case, which vanilla voice handles.
//! `m_iVoiceTransport` in LF_Phone.conf picks one. Either way the server tells each talker's
//! machine which radio entity to talk through (an owner RPC with its RplId), and the client adds a
//! telephone entry to the vanilla voice controller (LF_PhoneVoiceClient), so the normal radio key
//! and the voice HUD work. Never call RadioManagerEntity.GetTransceiversInRange: it crashed the
//! dedicated server in the spike.
//------------------------------------------------------------------------------------------------
enum LF_EPhoneVoiceTransport
{
	//! The radio on each phone's line entity.
	LINE,
	//! A handset item in each talker's inventory for the length of the call.
	HANDSET
}

//------------------------------------------------------------------------------------------------
//! One connected call's voice: its frequency, its key and the two radio entities.
class LF_PhoneVoiceLink : Managed
{
	int m_iCallId;
	int m_iFrequency;
	string m_sKey;
	EntityID m_CallerRadio = EntityID.INVALID;
	EntityID m_AnswererRadio = EntityID.INVALID;
	int m_iCallerId;
	int m_iAnswererId;
}

//------------------------------------------------------------------------------------------------
//! The seam. Connect fills the link's two radios, tuned to its frequency and key and powered;
//! Disconnect undoes it. Server only.
class LF_PhoneVoiceTransport : Managed
{
	//------------------------------------------------------------------------------------------------
	string Name()
	{
		return "none";
	}

	//------------------------------------------------------------------------------------------------
	//! \return False when either radio could not be prepared; the call then has no voice.
	bool Connect(notnull LF_PhoneCall call, notnull LF_PhoneVoiceLink link)
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	void Disconnect(notnull LF_PhoneVoiceLink link)
	{
	}
}

//------------------------------------------------------------------------------------------------
//! The default: the line entity at each phone carries the radio.
class LF_PhoneLineTransport : LF_PhoneVoiceTransport
{
	//------------------------------------------------------------------------------------------------
	override string Name()
	{
		return "line";
	}

	//------------------------------------------------------------------------------------------------
	override bool Connect(notnull LF_PhoneCall call, notnull LF_PhoneVoiceLink link)
	{
		IEntity a = LineOf(call.m_sFromPhone);
		IEntity b = LineOf(call.m_sToPhone);
		if (!a || !b)
		{
			LF_Debug.Error(LF_PhoneVoice.FEATURE, string.Format("call %1 has no voice: a phone line is missing (%2, %3)", call.m_iId, call.m_sFromPhone, call.m_sToPhone));
			return false;
		}

		if (!LF_PhoneVoice.Tune(a, link.m_iFrequency, link.m_sKey) || !LF_PhoneVoice.Tune(b, link.m_iFrequency, link.m_sKey))
		{
			LF_Debug.Error(LF_PhoneVoice.FEATURE, string.Format("call %1 has no voice: a line has no radio (%2)", call.m_iId, LF_PhoneComponent.LINE_PREFAB));
			return false;
		}

		link.m_CallerRadio = a.GetID();
		link.m_AnswererRadio = b.GetID();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void Disconnect(notnull LF_PhoneVoiceLink link)
	{
		array<EntityID> radios = {link.m_CallerRadio, link.m_AnswererRadio};
		foreach (EntityID id : radios)
		{
			IEntity line = LF_PhoneVoice.Find(id);
			if (line)
				LF_PhoneVoice.Tune(line, LF_PhoneVoice.BAND_MIN, LF_PhoneVoice.IdleKey());
		}
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity LineOf(string phoneKey)
	{
		LF_PhoneComponent phone = LF_PhoneComponent.ByKey(phoneKey);
		if (!phone)
			return null;

		LF_PhoneLineComponent line = phone.ServerLine();
		if (!line)
			return null;

		return line.GetOwner();
	}
}

//------------------------------------------------------------------------------------------------
//! The alternative: a handset item in each talker's inventory for the call.
class LF_PhoneHandsetTransport : LF_PhoneVoiceTransport
{
	static const ResourceName HANDSET_PREFAB = "{3942EAE9AFB4F8F3}Prefabs/Comms/LF_PhoneHandset.et";

	//------------------------------------------------------------------------------------------------
	override string Name()
	{
		return "handset";
	}

	//------------------------------------------------------------------------------------------------
	override bool Connect(notnull LF_PhoneCall call, notnull LF_PhoneVoiceLink link)
	{
		IEntity a = Hand(call.m_iCallerId, call.m_iId);
		IEntity b = Hand(call.m_iAnswererId, call.m_iId);
		if (a)
			link.m_CallerRadio = a.GetID();
		if (b)
			link.m_AnswererRadio = b.GetID();

		if (!a || !b)
		{
			Disconnect(link);
			return false;
		}

		return LF_PhoneVoice.Tune(a, link.m_iFrequency, link.m_sKey) && LF_PhoneVoice.Tune(b, link.m_iFrequency, link.m_sKey);
	}

	//------------------------------------------------------------------------------------------------
	//! The handsets go back to the exchange wherever they are: carried, dropped or in a box.
	override void Disconnect(notnull LF_PhoneVoiceLink link)
	{
		array<EntityID> radios = {link.m_CallerRadio, link.m_AnswererRadio};
		foreach (EntityID id : radios)
		{
			IEntity handset = LF_PhoneVoice.Find(id);
			if (handset)
				SCR_EntityHelper.DeleteEntityAndChildren(handset);
		}
		link.m_CallerRadio = EntityID.INVALID;
		link.m_AnswererRadio = EntityID.INVALID;
	}

	//------------------------------------------------------------------------------------------------
	//! A handset in the player's inventory; the new radio is the one they did not carry before.
	protected IEntity Hand(int playerId, int callId)
	{
		IEntity body;
		if (GetGame() && GetGame().GetPlayerManager())
			body = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);

		return HandTo(body, callId);
	}

	//------------------------------------------------------------------------------------------------
	//! The seam tests use with a fixture character.
	IEntity HandTo(IEntity body, int callId)
	{
		SCR_InventoryStorageManagerComponent inventory = LF_Component<SCR_InventoryStorageManagerComponent>.Find(body);
		if (!inventory)
		{
			LF_Debug.Error(LF_PhoneVoice.FEATURE, string.Format("call %1: no inventory to hand a receiver to", callId));
			return null;
		}

		array<BaseRadioComponent> before = {};
		LF_ServiceRadio.FindCarried(body, before);
		if (!LF_InventoryUtils.AddItem(inventory, HANDSET_PREFAB))
		{
			LF_Debug.Error(LF_PhoneVoice.FEATURE, string.Format("call %1: the receiver did not fit in the inventory (%2)", callId, HANDSET_PREFAB));
			return null;
		}

		array<BaseRadioComponent> after = {};
		LF_ServiceRadio.FindCarried(body, after);
		foreach (BaseRadioComponent radio : after)
		{
			if (before.Find(radio) < 0)
				return radio.GetOwner();
		}

		LF_Debug.Error(LF_PhoneVoice.FEATURE, string.Format("call %1: the receiver went in but no new radio is carried", callId));
		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! The server side: connects a call's voice when it is picked up and drops it when it is over.
class LF_PhoneVoice
{
	static const string FEATURE = "Phone";
	//! The band of the line and handset prefabs (their RadioTransceiver data); no vanilla radio
	//! tunes it. BAND_MIN is the idle frequency and is never given to a call.
	static const int BAND_MIN = 100000; // lf-fixed: mirrors the Prefabs/Comms line and handset band, not a gameplay knob
	static const int BAND_MAX = 165000; // lf-fixed: mirrors the Prefabs/Comms line and handset band, not a gameplay knob

	protected static ref LF_PhoneVoiceTransport s_Transport;
	protected static ref map<int, ref LF_PhoneVoiceLink> s_mLinks;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Transport = null;
		s_mLinks = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the call's frequency in kHz, inside the band and never the idle BAND_MIN. Two live
	//! calls share one only when their ids are BAND_MAX - BAND_MIN apart.
	static int FrequencyFor(int callId)
	{
		int channels = BAND_MAX - BAND_MIN;
		int slot = (Math.AbsInt(callId) - 1) % channels;
		if (slot < 0)
			slot += channels;

		return BAND_MIN + 1 + slot;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: a call's key. The salt is random per call, so an old key never opens a new call.
	static string KeyFor(int callId, int salt)
	{
		return string.Format("LF_call_%1_%2", callId, salt);
	}

	//------------------------------------------------------------------------------------------------
	static string IdleKey()
	{
		return string.Format("LF_line_idle_%1", Math.RandomInt(100000, 999999));
	}

	//------------------------------------------------------------------------------------------------
	//! The configured transport; tests set their own.
	static LF_PhoneVoiceTransport Transport()
	{
		if (s_Transport)
			return s_Transport;

		if (LF_PhoneConfig.VoiceTransport() == LF_EPhoneVoiceTransport.HANDSET)
			s_Transport = new LF_PhoneHandsetTransport();
		else
			s_Transport = new LF_PhoneLineTransport();

		LF_Debug.Info(FEATURE, string.Format("voice transport: %1", s_Transport.Name()));
		return s_Transport;
	}

	//------------------------------------------------------------------------------------------------
	static void SetTransport(LF_PhoneVoiceTransport transport)
	{
		s_Transport = transport;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<int, ref LF_PhoneVoiceLink> Links()
	{
		if (!s_mLinks)
			s_mLinks = new map<int, ref LF_PhoneVoiceLink>();

		return s_mLinks;
	}

	//------------------------------------------------------------------------------------------------
	static LF_PhoneVoiceLink LinkOf(int callId)
	{
		if (!s_mLinks)
			return null;

		return s_mLinks.Get(callId);
	}

	//------------------------------------------------------------------------------------------------
	static IEntity Find(EntityID id)
	{
		if (id == EntityID.INVALID || !GetGame() || !GetGame().GetWorld())
			return null;

		return GetGame().GetWorld().FindEntityByID(id);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: put a radio entity's first transceiver on a frequency and key, and power it.
	static bool Tune(IEntity radioEntity, int frequency, string key)
	{
		if (!radioEntity)
			return false;

		BaseRadioComponent radio = BaseRadioComponent.Cast(radioEntity.FindComponent(BaseRadioComponent));
		if (!radio || radio.TransceiversCount() < 1)
			return false;

		// The spike's order (power, key, frequency) is the one its clients saw arrive.
		BaseTransceiver transceiver = radio.GetTransceiver(0);
		radio.SetPower(true);
		radio.SetEncryptionKey(key);
		transceiver.SetFrequency(frequency);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! From LF_PhoneCalls after every change: connect on the pick-up, disconnect once it is over.
	static void OnCallChanged(notnull LF_PhoneCall call)
	{
		if (!Replication.IsServer())
			return;

		if (call.m_eState == LF_EPhoneCallState.CONNECTED && !LinkOf(call.m_iId))
		{
			Connect(call);
			return;
		}

		if (!call.IsLive() && LinkOf(call.m_iId))
			Disconnect(call.m_iId);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Connect(notnull LF_PhoneCall call)
	{
		LF_PhoneVoiceLink link = new LF_PhoneVoiceLink();
		link.m_iCallId = call.m_iId;
		link.m_iFrequency = FrequencyFor(call.m_iId);
		link.m_sKey = KeyFor(call.m_iId, Math.RandomInt(100000, 999999));
		link.m_iCallerId = call.m_iCallerId;
		link.m_iAnswererId = call.m_iAnswererId;
		Links().Set(call.m_iId, link);

		LF_PhoneVoiceTransport transport = Transport();
		if (!transport.Connect(call, link))
		{
			LF_Debug.Error(FEATURE, string.Format("call %1: the %2 transport could not connect the voice; the call stays up without it", call.m_iId, transport.Name()));
			return;
		}

		LF_Debug.Info(FEATURE, string.Format("call %1 voice up over the %2 transport: %3 kHz, caller radio %4, answerer radio %5", call.m_iId, transport.Name(), link.m_iFrequency, Describe(link.m_CallerRadio), Describe(link.m_AnswererRadio)));
		Order(link.m_iCallerId, true, link.m_iCallId, link.m_CallerRadio, link.m_iFrequency);
		Order(link.m_iAnswererId, true, link.m_iCallId, link.m_AnswererRadio, link.m_iFrequency);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Disconnect(int callId)
	{
		LF_PhoneVoiceLink link = LinkOf(callId);
		if (!link)
			return;

		Order(link.m_iCallerId, false, callId, EntityID.INVALID, 0);
		Order(link.m_iAnswererId, false, callId, EntityID.INVALID, 0);
		Transport().Disconnect(link);
		s_mLinks.Remove(callId);
		LF_Debug.Info(FEATURE, string.Format("call %1 voice down", callId));
	}

	//------------------------------------------------------------------------------------------------
	//! "rplId R powered=1 f=F key=K" of a radio entity, for the logs.
	static string Describe(EntityID id)
	{
		IEntity entity = Find(id);
		if (!entity)
			return "none";

		BaseRadioComponent radio = BaseRadioComponent.Cast(entity.FindComponent(BaseRadioComponent));
		if (!radio || radio.TransceiversCount() < 1)
			return "no radio";

		return string.Format("rplId %1 powered=%2 f=%3 key=%4", LF_NetworkUtils.GetRplId(entity), radio.IsPowered(), radio.GetTransceiver(0).GetFrequency(), radio.GetEncryptionKey());
	}

	//------------------------------------------------------------------------------------------------
	//! Tell one talker's machine which radio to talk through, or that the call's voice is over.
	protected static void Order(int playerId, bool on, int callId, EntityID radio, int frequency)
	{
		if (playerId <= 0 || !GetGame() || !GetGame().GetPlayerManager() || !GetGame().GetPlayerManager().IsPlayerConnected(playerId))
			return;

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (!controller)
		{
			LF_Debug.Warn(FEATURE, string.Format("call %1: player %2 has no controller; voice order not sent", callId, playerId));
			return;
		}

		RplId rplId = RplId.Invalid();
		IEntity entity = Find(radio);
		if (entity)
			rplId = LF_NetworkUtils.GetRplId(entity);

		controller.LF_PhoneVoiceOrder(on, callId, rplId, frequency);
	}
}

//------------------------------------------------------------------------------------------------
//! server -> owner: which radio this player talks through on a call.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	void LF_PhoneVoiceOrder(bool on, int callId, RplId radio, int frequency)
	{
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_PhoneVoiceOrder(on, callId, radio, frequency);
		else
			Rpc(RpcDo_LF_PhoneVoiceOrder, on, callId, radio, frequency);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_PhoneVoiceOrder(bool on, int callId, RplId radio, int frequency)
	{
		LF_PhoneVoiceClient.Order(on, callId, radio, frequency);
	}
}
