// red-proof: ran --only LF_Test_DebugWorldWeather with the SCR_TimeAndWeatherHandlerComponent removed from
// GameMode_Roleplay_Debug.et and saw "[LFTEST]   - the weather loops, so it never transitions into Cloudy"
// (SUMMARY tier=only passed=0 failed=1); restored, it passes 1/1.

//! #602: DebugWorld (the test and playtest town) holds Clear weather. The engine's Cloudy volumetric
//! clouds render as vertical streaks in this world, so the debug game mode pins the weather instead
//! of letting the stock state machine drift from Clear into Cloudy after 6 to 10 game hours.
// tier: WORLD
class LF_Test_DebugWorldWeather : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "world/debugworld-weather-pinned-clear";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		ctx.True(world != null, "a ChimeraWorld is loaded");
		if (!world)
			return;

		TimeAndWeatherManagerEntity clock = world.GetTimeAndWeatherManager();
		ctx.True(clock != null, "DebugWorld has a TimeAndWeatherManager");
		if (!clock)
			return;

		WeatherState state = clock.GetCurrentWeatherState();
		string name;
		if (state)
			name = state.GetStateName();

		ctx.EqualStr("Clear", name, "the weather starts Clear");
		ctx.True(clock.IsWeatherLooping(), "the weather loops, so it never transitions into Cloudy");
	}
}
