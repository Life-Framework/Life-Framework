//------------------------------------------------------------------------------------------------
//! What a film scene sets in the world and gives back after the take (#394): the local clock hour, the
//! weather state, and whether the day keeps advancing. Dev/harness only, like the rest of Feature/Film.
//! A scene's defaults make every take start from the same light; `scene start <name> [hour|-] [weather]`
//! overrides them for one take. The snapshot taken before a scene is what `LF_FilmEnvironment.Apply`
//! writes back on reset, so repeated takes never drift the clock.
class LF_FilmSceneEnv : Managed
{
	float Hour = -1;           // local clock hour (sun time); < 0 leaves the clock alone
	string Weather;            // weather state name; empty leaves the weather alone
	int AutoAdvance = -1;      // -1 leave, 0 freeze the day, 1 let it run
	bool Looping;             // weather looping; a snapshot records it so a reset gives it back

	//------------------------------------------------------------------------------------------------
	//! Defaults of a named scene. An unknown name returns an env that changes nothing.
	static LF_FilmSceneEnv ForScene(string scene)
	{
		LF_FilmSceneEnv env = new LF_FilmSceneEnv();
		if (scene == "street")
		{
			env.Hour = 16;
			env.Weather = "Clear";
			env.AutoAdvance = 0;
		}

		return env;
	}

	//------------------------------------------------------------------------------------------------
	//! Apply a per-take override from the harness. hour "" or "-" keeps the scene's hour; weather "" or "-"
	//! keeps its weather. Returns false (and changes nothing) for an hour that is not a number in 0..24.
	bool Override(string hour, string weather)
	{
		if (!hour.IsEmpty() && hour != "-")
		{
			if (!IsNumber(hour))
				return false;
			float h = hour.ToFloat();
			if (h < 0 || h >= 24)
				return false;
			Hour = h;
		}

		if (!weather.IsEmpty() && weather != "-")
			Weather = weather;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsNumber(string text)
	{
		int dots;
		for (int i = 0; i < text.Length(); i++)
		{
			string c = text.Get(i);
			if (c == ".")
			{
				dots++;
				if (dots > 1)
					return false;
			}
			else if (c < "0" || c > "9")
			{
				return false;
			}
		}

		return !text.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Local sun time to the game clock (the clock verb's rule: local = game + offset), wrapped into 0..24.
	static float LocalToGame(float local, float offset)
	{
		return Wrap24(local - offset);
	}

	//------------------------------------------------------------------------------------------------
	static float GameToLocal(float game, float offset)
	{
		return Wrap24(game + offset);
	}

	//------------------------------------------------------------------------------------------------
	static float Wrap24(float hour)
	{
		while (hour < 0)
			hour += 24;
		while (hour >= 24)
			hour -= 24;
		return hour;
	}

	//------------------------------------------------------------------------------------------------
	string Describe()
	{
		return string.Format("hour=%1 weather=%2 advance=%3", Hour, Weather, AutoAdvance);
	}
};

//------------------------------------------------------------------------------------------------
//! Reads and writes the world's clock and weather for a scene. Server only (the manager is replicated from there).
class LF_FilmEnvironment
{
	protected static const string FEATURE = "FilmScene";

	//------------------------------------------------------------------------------------------------
	protected static TimeAndWeatherManagerEntity GetClock()
	{
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world)
			return null;
		return world.GetTimeAndWeatherManager();
	}

	//------------------------------------------------------------------------------------------------
	protected static float Offset(TimeAndWeatherManagerEntity clock)
	{
		return clock.GetTimeZoneOffset() + clock.GetCurrentLongitude() / 15;
	}

	//------------------------------------------------------------------------------------------------
	//! The world's current environment, or null (logged) when there is no manager.
	static LF_FilmSceneEnv Capture()
	{
		TimeAndWeatherManagerEntity clock = GetClock();
		if (!clock)
		{
			LF_Debug.Error(FEATURE, "no time and weather manager: scene runs without a clock or weather");
			return null;
		}

		LF_FilmSceneEnv env = new LF_FilmSceneEnv();
		env.Hour = LF_FilmSceneEnv.GameToLocal(clock.GetTimeOfTheDay(), Offset(clock));
		WeatherState state = clock.GetCurrentWeatherState();
		if (state)
			env.Weather = state.GetStateName();
		if (clock.GetIsDayAutoAdvanced())
			env.AutoAdvance = 1;
		else
			env.AutoAdvance = 0;
		env.Looping = clock.IsWeatherLooping();
		return env;
	}

	//------------------------------------------------------------------------------------------------
	//! Write env into the world. Fields that say "leave" are skipped. Returns false when there is no manager.
	static bool Apply(LF_FilmSceneEnv env, string why)
	{
		TimeAndWeatherManagerEntity clock = GetClock();
		if (!clock || !env)
			return false;

		if (env.AutoAdvance == 0)
			clock.SetIsDayAutoAdvanced(false);
		if (env.Hour >= 0)
			clock.SetTimeOfTheDay(LF_FilmSceneEnv.LocalToGame(env.Hour, Offset(clock)), true);
		if (!env.Weather.IsEmpty())
			clock.ForceWeatherTo(env.Looping, env.Weather, 0, 3600);
		if (env.AutoAdvance == 1)
			clock.SetIsDayAutoAdvanced(true);

		LF_Debug.Info(FEATURE, string.Format("%1: %2, now hour=%3", why, env.Describe(), LF_FilmSceneEnv.GameToLocal(clock.GetTimeOfTheDay(), Offset(clock))));
		return true;
	}
};
