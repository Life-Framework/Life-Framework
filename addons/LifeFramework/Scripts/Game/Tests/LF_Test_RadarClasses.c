// red-proof: observed 2026-11-23 with `tools\cli test --no-build --only LF_Test_RadarClasses` before LF_RadarRules existed: the suite failed to compile (`LF_RadarRules` undeclared); with the rules in, green. Every band edge is asserted (15/16 and 30/31 over), so an off-by-one fails it.
//! police/radar-classes (#4211): the radar gun's reading maps against the road's limit to one of
//! three speed classes (minor 1 to 15 over, road 16 to 30, serious over 30), each tied to a
//! law-book code; at or under the limit is clear; a metre-per-second velocity reads in whole km/h;
//! a missing limit falls back to the default instead of flagging every car.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_RadarClasses : LF_Test
{
	override string GetName()
	{
		return "police/radar-classes";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.Equal(LF_RadarRules.CLASS_CLEAR, LF_RadarRules.Classify(50, 50), "at the limit is clear");
		ctx.Equal(LF_RadarRules.CLASS_CLEAR, LF_RadarRules.Classify(20, 50), "under the limit is clear");
		ctx.Equal(LF_RadarRules.CLASS_MINOR, LF_RadarRules.Classify(51, 50), "1 over is minor");
		ctx.Equal(LF_RadarRules.CLASS_MINOR, LF_RadarRules.Classify(65, 50), "15 over is still minor");
		ctx.Equal(LF_RadarRules.CLASS_ROAD, LF_RadarRules.Classify(66, 50), "16 over is road");
		ctx.Equal(LF_RadarRules.CLASS_ROAD, LF_RadarRules.Classify(80, 50), "30 over is still road");
		ctx.Equal(LF_RadarRules.CLASS_SERIOUS, LF_RadarRules.Classify(81, 50), "31 over is serious");
		ctx.Equal(LF_RadarRules.CLASS_SERIOUS, LF_RadarRules.Classify(200, 100), "100 over a 100 limit is serious");
		ctx.Equal(LF_RadarRules.CLASS_MINOR, LF_RadarRules.Classify(40, 30), "10 over a 30 limit is minor");

		ctx.Equal(LF_RadarRules.CLASS_MINOR, LF_RadarRules.Classify(60, 0), "no limit falls back to the default (50): 10 over is minor");
		ctx.Equal(LF_RadarRules.CLASS_CLEAR, LF_RadarRules.Classify(40, -5), "a negative limit falls back to the default: 40 is clear");

		ctx.True(LF_RadarRules.LawCode(LF_RadarRules.CLASS_MINOR) != "", "minor has a law code");
		ctx.True(LF_RadarRules.LawCode(LF_RadarRules.CLASS_ROAD) != "", "road has a law code");
		ctx.True(LF_RadarRules.LawCode(LF_RadarRules.CLASS_SERIOUS) != "", "serious has a law code");
		ctx.True(LF_RadarRules.LawCode(LF_RadarRules.CLASS_MINOR) != LF_RadarRules.LawCode(LF_RadarRules.CLASS_ROAD), "minor and road have different codes");
		ctx.True(LF_RadarRules.LawCode(LF_RadarRules.CLASS_ROAD) != LF_RadarRules.LawCode(LF_RadarRules.CLASS_SERIOUS), "road and serious have different codes");
		ctx.EqualStr("", LF_RadarRules.LawCode(LF_RadarRules.CLASS_CLEAR), "clear carries no code");

		ctx.Equal(36, LF_RadarRules.ToKmh(10), "10 m/s reads 36 km/h");
		ctx.Equal(36, LF_RadarRules.ToKmh(-10), "a car coming toward the gun reads the same (negative velocity)");
		ctx.Equal(0, LF_RadarRules.ToKmh(0.1), "a parked car reads 0");
	}
}
