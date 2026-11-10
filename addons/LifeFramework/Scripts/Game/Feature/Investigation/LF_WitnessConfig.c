//------------------------------------------------------------------------------------------------
//! Witness tuning (Configs/Investigation/LF_Witness.conf, #266, #267). A missing or broken file
//! logs once and falls back to these defaults. Distances are metres from the suspect at the crime.
[BaseContainerProps(configRoot: true)]
class LF_WitnessConfig
{
	static const ResourceName CONFIG_PATH = "{7C3A91E5D2B84F60}Configs/Investigation/LF_Witness.conf";
	static const float DEFAULT_SIGHT_RANGE = 60;
	static const float DEFAULT_CLOTHING_RANGE = 25;
	static const float DEFAULT_VEHICLE_RANGE = 45;
	static const float DEFAULT_PLATE_MAX_RANGE = 45;
	static const float DEFAULT_PLATE_STEP = 8;
	static const float DEFAULT_NPC_SIGHT_RANGE = 30;
	static const float DEFAULT_NPC_ACCURACY = 0.6;
	static const int DEFAULT_MAX_SCENES = 16;
	static const int DEFAULT_WINDOW_SECONDS = 900;
	static const int DEFAULT_MAX_STATEMENTS = 40;

	[Attribute("60", UIWidgets.Auto, "A player within this many metres of the crime saw it and can describe the place")]
	float m_fSightRange;

	[Attribute("25", UIWidgets.Auto, "Clothing is only seen this close")]
	float m_fClothingRange;

	[Attribute("45", UIWidgets.Auto, "The vehicle is only described from this close")]
	float m_fVehicleRange;

	[Attribute("45", UIWidgets.Auto, "No plate characters are seen beyond this distance")]
	float m_fPlateMaxRange;

	[Attribute("8", UIWidgets.Auto, "Every this many metres of distance costs one plate character")]
	float m_fPlateStepMetres;

	[Attribute("30", UIWidgets.Auto, "A town NPC within this many metres of the crime saw it")]
	float m_fNpcSightRange;

	[Attribute("0.6", UIWidgets.Auto, "Chance an NPC witness remembers each thing it could see (0 to 1)")]
	float m_fNpcAccuracy;

	[Attribute("1", UIWidgets.CheckBox, "A witness statement names no caller; off shows the player's character name")]
	bool m_bAnonymous;

	[Attribute("16", UIWidgets.Auto, "Crime scenes whose witnesses are remembered; the oldest drops first")]
	int m_iMaxScenes;

	[Attribute("900", UIWidgets.Auto, "A witness can describe a crime for this many seconds after it")]
	int m_iWindowSeconds;

	[Attribute("40", UIWidgets.Auto, "Witness statements kept on the boards; the oldest drops first")]
	int m_iMaxStatements;

	protected static ref LF_WitnessConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	static LF_WitnessConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error("Witness", string.Format("witness config does not resolve, using defaults: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_WitnessConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error("Witness", string.Format("witness config has an unexpected root class, using defaults: %1", CONFIG_PATH));

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	static float SightRange()
	{
		LF_WitnessConfig config = Get();
		if (!config || config.m_fSightRange <= 0)
			return DEFAULT_SIGHT_RANGE;

		return config.m_fSightRange;
	}

	//------------------------------------------------------------------------------------------------
	static float ClothingRange()
	{
		LF_WitnessConfig config = Get();
		if (!config || config.m_fClothingRange <= 0)
			return DEFAULT_CLOTHING_RANGE;

		return config.m_fClothingRange;
	}

	//------------------------------------------------------------------------------------------------
	static float VehicleRange()
	{
		LF_WitnessConfig config = Get();
		if (!config || config.m_fVehicleRange <= 0)
			return DEFAULT_VEHICLE_RANGE;

		return config.m_fVehicleRange;
	}

	//------------------------------------------------------------------------------------------------
	static float PlateMaxRange()
	{
		LF_WitnessConfig config = Get();
		if (!config || config.m_fPlateMaxRange <= 0)
			return DEFAULT_PLATE_MAX_RANGE;

		return config.m_fPlateMaxRange;
	}

	//------------------------------------------------------------------------------------------------
	static float PlateStep()
	{
		LF_WitnessConfig config = Get();
		if (!config || config.m_fPlateStepMetres <= 0)
			return DEFAULT_PLATE_STEP;

		return config.m_fPlateStepMetres;
	}

	//------------------------------------------------------------------------------------------------
	static float NpcSightRange()
	{
		LF_WitnessConfig config = Get();
		if (!config || config.m_fNpcSightRange <= 0)
			return DEFAULT_NPC_SIGHT_RANGE;

		return config.m_fNpcSightRange;
	}

	//------------------------------------------------------------------------------------------------
	static float NpcAccuracy()
	{
		LF_WitnessConfig config = Get();
		if (!config)
			return DEFAULT_NPC_ACCURACY;

		return Math.Clamp(config.m_fNpcAccuracy, 0, 1);
	}

	//------------------------------------------------------------------------------------------------
	static bool Anonymous()
	{
		LF_WitnessConfig config = Get();
		if (!config)
			return true;

		return config.m_bAnonymous;
	}

	//------------------------------------------------------------------------------------------------
	static int MaxScenes()
	{
		LF_WitnessConfig config = Get();
		if (!config || config.m_iMaxScenes < 1)
			return DEFAULT_MAX_SCENES;

		return config.m_iMaxScenes;
	}

	//------------------------------------------------------------------------------------------------
	static int WindowSeconds()
	{
		LF_WitnessConfig config = Get();
		if (!config || config.m_iWindowSeconds < 1)
			return DEFAULT_WINDOW_SECONDS;

		return config.m_iWindowSeconds;
	}

	//------------------------------------------------------------------------------------------------
	static int MaxStatements()
	{
		LF_WitnessConfig config = Get();
		if (!config || config.m_iMaxStatements < 1)
			return DEFAULT_MAX_STATEMENTS;

		return config.m_iMaxStatements;
	}
}
