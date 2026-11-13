//------------------------------------------------------------------------------------------------
//! The talker's machine on a call (#147). The server names a radio entity (LF_PhoneVoice); this
//! adds a telephone entry for its transceiver to the vanilla voice controller and makes it the
//! active entry, so the player's normal radio key talks into the telephone and the voice HUD shows
//! it. When the call is over the entry goes and the radio entry they had before is active again.
//!
//! Why the entry is not an SCR_VONEntryRadio: that class's ActivateVON path refuses a radio whose
//! key is not the faction key (a call has its own key), and the radial menu reads its gadget's
//! owner for a preview, which a telephone line does not have. A plain SCR_VONEntry with
//! SQUAD_RADIO is safe in the menu, the HUD and the auto-tune, and the modded
//! SCR_VONController.SetActiveTransmit below points the voice at its transceiver. That override is
//! a modded vanilla surface: re-check it on every game update.
//------------------------------------------------------------------------------------------------
class LF_PhoneVONEntry : SCR_VONEntry
{
	//! The radio entity, by RplId: a transceiver handle read once on a client did not stay valid
	//! (2026-11-13 session), so it is looked up again at every use.
	protected RplId m_Radio = RplId.Invalid();

	//------------------------------------------------------------------------------------------------
	void SetRadio(RplId radio)
	{
		m_Radio = radio;
	}

	//------------------------------------------------------------------------------------------------
	BaseTransceiver GetTransceiver()
	{
		return LF_PhoneVoiceClient.TransceiverOf(m_Radio);
	}

	//------------------------------------------------------------------------------------------------
	override void InitEntry()
	{
		m_sText = WidgetManager.Translate("#LF-Phone_VonEntry");
		SetUsable(GetTransceiver() != null);
	}

	//------------------------------------------------------------------------------------------------
	override ECommMethod GetVONMethod()
	{
		return ECommMethod.SQUAD_RADIO;
	}
}

//------------------------------------------------------------------------------------------------
//! The telephone entry talks through its own transceiver; every other entry is vanilla.
modded class SCR_VONController
{
	//------------------------------------------------------------------------------------------------
	override protected void SetActiveTransmit(notnull SCR_VONEntry entry)
	{
		LF_PhoneVONEntry phone = LF_PhoneVONEntry.Cast(entry);
		if (!phone)
		{
			super.SetActiveTransmit(entry);
			return;
		}

		if (!m_VONComp && !AssignVONComponent())
			return;

		BaseTransceiver transceiver = phone.GetTransceiver();
		if (!transceiver)
		{
			LF_Debug.Warn(LF_PhoneVoice.FEATURE, "telephone entry has no transceiver; talking directly instead");
			m_VONComp.SetCommMethod(ECommMethod.DIRECT);
			m_VONComp.SetTransmitRadio(null);
			return;
		}

		m_VONComp.SetCommMethod(ECommMethod.SQUAD_RADIO);
		m_VONComp.SetTransmitRadio(transceiver);
		SetEntryActive(entry);
		LF_PhoneVoiceClient.NoteTransmitSet();
	}
}

//------------------------------------------------------------------------------------------------
class LF_PhoneVoiceClient
{
	//! The line or handset may stream in after the order: wait for it this often, this many times.
	static const int ATTACH_RETRY_MS = 250; // lf-fixed: streaming wait step, not a gameplay knob
	static const int ATTACH_TRIES = 60; // lf-fixed: 15 s streaming budget, not a gameplay knob

	protected static ref LF_PhoneVONEntry s_Entry;
	protected static SCR_VONEntry s_Previous;
	protected static int s_iCallId;
	protected static RplId s_Radio;
	protected static int s_iFrequency;
	protected static int s_iTries;
	protected static bool s_bWaiting;
	protected static bool s_bHooked;
	protected static int s_iTransmitSets;
	protected static int s_iCaptures;
	protected static int s_iCapturesOnLine;
	protected static int s_iReceives;
	protected static int s_iReceivesOnLine;
	protected static string s_sState = "idle";

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Entry = null;
		s_Previous = null;
		s_bWaiting = false;
		s_bHooked = false;
		s_sState = "idle";
	}

	//------------------------------------------------------------------------------------------------
	protected static SCR_VONController Controller()
	{
		if (!GetGame() || !GetGame().GetPlayerController())
			return null;

		return SCR_VONController.Cast(GetGame().GetPlayerController().FindComponent(SCR_VONController));
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
	protected static BaseTransceiver RadioTransceiver()
	{
		return TransceiverOf(s_Radio);
	}

	//------------------------------------------------------------------------------------------------
	//! The first transceiver of the radio entity with this RplId on this machine, or null.
	static BaseTransceiver TransceiverOf(RplId radioId)
	{
		if (!radioId.IsValid())
			return null;

		IEntity entity = LF_NetworkUtils.FindEntityByRplId(radioId);
		if (!entity)
			return null;

		BaseRadioComponent radio = BaseRadioComponent.Cast(entity.FindComponent(BaseRadioComponent));
		if (!radio || radio.TransceiversCount() < 1)
			return null;

		return radio.GetTransceiver(0);
	}

	//------------------------------------------------------------------------------------------------
	//! From the server's owner RPC.
	static void Order(bool on, int callId, RplId radio, int frequency)
	{
		if (!on)
		{
			if (callId == s_iCallId)
				Detach("the call is over");
			return;
		}

		Detach("a new call");
		s_iCallId = callId;
		s_Radio = radio;
		s_iFrequency = frequency;
		s_iTries = 0;
		s_iTransmitSets = 0;
		s_iCaptures = 0;
		s_iCapturesOnLine = 0;
		s_iReceives = 0;
		s_iReceivesOnLine = 0;
		s_sState = "ordered";
		s_bWaiting = true;
		LF_Debug.Info(LF_PhoneVoice.FEATURE, string.Format("voice order: call %1, radio rplId %2, %3 kHz", callId, radio, frequency));
		GetGame().GetCallqueue().Remove(TryAttach);
		TryAttach();
		if (s_bWaiting)
			GetGame().GetCallqueue().CallLater(TryAttach, ATTACH_RETRY_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Waits for the radio and the voice controller, then adds the telephone entry once.
	protected static void TryAttach()
	{
		if (!s_bWaiting)
		{
			GetGame().GetCallqueue().Remove(TryAttach);
			return;
		}

		SCR_VONController controller = Controller();
		BaseTransceiver transceiver = RadioTransceiver();
		if (!controller || !transceiver)
		{
			s_iTries++;
			if (s_iTries >= ATTACH_TRIES)
			{
				LF_Debug.Error(LF_PhoneVoice.FEATURE, string.Format("call %1: no voice on this machine, the radio never streamed in (controller=%2, radio=%3)", s_iCallId, controller != null, transceiver != null));
				s_bWaiting = false;
				s_sState = "radio never streamed";
				GetGame().GetCallqueue().Remove(TryAttach);
			}
			return;
		}

		// Attach as soon as the radio entity is here. Its frequency and power can read 0 on this
		// machine until the entry is in the voice controller (2026-11-13 session: 0 for 15 s while
		// waiting, the call's values right after attaching); the entry looks the radio up again at
		// every key press, and LF_PhoneVoiceClient.Describe shows what this machine sees.

		s_bWaiting = false;
		GetGame().GetCallqueue().Remove(TryAttach);
		s_Previous = controller.GetActiveEntry();
		s_Entry = new LF_PhoneVONEntry();
		s_Entry.SetRadio(s_Radio);
		controller.AddEntry(s_Entry);
		controller.SetEntryActive(s_Entry);
		Hook(true);
		s_sState = "up";
		BaseRadioComponent radio = transceiver.GetRadio();
		LF_Debug.Info(LF_PhoneVoice.FEATURE, string.Format("voice up: call %1, telephone entry active=%2, radio f=%3 kHz (ordered %4) powered=%5 after %6 wait(s); hold the radio key to talk", s_iCallId, controller.GetActiveEntry() == s_Entry, transceiver.GetFrequency(), s_iFrequency, radio.IsPowered(), s_iTries));
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the telephone entry out and gives back the entry the player had before.
	protected static void Detach(string why)
	{
		s_bWaiting = false;
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(TryAttach);

		if (!s_Entry)
		{
			if (s_sState == "ordered")
				s_sState = "down";
			return;
		}

		SCR_VONController controller = Controller();
		if (controller)
		{
			controller.RemoveEntry(s_Entry);
			array<ref SCR_VONEntry> entries = {};
			controller.GetVONEntries(entries);
			if (s_Previous && entries.Find(s_Previous) >= 0)
				controller.SetEntryActive(s_Previous);
		}

		Hook(false);
		LF_Debug.Info(LF_PhoneVoice.FEATURE, string.Format("voice down: call %1 (%2); transmit set %3 time(s), captures %4 (on the telephone %5), receives %6 (on the telephone %7)", s_iCallId, why, s_iTransmitSets, s_iCaptures, s_iCapturesOnLine, s_iReceives, s_iReceivesOnLine));
		s_Entry = null;
		s_Previous = null;
		s_sState = "down";
	}

	//------------------------------------------------------------------------------------------------
	protected static void Hook(bool on)
	{
		SCR_VoNComponent von = LocalVon();
		if (!von || on == s_bHooked)
			return;

		if (on)
		{
			von.m_OnCaptureVON.Insert(OnCapture);
			von.m_OnReceivedVON.Insert(OnReceive);
		}
		else
		{
			von.m_OnCaptureVON.Remove(OnCapture);
			von.m_OnReceivedVON.Remove(OnReceive);
		}
		s_bHooked = on;
	}

	//------------------------------------------------------------------------------------------------
	//! From the modded controller: the radio key put the voice on the telephone.
	static void NoteTransmitSet()
	{
		s_iTransmitSets++;
		if (s_iTransmitSets == 1)
			LF_Debug.Info(LF_PhoneVoice.FEATURE, string.Format("call %1: the radio key talks into the telephone", s_iCallId));
	}

	//------------------------------------------------------------------------------------------------
	protected static void OnCapture(BaseTransceiver transmitter)
	{
		s_iCaptures++;
		if (transmitter && s_Entry && transmitter == s_Entry.GetTransceiver())
			s_iCapturesOnLine++;
	}

	//------------------------------------------------------------------------------------------------
	protected static void OnReceive(int playerId, BaseTransceiver receiver, int frequency, float quality)
	{
		s_iReceives++;
		bool onLine = frequency == s_iFrequency;
		if (onLine)
			s_iReceivesOnLine++;

		if (onLine && s_iReceivesOnLine == 1)
			LF_Debug.Info(LF_PhoneVoice.FEATURE, string.Format("call %1: voice heard from player %2 on the telephone, %3 kHz, quality %4", s_iCallId, playerId, frequency, quality));
	}

	//------------------------------------------------------------------------------------------------
	//! For the harness `phone` verb.
	static string Describe()
	{
		bool active;
		bool tx;
		SCR_VONController controller = Controller();
		if (controller && s_Entry)
			active = controller.GetActiveEntry() == s_Entry;

		SCR_VoNComponent von = LocalVon();
		if (von && s_Entry && s_Entry.GetTransceiver())
			tx = von.GetTransmitRadio() == s_Entry.GetTransceiver();

		string radio = "none";
		BaseTransceiver seen;
		if (s_Entry)
			seen = s_Entry.GetTransceiver();
		else if (s_bWaiting)
			seen = RadioTransceiver();
		if (seen)
			radio = string.Format("%1:%2", seen.GetFrequency(), seen.GetRadio().IsPowered());

		return string.Format("voice=%1 call=%2 entry=%3 radio=%4 txSet=%5 txNow=%6 captures=%7/%8", s_sState, s_iCallId, active, radio, s_iTransmitSets, tx, s_iCapturesOnLine, s_iCaptures) + string.Format(" receives=%1/%2", s_iReceivesOnLine, s_iReceives);
	}
}
