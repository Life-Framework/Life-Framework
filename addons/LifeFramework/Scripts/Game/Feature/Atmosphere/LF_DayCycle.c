//------------------------------------------------------------------------------------------------
//! What the engine's clock should be set to (Atmosphere, M4 #1300). Built by LF_DayCycle.Resolve;
//! LF_DayCycleApply (X2) hands it to the vanilla TimeAndWeatherManagerEntity.
class LF_DayCycleSettings
{
	//! False: leave the world's clock exactly as the mission set it (vanilla mode, or a config that cannot be applied).
	bool m_bApply;

	//! Real seconds one in-game day lasts at the daytime rate: TimeAndWeatherManagerEntity.SetDayDuration.
	float m_fDayDurationSeconds;

	//! Daytime speed against the 24 h vanilla day (a 120-minute day is 12).
	float m_fDayAcceleration;

	//! Night speed against the 24 h vanilla day: the daytime speed times `nightSpeed`.
	float m_fNightAcceleration;

	//! True when m_fStartHour is meant to be applied (SetTimeOfTheDay) on a fresh world.
	bool m_bSetStartHour;

	//! In-game hour, 0 (inclusive) to 24 (exclusive).
	float m_fStartHour;
}

//------------------------------------------------------------------------------------------------
//! The day-cycle pure function (Atmosphere, M4 #1300): `dayCycle` config in, engine settings out. It
//! reads no world, clock or singleton, so a test can run every mode. The caller passes the server's
//! real clock hour for `realtime`. Prefers the vanilla time manager's own day duration over a script
//! loop (docs/decisions.md 2026-11-08).
//------------------------------------------------------------------------------------------------
class LF_DayCycle
{
	static const string FEATURE = "DayCycle";

	//! The vanilla day, 24 real hours, in seconds: the engine's own default.
	static const float VANILLA_DAY_SECONDS = 86400;	// lf-fixed: the engine's vanilla day, not a tuning value
	static const float VANILLA_DAY_MINUTES = 1440;	// lf-fixed: the vanilla day in minutes

	//------------------------------------------------------------------------------------------------
	//! Wraps hours into [0, 24).
	static float WrapHour(float hour)
	{
		while (hour < 0)
			hour += 24;
		while (hour >= 24)
			hour -= 24;
		return hour;
	}

	//------------------------------------------------------------------------------------------------
	//! realClockHour is the server's clock as hours 0 to 24; only `realtime` reads it.
	static LF_DayCycleSettings Resolve(notnull LF_DayCycleConfig config, float realClockHour)
	{
		LF_DayCycleSettings settings = new LF_DayCycleSettings();
		settings.m_bApply = false;
		settings.m_fDayDurationSeconds = VANILLA_DAY_SECONDS;
		settings.m_fDayAcceleration = 1;
		settings.m_fNightAcceleration = 1;
		settings.m_bSetStartHour = false;
		settings.m_fStartHour = config.m_fStartHour;

		if (config.m_sMode == LF_DayCycleConfig.MODE_ACCELERATED)
		{
			if (config.m_iDayLengthMinutes < 1)
			{
				LF_Debug.Error(FEATURE, string.Format("dayCycle.dayLengthMinutes = %1 cannot be applied; keeping the vanilla day", config.m_iDayLengthMinutes));
				return settings;
			}

			float nightSpeed = config.m_fNightSpeed;
			if (nightSpeed <= 0)
				nightSpeed = 1;

			settings.m_bApply = true;
			settings.m_fDayDurationSeconds = config.m_iDayLengthMinutes * 60;
			settings.m_fDayAcceleration = VANILLA_DAY_MINUTES / config.m_iDayLengthMinutes;
			settings.m_fNightAcceleration = settings.m_fDayAcceleration * nightSpeed;
			settings.m_bSetStartHour = true;
			settings.m_fStartHour = WrapHour(config.m_fStartHour);
			return settings;
		}

		if (config.m_sMode == LF_DayCycleConfig.MODE_REALTIME)
		{
			settings.m_bApply = true;
			settings.m_bSetStartHour = true;
			settings.m_fStartHour = WrapHour(realClockHour + config.m_iClockOffsetMinutes / 60.0);
			return settings;
		}

		// vanilla, or a mode the reader should have refused: the engine is left alone.
		if (config.m_sMode != LF_DayCycleConfig.MODE_VANILLA)
			LF_Debug.Error(FEATURE, string.Format("dayCycle.mode = %1 is not vanilla, accelerated or realtime; keeping the vanilla day", config.m_sMode));

		return settings;
	}
}
