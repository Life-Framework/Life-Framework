//------------------------------------------------------------------------------------------------
//! Police tuning: per-rank permissions, the wanted board and crime reports (M1-11), and the
//! paper ticket (M1-19): offences with fixed amounts, both deadlines, and the late rule.
//! Loaded from Configs/Police/LF_Police.conf.
//! Ranks are LF_PoliceRanks.PATROL (0) and SENIOR (1). A minimum rank of 0 opens the action to
//! every on-duty officer. Flagship default (docs/decisions.md, "Every on-duty officer can arrest"):
//! arrest and escort are open to every officer; rank gates the fine cap and the armory tier.
//! A missing or broken file logs once and falls back to the same defaults.
[BaseContainerProps(configRoot: true)]
class LF_PoliceConfig
{
	static const ResourceName CONFIG_PATH = "{916BDC092C01B7D1}Configs/Police/LF_Police.conf";

	static const int DEFAULT_ARREST_MIN_RANK = 0;
	static const int DEFAULT_ESCORT_MIN_RANK = 0;
	static const float DEFAULT_CATCH_IN_ACT_RADIUS = 10;
	static const float DEFAULT_PLACE_RADIUS = 80;
	static const float DEFAULT_BOARD_READ_DISTANCE = 4;
	static const int DEFAULT_BOARD_MAX_POSTERS = 8;
	static const float DEFAULT_ALARM_SECONDS = 30;
	static const float DEFAULT_LOUD_ALARM_SECONDS = 60;
	static const ResourceName DEFAULT_ALARM_PREFAB = "{3C7A51E2D94B08F6}Prefabs/Police/LF_SiteAlarm.et";
	static const ResourceName DEFAULT_LOUD_ALARM_PREFAB = "{8E15F0B7A26C4D93}Prefabs/Police/LF_SiteAlarm_Loud.et";
	static const ResourceName DEFAULT_RADIO_SOUND_PROJECT = "{FD5E9674E51B9D13}Sounds/Props/Military/Radios/Props_Radios_Signals.acp";
	static const string DEFAULT_RADIO_SOUND_EVENT = "SOUND_RADIO_SIGNAL_2";
	static const int DEFAULT_DISPATCH_LOG_LINES = 5;
	static const int DEFAULT_TICKET_FIRST_DEADLINE = 1800;
	static const int DEFAULT_TICKET_SECOND_DEADLINE = 1800;
	static const float DEFAULT_TICKET_LATE_MULTIPLIER = 2;
	static const bool DEFAULT_OFFICER_KILL_OF_WANTED_EXEMPT = true;
	static const int DEFAULT_SEARCH_COOLDOWN_SECONDS = 120;
	static const int DEFAULT_COMPLIANT_SECONDS = 60;
	static const int DEFAULT_SPIKE_STRIP_LIFETIME_SECONDS = 600;

	[Attribute("0", UIWidgets.Auto, "Lowest police rank that may arrest (0 patrol, 1 senior)")]
	int m_iArrestMinRank;

	[Attribute("0", UIWidgets.Auto, "Lowest police rank that may escort a suspect (0 patrol, 1 senior)")]
	int m_iEscortMinRank;

	[Attribute("", UIWidgets.Object, "The rank ladder, lowest first. A rank is its index here and a saved index never changes meaning: append, never reorder. Index 0 is patrol and 1 is senior")]
	ref array<ref LF_PoliceRank> m_aRanks;

	[Attribute("10", UIWidgets.Auto, "An on-duty officer this close (metres) when a crime is committed catches the suspect in the act: the report is identified at once")]
	float m_fCatchInActRadius;

	[Attribute("80", UIWidgets.Auto, "A crime report names the nearest sign within this many metres as the place; beyond it the place is a map grid square")]
	float m_fPlaceRadius;

	[Attribute("4", UIWidgets.Auto, "How close (metres) a player must stand to the wanted board for the server to answer a read")]
	float m_fBoardReadDistance;

	[Attribute("8", UIWidgets.Auto, "Most posters the wanted board shows, newest first")]
	int m_iBoardMaxPosters;

	[Attribute("30", UIWidgets.Auto, "Seconds the site alarm sounds when at least one officer is on duty (M1-20)")]
	float m_fAlarmSeconds;

	[Attribute("60", UIWidgets.Auto, "Seconds the site alarm sounds when nobody is on duty: the louder, longer alarm")]
	float m_fLoudAlarmSeconds;

	[Attribute("{3C7A51E2D94B08F6}Prefabs/Police/LF_SiteAlarm.et", UIWidgets.ResourceNamePicker, "Alarm spawned at the site when officers are on duty. Its LF_SiteAlarmComponent sets the sound, hear range, and light", "et")]
	ResourceName m_sAlarmPrefab;

	[Attribute("{8E15F0B7A26C4D93}Prefabs/Police/LF_SiteAlarm_Loud.et", UIWidgets.ResourceNamePicker, "Alarm spawned at the site when nobody is on duty: the loud siren, a longer range, a brighter flashing light", "et")]
	ResourceName m_sLoudAlarmPrefab;

	[Attribute("{FD5E9674E51B9D13}Sounds/Props/Military/Radios/Props_Radios_Signals.acp", UIWidgets.ResourceNamePicker, "Sound project of the radio chirp an on-duty officer hears with a dispatch", "acp")]
	ResourceName m_sRadioSoundProject;

	[Attribute("SOUND_RADIO_SIGNAL_2", UIWidgets.EditBox, "Sound event of the radio chirp")]
	string m_sRadioSoundEvent;

	[Attribute("5", UIWidgets.Auto, "Dispatch lines the wanted board shows to on-duty police, newest first")]
	int m_iDispatchLogLines;

	[Attribute("1800", UIWidgets.Auto, "Online seconds a ticket holder has to pay at the police desk before the amount goes up")]
	int m_iTicketFirstDeadline;

	[Attribute("1800", UIWidgets.Auto, "Further online seconds after the first deadline before an unpaid ticket makes the holder wanted")]
	int m_iTicketSecondDeadline;

	[Attribute("2", UIWidgets.Auto, "The amount is multiplied by this once, at the first deadline")]
	float m_fTicketLateMultiplier;

	[Attribute("1", UIWidgets.Auto, "An on-duty officer who kills a suspect with wanted above 0 is not charged with murder")]
	bool m_bOfficerKillOfWantedExempt;

	[Attribute("120", UIWidgets.Auto, "Seconds before the same player or vehicle can be searched for chain goods again. A repeat inside it is not rolled again (#472). 0 disables")]
	int m_iSearchCooldownSeconds;

	[Attribute("60", UIWidgets.Auto, "Seconds after a hold-up during which the held-up player, holding no weapon, may be handcuffed by an on-duty officer. A downed player may always be. 0 allows downed players only", params: "0 600")]
	int m_iCompliantSeconds;

	[Attribute("600", UIWidgets.Auto, "Seconds a deployed spike strip stays before it goes away by itself (#646). An officer can pick it up sooner", params: "30 7200")]
	int m_iSpikeStripLifetimeSeconds;

	protected static ref LF_PoliceConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded config, or null (logged once) when the file does not resolve.
	static LF_PoliceConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error("Ranks", string.Format("police config does not resolve, using defaults: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_PoliceConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error("Ranks", string.Format("police config has an unexpected root class, using defaults: %1", CONFIG_PATH));

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	static int SearchCooldownSeconds()
	{
		LF_PoliceConfig config = Get();
		if (!config)
			return DEFAULT_SEARCH_COOLDOWN_SECONDS;

		return Math.Max(config.m_iSearchCooldownSeconds, 0);
	}

	//------------------------------------------------------------------------------------------------
	static int CompliantSeconds()
	{
		LF_PoliceConfig config = Get();
		if (!config)
			return DEFAULT_COMPLIANT_SECONDS;

		return Math.ClampInt(config.m_iCompliantSeconds, 0, 600);
	}

	//------------------------------------------------------------------------------------------------
	static int SpikeStripLifetimeSeconds()
	{
		LF_PoliceConfig config = Get();
		if (!config)
			return DEFAULT_SPIKE_STRIP_LIFETIME_SECONDS;

		return Math.ClampInt(config.m_iSpikeStripLifetimeSeconds, 30, 7200);
	}

	//------------------------------------------------------------------------------------------------
	static int ArrestMinRank()
	{
		LF_PoliceConfig config = Get();
		if (!config)
			return DEFAULT_ARREST_MIN_RANK;

		return config.m_iArrestMinRank;
	}

	//------------------------------------------------------------------------------------------------
	static int EscortMinRank()
	{
		LF_PoliceConfig config = Get();
		if (!config)
			return DEFAULT_ESCORT_MIN_RANK;

		return config.m_iEscortMinRank;
	}

	//------------------------------------------------------------------------------------------------
	static int PatrolFineCap()
	{
		return LF_PoliceRanks.FineCap(LF_PoliceRanks.PATROL);
	}

	//------------------------------------------------------------------------------------------------
	//! The ladder, lowest rank first. Rows with an empty id are dropped. A missing config or an
	//! empty list is the built-in two-rank ladder (patrol, senior): the numbers from before the
	//! ladder was config.
	static void Ranks(notnull array<ref LF_PoliceRank> outRanks)
	{
		outRanks.Clear();
		LF_PoliceConfig config = Get();
		if (config && config.m_aRanks)
		{
			foreach (LF_PoliceRank rank : config.m_aRanks)
			{
				if (!rank || rank.m_sId.IsEmpty())
				{
					LF_Debug.Warn("Ranks", "ladder row with no id skipped");
					continue;
				}

				outRanks.Insert(rank);
			}

			if (!outRanks.IsEmpty())
				return;
		}

		outRanks.Insert(LF_PoliceRank.Create("patrol", "EL-Rank_Patrol", LF_PoliceRanks.PATROL_FINE_CAP, -1, LF_PoliceRanks.DUTY_SECONDS, LF_PoliceRanks.SIDEARM));
		outRanks.Insert(LF_PoliceRank.Create("senior", "EL-Rank_Senior", LF_PoliceUtils.MAX_FINE, -1, 0, LF_PoliceRanks.LONG_GUN));
	}

	//------------------------------------------------------------------------------------------------
	static float CatchInActRadius()
	{
		LF_PoliceConfig config = Get();
		if (!config)
			return DEFAULT_CATCH_IN_ACT_RADIUS;

		return config.m_fCatchInActRadius;
	}

	//------------------------------------------------------------------------------------------------
	static float PlaceRadius()
	{
		LF_PoliceConfig config = Get();
		if (!config)
			return DEFAULT_PLACE_RADIUS;

		return config.m_fPlaceRadius;
	}

	//------------------------------------------------------------------------------------------------
	static float BoardReadDistance()
	{
		LF_PoliceConfig config = Get();
		if (!config)
			return DEFAULT_BOARD_READ_DISTANCE;

		return config.m_fBoardReadDistance;
	}

	//------------------------------------------------------------------------------------------------
	static int BoardMaxPosters()
	{
		LF_PoliceConfig config = Get();
		if (!config)
			return DEFAULT_BOARD_MAX_POSTERS;

		return config.m_iBoardMaxPosters;
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds the site alarm runs; the loud alarm when nobody is on duty.
	static float AlarmSeconds(bool loud)
	{
		LF_PoliceConfig config = Get();
		if (loud)
		{
			if (!config)
				return DEFAULT_LOUD_ALARM_SECONDS;

			return config.m_fLoudAlarmSeconds;
		}

		if (!config)
			return DEFAULT_ALARM_SECONDS;

		return config.m_fAlarmSeconds;
	}

	//------------------------------------------------------------------------------------------------
	static ResourceName AlarmPrefab(bool loud)
	{
		LF_PoliceConfig config = Get();
		if (loud)
		{
			if (!config)
				return DEFAULT_LOUD_ALARM_PREFAB;

			return config.m_sLoudAlarmPrefab;
		}

		if (!config)
			return DEFAULT_ALARM_PREFAB;

		return config.m_sAlarmPrefab;
	}

	//------------------------------------------------------------------------------------------------
	static ResourceName RadioSoundProject()
	{
		LF_PoliceConfig config = Get();
		if (!config)
			return DEFAULT_RADIO_SOUND_PROJECT;

		return config.m_sRadioSoundProject;
	}

	//------------------------------------------------------------------------------------------------
	static string RadioSoundEvent()
	{
		LF_PoliceConfig config = Get();
		if (!config)
			return DEFAULT_RADIO_SOUND_EVENT;

		return config.m_sRadioSoundEvent;
	}

	//------------------------------------------------------------------------------------------------
	static int DispatchLogLines()
	{
		LF_PoliceConfig config = Get();
		if (!config)
			return DEFAULT_DISPATCH_LOG_LINES;

		return config.m_iDispatchLogLines;
	}

	//------------------------------------------------------------------------------------------------
	//! The ticket offences, in form order. A missing config or an empty list is the flagship list.
	static void TicketOffences(notnull array<ref LF_TicketOffence> outOffences)
	{
		outOffences.Clear();
		LF_CrimeCatalog.CollectTicketOffences(outOffences);
		if (!outOffences.IsEmpty())
			return;

		// The catalog is missing or lists no fine: the shipped form, so an officer can still write a ticket.
		outOffences.Insert(LF_TicketOffence.Create("#LF-Ticket_OffenceSpeeding", 200));
		outOffences.Insert(LF_TicketOffence.Create("#LF-Ticket_OffenceDisturbance", 150));
		outOffences.Insert(LF_TicketOffence.Create("#LF-Ticket_OffenceRecklessDriving", 500));
		outOffences.Insert(LF_TicketOffence.Create("#LF-Ticket_OffenceBrandishing", 800));
		outOffences.Insert(LF_TicketOffence.Create("#LF-Ticket_OffenceCrash", 1500));
	}

	//------------------------------------------------------------------------------------------------
	static int TicketFirstDeadline()
	{
		LF_PoliceConfig config = Get();
		if (!config || config.m_iTicketFirstDeadline <= 0)
			return DEFAULT_TICKET_FIRST_DEADLINE;

		return config.m_iTicketFirstDeadline;
	}

	//------------------------------------------------------------------------------------------------
	static int TicketSecondDeadline()
	{
		LF_PoliceConfig config = Get();
		if (!config || config.m_iTicketSecondDeadline <= 0)
			return DEFAULT_TICKET_SECOND_DEADLINE;

		return config.m_iTicketSecondDeadline;
	}

	//------------------------------------------------------------------------------------------------
	static float TicketLateMultiplier()
	{
		LF_PoliceConfig config = Get();
		if (!config || config.m_fTicketLateMultiplier < 1)
			return DEFAULT_TICKET_LATE_MULTIPLIER;

		return config.m_fTicketLateMultiplier;
	}

}

//------------------------------------------------------------------------------------------------
//! One line of the ticket form: the offence the officer ticks and its fixed amount.
[BaseContainerProps()]
class LF_TicketOffence
{
	[Attribute("", UIWidgets.EditBox, "Offence name, a localization key")]
	string m_sName;

	[Attribute("100", UIWidgets.Auto, "Fixed amount in cash")]
	int m_iAmount;

	//------------------------------------------------------------------------------------------------
	static LF_TicketOffence Create(string name, int amount)
	{
		LF_TicketOffence offence = new LF_TicketOffence();
		offence.m_sName = name;
		offence.m_iAmount = amount;
		return offence;
	}
}

//------------------------------------------------------------------------------------------------
//! One rung of the police ladder. The rank saved on an account is this row's index.
[BaseContainerProps()]
class LF_PoliceRank
{
	[Attribute("", UIWidgets.EditBox, "Stable name of the rank, for logs and packs")]
	string m_sId;

	[Attribute("", UIWidgets.EditBox, "Localization key of the title, without the leading #")]
	string m_sTitleKey;

	[Attribute("1000", UIWidgets.Auto, "Largest fine an officer of this rank may write")]
	int m_iFineCap;

	[Attribute("-1", UIWidgets.Auto, "Cash per on-duty second at clock-out. Negative uses the jobs config police rate")]
	float m_fPayPerSecond;

	[Attribute("0", UIWidgets.Auto, "On-duty seconds that promote an officer to the next rank. 0 means no time promotion")]
	int m_iDutySecondsForNext;

	[Attribute("", UIWidgets.ResourceNamePicker, "Kit prefabs drawn at the armory. Only prefabs in the station catalog are issued")]
	ref array<ResourceName> m_aKit;

	//------------------------------------------------------------------------------------------------
	static LF_PoliceRank Create(string id, string titleKey, int fineCap, float payPerSecond, int dutySecondsForNext, ResourceName kitPrefab)
	{
		LF_PoliceRank rank = new LF_PoliceRank();
		rank.m_sId = id;
		rank.m_sTitleKey = titleKey;
		rank.m_iFineCap = fineCap;
		rank.m_fPayPerSecond = payPerSecond;
		rank.m_iDutySecondsForNext = dutySecondsForNext;
		rank.m_aKit = {};
		rank.m_aKit.Insert(kitPrefab);
		return rank;
	}
}
