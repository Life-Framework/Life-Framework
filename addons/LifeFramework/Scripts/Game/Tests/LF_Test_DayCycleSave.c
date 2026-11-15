// red-proof: RED-OK 2026-11-15 `tools\cli test --no-build --red LF_Test_DayCycleSave`: without the continued-save override Apply set startHour, "the clock resumes at the saved 17:15, not startHour (expected 17.25, got 6.5)". Restart: `tools\cli wt e2e slice-1302` 2026-11-15, reload boot PASS time/save (5 assertions).
// tier: PERSISTENCE
//! #1302: accelerated time resumes from the save after a restart; in realtime the clock wins.
//! The time is saved by vanilla persistence (TimeAndWeatherManagerEntitySerializer), so there is no
//! LF payload; LF_DayCycleSave decides who wins on a continued save.
//!
//! In session: on a continued save an accelerated Apply keeps the clock's hour and still sets the
//! configured day length; a fresh world gets `startHour`; realtime on a continued save follows the
//! server clock; ApplyAfterLoad (the persistence ACTIVE path) puts the configured day length back
//! over the one the save restored and keeps the hour.
//!
//! Across a restart (`tools\cli wt e2e`): the save boot sets the clock to 03:00 on a 24 h day, so the
//! shutdown save carries it. The reload boot (a real -loadSessionSave) finds the clock near 03:00 and
//! an accelerated Apply with another startHour leaves it there.
class LF_Test_DayCycleSave : LF_Test
{
	protected static const float SAVED_HOUR = 3;
	protected static const float SAVED_HOUR_SLACK = 0.75;

	override string GetName()
	{
		return "time/save";
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_DayCycleConfig Make(string mode, int dayMinutes, float startHour)
	{
		LF_DayCycleConfig config = LF_DayCycleConfig.CreateDefault();
		config.m_sMode = mode;
		config.m_iDayLengthMinutes = dayMinutes;
		config.m_fStartHour = startHour;
		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		TimeAndWeatherManagerEntity clock;
		if (world)
			clock = world.GetTimeAndWeatherManager();
		ctx.True(clock != null, "the world has a TimeAndWeatherManager");
		if (!clock)
			return;

#ifdef LF_E2E_LOAD_BOOT
		RunReloadBoot(ctx, clock);
#else
		float oldDuration = clock.GetDayDuration();
		bool oldAuto = clock.GetIsDayAutoAdvanced();
		float oldTime = clock.GetTimeOfTheDay();

		RunSession(ctx, clock);

		LF_DayCycleSave.SetLoadedOverride(-1);
		LF_DayCycleSave.SetPending(null, string.Empty);
		clock.SetDayDuration(oldDuration);
		clock.SetIsDayAutoAdvanced(oldAuto);
		clock.SetTimeOfTheDay(oldTime, true);
	#ifdef LF_E2E_HOST_STOP
		SeedSaveBoot(ctx, clock);
	#endif
#endif
	}

	//------------------------------------------------------------------------------------------------
	protected void RunSession(LF_TestContext ctx, TimeAndWeatherManagerEntity clock)
	{
		ctx.False(LF_DayCycleSave.AppliesStartHour(LF_DayCycleConfig.MODE_ACCELERATED, true), "accelerated on a continued save keeps the saved hour");
		ctx.True(LF_DayCycleSave.AppliesStartHour(LF_DayCycleConfig.MODE_ACCELERATED, false), "accelerated on a fresh world sets startHour");
		ctx.True(LF_DayCycleSave.AppliesStartHour(LF_DayCycleConfig.MODE_REALTIME, true), "realtime on a continued save follows the real clock");

		LF_DayCycleConfig fast = Make(LF_DayCycleConfig.MODE_ACCELERATED, 60, 6.5);
		LF_DayCycleSettings fastSettings = LF_DayCycle.Resolve(fast, 13);

		// A continued save: the vanilla load restored 17:15 on the vanilla day.
		clock.SetDayDuration(LF_DayCycle.VANILLA_DAY_SECONDS);
		clock.SetTimeOfTheDay(17.25, true);
		if (!RedProof())
			LF_DayCycleSave.SetLoadedOverride(1);
		ctx.True(LF_DayCycleApply.Apply(fastSettings, fast.m_sMode), "an accelerated day is applied on a continued save");
		ctx.EqualFloat(17.25, clock.GetTimeOfTheDay(), 0.05, "the clock resumes at the saved 17:15, not startHour");
		ctx.EqualFloat(3600, clock.GetDayDuration(), 0.01, "the configured 60-minute day wins over the saved day length");

		// The ACTIVE path: the load landed after the apply and restored the save's day length.
		clock.SetDayDuration(LF_DayCycle.VANILLA_DAY_SECONDS);
		LF_DayCycleSave.SetPending(fastSettings, fast.m_sMode);
		ctx.True(LF_DayCycleSave.ApplyAfterLoad(), "pending settings apply after the save loads");
		ctx.EqualFloat(3600, clock.GetDayDuration(), 0.01, "after the load the configured day length is back");
		ctx.EqualFloat(17.25, clock.GetTimeOfTheDay(), 0.05, "after the load the saved hour is kept");
		ctx.False(LF_DayCycleSave.ApplyAfterLoad(), "nothing is pending the second time");

		// Realtime on a continued save: the real clock wins.
		LF_DayCycleConfig real = Make(LF_DayCycleConfig.MODE_REALTIME, 60, 6.5);
		ctx.True(LF_DayCycleApply.Apply(LF_DayCycle.Resolve(real, 10), real.m_sMode), "a realtime day is applied on a continued save");
		ctx.EqualFloat(10, clock.GetTimeOfTheDay(), 0.05, "realtime sets the server clock's 10:00 over the saved hour");

		// A fresh world: startHour.
		LF_DayCycleSave.SetLoadedOverride(0);
		ctx.True(LF_DayCycleApply.Apply(fastSettings, fast.m_sMode), "an accelerated day is applied on a fresh world");
		ctx.EqualFloat(6.5, clock.GetTimeOfTheDay(), 0.05, "a fresh world starts at startHour");
	}

	//------------------------------------------------------------------------------------------------
	//! e2e save boot: the shutdown save carries 03:00 on the vanilla day.
	protected void SeedSaveBoot(LF_TestContext ctx, TimeAndWeatherManagerEntity clock)
	{
		clock.SetDayDuration(LF_DayCycle.VANILLA_DAY_SECONDS);
		clock.SetIsDayAutoAdvanced(true);
		clock.SetTimeOfTheDay(SAVED_HOUR, true);
		ctx.EqualFloat(SAVED_HOUR, clock.GetTimeOfTheDay(), 0.05, "the save boot sets the clock to 03:00");
		LF_Debug.Info(LF_DayCycleSave.FEATURE, string.Format("e2e: the save boot set the clock to %1 h", clock.GetTimeOfTheDay()));
	}

	//------------------------------------------------------------------------------------------------
	//! e2e reload boot: the clock came back from the save, and accelerated keeps it.
	protected void RunReloadBoot(LF_TestContext ctx, TimeAndWeatherManagerEntity clock)
	{
		ctx.True(LF_DayCycleSave.IsLoadedSave(), "the reload boot continues a save point");
		float loaded = clock.GetTimeOfTheDay();
		LF_Debug.Info(LF_DayCycleSave.FEATURE, string.Format("e2e: the reload boot found the clock at %1 h", loaded));
		ctx.True(loaded >= SAVED_HOUR - 0.05 && loaded <= SAVED_HOUR + SAVED_HOUR_SLACK, string.Format("the clock resumes near the saved 03:00 (got %1 h)", loaded));

		float oldDuration = clock.GetDayDuration();
		LF_DayCycleConfig fast = Make(LF_DayCycleConfig.MODE_ACCELERATED, 120, 12);
		ctx.True(LF_DayCycleApply.Apply(LF_DayCycle.Resolve(fast, 13), fast.m_sMode), "an accelerated day is applied on the reload boot");
		ctx.EqualFloat(loaded, clock.GetTimeOfTheDay(), 0.05, "accelerated on the reload boot keeps the saved hour, not startHour 12");
		clock.SetDayDuration(oldDuration);
	}
}
