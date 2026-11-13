//------------------------------------------------------------------------------------------------
//! The posters drawn on the wanted board's face (docs/architecture/visual-boards-budget.md):
//! Configs/Police/LF_WantedPosters.conf with the owner's `wantedBoard` section layered over it.
//! Read on the server; the draw radius reaches clients inside the board's own feed, so a client
//! never reads the owner file.
[BaseContainerProps(configRoot: true)]
class LF_WantedPostersConfig
{
	static const ResourceName CONFIG_PATH = "{5B6D83094866AA06}Configs/Police/LF_WantedPosters.conf";

	//! The owner config.json section this class reads (LF_ConfigService).
	static const string SECTION = "wantedBoard";

	static const int DEFAULT_POSTERS = 4;
	static const int DEFAULT_STOLEN_POSTERS = 1;
	static const float DEFAULT_DRAW_RADIUS = 30;

	static const int POSTERS_MIN = 1;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int POSTERS_MAX = 8;	// lf-fixed: the layout has two rows of four
	static const int STOLEN_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int STOLEN_MAX = 8;	// lf-fixed: the layout has two rows of four
	static const float RADIUS_MIN = 5;	// lf-fixed: the schema limit, mirrors the attribute params
	static const float RADIUS_MAX = 100;	// lf-fixed: the schema limit, mirrors the attribute params

	[Attribute("4", UIWidgets.Auto, "Posters on the wanted board's face, newest first: one row up to 4, two rows up to 8. Each picture costs the viewer a few ms for 3 s after a change (docs/architecture/visual-boards-budget.md)", params: "1 8")]
	int m_iPosters;

	[Attribute("1", UIWidgets.Auto, "How many of those places reported stolen cars may take, newest first, after the wanted people. 0 keeps stolen cars off the face (they stay on the officers' Read card)", params: "0 8")]
	int m_iStolenPosters;

	[Attribute("30", UIWidgets.Auto, "Metres from the board within which a player's game draws the posters; beyond it plus 5 m they are released", params: "5 100")]
	float m_fDrawRadius;

	protected static ref LF_WantedPostersConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded and layered config, or null (logged once) when the file does not resolve.
	static LF_WantedPostersConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(LF_WantedPosters.FEATURE, string.Format("wanted poster config does not resolve, using defaults: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_WantedPostersConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error(LF_WantedPosters.FEATURE, string.Format("wanted poster config has an unexpected root class, using defaults: %1", CONFIG_PATH));
		else
			s_Config.ApplyLayers();

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached instance so the next Get() layers the owner file again (/configreload).
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: makes `config` the one Get returns until Reload.
	static void UseForTest(LF_WantedPostersConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges the owner's "wantedBoard" section over this instance, which holds the core `.conf`.
	void ApplyLayers()
	{
		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(SECTION);
		if (!reader)
			return;

		ApplyOwner(reader);
		LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("wantedBoard: owner config read %1 fields, rejected %2", reader.GetAppliedCount(), reader.GetRejectedCount()));
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the owner's "wantedBoard" section field by field; a key left out keeps the shipped value.
	//! \param[in] reader A reader on the section.
	void ApplyOwner(notnull LF_ConfigReader reader)
	{
		array<string> ownerKeys = {"posters", "stolenPosters", "drawRadius"};
		reader.WarnUnknownKeys(ownerKeys);
		reader.ReadInt("posters", m_iPosters, POSTERS_MIN, POSTERS_MAX);
		reader.ReadInt("stolenPosters", m_iStolenPosters, STOLEN_MIN, STOLEN_MAX);
		reader.ReadFloat("drawRadius", m_fDrawRadius, RADIUS_MIN, RADIUS_MAX);
	}

	//------------------------------------------------------------------------------------------------
	//! Posters on the face, clamped to the layout.
	static int Posters()
	{
		LF_WantedPostersConfig config = Get();
		if (!config)
			return DEFAULT_POSTERS;

		return Math.ClampInt(config.m_iPosters, POSTERS_MIN, POSTERS_MAX);
	}

	//------------------------------------------------------------------------------------------------
	//! Places a stolen car may take, never more than the face has.
	static int StolenPosters()
	{
		int stolen = DEFAULT_STOLEN_POSTERS;
		LF_WantedPostersConfig config = Get();
		if (config)
			stolen = config.m_iStolenPosters;

		return Math.ClampInt(stolen, STOLEN_MIN, Posters());
	}

	//------------------------------------------------------------------------------------------------
	static float DrawRadius()
	{
		LF_WantedPostersConfig config = Get();
		if (!config)
			return DEFAULT_DRAW_RADIUS;

		return Math.Clamp(config.m_fDrawRadius, RADIUS_MIN, RADIUS_MAX);
	}
}
