//------------------------------------------------------------------------------------------------
//! Applies the configured day cycle (start hour, day length) to the world clock (Atmosphere, M4 #1301).
//! The game mode calls ApplyFromConfig() beside LF_HostStop.StartIfRequested() on the server;
//! LF_DayCycle.Resolve (#1300) turns the config into settings and this class hands them to the vanilla
//! TimeAndWeatherManagerEntity (SetDayDuration, SetIsDayAutoAdvanced, SetTimeOfTheDay), which
//! replicates the clock to every client itself. No script loop drives the time.
//!
//! `nightSpeed` is not applied: the vanilla manager has one day duration and no night rate, so a
//! config that asks for another night speed logs a warning and runs the day at its daytime speed.
class LF_DayCycleApply
{
	static const string FEATURE = "Atmosphere";

	static const int RATE_CHECK_MS = 20000;	// lf-fixed: how long after start the one rate check reads the clock
	static const float RATE_TOLERANCE = 0.15;	// lf-fixed: a check within 15% of the promised rate is healthy

	protected static string s_sLastLog;
	protected static float s_fCheckStartHour;
	protected static int s_iCheckStartTick;
	protected static float s_fCheckDaySeconds;

	//------------------------------------------------------------------------------------------------
	//! Server: reads the day-cycle config and applies it.
	static void ApplyFromConfig()
	{
		int hour, minute, second;
		System.GetHourMinuteSecond(hour, minute, second);
		float realClockHour = hour + minute / 60.0 + second / 3600.0;

		LF_DayCycleConfig config = LF_DayCycleConfig.Get();
		LF_DayCycleSettings settings = LF_DayCycle.Resolve(config, realClockHour);
		if (Apply(settings, config.m_sMode))
			ScheduleRateCheck(settings);
	}

	//------------------------------------------------------------------------------------------------
	protected static TimeAndWeatherManagerEntity GetClock()
	{
		ChimeraWorld world;
		if (GetGame())
			world = ChimeraWorld.CastFrom(GetGame().GetWorld());

		if (!world)
			return null;

		return world.GetTimeAndWeatherManager();
	}

	//------------------------------------------------------------------------------------------------
	//! One check, RATE_CHECK_MS after a start: how far the clock really moved against what the config
	//! promises. It catches a manager that took the day length and still does not advance. Not a loop.
	protected static void ScheduleRateCheck(notnull LF_DayCycleSettings settings)
	{
		TimeAndWeatherManagerEntity clock = GetClock();
		if (!clock || !GetGame())
			return;

		s_fCheckStartHour = clock.GetTimeOfTheDay();
		s_iCheckStartTick = System.GetTickCount();
		s_fCheckDaySeconds = settings.m_fDayDurationSeconds;
		GetGame().GetCallqueue().CallLater(CheckRate, RATE_CHECK_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	protected static void CheckRate()
	{
		TimeAndWeatherManagerEntity clock = GetClock();
		if (!clock)
			return;

		float elapsedSeconds = System.GetTickCount(s_iCheckStartTick) / 1000.0;
		float moved = clock.GetTimeOfTheDay() - s_fCheckStartHour;
		if (moved < 0)
			moved += 24;

		float expected = elapsedSeconds / s_fCheckDaySeconds * 24;
		string line = string.Format("day cycle rate check: the clock moved %1 h in %2 s, the config expects %3 h", moved, elapsedSeconds, expected);
		if (expected > 0 && Math.AbsFloat(moved - expected) > expected * RATE_TOLERANCE)
			LF_Debug.Warn(FEATURE, line);
		else
			LF_Debug.Info(FEATURE, line);
	}

	//------------------------------------------------------------------------------------------------
	//! Hands resolved settings to the world's time manager. \return true when the clock was changed.
	//! A vanilla (or unusable) config returns false and leaves the clock as the mission set it.
	static bool Apply(notnull LF_DayCycleSettings settings, string mode)
	{
		if (!settings.m_bApply)
		{
			s_sLastLog = string.Format("day cycle: %1, clock left as the mission set it", mode);
			LF_Debug.Info(FEATURE, s_sLastLog);
			return false;
		}

		if (!Replication.IsServer())
		{
			s_sLastLog = "day cycle: only the server sets the clock";
			LF_Debug.Warn(FEATURE, s_sLastLog);
			return false;
		}

		TimeAndWeatherManagerEntity clock = GetClock();
		if (!clock)
		{
			s_sLastLog = string.Format("day cycle: %1 not applied, the world has no time and weather manager", mode);
			LF_Debug.Error(FEATURE, s_sLastLog);
			return false;
		}

		if (!clock.SetDayDuration(settings.m_fDayDurationSeconds))
		{
			s_sLastLog = string.Format("day cycle: %1 not applied, the time manager refused a %2 s day", mode, settings.m_fDayDurationSeconds);
			LF_Debug.Error(FEATURE, s_sLastLog);
			return false;
		}

		// A mission whose manager has auto-advance off would show no movement at any day length.
		clock.SetIsDayAutoAdvanced(true);

		if (settings.m_bSetStartHour)
			clock.SetTimeOfTheDay(settings.m_fStartHour);

		if (Math.AbsFloat(settings.m_fNightAcceleration - settings.m_fDayAcceleration) > 0.001)
			LF_Debug.Warn(FEATURE, string.Format("day cycle: nightSpeed is not applied, the vanilla time manager has one day rate (%1x all day)", settings.m_fDayAcceleration));

		s_sLastLog = string.Format("day cycle: %1, day %2 s (%3x), start %4 h", mode, settings.m_fDayDurationSeconds, settings.m_fDayAcceleration, settings.m_fStartHour);
		LF_Debug.Info(FEATURE, s_sLastLog);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The line the last ApplyFromConfig() logged; tests read it.
	static string GetLastLog()
	{
		return s_sLastLog;
	}
}
