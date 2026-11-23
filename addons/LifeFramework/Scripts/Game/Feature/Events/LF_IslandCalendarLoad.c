//------------------------------------------------------------------------------------------------
//! Loads the island calendar rows from Configs/Events/LF_IslandCalendar.conf. The core file only:
//! a pack changing a row by overlay needs the pack-layer tooling to know the list, a separate card.
//! Server only in practice; nothing is replicated.
class LF_IslandCalendarLoad
{
	static const string FEATURE = "Events";
	static const ResourceName CONFIG_PATH = "{519604F12B3FE66C}Configs/Events/LF_IslandCalendar.conf";

	protected static ref LF_IslandCalendarConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The core file's rows, or null (logged once) when it does not resolve.
	static LF_IslandCalendarConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(FEATURE, string.Format("island calendar config does not resolve, no row can run: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_IslandCalendarConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
		{
			LF_Debug.Error(FEATURE, string.Format("island calendar config has an unexpected root class, no row can run: %1", CONFIG_PATH));
			return null;
		}

		if (!s_Config.m_aRows)
			s_Config.m_aRows = new array<ref LF_IslandCalendarRow>();

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Forgets the loaded rows; the next Get reads the file again.
	static void ResetStatics()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_IslandCalendarLoad's statics (LF_StaticReset finds it by reflection).
class LF_IslandCalendarLoad_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_IslandCalendarLoad";
	}

	override void Apply()
	{
		LF_IslandCalendarLoad.ResetStatics();
	}
}
