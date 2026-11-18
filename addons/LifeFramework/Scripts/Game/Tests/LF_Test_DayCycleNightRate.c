// red-proof: Observed 2026-11-18 on main's Apply() (nightSpeed ignored with a warning, one day duration): at 23:00 with nightSpeed 2 the manager read back 7200 s instead of 3600, and the switch API did not exist. With the switch in it passes; `--red` skips the night switch and fails.
//! #3149: the flagship's `nightSpeed` works. The pure part: the night is between sunset and sunrise and the
//! night duration is the day duration over nightSpeed. The world part: with a 120-minute day and nightSpeed 2,
//! the manager's day duration is 3600 s at 23:00 and 7200 s at 10:00 after Apply and after SwitchPhase, and
//! nightSpeed 1 never changes it. The test puts the manager's day length, auto-advance and time back.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_DayCycleNightRate : LF_Test
{
	override string GetName()
	{
		return "time/night-rate";
	}

	protected LF_DayCycleSettings Make(float nightSpeed)
	{
		LF_DayCycleConfig config = LF_DayCycleConfig.CreateDefault();
		config.m_sMode = LF_DayCycleConfig.MODE_ACCELERATED;
		config.m_iDayLengthMinutes = 120;
		config.m_fNightSpeed = nightSpeed;
		config.m_fStartHour = 10;
		LF_DayCycleSettings settings = LF_DayCycle.Resolve(config, 13);
		settings.m_bSetStartHour = false;	// the test sets the clock itself
		return settings;
	}

	override void Run(LF_TestContext ctx)
	{
		// Pure: the switch hours and the two durations.
		ctx.True(LF_DayCycle.IsNight(23, 6, 20), "23:00 is night");
		ctx.True(LF_DayCycle.IsNight(2.5, 6, 20), "02:30 is night");
		ctx.True(LF_DayCycle.IsNight(20, 6, 20), "sunset itself is night");
		ctx.False(LF_DayCycle.IsNight(6, 6, 20), "sunrise itself is day");
		ctx.False(LF_DayCycle.IsNight(10, 6, 20), "10:00 is day");
		ctx.True(LF_DayCycle.IsNight(24.5, 6, 20), "an hour past 24 wraps into the night");

		LF_DayCycleSettings two = Make(2);
		ctx.EqualFloat(7200, two.m_fDayDurationSeconds, 0.01, "nightSpeed 2: the day lasts 7200 s");
		ctx.EqualFloat(3600, two.m_fNightDurationSeconds, 0.01, "nightSpeed 2: the night runs twice as fast, 3600 s a day");
		ctx.EqualFloat(3600, LF_DayCycle.DurationForHour(two, 23, 6, 20), 0.01, "the duration at 23:00 is the night one");
		ctx.EqualFloat(7200, LF_DayCycle.DurationForHour(two, 10, 6, 20), 0.01, "the duration at 10:00 is the day one");
		ctx.True(LF_DayCycle.HasNightRate(two), "nightSpeed 2 has a night rate");

		LF_DayCycleSettings one = Make(1);
		ctx.False(LF_DayCycle.HasNightRate(one), "nightSpeed 1 has no night rate");
		ctx.EqualFloat(7200, LF_DayCycle.DurationForHour(one, 23, 6, 20), 0.01, "nightSpeed 1: night keeps the day duration");
		ctx.EqualFloat(7200, Make(0).m_fNightDurationSeconds, 0.01, "nightSpeed 0 is read as 1");

		// World: the manager's day duration follows the clock.
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		ctx.True(world != null, "a ChimeraWorld is loaded");
		if (!world)
			return;

		TimeAndWeatherManagerEntity clock = world.GetTimeAndWeatherManager();
		ctx.True(clock != null, "the world has a TimeAndWeatherManager");
		if (!clock)
			return;

		float oldDuration = clock.GetDayDuration();
		bool oldAuto = clock.GetIsDayAutoAdvanced();
		float oldTime = clock.GetTimeOfTheDay();

		// The red-proof hook skips the switch: the night then keeps the day's duration and the checks fail.
		clock.SetTimeOfTheDay(23, true);
		clock.SetDayDuration(two.m_fDayDurationSeconds);
		if (!RedProof())
			LF_DayCycleApply.SwitchPhase(two);
		ctx.EqualFloat(3600, clock.GetDayDuration(), 0.01, "at 23:00 the manager's day duration is half the day value");

		ctx.False(LF_DayCycleApply.SwitchPhase(two), "a second switch at the same hour changes nothing");

		clock.SetTimeOfTheDay(10, true);
		ctx.True(LF_DayCycleApply.SwitchPhase(two), "the switch at 10:00 flips back");
		ctx.EqualFloat(7200, clock.GetDayDuration(), 0.01, "at 10:00 the manager's day duration is the day value");

		// Apply sets the right duration for the hour it runs at, and keeps the watch only with a night rate.
		clock.SetTimeOfTheDay(23, true);
		clock.SetDayDuration(86400);
		ctx.True(LF_DayCycleApply.Apply(two, LF_DayCycleConfig.MODE_ACCELERATED), "an accelerated night day is applied");
		ctx.EqualFloat(3600, clock.GetDayDuration(), 0.01, "Apply at 23:00 starts on the night duration");
		ctx.True(LF_DayCycleApply.GetLastLog().Contains("night"), "the log names the night rate");
		LF_DayCycleApply.StopPhaseWatch();

		// nightSpeed 1 never changes it.
		clock.SetTimeOfTheDay(23, true);
		ctx.True(LF_DayCycleApply.Apply(one, LF_DayCycleConfig.MODE_ACCELERATED), "a one-speed day is applied");
		ctx.EqualFloat(7200, clock.GetDayDuration(), 0.01, "nightSpeed 1: 23:00 keeps the day value");
		ctx.False(LF_DayCycleApply.SwitchPhase(one), "nightSpeed 1: the switch changes nothing at night");
		clock.SetTimeOfTheDay(10, true);
		ctx.False(LF_DayCycleApply.SwitchPhase(one), "nightSpeed 1: the switch changes nothing by day");
		ctx.EqualFloat(7200, clock.GetDayDuration(), 0.01, "nightSpeed 1: 10:00 keeps the day value");

		LF_DayCycleApply.StopPhaseWatch();
		clock.SetDayDuration(oldDuration);
		clock.SetIsDayAutoAdvanced(oldAuto);
		clock.SetTimeOfTheDay(oldTime, true);
	}
}
