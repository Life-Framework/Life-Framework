//------------------------------------------------------------------------------------------------
//! Scheduled-restart warner (Ops, M4). Stub from the contract slice (#1344): the game mode calls
//! StartIfConfigured() beside LF_HostStop.StartIfRequested(); the schedule and ticker slices (O5,
//! O6T) fill the body and publish LF_OnRestartWarning / LF_OnRestartDue (LF_OpsEvents.c).
class LF_RestartWarner
{
	static const string FEATURE = "Ops";

	protected static string s_sLastLog;

	//------------------------------------------------------------------------------------------------
	//! Server: starts the warning ticker when the owner configured a restart schedule.
	static void StartIfConfigured()
	{
		s_sLastLog = "restart warner: no schedule yet (stub)";
		LF_Debug.Info(FEATURE, s_sLastLog);
	}

	//------------------------------------------------------------------------------------------------
	//! The line the last StartIfConfigured() logged; tests read it.
	static string GetLastLog()
	{
		return s_sLastLog;
	}
}
