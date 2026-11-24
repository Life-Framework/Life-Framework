//------------------------------------------------------------------------------------------------
//! Gang radio bands (GA-C7, #4215): the native R148 range a gang band may sit in. Loaded from
//! Configs/Comms/LF_GangRadio.conf; the members carry no initializers (#1077) and CreateDefault
//! builds the shipped values, so a missing file degrades to the same band.
[BaseContainerProps(configRoot: true)]
class LF_GangRadioConfig
{
	static const ResourceName CONFIG_PATH = "{DF3DF77B46B7115F}Configs/Comms/LF_GangRadio.conf";

	[Attribute("44000", UIWidgets.Auto, "Lowest gang frequency in kHz. Keep it inside the gang radio's tunable range (R148: 38000 to 54000) and above the R148 factory channel (38000)", params: "1 1000000")]
	int m_iMinKHz;

	[Attribute("54000", UIWidgets.Auto, "Highest gang frequency in kHz", params: "1 1000000")]
	int m_iMaxKHz;

	[Attribute("200", UIWidgets.Auto, "Spacing between gang channels in kHz; the R148 tunes in 200 kHz steps", params: "1 10000")]
	int m_iStepKHz;

	[Attribute(desc: "Frequencies in kHz never given to a gang (the service radios' own, and any band a pack keeps)")]
	ref array<int> m_aReservedKHz;

	//------------------------------------------------------------------------------------------------
	static LF_GangRadioConfig CreateDefault()
	{
		LF_GangRadioConfig config = new LF_GangRadioConfig();
		config.m_iMinKHz = 44000;
		config.m_iMaxKHz = 54000;
		config.m_iStepKHz = 200;
		config.m_aReservedKHz = {};
		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! The loaded config. When the file does not resolve, logs once and returns built-in defaults.
	static LF_GangRadioConfig Load()
	{
		LF_GangRadioConfig config;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (container && container.GetResource())
			config = LF_GangRadioConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));

		if (!config)
		{
			LF_Debug.Error(LF_GangRadio.FEATURE, string.Format("gang radio config does not resolve, using defaults: %1", CONFIG_PATH));
			config = CreateDefault();
		}

		if (!config.m_aReservedKHz)
			config.m_aReservedKHz = {};

		return config;
	}
}

//------------------------------------------------------------------------------------------------
//! Server entry point for gang bands. BandFor gives an organization its hidden band (assigned on
//! first use); the band goes back to the free set when the organization loses its last member
//! (LF_OnOrganizationEmptied, the disband). Nothing here replicates: the radio item (GA-C8) tunes a
//! member's radio on the server and vanilla radio replication carries it. The service radio
//! frequencies (LF_ServiceCalls.conf) are always reserved, whatever the gang config says.
class LF_GangRadio
{
	static const string FEATURE = "GangRadio";

	protected static ref LF_GangRadioBands s_Bands;
	protected static ref LF_GangRadioEmptiedListener s_Emptied;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Bands = null;
		if (s_Emptied)
			LF_EventBus.Unsubscribe(LF_OnOrganizationEmptied, s_Emptied);

		s_Emptied = null;
	}

	//------------------------------------------------------------------------------------------------
	//! The live allocator, built from the config on first use.
	static LF_GangRadioBands Bands()
	{
		if (s_Bands)
			return s_Bands;

		LF_GangRadioConfig config = LF_GangRadioConfig.Load();
		array<int> reserved = {};
		reserved.Copy(config.m_aReservedKHz);
		array<string> services = {"police", "medic"};
		foreach (string service : services)
		{
			int frequency = LF_ServiceCallsConfig.Frequency(service);
			if (frequency > 0 && reserved.Find(frequency) < 0)
				reserved.Insert(frequency);
		}

		s_Bands = new LF_GangRadioBands(config.m_iMinKHz, config.m_iMaxKHz, config.m_iStepKHz, reserved);
		WatchEmptiedOrganizations();
		return s_Bands;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the organization's band, assigned on first use. Null on a client, for an empty id, or
	//! when every gang channel is held (each logged).
	static LF_GangRadioBand BandFor(string orgId)
	{
		if (!Replication.IsServer())
		{
			LF_Debug.Warn(FEATURE, "band asked for on a client, refused");
			return null;
		}

		return Bands().Assign(orgId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the organization's band if it holds one, never assigning.
	static LF_GangRadioBand Find(string orgId)
	{
		if (!s_Bands)
			return null;

		return s_Bands.Find(orgId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: frees the organization's band. \return False when it held none.
	static bool Release(string orgId, string reason)
	{
		if (!s_Bands)
			return false;

		return s_Bands.Release(orgId, reason);
	}

	//------------------------------------------------------------------------------------------------
	//! Subscribes the disband release to the bus. Idempotent: the bus refuses a second subscription
	//! of the same subscriber, kept for the world's life.
	protected static void WatchEmptiedOrganizations()
	{
		if (!s_Emptied)
			s_Emptied = new LF_GangRadioEmptiedListener();

		// The bus warns on a second Subscribe of the same instance, so ask it first.
		if (LF_EventBus.IsSubscribed(LF_OnOrganizationEmptied, s_Emptied))
			return;

		LF_EventBus.Subscribe(LF_OnOrganizationEmptied, s_Emptied);
	}
}

//------------------------------------------------------------------------------------------------
//! Bus subscriber: an organization that lost its last member gives its band back.
class LF_GangRadioEmptiedListener : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnOrganizationEmptied emptied = LF_OnOrganizationEmptied.Cast(busEvent);
		if (!emptied)
			return false;

		LF_GangRadio.Release(emptied.m_sOrgId, "organization emptied: " + emptied.m_sReason);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_GangRadio's statics (LF_StaticReset finds it by reflection).
class LF_GangRadio_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_GangRadio";
	}

	override void Apply()
	{
		LF_GangRadio.ResetStatics();
	}
}
