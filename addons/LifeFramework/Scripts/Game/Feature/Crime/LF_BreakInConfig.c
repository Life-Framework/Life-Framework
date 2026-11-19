//------------------------------------------------------------------------------------------------
//! The break-in rules' numbers (#3368, parent #1023): Configs/Crime/LF_BreakIn.conf. A break-in is a
//! run of stages; every stage has work points that must all be worked at once, and the numbers
//! here say how long a stage takes, how long work may stop before the stage falls back, and where
//! the progress words change. Core ships neutral numbers.
[BaseContainerProps(configRoot: true)]
class LF_BreakInConfig
{
	static const string FEATURE = "BreakIn";
	static const ResourceName CONFIG_PATH = "{664D4219300F528E}Configs/Crime/LF_BreakIn.conf";

	[Attribute("3", UIWidgets.Auto, "Stages in one break-in", params: "1 10")]
	int m_iStageCount;

	[Attribute("20", UIWidgets.Auto, "Seconds one stage takes when every work point is worked the whole time", params: "1 3600")]
	float m_fSecondsPerStage;

	[Attribute("3", UIWidgets.Auto, "Seconds work may stop before the stage falls back and the defenders reset", params: "0 600")]
	float m_fGraceSeconds;

	[Attribute("0.34", UIWidgets.Auto, "Overall progress (0..1) from which the word is halfway; below it is barely started", params: "0 1")]
	float m_fHalfwayFrom;

	[Attribute("0.75", UIWidgets.Auto, "Overall progress (0..1) from which the word is nearly through", params: "0 1")]
	float m_fNearlyFrom;

	protected static ref LF_BreakInConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded table, or null (logged once) when the file does not resolve.
	static LF_BreakInConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(FEATURE, string.Format("break-in config does not resolve: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_BreakInConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error(FEATURE, string.Format("break-in config has an unexpected root class: %1", CONFIG_PATH));

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached table so the next Get() loads it again.
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}
}
