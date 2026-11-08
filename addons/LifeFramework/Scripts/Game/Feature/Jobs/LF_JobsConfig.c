//------------------------------------------------------------------------------------------------
//! Job pay values. Loaded from Configs/Jobs/LF_Jobs.conf; systems hold no prices.
//! There is no timer paycheck: civilians earn cash per completed run and from trader sales,
//! police earn duty pay at clock-out, and a broke player collects a benefit at the counter.
[BaseContainerProps(configRoot: true)]
class LF_JobsConfig
{
	static const ResourceName CONFIG_PATH = "{94C638F6312DA36A}Configs/Jobs/LF_Jobs.conf";

	[Attribute("0.5", UIWidgets.Auto, "Police duty pay in cash per second on duty, paid at the station desk on going off duty. 0 disables duty pay")]
	float m_fPoliceDutyPayPerSecond;

	[Attribute("500", UIWidgets.Auto, "Unemployment benefit: a bank balance strictly below this may collect")]
	int m_iBenefitBalanceThreshold;

	[Attribute("100", UIWidgets.Auto, "Unemployment benefit: cash handed over per collection. 0 disables the benefit")]
	int m_iBenefitAmount;

	[Attribute("1200", UIWidgets.Auto, "Unemployment benefit: seconds between collections for one account")]
	float m_fBenefitCooldownSeconds;

	protected static ref LF_JobsConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded config, or null (logged once) when the file does not resolve.
	static LF_JobsConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			EL_Debug.Error("Jobs", string.Format("jobs config does not resolve: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_JobsConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			EL_Debug.Error("Jobs", string.Format("jobs config has an unexpected root class: %1", CONFIG_PATH));

		return s_Config;
	}
}
