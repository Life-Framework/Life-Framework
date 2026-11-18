//------------------------------------------------------------------------------------------------
//! Applies the configured day cycle (start hour, day length) to the world clock (Atmosphere, M4 #1301).
//! The game mode calls ApplyFromConfig() beside LF_HostStop.StartIfRequested() on the server;
//! LF_DayCycle.Resolve (#1300) turns the config into settings and this class hands them to the vanilla
//! TimeAndWeatherManagerEntity (SetDayDuration, SetIsDayAutoAdvanced, SetTimeOfTheDay), which
//! replicates the clock to every client itself. No script loop drives the time.
//!
//! `nightSpeed` (#3149): the vanilla manager has one day duration, so the server switches it at dusk and
//! dawn. SwitchPhase() sets the day duration while the sun is up and the night duration (the day's divided by
//! `nightSpeed`) between sunset and sunrise, read from the manager (the vanilla handler's 5 h and 19 h when
//! the world has none). Apply() runs it once; a 10 s call-queue check runs it again only when the settings
//! have a night rate, and it logs once per flip. The clock still replicates through the manager.
class LF_DayCycleApply
{
	static const string FEATURE = "Atmosphere";

	static const int PHASE_CHECK_MS = 10000;	// lf-fixed: how often the server looks for dusk or dawn
	static const int RATE_CHECK_MS = 20000;	// lf-fixed: how long after start the one rate check reads the clock
	static const float RATE_TOLERANCE = 0.15;	// lf-fixed: a check within 15% of the promised rate is healthy

	protected static string s_sLastLog;
	protected static float s_fCheckStartHour;
	protected static int s_iCheckStartTick;
	protected static float s_fCheckDaySeconds;
	protected static ref LF_DayCycleSettings s_PhaseSettings;

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
		{
			ScheduleRateCheck(settings);
			LF_DayCycleSave.Watch(settings, config.m_sMode);
		}
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
		s_fCheckDaySeconds = clock.GetDayDuration();	// the phase's duration: dusk or dawn inside the 20 s skips the check
		GetGame().GetCallqueue().CallLater(CheckRate, RATE_CHECK_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	protected static void CheckRate()
	{
		TimeAndWeatherManagerEntity clock = GetClock();
		if (!clock)
			return;

		if (Math.AbsFloat(clock.GetDayDuration() - s_fCheckDaySeconds) > LF_DayCycle.DURATION_EPSILON)
		{
			LF_Debug.Info(FEATURE, "day cycle rate check: skipped, dusk or dawn changed the day duration during the check");
			return;
		}

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

		// #1302: an accelerated day on a continued save resumes from the save (LF_DayCycleSave).
		if (settings.m_bSetStartHour && LF_DayCycleSave.ShouldSetStartHour(mode))
			clock.SetTimeOfTheDay(settings.m_fStartHour);

		// #3149: night runs at nightSpeed times the day rate; the right duration for this hour goes in now.
		StartPhaseWatch(settings);

		s_sLastLog = string.Format("day cycle: %1, day %2 s (%3x), start %4 h", mode, settings.m_fDayDurationSeconds, settings.m_fDayAcceleration, settings.m_fStartHour);
		if (LF_DayCycle.HasNightRate(settings))
			s_sLastLog = s_sLastLog + string.Format(", night %1 s (%2x)", settings.m_fNightDurationSeconds, settings.m_fNightAcceleration);
		LF_Debug.Info(FEATURE, s_sLastLog);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Sets the duration for the current hour, then keeps checking at dusk and dawn when the settings have a
	//! night rate. Applying again (the save load) replaces the watch.
	protected static void StartPhaseWatch(notnull LF_DayCycleSettings settings)
	{
		StopPhaseWatch();
		SwitchPhase(settings);
		if (!LF_DayCycle.HasNightRate(settings) || !GetGame())
			return;

		s_PhaseSettings = settings;
		GetGame().GetCallqueue().CallLater(CheckPhase, PHASE_CHECK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Stops the dusk and dawn check. Tests call it so the repeating call does not outlive them.
	static void StopPhaseWatch()
	{
		if (s_PhaseSettings && GetGame())
			GetGame().GetCallqueue().Remove(CheckPhase);

		s_PhaseSettings = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static void CheckPhase()
	{
		if (!s_PhaseSettings)
			return;

		SwitchPhase(s_PhaseSettings);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: gives the manager the day duration for its current hour (the night duration after sunset
	//! and before sunrise). \return true when the duration changed; each change logs once.
	static bool SwitchPhase(notnull LF_DayCycleSettings settings)
	{
		TimeAndWeatherManagerEntity clock = GetClock();
		if (!clock)
			return false;

		float sunrise = LF_DayCycle.FALLBACK_SUNRISE_HOUR;
		float sunset = LF_DayCycle.FALLBACK_SUNSET_HOUR;
		float worldSunrise, worldSunset;
		if (clock.GetSunriseHour(worldSunrise) && clock.GetSunsetHour(worldSunset))
		{
			sunrise = worldSunrise;
			sunset = worldSunset;
		}

		float hour = clock.GetTimeOfTheDay();
		float wanted = LF_DayCycle.DurationForHour(settings, hour, sunrise, sunset);
		if (Math.AbsFloat(clock.GetDayDuration() - wanted) <= LF_DayCycle.DURATION_EPSILON)
			return false;

		if (!clock.SetDayDuration(wanted))
		{
			LF_Debug.Error(FEATURE, string.Format("day cycle: the time manager refused a %1 s day at %2 h", wanted, hour));
			return false;
		}

		string phase = "day";
		if (LF_DayCycle.IsNight(hour, sunrise, sunset))
			phase = "night";
		LF_Debug.Info(FEATURE, string.Format("day cycle: %1 at %2 h (sunrise %3, sunset %4), a day now lasts %5 s", phase, hour, sunrise, sunset, wanted));
		return true;
	}
	//------------------------------------------------------------------------------------------------
	//! The line the last ApplyFromConfig() logged; tests read it.
	static string GetLastLog()
	{
		return s_sLastLog;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_DayCycleApply's phase watch (LF_StaticReset finds it by reflection).
class LF_DayCycleApply_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_DayCycleApply";
	}

	override void Apply()
	{
		LF_DayCycleApply.StopPhaseWatch();
	}
}
