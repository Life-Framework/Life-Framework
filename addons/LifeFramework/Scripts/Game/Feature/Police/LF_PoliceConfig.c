//------------------------------------------------------------------------------------------------
//! Police tuning: per-rank permissions, and the wanted board and crime reports (M1-11).
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
	static const int DEFAULT_PATROL_FINE_CAP = 1000;
	static const float DEFAULT_CATCH_IN_ACT_RADIUS = 10;
	static const float DEFAULT_PLACE_RADIUS = 80;
	static const float DEFAULT_BOARD_READ_DISTANCE = 4;
	static const int DEFAULT_BOARD_MAX_POSTERS = 8;
	static const int DEFAULT_MURDER_WANTED = 3;
	static const bool DEFAULT_OFFICER_KILL_OF_WANTED_EXEMPT = true;

	[Attribute("0", UIWidgets.Auto, "Lowest police rank that may arrest (0 patrol, 1 senior)")]
	int m_iArrestMinRank;

	[Attribute("0", UIWidgets.Auto, "Lowest police rank that may escort a suspect (0 patrol, 1 senior)")]
	int m_iEscortMinRank;

	[Attribute("1000", UIWidgets.Auto, "Largest fine a patrol officer may issue. Senior officers use the global maximum")]
	int m_iPatrolFineCap;

	[Attribute("10", UIWidgets.Auto, "An on-duty officer this close (metres) when a crime is committed catches the suspect in the act: the report is identified at once")]
	float m_fCatchInActRadius;

	[Attribute("80", UIWidgets.Auto, "A crime report names the nearest sign within this many metres as the place; beyond it the place is a map grid square")]
	float m_fPlaceRadius;

	[Attribute("4", UIWidgets.Auto, "How close (metres) a player must stand to the wanted board for the server to answer a read")]
	float m_fBoardReadDistance;

	[Attribute("8", UIWidgets.Auto, "Most posters the wanted board shows, newest first")]
	int m_iBoardMaxPosters;

	[Attribute("3", UIWidgets.Auto, "Wanted levels a murder adds (M1-27). Robbery adds 1; wanted caps at 5")]
	int m_iMurderWanted;

	[Attribute("1", UIWidgets.Auto, "An on-duty officer who kills a suspect with wanted above 0 is not charged with murder")]
	bool m_bOfficerKillOfWantedExempt;

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
			EL_Debug.Error("Ranks", string.Format("police config does not resolve, using defaults: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_PoliceConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			EL_Debug.Error("Ranks", string.Format("police config has an unexpected root class, using defaults: %1", CONFIG_PATH));

		return s_Config;
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
		LF_PoliceConfig config = Get();
		if (!config)
			return DEFAULT_PATROL_FINE_CAP;

		return config.m_iPatrolFineCap;
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
}
