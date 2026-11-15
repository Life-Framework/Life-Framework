// red-proof: Observed 2026-11-15 with Apply() a stub returning false: `tools\cli test --no-build --only LF_Test_DayCycleApply` failed 1/1 ("the manager reads back a 60-minute day (expected 3600, got 86400)", "an accelerated day is applied"); with the manager calls in, it passes.
// tier: WORLD
//! #1301: LF_DayCycleApply hands the resolved day cycle to the world's vanilla TimeAndWeatherManagerEntity.
//! An accelerated 60-minute day reads back as 3600 s from GetDayDuration() with the clock advancing on its
//! own and the start hour set; realtime keeps a 24 h day at the server's clock; vanilla touches nothing.
//! The test puts the manager's day length, auto-advance and time back so later tests see the world it found.
class LF_Test_DayCycleApply : LF_Test
{
	override string GetName()
	{
		return "time/apply";
	}

	protected LF_DayCycleConfig Make(string mode, int dayMinutes, float startHour, int offsetMinutes)
	{
		LF_DayCycleConfig config = LF_DayCycleConfig.CreateDefault();
		config.m_sMode = mode;
		config.m_iDayLengthMinutes = dayMinutes;
		config.m_fStartHour = startHour;
		config.m_iClockOffsetMinutes = offsetMinutes;
		return config;
	}

	override void Run(LF_TestContext ctx)
	{
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

		// Accelerated: a 60-minute day, starting at 6:30.
		LF_DayCycleConfig fast = Make(LF_DayCycleConfig.MODE_ACCELERATED, 60, 6.5, 0);
		LF_DayCycleSettings fastSettings = LF_DayCycle.Resolve(fast, 13);
		ctx.True(LF_DayCycleApply.Apply(fastSettings, fast.m_sMode), "an accelerated day is applied");
		ctx.EqualFloat(3600, clock.GetDayDuration(), 0.01, "the manager reads back a 60-minute day");
		ctx.True(clock.GetIsDayAutoAdvanced(), "the clock advances on its own");
		ctx.EqualFloat(6.5, clock.GetTimeOfTheDay(), 0.05, "the day starts at startHour");
		ctx.True(LF_DayCycleApply.GetLastLog().Contains("accelerated"), "the log names the mode");

		// Realtime: the 24 h day, started at the server's clock plus the offset.
		LF_DayCycleConfig real = Make(LF_DayCycleConfig.MODE_REALTIME, 60, 6.5, 90);
		LF_DayCycleSettings realSettings = LF_DayCycle.Resolve(real, 10);
		ctx.True(LF_DayCycleApply.Apply(realSettings, real.m_sMode), "a realtime day is applied");
		ctx.EqualFloat(86400, clock.GetDayDuration(), 0.01, "realtime reads back a 24 hour day");
		ctx.EqualFloat(11.5, clock.GetTimeOfTheDay(), 0.05, "10:00 plus 90 minutes is 11:30");
		ctx.True(LF_DayCycleApply.GetLastLog().Contains("realtime"), "the log names the mode");

		// Vanilla: nothing is touched.
		clock.SetDayDuration(1800);
		LF_DayCycleConfig stock = LF_DayCycleConfig.CreateDefault();
		ctx.False(LF_DayCycleApply.Apply(LF_DayCycle.Resolve(stock, 13), stock.m_sMode), "vanilla applies nothing");
		ctx.EqualFloat(1800, clock.GetDayDuration(), 0.01, "vanilla leaves the day length alone");
		ctx.True(LF_DayCycleApply.GetLastLog().Contains("vanilla"), "the log names the mode");

		clock.SetDayDuration(oldDuration);
		clock.SetIsDayAutoAdvanced(oldAuto);
		clock.SetTimeOfTheDay(oldTime, true);
	}
}
