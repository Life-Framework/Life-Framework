//------------------------------------------------------------------------------------------------
//! The photo identity card (#902): Configs/Identity/LF_PhotoIdCard.conf with the owner's `idCard`
//! section layered over it. Read on the server only: the clerk charges the fee, and the server sends
//! how long a shown card stays up with the card itself.
[BaseContainerProps(configRoot: true)]
class LF_PhotoIdCardConfig
{
	static const ResourceName CONFIG_PATH = "{7C1E4A92D35B08F6}Configs/Identity/LF_PhotoIdCard.conf";

	//! The owner config.json section this class reads (LF_ConfigService).
	static const string SECTION = "idCard";

	static const int DEFAULT_FEE = 50;
	static const int FEE_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int FEE_MAX = 100000;	// lf-fixed: the schema limit, mirrors the attribute params

	static const float DEFAULT_SHOW_SECONDS = 8;
	static const float SHOW_MIN = 2;	// lf-fixed: the schema limit, mirrors the attribute params
	static const float SHOW_MAX = 60;	// lf-fixed: the schema limit, mirrors the attribute params

	[Attribute("50", UIWidgets.Auto, "Cash the town clerk charges for a replacement identity card (a lost or stolen one). 0 replaces it free", params: "0 100000")]
	int m_iReplacementFee;

	[Attribute("8", UIWidgets.Auto, "Seconds a card held up to another player (Show ID, a police request) stays on their screen", params: "2 60")]
	float m_fShowSeconds;

	protected static ref LF_PhotoIdCardConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded and layered config, or null (logged once) when the file does not resolve.
	static LF_PhotoIdCardConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(LF_PhotoIdCard.FEATURE, string.Format("identity card config does not resolve, using defaults: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_PhotoIdCardConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error(LF_PhotoIdCard.FEATURE, string.Format("identity card config has an unexpected root class, using defaults: %1", CONFIG_PATH));
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
	static void UseForTest(LF_PhotoIdCardConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges the owner's "idCard" section over this instance, which holds the core `.conf`.
	void ApplyLayers()
	{
		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(SECTION);
		if (!reader)
			return;

		ApplyOwner(reader);
		LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("idCard: owner config read %1 fields, rejected %2", reader.GetAppliedCount(), reader.GetRejectedCount()));
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the owner's "idCard" section field by field; a key left out keeps the shipped value.
	void ApplyOwner(notnull LF_ConfigReader reader)
	{
		array<string> ownerKeys = {"replacementFee", "showSeconds"};
		reader.WarnUnknownKeys(ownerKeys);
		reader.ReadInt("replacementFee", m_iReplacementFee, FEE_MIN, FEE_MAX);
		reader.ReadFloat("showSeconds", m_fShowSeconds, SHOW_MIN, SHOW_MAX);
	}

	//------------------------------------------------------------------------------------------------
	//! The clerk's fee for a replacement card, clamped to the schema.
	static int ReplacementFee()
	{
		LF_PhotoIdCardConfig config = Get();
		if (!config)
			return DEFAULT_FEE;

		return Math.ClampInt(config.m_iReplacementFee, FEE_MIN, FEE_MAX);
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds a card held up to someone stays on their screen.
	static float ShowSeconds()
	{
		LF_PhotoIdCardConfig config = Get();
		if (!config)
			return DEFAULT_SHOW_SECONDS;

		return Math.Clamp(config.m_fShowSeconds, SHOW_MIN, SHOW_MAX);
	}
}
