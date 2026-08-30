// EL_Debug.c - per-feature debug logging with a stable greppable prefix.
//
// Every feature logs through EL_Debug so a validation run's console.log can be
// grepped by feature tag: [ELDebug:VehicleLock] ...  The prefix is the contract
// tools/validation and the test harness rely on; do not rename it.
//
// Enable/disable per feature with SetFeatureEnabled(). Defaults to enabled so a
// fresh checkout produces feature logs immediately; the test harness clears the
// table each boot, so a run is deterministic.

class EL_Debug
{
	protected static ref map<string, bool> s_mFeatureEnabled;
	protected static bool s_bAllEnabled = true;

	//------------------------------------------------------------------------------------------------
	static void SetAllEnabled(bool enabled)
	{
		s_bAllEnabled = enabled;
	}

	//------------------------------------------------------------------------------------------------
	static void SetFeatureEnabled(string feature, bool enabled)
	{
		if (!s_mFeatureEnabled)
			s_mFeatureEnabled = new map<string, bool>();
		s_mFeatureEnabled.Set(feature, enabled);
	}

	//------------------------------------------------------------------------------------------------
	static bool IsFeatureEnabled(string feature)
	{
		if (!s_bAllEnabled)
			return false;
		if (!s_mFeatureEnabled)
			return true;
		bool enabled;
		if (s_mFeatureEnabled.Find(feature, enabled))
			return enabled;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void Log(string feature, string message, LogLevel level = LogLevel.NORMAL)
	{
		if (!IsFeatureEnabled(feature))
			return;
		Print(string.Format("[ELDebug:%1] %2", feature, message), level);
	}

	//------------------------------------------------------------------------------------------------
	static void Info(string feature, string message)
	{
		Log(feature, message, LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	static void Warn(string feature, string message)
	{
		Log(feature, message, LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	static void Error(string feature, string message)
	{
		Log(feature, message, LogLevel.ERROR);
	}
}