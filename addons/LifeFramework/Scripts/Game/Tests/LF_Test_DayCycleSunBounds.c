// red-proof: Observed on main (#3641): the night switch took the test world's sunrise 1.55 h and sunset 13.36 h as they came, so the faster night started at 13:22. AreSunHoursSane did not exist; `--red` makes the bad hours pass as sane and the checks fail.
//! #3641: a world's own sunrise and sunset are used only inside sane bounds (sunrise 4 to 8 h, sunset 17 to 21 h);
//! anything else falls back to the vanilla hours, so the faster night never starts mid-afternoon.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_DayCycleSunBounds : LF_Test
{
	override string GetName()
	{
		return "time/sun-bounds";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.True(LF_DayCycle.AreSunHoursSane(5, 19), "the vanilla 5 h and 19 h are sane");
		ctx.True(LF_DayCycle.AreSunHoursSane(6.2, 18.4), "an ordinary Everon day is sane");
		ctx.True(LF_DayCycle.AreSunHoursSane(4, 21), "the bounds themselves are sane");

		bool testWorld = LF_DayCycle.AreSunHoursSane(1.55, 13.36);
		if (RedProof())
			testWorld = true;
		ctx.False(testWorld, "the test world's 1.55 h and 13.36 h are refused");

		ctx.False(LF_DayCycle.AreSunHoursSane(3.9, 19), "a sunrise before 4 h is refused");
		ctx.False(LF_DayCycle.AreSunHoursSane(8.1, 19), "a sunrise after 8 h is refused");
		ctx.False(LF_DayCycle.AreSunHoursSane(5, 16.9), "a sunset before 17 h is refused");
		ctx.False(LF_DayCycle.AreSunHoursSane(5, 21.1), "a sunset after 21 h is refused");
		ctx.False(LF_DayCycle.AreSunHoursSane(0, 0), "an unset 0 and 0 is refused");

		// With the fallback hours 13:22 is day, so the faster night does not start there.
		ctx.False(LF_DayCycle.IsNight(13.37, LF_DayCycle.FALLBACK_SUNRISE_HOUR, LF_DayCycle.FALLBACK_SUNSET_HOUR), "13:22 is day on the fallback hours");
		ctx.True(LF_DayCycle.IsNight(19.5, LF_DayCycle.FALLBACK_SUNRISE_HOUR, LF_DayCycle.FALLBACK_SUNSET_HOUR), "19:30 is night on the fallback hours");
	}
}
