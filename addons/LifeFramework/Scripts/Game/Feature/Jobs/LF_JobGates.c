//------------------------------------------------------------------------------------------------
//! The testing bypass for gated jobs. A tester who is not on the police roster, has too little
//! playtime or has no approved application cannot clock in as police; with the bypass on, every
//! whitelist-gated job opens to every player (LF_WhitelistManager reports nothing restricted), so
//! one build can play police, medic or any other gated job without editing a roster file.
//!
//! The bypass is a test tool, never a server feature:
//!  - Off by default. The owner config `jobs.testUnlockAll` (LF_JobsConfig) or the mission header
//!    `m_bLF_TestUnlockAllJobs` (the playtest mission) asks for it.
//!  - A dedicated server refuses it unless the mission header also sets
//!    `m_bLF_TestUnlockOnDedicated`. Only the playtest mission does.
//!  - The flagship config and the flagship mission header never enable it
//!    (`validate-flagship-job-gates`).
//!  - It logs LF_Debug.Warn "job gates bypassed (testing)" once, so a live log shows it.
//------------------------------------------------------------------------------------------------
class LF_JobGates
{
	static const string FEATURE = "Jobs";

	//! Tests: no override, the real config and header decide.
	static const int OVERRIDE_NONE = -1;
	static const int OVERRIDE_OFF = 0;
	static const int OVERRIDE_ON = 1;

	protected static int s_iOverride = OVERRIDE_NONE;
	protected static bool s_bAnnounced;
	protected static bool s_bRefusalAnnounced;

	//------------------------------------------------------------------------------------------------
	//! Pure: whether the bypass is active.
	//! \param configOn The owner config `jobs.testUnlockAll`.
	//! \param headerOn The mission header asks for it (`m_bLF_TestUnlockAllJobs`).
	//! \param dedicated This process is a dedicated server.
	//! \param headerAllowsDedicated The mission header allows it on a dedicated server.
	static bool Resolve(bool configOn, bool headerOn, bool dedicated, bool headerAllowsDedicated)
	{
		if (!configOn && !headerOn)
			return false;

		if (dedicated && !headerAllowsDedicated)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the job gates (whitelist, roster, application, playtime) are bypassed for testing.
	static bool Unlocked()
	{
		if (s_iOverride != OVERRIDE_NONE)
			return s_iOverride == OVERRIDE_ON;

		bool configOn = false;
		LF_JobsConfig config = LF_JobsConfig.Get();
		if (config)
			configOn = config.TestUnlockAll();

		bool headerOn = false;
		bool headerAllowsDedicated = false;
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		if (header)
		{
			headerOn = header.m_bLF_TestUnlockAllJobs;
			headerAllowsDedicated = header.m_bLF_TestUnlockOnDedicated;
		}

		bool dedicated = RplSession.Mode() == RplMode.Dedicated;
		bool active = Resolve(configOn, headerOn, dedicated, headerAllowsDedicated);
		if (active && !s_bAnnounced)
		{
			s_bAnnounced = true;
			LF_Debug.Warn(FEATURE, "job gates bypassed (testing)");
		}

		if (!active && (configOn || headerOn) && !s_bRefusalAnnounced)
		{
			s_bRefusalAnnounced = true;
			LF_Debug.Error(FEATURE, "job gate bypass refused: a dedicated server needs the mission header's m_bLF_TestUnlockOnDedicated");
		}

		return active;
	}

	//------------------------------------------------------------------------------------------------
	//! Game start: evaluates the switch once so the warning is in the log before the first player joins.
	static void Announce()
	{
		Unlocked();
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: force the switch (OVERRIDE_ON or OVERRIDE_OFF), or OVERRIDE_NONE for the real inputs.
	static void UseForTest(int mode)
	{
		s_iOverride = mode;
	}

	//------------------------------------------------------------------------------------------------
	//! World end (LF_StaticReset) and tests.
	static void ResetStatics()
	{
		s_iOverride = OVERRIDE_NONE;
		s_bAnnounced = false;
		s_bRefusalAnnounced = false;
	}
}
