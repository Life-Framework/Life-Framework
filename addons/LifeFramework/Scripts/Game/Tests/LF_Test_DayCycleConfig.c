// red-proof: Observed 2026-11-15 before LF_DayCycle existed: `tools\cli test --no-build --only LF_Test_DayCycleConfig` failed to compile ("Unknown type 'LF_DayCycleSettings'"), so the test could not pass. Then green on the function.
// tier: LOGIC
//! The day-cycle pure function (#1300): LF_DayCycle.Resolve(config, realClockHour) turns the `dayCycle`
//! config into engine settings. Vanilla changes nothing, accelerated gives the day length, the speed
//! against the 24 h vanilla day, the night speed and the start hour (a 120-minute day is 12x), and
//! realtime keeps a 24 h day started at the server clock plus the offset.
class LF_Test_DayCycleConfig : LF_Test
{
	override string GetName()
	{
		return "time/config";
	}

	protected LF_DayCycleConfig Make(string mode, int dayMinutes, float nightSpeed, float startHour, int offsetMinutes)
	{
		LF_DayCycleConfig config = LF_DayCycleConfig.CreateDefault();
		config.m_sMode = mode;
		config.m_iDayLengthMinutes = dayMinutes;
		config.m_fNightSpeed = nightSpeed;
		config.m_fStartHour = startHour;
		config.m_iClockOffsetMinutes = offsetMinutes;
		return config;
	}

	override void Run(LF_TestContext ctx)
	{
		// The shipped default is vanilla: the engine is left alone.
		LF_DayCycleSettings stock = LF_DayCycle.Resolve(LF_DayCycleConfig.CreateDefault(), 13.5);
		ctx.False(stock.m_bApply, "vanilla leaves the clock alone");
		ctx.EqualFloat(86400, stock.m_fDayDurationSeconds, 0.01, "vanilla day is 24 real hours");
		ctx.EqualFloat(1, stock.m_fDayAcceleration, 0.0001, "vanilla runs at 1x");
		ctx.EqualFloat(1, stock.m_fNightAcceleration, 0.0001, "vanilla night runs at 1x");
		ctx.False(stock.m_bSetStartHour, "vanilla keeps the mission's start time");

		// A 120-minute accelerated day is the flagship's 12x.
		LF_DayCycleSettings fast = LF_DayCycle.Resolve(Make(LF_DayCycleConfig.MODE_ACCELERATED, 120, 1, 6.5, 0), 13.5);
		ctx.True(fast.m_bApply, "accelerated applies");
		ctx.EqualFloat(7200, fast.m_fDayDurationSeconds, 0.01, "a 120-minute day lasts 7200 real seconds");
		ctx.EqualFloat(12, fast.m_fDayAcceleration, 0.0001, "a 120-minute day is 12x");
		ctx.EqualFloat(12, fast.m_fNightAcceleration, 0.0001, "night speed 1 keeps the day rate");
		ctx.True(fast.m_bSetStartHour, "accelerated sets the start hour");
		ctx.EqualFloat(6.5, fast.m_fStartHour, 0.0001, "accelerated starts at startHour, not the real clock");

		// Night speed is a multiple of the daytime speed.
		LF_DayCycleSettings night = LF_DayCycle.Resolve(Make(LF_DayCycleConfig.MODE_ACCELERATED, 240, 2.5, 20, 0), 3);
		ctx.EqualFloat(14400, night.m_fDayDurationSeconds, 0.01, "a 240-minute day lasts 14400 real seconds");
		ctx.EqualFloat(6, night.m_fDayAcceleration, 0.0001, "a 240-minute day is 6x");
		ctx.EqualFloat(15, night.m_fNightAcceleration, 0.0001, "night speed 2.5 on 6x is 15x");
		ctx.EqualFloat(20, night.m_fStartHour, 0.0001, "start hour 20");

		// Realtime: a 24 h day that starts at the server clock plus the offset, wrapped into 0..24.
		LF_DayCycleSettings real = LF_DayCycle.Resolve(Make(LF_DayCycleConfig.MODE_REALTIME, 120, 3, 6, 90), 10);
		ctx.True(real.m_bApply, "realtime applies");
		ctx.EqualFloat(86400, real.m_fDayDurationSeconds, 0.01, "realtime day is 24 real hours");
		ctx.EqualFloat(1, real.m_fDayAcceleration, 0.0001, "realtime runs at 1x");
		ctx.EqualFloat(1, real.m_fNightAcceleration, 0.0001, "realtime ignores night speed");
		ctx.True(real.m_bSetStartHour, "realtime sets the start hour");
		ctx.EqualFloat(11.5, real.m_fStartHour, 0.0001, "10:00 plus 90 minutes is 11:30");

		LF_DayCycleSettings late = LF_DayCycle.Resolve(Make(LF_DayCycleConfig.MODE_REALTIME, 120, 1, 6, 120), 23);
		ctx.EqualFloat(1, late.m_fStartHour, 0.0001, "23:00 plus 2 h wraps to 01:00");
		LF_DayCycleSettings early = LF_DayCycle.Resolve(Make(LF_DayCycleConfig.MODE_REALTIME, 120, 1, 6, -180), 1);
		ctx.EqualFloat(22, early.m_fStartHour, 0.0001, "01:00 minus 3 h wraps to 22:00");

		// A config that slipped past the reader (bad mode, zero length) degrades to vanilla instead of dividing by zero.
		LF_DayCycleSettings bad = LF_DayCycle.Resolve(Make("sideways", 0, 1, 6, 0), 10);
		ctx.False(bad.m_bApply, "an unknown mode is vanilla");
		LF_DayCycleSettings zero = LF_DayCycle.Resolve(Make(LF_DayCycleConfig.MODE_ACCELERATED, 0, 1, 6, 0), 10);
		ctx.False(zero.m_bApply, "a zero-length day is vanilla");
	}
}
