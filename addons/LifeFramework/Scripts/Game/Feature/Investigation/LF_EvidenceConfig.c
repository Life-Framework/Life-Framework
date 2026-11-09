//------------------------------------------------------------------------------------------------
//! Scene evidence tuning (Configs/Investigation/LF_Evidence.conf, #268). A missing or broken file
//! logs once and falls back to these defaults. Every number bounds something: the server never
//! holds more evidence entities than m_iMaxEvidence, and each piece expires untouched.
[BaseContainerProps(configRoot: true)]
class LF_EvidenceConfig
{
	static const ResourceName CONFIG_PATH = "{5B2E7C94A1D8F063}Configs/Investigation/LF_Evidence.conf";
	static const int DEFAULT_SCENE_SECONDS = 300;
	static const float DEFAULT_SCENE_RADIUS = 30;
	static const int DEFAULT_MAX_SCENES = 8;
	static const int DEFAULT_MAX_CASINGS_PER_SCENE = 6;
	static const int DEFAULT_MAX_EVIDENCE = 40;
	static const int DEFAULT_EXPIRY_SECONDS = 900;

	[Attribute("300", UIWidgets.Auto, "A crime scene stays open this many seconds after the report")]
	int m_iSceneSeconds;

	[Attribute("30", UIWidgets.Auto, "Shots and dropped items within this many metres of an open scene are evidence")]
	float m_fSceneRadius;

	[Attribute("8", UIWidgets.Auto, "Open scenes kept at once; the oldest closes first")]
	int m_iMaxScenes;

	[Attribute("6", UIWidgets.Auto, "Casings one scene may leave")]
	int m_iMaxCasingsPerScene;

	[Attribute("40", UIWidgets.Auto, "Evidence entities in the world at once, all scenes together; the oldest is removed first")]
	int m_iMaxEvidence;

	[Attribute("900", UIWidgets.Auto, "Untouched evidence is removed after this many seconds")]
	int m_iExpirySeconds;

	protected static ref LF_EvidenceConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	static LF_EvidenceConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error("Evidence", string.Format("evidence config does not resolve, using defaults: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_EvidenceConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error("Evidence", string.Format("evidence config has an unexpected root class, using defaults: %1", CONFIG_PATH));

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	static int SceneSeconds()
	{
		LF_EvidenceConfig config = Get();
		if (!config || config.m_iSceneSeconds < 1)
			return DEFAULT_SCENE_SECONDS;

		return config.m_iSceneSeconds;
	}

	//------------------------------------------------------------------------------------------------
	static float SceneRadius()
	{
		LF_EvidenceConfig config = Get();
		if (!config || config.m_fSceneRadius <= 0)
			return DEFAULT_SCENE_RADIUS;

		return config.m_fSceneRadius;
	}

	//------------------------------------------------------------------------------------------------
	static int MaxScenes()
	{
		LF_EvidenceConfig config = Get();
		if (!config || config.m_iMaxScenes < 1)
			return DEFAULT_MAX_SCENES;

		return config.m_iMaxScenes;
	}

	//------------------------------------------------------------------------------------------------
	static int MaxCasingsPerScene()
	{
		LF_EvidenceConfig config = Get();
		if (!config || config.m_iMaxCasingsPerScene < 1)
			return DEFAULT_MAX_CASINGS_PER_SCENE;

		return config.m_iMaxCasingsPerScene;
	}

	//------------------------------------------------------------------------------------------------
	static int MaxEvidence()
	{
		LF_EvidenceConfig config = Get();
		if (!config || config.m_iMaxEvidence < 1)
			return DEFAULT_MAX_EVIDENCE;

		return config.m_iMaxEvidence;
	}

	//------------------------------------------------------------------------------------------------
	static int ExpirySeconds()
	{
		LF_EvidenceConfig config = Get();
		if (!config || config.m_iExpirySeconds < 1)
			return DEFAULT_EXPIRY_SECONDS;

		return config.m_iExpirySeconds;
	}
}
