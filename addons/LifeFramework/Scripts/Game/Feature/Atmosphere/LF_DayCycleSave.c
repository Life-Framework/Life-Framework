//------------------------------------------------------------------------------------------------
//! The day cycle across a restart (Atmosphere, M4 #1302). The time of day is not an LF payload: the
//! vanilla persistence setup (Configs/Systems/Persistence/Group/Entities/Setup.conf, TimeAndWeather.conf,
//! inherited by our LifeFramework.conf) saves the TimeAndWeatherManagerEntity through
//! TimeAndWeatherManagerEntitySerializer: date, hour, minute, second, day duration and auto-advance.
//!
//! What this class decides is who wins on a boot that continues a save:
//! - `accelerated`: the save. LF_DayCycleApply skips `startHour`, so the clock resumes where it stopped.
//! - `realtime`: the real clock. The hour is set again from the server clock after the load.
//! - `vanilla`: nothing of ours runs; the vanilla load is the whole behaviour.
//! In both applied modes the configured day length wins over the one in the save (an owner may have
//! changed `dayLengthMinutes` between restarts).
//!
//! The vanilla load can land after OnGameStart, where ApplyFromConfig runs. Watch() then waits for
//! the persistence system to reach ACTIVE (vanilla's own "data is in" signal) and applies once more.
//------------------------------------------------------------------------------------------------
class LF_DayCycleSave
{
	static const string FEATURE = "Atmosphere";

	protected static ref LF_DayCycleSettings s_PendingSettings;
	protected static string s_sPendingMode;
	protected static bool s_bSubscribed;

	//! Tests: -1 reads the real save manager, 0 a fresh world, 1 a continued save.
	protected static int s_iLoadedOverride = -1;

	//------------------------------------------------------------------------------------------------
	//! \return true when this session continues a save point (-loadSessionSave or a loaded save).
	static bool IsLoadedSave()
	{
		if (s_iLoadedOverride >= 0)
			return s_iLoadedOverride == 1;

		if (!GetGame())
			return false;

		SaveGameManager manager = GetGame().GetSaveGameManager();
		return manager && manager.GetActiveSave() != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: whether the configured start hour is applied. Only an accelerated day on a continued save
	//! keeps the saved hour; realtime always follows the real clock.
	static bool AppliesStartHour(string mode, bool loadedSave)
	{
		if (mode == LF_DayCycleConfig.MODE_ACCELERATED && loadedSave)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! LF_DayCycleApply's hook: true when Apply sets `startHour` on this boot.
	static bool ShouldSetStartHour(string mode)
	{
		bool loaded = IsLoadedSave();
		bool applies = AppliesStartHour(mode, loaded);
		if (!applies)
			LF_Debug.Info(FEATURE, string.Format("day cycle: %1 on a continued save, the clock resumes from the save", mode));

		return applies;
	}

	//------------------------------------------------------------------------------------------------
	//! LF_DayCycleApply's hook after a successful Apply. On a continued save whose load has not run yet,
	//! applies the settings again when persistence reaches ACTIVE. Otherwise does nothing.
	static void Watch(notnull LF_DayCycleSettings settings, string mode)
	{
		if (!IsLoadedSave())
			return;

		SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetScriptedInstance();
		if (!persistence)
		{
			LF_Debug.Warn(FEATURE, "day cycle: continued save but no persistence system, the clock is left as applied");
			return;
		}

		if (persistence.GetState() == EPersistenceSystemState.ACTIVE)
		{
			LF_Debug.Info(FEATURE, "day cycle: the save loaded before the day cycle applied");
			return;
		}

		s_PendingSettings = settings;
		s_sPendingMode = mode;
		if (!s_bSubscribed)
		{
			persistence.GetOnStateChanged().Insert(OnPersistenceStateChanged);
			s_bSubscribed = true;
		}

		LF_Debug.Info(FEATURE, string.Format("day cycle: waiting for the save to load (persistence %1)", typename.EnumToString(EPersistenceSystemState, persistence.GetState())));
	}

	//------------------------------------------------------------------------------------------------
	protected static void OnPersistenceStateChanged(EPersistenceSystemState oldState, EPersistenceSystemState newState)
	{
		if (newState != EPersistenceSystemState.ACTIVE)
			return;

		SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetScriptedInstance();
		if (persistence && s_bSubscribed)
			persistence.GetOnStateChanged().Remove(OnPersistenceStateChanged);
		s_bSubscribed = false;

		ApplyAfterLoad();
	}

	//------------------------------------------------------------------------------------------------
	//! The pending settings over the clock the save restored. Public so a test drives it.
	//! \return true when settings were pending and applied.
	static bool ApplyAfterLoad()
	{
		LF_DayCycleSettings settings = s_PendingSettings;
		string mode = s_sPendingMode;
		s_PendingSettings = null;
		if (!settings)
			return false;

		bool applied = LF_DayCycleApply.Apply(settings, mode);
		if (applied)
			LF_Debug.Info(FEATURE, string.Format("day cycle: applied again after the save loaded (%1)", mode));
		else
			LF_Debug.Warn(FEATURE, string.Format("day cycle: could not apply again after the save loaded (%1), the clock keeps the save's day length", mode));
		return applied;
	}

	//------------------------------------------------------------------------------------------------
	//! LF_StaticReset: a world that ended before its save loaded leaves no subscription or pending apply.
	static void ResetStatics()
	{
		if (s_bSubscribed)
		{
			SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetScriptedInstance();
			if (persistence)
				persistence.GetOnStateChanged().Remove(OnPersistenceStateChanged);
		}

		s_bSubscribed = false;
		s_PendingSettings = null;
		s_sPendingMode = string.Empty;
		s_iLoadedOverride = -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: -1 reads the real save manager, 0 a fresh world, 1 a continued save.
	static void SetLoadedOverride(int loaded)
	{
		s_iLoadedOverride = loaded;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: the settings to apply at ACTIVE, as Watch() keeps them.
	static void SetPending(LF_DayCycleSettings settings, string mode)
	{
		s_PendingSettings = settings;
		s_sPendingMode = mode;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_DayCycleSave's statics (registered at script load; Core/LF_StaticReset.c).
class LF_DayCycleSave_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_DayCycleSave_StaticResetHook());

	override string Owner()
	{
		return "LF_DayCycleSave";
	}

	override void Apply()
	{
		LF_DayCycleSave.ResetStatics();
	}
}
