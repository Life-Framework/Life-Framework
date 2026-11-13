//------------------------------------------------------------------------------------------------
//! Telephone tuning (#147): how long a call rings, how far the receiver cord reaches, how long a
//! call may last, and the ring. Loaded from Configs/Comms/LF_Phone.conf. The field initializers
//! match the shipped .conf, so a missing file degrades to the same values.
//! Comms has no owner config.json section (tools/config/inventory.mjs leaves the area unrouted and
//! decision D8 names none for it), so these are .conf values today.
[BaseContainerProps(configRoot: true)]
class LF_PhoneConfig
{
	static const ResourceName CONFIG_PATH = "{84013E446C74CB2B}Configs/Comms/LF_Phone.conf";

	static const int DEFAULT_RING_SECONDS = 30;
	static const int DEFAULT_RING_PULSE_SECONDS = 4;
	static const float DEFAULT_CORD_METERS = 6;
	static const int DEFAULT_MAX_CALL_SECONDS = 600;
	static const ResourceName DEFAULT_RING_SOUND_PROJECT = "{FD5E9674E51B9D13}Sounds/Props/Military/Radios/Props_Radios_Signals.acp";
	static const string DEFAULT_RING_SOUND_EVENT = "SOUND_RADIO_SIGNAL_2";
	static const int DEFAULT_CALL_BACK_OFFER_SECONDS = 120;
	static const int DEFAULT_MESSAGE_MAX_CHARS = 120;
	static const int DEFAULT_MAX_CALL_BACKS = 20;
	static const int DEFAULT_VOICE_TRANSPORT = 0;

	[Attribute("30", UIWidgets.Auto, "Seconds a call rings at the far end before it counts as missed", params: "5 300")]
	int m_iRingSeconds = DEFAULT_RING_SECONDS;

	[Attribute("4", UIWidgets.Auto, "Seconds between two rings of a ringing telephone, on every machine that hears it", params: "1 30")]
	int m_iRingPulseSeconds = DEFAULT_RING_PULSE_SECONDS;

	[Attribute("6", UIWidgets.Auto, "Metres a talker may stand from the telephone. Further away, or downed, dead or gone, ends the call", params: "1 30")]
	float m_fCordMeters = DEFAULT_CORD_METERS;

	[Attribute("600", UIWidgets.Auto, "Seconds a connected call may last before the exchange cuts it", params: "30 3600")]
	int m_iMaxCallSeconds = DEFAULT_MAX_CALL_SECONDS;

	[Attribute("{FD5E9674E51B9D13}Sounds/Props/Military/Radios/Props_Radios_Signals.acp", UIWidgets.ResourceNamePicker, "Sound project of the ring a ringing telephone plays", "acp")]
	ResourceName m_sRingSoundProject = DEFAULT_RING_SOUND_PROJECT;

	[Attribute("SOUND_RADIO_SIGNAL_2", UIWidgets.EditBox, "Sound event of the ring")]
	string m_sRingSoundEvent = DEFAULT_RING_SOUND_EVENT;

	[Attribute("120", UIWidgets.Auto, "Seconds a caller whose call rang out, or found nobody on duty, may leave a call-back request at that telephone", params: "10 900")]
	int m_iCallBackOfferSeconds = DEFAULT_CALL_BACK_OFFER_SECONDS;

	[Attribute("120", UIWidgets.Auto, "Longest typed message on a call-back request, in characters. 0 allows no message, only the request", params: "0 200")]
	int m_iMessageMaxChars = DEFAULT_MESSAGE_MAX_CHARS;

	[Attribute("20", UIWidgets.Auto, "Most call-back requests waiting at once, all services together; the oldest goes first. They expire with the dispatch board (m_iBoardExpirySeconds)", params: "1 100")]
	int m_iMaxCallBacks = DEFAULT_MAX_CALL_BACKS;

	[Attribute("0", UIWidgets.Auto, "Where a call's two radios live (LF_EPhoneVoiceTransport): 0 the line at each telephone, 1 a handset item in each talker's inventory for the call", params: "0 1")]
	int m_iVoiceTransport = DEFAULT_VOICE_TRANSPORT;

	protected static ref LF_PhoneConfig s_Config;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Config = null;
	}

	//------------------------------------------------------------------------------------------------
	//! The loaded config. When the file does not resolve, logs once and returns built-in defaults.
	static LF_PhoneConfig Get()
	{
		if (s_Config)
			return s_Config;

		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (container && container.GetResource())
			s_Config = LF_PhoneConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));

		if (!s_Config)
		{
			LF_Debug.Error("Phone", string.Format("phone config does not resolve, using defaults: %1", CONFIG_PATH));
			s_Config = new LF_PhoneConfig();
		}

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	static int RingSeconds()
	{
		return Math.Max(Get().m_iRingSeconds, 1);
	}

	//------------------------------------------------------------------------------------------------
	static int RingPulseSeconds()
	{
		return Math.Max(Get().m_iRingPulseSeconds, 1);
	}

	//------------------------------------------------------------------------------------------------
	static float CordMeters()
	{
		return Math.Max(Get().m_fCordMeters, 1);
	}

	//------------------------------------------------------------------------------------------------
	static int MaxCallSeconds()
	{
		return Math.Max(Get().m_iMaxCallSeconds, 1);
	}

	//------------------------------------------------------------------------------------------------
	static int CallBackOfferSeconds()
	{
		return Math.Max(Get().m_iCallBackOfferSeconds, 1);
	}

	//------------------------------------------------------------------------------------------------
	static int MessageMaxChars()
	{
		return Math.Max(Get().m_iMessageMaxChars, 0);
	}

	//------------------------------------------------------------------------------------------------
	static int MaxCallBacks()
	{
		return Math.Max(Get().m_iMaxCallBacks, 1);
	}

	//------------------------------------------------------------------------------------------------
	static LF_EPhoneVoiceTransport VoiceTransport()
	{
		if (Get().m_iVoiceTransport == LF_EPhoneVoiceTransport.HANDSET)
			return LF_EPhoneVoiceTransport.HANDSET;

		return LF_EPhoneVoiceTransport.LINE;
	}

	//------------------------------------------------------------------------------------------------
	static ResourceName RingSoundProject()
	{
		return Get().m_sRingSoundProject;
	}

	//------------------------------------------------------------------------------------------------
	static string RingSoundEvent()
	{
		return Get().m_sRingSoundEvent;
	}
}
