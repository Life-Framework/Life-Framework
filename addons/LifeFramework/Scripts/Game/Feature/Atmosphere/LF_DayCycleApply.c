//------------------------------------------------------------------------------------------------
//! Applies the configured day cycle (start hour, time speed) to the world clock (Atmosphere, M4).
//! Stub from the contract slice (#1344): the game mode calls ApplyFromConfig() beside
//! LF_HostStop.StartIfRequested(); the day-cycle slice (X2) fills the body without touching the
//! game mode.
class LF_DayCycleApply
{
	static const string FEATURE = "Atmosphere";

	protected static string s_sLastLog;

	//------------------------------------------------------------------------------------------------
	//! Server: reads the day-cycle config and applies it.
	static void ApplyFromConfig()
	{
		s_sLastLog = "day cycle: no config applied yet (stub)";
		LF_Debug.Info(FEATURE, s_sLastLog);
	}

	//------------------------------------------------------------------------------------------------
	//! The line the last ApplyFromConfig() logged; tests read it.
	static string GetLastLog()
	{
		return s_sLastLog;
	}
}
