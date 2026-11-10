//------------------------------------------------------------------------------------------------
//! Where a dead player wakes and what it costs. Loaded from Configs/Spawning/LF_Respawn.conf;
//! the spawn logic holds no prices. Decision 2026-11-08 "You wake up in hospital".
[BaseContainerProps(configRoot: true)]
class LF_RespawnConfig
{
	static const ResourceName CONFIG_PATH = "{3092F3800BA35C61}Configs/Spawning/LF_Respawn.conf";

	[Attribute("1", UIWidgets.CheckBox, "After a death, respawn in a hospital bed (LF_HospitalBedComponent). Off, or no bed in the world: the spawn plaza")]
	bool m_bWakeInHospital;

	[Attribute("150", UIWidgets.Auto, "Treatment bill written on paper at every respawn after a death, paid in cash at the hospital desk (#260). 0 disables the bill")]
	int m_iBillAmount;

	[Attribute("7", UIWidgets.Auto, "Loiter animation the waking body plays: the base game tutorial's bed wake (7 full, 8 fast). 0 skips the animation")]
	int m_iWakeLoiterType;

	[Attribute("1", UIWidgets.CheckBox, "The wake animation moves the body (root motion), so it gets off the bed instead of standing on it")]
	bool m_bWakeRootMotion;

	[Attribute("0", UIWidgets.Auto, "Seconds before the bed loiter is stopped into the get-up. 0 lets the animation end by itself")]
	float m_fWakeLieSeconds;

	protected static ref LF_RespawnConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded config, or null (logged once) when the file does not resolve.
	static LF_RespawnConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error("Respawn", string.Format("respawn config does not resolve: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_RespawnConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error("Respawn", string.Format("respawn config has an unexpected root class: %1", CONFIG_PATH));

		return s_Config;
	}
}
