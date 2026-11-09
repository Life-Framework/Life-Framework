//------------------------------------------------------------------------------------------------
//! Incident log settings (Configs/Moderation/LF_Incident.conf). A missing or broken file logs once
//! and falls back to the defaults, which record every kind.
[BaseContainerProps(configRoot: true)]
class LF_IncidentConfig
{
	static const ResourceName CONFIG_PATH = "{6A3E91C4B7D2085F}Configs/Moderation/LF_Incident.conf";

	static const string KIND_DEATH = "death";
	static const string KIND_DAMAGE = "damage";
	static const string KIND_CASH = "cash";
	static const string KIND_DOWNED = "downed";
	static const string KIND_HOLDUP = "holdup";
	static const string KINDS_ALL = "all";

	static const int DEFAULT_MAX_RECORDS = 2000;
	static const int DEFAULT_RETENTION_HOURS = 336;
	static const float DEFAULT_DAMAGE_SAMPLE_SECONDS = 5;
	static const int DEFAULT_REPORT_WINDOW_MINUTES = 10;
	static const int DEFAULT_REPORT_COOLDOWN_SECONDS = 120;

	protected static ref LF_IncidentConfig s_Config;
	protected static bool s_bLoadAttempted;

	[Attribute("all", UIWidgets.Auto, "Event kinds recorded: 'all' or a comma list of death, damage, cash, downed, holdup")]
	string m_sKinds;

	[Attribute("2000", UIWidgets.Auto, "Most records kept; the oldest go first")]
	int m_iMaxRecords;

	[Attribute("336", UIWidgets.Auto, "Hours a record is kept (336 is 14 days)")]
	int m_iRetentionHours;

	[Attribute("5", UIWidgets.Auto, "Hits between the same two players fold into one damage record per this many seconds")]
	float m_fDamageSampleSeconds;

	[Attribute("10", UIWidgets.Auto, "A player report flags the reporter's incidents from this many minutes back")]
	int m_iReportWindowMinutes;

	[Attribute("120", UIWidgets.Auto, "Seconds a player must wait between two reports")]
	int m_iReportCooldownSeconds;

	[Attribute("2", UIWidgets.Auto, "Metres a hold-up reaches. The interaction menu itself reaches 2 m, so more changes nothing until the menu does")]
	float m_fHoldUpRange;

	[Attribute("30", UIWidgets.Auto, "Seconds a player waits between two hold-ups")]
	int m_iHoldUpCooldownSeconds;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	static LF_IncidentConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(LF_IncidentLog.FEATURE, string.Format("incident config does not resolve, using defaults: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_IncidentConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error(LF_IncidentLog.FEATURE, string.Format("incident config has an unexpected root class, using defaults: %1", CONFIG_PATH));

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: does the kinds setting record this kind? An empty setting records nothing.
	static bool KindEnabled(string kinds, string kind)
	{
		kinds.ToLower();
		kinds.Replace(" ", "");
		if (kinds == KINDS_ALL)
			return true;

		array<string> parts = {};
		kinds.Split(",", parts, true);
		return parts.Find(kind) >= 0;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsEnabled(string kind)
	{
		LF_IncidentConfig config = Get();
		if (!config)
			return true;

		return KindEnabled(config.m_sKinds, kind);
	}

	//------------------------------------------------------------------------------------------------
	static float HoldUpRange()
	{
		LF_IncidentConfig config = Get();
		if (!config || config.m_fHoldUpRange <= 0)
			return 2;

		return config.m_fHoldUpRange;
	}

	//------------------------------------------------------------------------------------------------
	static int HoldUpCooldownSeconds()
	{
		LF_IncidentConfig config = Get();
		if (!config || config.m_iHoldUpCooldownSeconds < 0)
			return 30;

		return config.m_iHoldUpCooldownSeconds;
	}

	//------------------------------------------------------------------------------------------------
	static int MaxRecords()
	{
		LF_IncidentConfig config = Get();
		if (!config || config.m_iMaxRecords < 1)
			return DEFAULT_MAX_RECORDS;

		return config.m_iMaxRecords;
	}

	//------------------------------------------------------------------------------------------------
	static int RetentionSeconds()
	{
		LF_IncidentConfig config = Get();
		if (!config || config.m_iRetentionHours < 1)
			return DEFAULT_RETENTION_HOURS * 3600;

		return config.m_iRetentionHours * 3600;
	}

	//------------------------------------------------------------------------------------------------
	static float DamageSampleSeconds()
	{
		LF_IncidentConfig config = Get();
		if (!config || config.m_fDamageSampleSeconds <= 0)
			return DEFAULT_DAMAGE_SAMPLE_SECONDS;

		return config.m_fDamageSampleSeconds;
	}

	//------------------------------------------------------------------------------------------------
	static int ReportWindowSeconds()
	{
		LF_IncidentConfig config = Get();
		if (!config || config.m_iReportWindowMinutes < 1)
			return DEFAULT_REPORT_WINDOW_MINUTES * 60;

		return config.m_iReportWindowMinutes * 60;
	}

	//------------------------------------------------------------------------------------------------
	static int ReportCooldownSeconds()
	{
		LF_IncidentConfig config = Get();
		if (!config || config.m_iReportCooldownSeconds < 0)
			return DEFAULT_REPORT_COOLDOWN_SECONDS;

		return config.m_iReportCooldownSeconds;
	}
}
