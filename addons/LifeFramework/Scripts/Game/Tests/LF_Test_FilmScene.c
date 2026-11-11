// red-proof: 2026-11-10, Hour == 0 -> == 1, cli test --no-build --only LF_Test_FilmScene:
//   [LFTEST]   - midnight kept, weather kept
// Restored; green.

// tier: LOGIC
class LF_Test_FilmScene : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "film/scene-env";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_FilmSceneEnv street = LF_FilmSceneEnv.ForScene("street");
		ctx.True(street.Hour == 16 && street.Weather == "Clear", "street has a fixed clock and weather");
		ctx.True(street.AutoAdvance == 0, "a scene freezes the day so a take cannot drift");

		LF_FilmSceneEnv unknown = LF_FilmSceneEnv.ForScene("nope");
		ctx.True(unknown.Hour < 0 && unknown.Weather.IsEmpty(), "an unknown scene leaves the clock and weather alone");

		ctx.True(street.Override("-", ""), "'-' and empty keep the scene values");
		ctx.True(street.Hour == 16 && street.Weather == "Clear", "keeping changes nothing");
		ctx.True(street.Override("21.5", "Overcast"), "an hour and a weather override");
		ctx.True(street.Hour == 21.5 && street.Weather == "Overcast", "the override replaces both");
		ctx.True(street.Override("0", "-"), "hour 0 is midnight, not 'unset'");
		ctx.True(street.Hour == 0 && street.Weather == "Overcast", "midnight kept, weather kept");

		ctx.False(street.Override("25", ""), "hour 25 is refused");
		ctx.False(street.Override("noon", ""), "a word is not an hour");
		ctx.False(street.Override("-3", ""), "a negative hour is refused");
		ctx.True(street.Hour == 0, "a refused override changes nothing");

		ctx.True(LF_FilmSceneEnv.LocalToGame(2, 5) == 21, "local to game wraps below 0");
		ctx.True(LF_FilmSceneEnv.GameToLocal(21, 5) == 2, "game to local wraps above 24");
		float local = 7.25;
		ctx.True(LF_FilmSceneEnv.GameToLocal(LF_FilmSceneEnv.LocalToGame(local, -3.5), -3.5) == local, "the two conversions invert each other");
	}
}
